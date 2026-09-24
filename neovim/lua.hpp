// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_NEOVIM_LUA_HPP
#define ZENGINE_NEOVIM_LUA_HPP

// THE LUA A HOSTED NEOVIM RUNS FOR ZENGINE, AS TEXT -- one module, installed once per Neovim
// through `nvim_exec_lua`, under one global table (`zengine_neovim`) and one autocommand group
// (`zengine_neovim`). Nothing is written to disk and nothing in a maker's configuration is read
// or changed: a maker's own `vim.g.clipboard` is left alone, and so is every option this module
// does not set on a buffer it adopted.
//
// EVERY RECIPE HERE WAS MEASURED BEFORE IT WAS WRITTEN DOWN, on Neovim 0.11.6 and 0.12.5: adopting
// a document leaves no undo step and writes back byte-exact for LF, CRLF, with and without a final
// newline, empty, newline-only, clean and modified; a file another Neovim holds is refused by its
// swap name without a prompt, UI attached or not; the export reads every mode and position the
// conversion needs; a clipboard provider built on `rpcrequest` is answered late by the embedder
// and pastes. Suite `neovim_live` keeps each of those claims.
//
// ⚠ NON-FAST REQUESTS ARE NOT SERVED WHILE NEOVIM WAITS AT A PROMPT (measured): a "Press ENTER"
// after a broken configuration, or a typed count, holds every `nvim_exec_lua` until the prompt is
// gone. An owner therefore never waits on one of these without asking the fast `nvim_get_mode`
// beside it, and never waits unboundedly at all.

namespace zengine::neovim::lua {

/// THE MODULE. Called with one argument, the embedder's channel id; returns
/// `{ did_enter, clipboard, version = {major, minor, patch} }`.
inline constexpr const char* kModule = R"lua(
local chan = ...
local M = { chan = chan }

local function notify(event, payload)
  pcall(vim.rpcnotify, chan, event, payload)
end

local function doc_facts()
  local buf = vim.api.nvim_get_current_buf()
  return { buf = buf, name = vim.api.nvim_buf_get_name(buf), modified = vim.bo[buf].modified,
           tick = vim.api.nvim_buf_get_changedtick(buf), buftype = vim.bo[buf].buftype }
end
M.doc_facts = doc_facts

local group = vim.api.nvim_create_augroup('zengine_neovim', { clear = true })
vim.api.nvim_create_autocmd({ 'BufEnter', 'BufModifiedSet', 'TextChanged', 'TextChangedI',
                              'TextChangedP', 'BufWritePost', 'BufFilePost' }, {
  group = group, callback = function()
    -- A BUFFER LOADED WHERE NO WINDOW SHOWS IT runs its autocommands in Neovim's autocommand
    -- window, current there for the moment (measured: BufEnter fires) -- not the maker's document.
    if vim.fn.win_gettype() == 'autocmd' then return end
    notify('zengine_doc', doc_facts())
  end })
vim.api.nvim_create_autocmd('ModeChanged', {
  group = group, callback = function() notify('zengine_mode', vim.api.nvim_get_mode().mode) end })
vim.api.nvim_create_autocmd('VimLeavePre', {
  group = group, callback = function() notify('zengine_leaving', {}) end })

-- THE CLIPBOARD, bridged to the embedder -- only where the maker has not chosen a provider.
local clipboard = false
if vim.g.clipboard == nil then
  local function copy(lines, regtype) notify('zengine_clipboard_copy', { lines, regtype }) end
  local function paste()
    local ok, got = pcall(vim.rpcrequest, chan, 'zengine_clipboard_paste')
    if ok and type(got) == 'table' then return got end
    return { {}, 'v' }
  end
  vim.g.clipboard = { name = 'zengine', copy = { ['+'] = copy, ['*'] = copy },
                      paste = { ['+'] = paste, ['*'] = paste }, cache_enabled = 0 }
  clipboard = true
end

-- LOAD A FILE WITHOUT EVER PROMPTING. `bufload` of a file whose swap file already exists prints
-- Vim's multi-line ATTENTION message, and with a UI attached that message leaves Neovim at a
-- hit-enter prompt (measured; SwapExists does not fire for `bufload`). With `shortmess` A the
-- message is not given and Neovim takes the next swap name instead -- so a first choice that is
-- not `.swp` means another swap file exists: another Neovim is editing the file, or one ended
-- without cleaning up. Either way the maker decides, not this module: refused, and the buffer
-- this load made is wiped with its own swap file.
local function load_guarded(path)
  local buf = vim.fn.bufadd(path)
  if vim.api.nvim_buf_is_loaded(buf) then return buf, nil end
  local saved = vim.o.shortmess
  vim.opt.shortmess:append('A')
  local ok, err = pcall(vim.fn.bufload, buf)
  vim.o.shortmess = saved
  if not ok then
    return nil, { refused = 'load', why = tostring(err) }
  end
  local swap = vim.fn.swapname(buf)
  if swap ~= '' and not swap:match('%.swp$') then
    pcall(vim.api.nvim_buf_delete, buf, { force = true })
    return nil, { refused = 'swap',
      why = 'E325: a swap file for this file already exists (another Neovim is editing it, or one ended without removing it)' }
  end
  return buf, nil
end

-- ADOPT a transferred document as the current buffer: load it from disk (so its file facts are
-- Neovim's own and a later :w raises no changed-since-reading warning), replace its lines with
-- no undo step, set its conventions and its modified flag.
function M.adopt(path, lines, dos, eol, modified)
  if not vim.o.hidden and vim.bo.modified then
    return { refused = 'hidden', why = "'hidden' is off and the current buffer has unsaved changes" }
  end
  local buf, refused = load_guarded(path)
  if not buf then return refused end
  if vim.bo[buf].modified then
    return { refused = 'held', why = 'Neovim already holds unsaved changes to this file' }
  end
  vim.api.nvim_set_current_buf(buf)
  local levels = vim.bo[buf].undolevels
  vim.bo[buf].undolevels = -1
  vim.api.nvim_buf_set_lines(buf, 0, -1, false, lines)
  vim.bo[buf].undolevels = levels
  vim.bo[buf].fileformat = dos and 'dos' or 'unix'
  vim.bo[buf].endofline = eol
  if not eol then vim.bo[buf].fixendofline = false end
  vim.bo[buf].modified = modified
  return { buf = buf, tick = vim.api.nvim_buf_get_changedtick(buf) }
end

-- EXPORT everything a transfer, a judgment of losses and a refusal are decided from.
function M.export()
  local m = vim.api.nvim_get_mode()
  local buf = vim.api.nvim_get_current_buf()
  local win = vim.api.nvim_get_current_win()
  local c = vim.api.nvim_win_get_cursor(win)
  local v = vim.fn.getpos('v')
  local view = vim.fn.winsaveview()
  local others = {}
  for _, b in ipairs(vim.api.nvim_list_bufs()) do
    if b ~= buf and vim.api.nvim_buf_is_loaded(b) and vim.bo[b].modified then
      others[#others + 1] = { name = vim.api.nvim_buf_get_name(b), buftype = vim.bo[b].buftype }
    end
  end
  local jobs = {}
  for _, ch in ipairs(vim.api.nvim_list_chans()) do
    if ch.mode == 'terminal' and ch.buffer and vim.fn.jobwait({ ch.id }, 0)[1] == -1 then
      jobs[#jobs + 1] = { name = vim.api.nvim_buf_get_name(ch.buffer), id = ch.id }
    end
  end
  return {
    mode = m.mode, blocking = m.blocking,
    buf = buf, name = vim.api.nvim_buf_get_name(buf), buftype = vim.bo[buf].buftype,
    modified = vim.bo[buf].modified, tick = vim.api.nvim_buf_get_changedtick(buf),
    lines = vim.api.nvim_buf_get_lines(buf, 0, -1, true),
    fileformat = vim.bo[buf].fileformat, eol = vim.bo[buf].endofline,
    fixeol = vim.bo[buf].fixendofline, binary = vim.bo[buf].binary, bomb = vim.bo[buf].bomb,
    bytes = vim.fn.wordcount().bytes,
    cursor = { c[1] - 1, c[2] }, vstart = { v[2] - 1, v[3] - 1 },
    topline = view.topline, leftcol = view.leftcol, wrap = vim.wo[win].wrap,
    others = others, jobs = jobs, recording = vim.fn.reg_recording(),
    windows = #vim.api.nvim_list_wins(), tabs = #vim.api.nvim_list_tabpages(),
    listed = #vim.fn.getbufinfo({ buflisted = 1 }),
  }
end

-- PREPARE an open: load the file hidden (never shown), and say what it holds.
function M.prepare(path, rows)
  local existed = vim.fn.bufexists(path) == 1 and vim.api.nvim_buf_is_loaded(vim.fn.bufadd(path))
  local buf, refused = load_guarded(path)
  if not buf then return refused end
  return { buf = buf, name = vim.api.nvim_buf_get_name(buf), existed = existed,
           modified = vim.bo[buf].modified, tick = vim.api.nvim_buf_get_changedtick(buf),
           fileformat = vim.bo[buf].fileformat, line_count = vim.api.nvim_buf_line_count(buf),
           lines = vim.api.nvim_buf_get_lines(buf, 0, rows, false) }
end

-- LOOK AT the document from a viewport (1-based `topline`); Neovim keeps the cursor visible.
function M.view(topline, leftcol)
  return pcall(vim.fn.winrestview, { topline = topline, leftcol = leftcol })
end

-- SHOW a prepared buffer in the current window.
function M.show(buf)
  if not vim.api.nvim_buf_is_valid(buf) then
    return { refused = 'gone', why = 'the prepared buffer is gone' }
  end
  if vim.api.nvim_get_current_buf() ~= buf and not vim.o.hidden and vim.bo.modified then
    return { refused = 'hidden', why = "'hidden' is off and the current buffer has unsaved changes" }
  end
  local ok, err = pcall(vim.api.nvim_set_current_buf, buf)
  if not ok then return { refused = 'switch', why = tostring(err) } end
  return doc_facts()
end

-- DISCARD a prepared buffer nobody showed, when it holds nothing and was not there before.
function M.discard(buf, existed)
  if existed or not vim.api.nvim_buf_is_valid(buf) then return false end
  if vim.api.nvim_get_current_buf() == buf or vim.bo[buf].modified then return false end
  return pcall(vim.api.nvim_buf_delete, buf, {})
end

-- WHAT WOULD BE LOST if this Neovim ended now: modified buffers and running terminal jobs.
function M.unsaved()
  local out = { buffers = {}, jobs = {} }
  for _, b in ipairs(vim.api.nvim_list_bufs()) do
    if vim.api.nvim_buf_is_loaded(b) and vim.bo[b].modified then
      out.buffers[#out.buffers + 1] = { name = vim.api.nvim_buf_get_name(b), buftype = vim.bo[b].buftype }
    end
  end
  for _, ch in ipairs(vim.api.nvim_list_chans()) do
    if ch.mode == 'terminal' and ch.buffer and vim.fn.jobwait({ ch.id }, 0)[1] == -1 then
      out.jobs[#out.jobs + 1] = { name = vim.api.nvim_buf_get_name(ch.buffer), id = ch.id }
    end
  end
  return out
end

-- ---- TRANSFERS: a selection copied out, material dropped in, a location opened ---------------

local VISUAL = { v = 'v', V = 'V', ['\22'] = '\22', s = 'v', S = 'V', ['\19'] = '\22' }

local function before(a, b) return a[2] < b[2] or (a[2] == b[2] and a[3] < b[3]) end

-- THE SELECTION'S TEXT, AS NEOVIM'S OWN YANK WOULD HAVE IT, and nothing else moved: no register,
-- no mode, no mark. `getregion` is corrected for the two things it does not see (measured): the
-- newline a charwise selection ending on a line's end (`$`) or on an empty line includes, and a
-- `$` block running each row to its own end. A block's rows are Neovim's (a tab the edge cuts
-- becomes spaces, as its yank does). A NUL -- which `getregion` spells as a line break -- refuses.
function M.selection()
  local m = vim.api.nvim_get_mode()
  if m.blocking then return { refused = 'waiting', why = 'Neovim is waiting for input' } end
  local kind = VISUAL[m.mode:sub(1, 1)]
  if not kind then return { refused = 'mode', why = 'nothing is selected (Neovim is in mode ' .. m.mode .. ')' } end
  local buf = vim.api.nvim_get_current_buf()
  local vpos, cur = vim.fn.getpos('v'), vim.fn.getcurpos()
  local dollar = cur[5] >= vim.v.maxcol
  local exclusive = vim.o.selection == 'exclusive'
  local first, last = vpos, cur
  if before(cur, vpos) then first, last = cur, vpos end
  local opts = { type = kind }
  if kind == '\22' and dollar then
    local a = vim.fn.virtcol({ vpos[2], vpos[3] }, true)[1]
    local b = vim.fn.virtcol({ cur[2], cur[3] }, true)[1]
    local left = math.min(a, b)
    local width = 1
    for l = first[2], last[2] do width = math.max(width, vim.fn.virtcol({ l, '$' }) - left) end
    opts.type = '\22' .. width
  end
  local lines = vim.fn.getregion(vpos, cur, opts)
  for _, l in ipairs(lines) do
    if l:find('\n', 1, true) then
      return { refused = 'nul', why = 'the selection holds a NUL byte, which no editor here inserts faithfully' }
    end
  end
  local newline = false
  if kind == 'V' then
    newline = true
  elseif kind == 'v' and not exclusive and last[2] < vim.api.nvim_buf_line_count(buf) then
    -- a break exists only before a next line: on the buffer's last line the yank takes none
    local endline = vim.fn.getline(last[2])
    newline = (last == cur and dollar) or #endline == 0
  end
  local eline = vim.fn.getline(last[2])
  local width = #eline >= last[3] and (vim.str_utf_end(eline, last[3]) + 1) or 0
  local after = (kind == 'V' or newline) and { last[2] + 1, 1 } or { last[2], last[3] + width }
  return { mode = m.mode, kind = kind, lines = lines, newline = newline, buf = buf,
           tick = vim.api.nvim_buf_get_changedtick(buf), name = vim.api.nvim_buf_get_name(buf),
           buftype = vim.bo[buf].buftype, modified = vim.bo[buf].modified,
           fileformat = vim.bo[buf].fileformat,
           first = { first[2], kind == 'V' and 1 or first[3] }, last = { last[2], last[3] }, after = after }
end

-- WHERE A SCREEN CELL IS IN A WINDOW'S BUFFER, by Neovim's own forward map (`screenpos`): the
-- character drawn there, or the end of the text on that row. 1-based screen row and column.
local function cell_at(win, srow, scol)
  local info = vim.fn.getwininfo(win)[1]
  if srow < info.winrow or srow >= info.winrow + info.height then return nil, nil, 'outside the window' end
  if scol < info.wincol + info.textoff or scol >= info.wincol + info.width then
    return nil, nil, 'on the sign or number column, not the text'
  end
  local buf = vim.api.nvim_win_get_buf(win)
  local lnum = info.topline
  while lnum <= info.botline do
    local closed = vim.fn.foldclosed(lnum)
    if closed ~= -1 then
      if vim.fn.screenpos(win, lnum, 1).row == srow then return nil, nil, 'on a closed fold' end
      lnum = vim.fn.foldclosedend(lnum) + 1
    else
      local line = vim.api.nvim_buf_get_lines(buf, lnum - 1, lnum, true)[1]
      if #line == 0 then
        if vim.fn.screenpos(win, lnum, 1).row == srow then return lnum, 0 end
      else
        local col, after = 1, nil
        while col <= #line do
          local p = vim.fn.screenpos(win, lnum, col)
          local len = vim.str_utf_end(line, col) + 1
          if p.row == srow then
            if scol >= p.col and scol <= p.endcol then return lnum, col - 1 end
            after = col - 1 + len
          elseif p.row > srow then
            break
          end
          col = col + len
        end
        if after then return lnum, after end
      end
      lnum = lnum + 1
    end
  end
  return nil, nil, 'no text is under the drop'
end

-- DROP LINES INTO THE CURRENT BUFFER AS DATA, where the aimed cell is -- never as keys. Refused,
-- with nothing changed, unless the buffer is the one the pane last heard (`buf`, `tick`), the
-- screen row still reads as the pane painted it (`row_text`), and the mode is Normal, Insert, or
-- Visual/Select with the drop ON the charwise or linewise highlight (then the selection is
-- replaced); with `whole`, the lines go in whole before the landing line. One undo block: `undolevels` re-set before and after (measured: without it the drop
-- joins the previous change).
function M.drop(buf, tick, row0, col0, lines, row_text, whole)
  local m = vim.api.nvim_get_mode()
  if m.blocking then return { refused = 'waiting', why = 'Neovim is waiting for input; answer it, then drop again' } end
  if vim.api.nvim_get_current_buf() ~= buf or vim.api.nvim_buf_get_changedtick(buf) ~= tick then
    return { refused = 'moved', why = 'the buffer changed after the drop was aimed; drop it again' }
  end
  if not vim.bo[buf].modifiable or vim.bo[buf].readonly then
    return { refused = 'readonly', why = 'this buffer cannot be changed here (read-only or not modifiable)' }
  end
  local shown = {}
  for c = 1, vim.o.columns do shown[#shown + 1] = vim.fn.screenstring(row0 + 1, c) end
  if row_text ~= nil and row_text ~= vim.NIL and table.concat(shown) ~= row_text then
    return { refused = 'moved', why = 'the screen changed under the drop; drop it again' }
  end
  local lnum, byte, where = cell_at(vim.api.nvim_get_current_win(), row0 + 1, col0 + 1)
  if not lnum then return { refused = 'place', why = 'nothing was inserted there: ' .. where } end
  local k = m.mode:sub(1, 1)
  local kind = VISUAL[k]
  if whole == true then
    -- WHOLE LINES (generated code): before the line the drop landed on, joining no text.
    if kind or (m.mode ~= 'n' and m.mode ~= 'i') then
      return { refused = 'mode', why = 'generated code goes in as whole lines from Normal or Insert mode; press Escape and choose again' }
    end
    vim.o.undolevels = vim.o.undolevels
    vim.api.nvim_buf_set_lines(buf, lnum - 1, lnum - 1, true, lines)
    vim.o.undolevels = vim.o.undolevels
    pcall(vim.api.nvim_win_set_cursor, 0, { lnum, 0 })
    return { line = lnum, col = 1, end_line = lnum + #lines - 1, end_col = #lines[#lines] + 1,
             replaced = false, mode = m.mode, tick = vim.api.nvim_buf_get_changedtick(buf) }
  end
  local s_row, s_col, e_row, e_col, linewise
  if kind then
    if kind == '\22' then
      return { refused = 'block', why = 'a block selection cannot be replaced by a drop; press Escape and drop again' }
    end
    local region = vim.fn.getregionpos(vim.fn.getpos('v'), vim.fn.getcurpos(), { type = kind, eol = true })
    local first, last = region[1][1], region[#region][2]
    linewise = kind == 'V'
    local on = false
    if linewise then
      on = lnum >= first[2] and lnum <= last[2] -- the whole line is the highlight
    else
      for _, seg in ipairs(region) do
        local a, b = seg[1], seg[2]
        if a[2] == lnum and byte + 1 >= a[3] and byte + 1 <= math.max(b[3], a[3]) then on = true end
      end
    end
    if not on then
      return { refused = 'visual', why = 'in Visual mode a drop replaces the highlight only when dropped onto it; press Escape to drop elsewhere' }
    end
    s_row, s_col = first[2] - 1, first[3] - 1
    e_row = last[2] - 1
    local eline = vim.api.nvim_buf_get_lines(buf, e_row, e_row + 1, true)[1]
    if linewise then
      s_col, e_col = 0, #eline -- whole lines: set_lines replaces rows s_row..e_row
    elseif last[3] > #eline then
      -- THE SELECTION TOOK THIS LINE'S BREAK TOO (`$`, or an empty line): the replacement does.
      if e_row + 1 < vim.api.nvim_buf_line_count(buf) then e_row, e_col = e_row + 1, 0 else e_col = #eline end
    else
      e_col = last[3] - 1 + vim.str_utf_end(eline, last[3]) + 1
    end
  elseif m.mode ~= 'n' and m.mode ~= 'i' then
    return { refused = 'mode', why = 'Neovim is in mode ' .. m.mode .. '; finish or leave it (Escape), then drop again' }
  end
  vim.o.undolevels = vim.o.undolevels
  local at_row, at_col
  if kind and linewise then
    local whole = { unpack(lines) }
    if #whole > 1 and whole[#whole] == '' then table.remove(whole) end -- a linewise text's own break
    vim.api.nvim_buf_set_lines(buf, s_row, e_row + 1, true, whole)
    lines = whole
    at_row, at_col = s_row, 0
  elseif kind then
    vim.api.nvim_buf_set_text(buf, s_row, s_col, e_row, e_col, lines)
    at_row, at_col = s_row, s_col
  else
    vim.api.nvim_buf_set_text(buf, lnum - 1, byte, lnum - 1, byte, lines)
    at_row, at_col = lnum - 1, byte
  end
  vim.o.undolevels = vim.o.undolevels
  local end_row = at_row + #lines - 1
  local end_col = (#lines == 1 and at_col or 0) + #lines[#lines]
  if kind then vim.cmd('normal! \27') end
  local now = vim.api.nvim_get_mode().mode
  local cursor_col = end_col
  if now ~= 'i' and cursor_col > 0 then cursor_col = cursor_col - 1 end
  pcall(vim.api.nvim_win_set_cursor, 0, { end_row + 1, cursor_col })
  return { line = at_row + 1, col = at_col + 1, end_line = end_row + 1, end_col = end_col + 1,
           replaced = kind ~= nil, mode = m.mode, tick = vim.api.nvim_buf_get_changedtick(buf) }
end

-- WHAT A DROP NEEDS TO KNOW ABOUT THE CURRENT BUFFER: its identity, its language (for C++), and
-- its first lines (for the includes a generated function names).
function M.language(max_lines)
  local buf = vim.api.nvim_get_current_buf()
  return { buf = buf, tick = vim.api.nvim_buf_get_changedtick(buf), name = vim.api.nvim_buf_get_name(buf),
           filetype = vim.bo[buf].filetype, buftype = vim.bo[buf].buftype,
           lines = vim.api.nvim_buf_get_lines(buf, 0, max_lines, false) }
end

-- WHERE THE CARET IS, for a saved location: the file, the line, the byte, and the line's text.
function M.location()
  local buf = vim.api.nvim_get_current_buf()
  local c = vim.api.nvim_win_get_cursor(0)
  return { buf = buf, name = vim.api.nvim_buf_get_name(buf), buftype = vim.bo[buf].buftype,
           modified = vim.bo[buf].modified, line = c[1], col = c[2] + 1,
           text = vim.api.nvim_buf_get_lines(buf, c[1] - 1, c[1], true)[1] }
end

-- PUT THE CURSOR WHERE A SAVED LOCATION SAYS -- only in `buf`, unchanged since `tick`, on a line
-- that still reads as it did (`text`, a prefix when it was cut). A view change, never an edit.
function M.locate(buf, tick, line, col, text)
  if vim.api.nvim_get_current_buf() ~= buf or vim.api.nvim_buf_get_changedtick(buf) ~= tick then
    return { placed = false, why = 'the buffer moved before the location arrived' }
  end
  if line < 1 or line > vim.api.nvim_buf_line_count(buf) then
    return { placed = false, why = 'it has no line ' .. line .. ' now' }
  end
  local now = vim.api.nvim_buf_get_lines(buf, line - 1, line, true)[1]
  if text ~= nil and text ~= vim.NIL and text ~= '' and now:sub(1, #text) ~= text then
    return { placed = false, why = 'line ' .. line .. ' no longer reads as it did when the location was saved' }
  end
  local ok = pcall(vim.api.nvim_win_set_cursor, 0, { line, math.max(0, math.min(col - 1, #now)) })
  return { placed = ok }
end

_G.zengine_neovim = M
local version = vim.version()
return { did_enter = vim.v.vim_did_enter, clipboard = clipboard,
         version = { version.major, version.minor, version.patch } }
)lua";

/// The calls an owner makes after the module is installed, each one `nvim_exec_lua` chunk.
inline constexpr const char* kExport = "return zengine_neovim.export()";
inline constexpr const char* kAdopt = "return zengine_neovim.adopt(...)";
inline constexpr const char* kPrepare = "return zengine_neovim.prepare(...)";
inline constexpr const char* kShow = "return zengine_neovim.show(...)";
inline constexpr const char* kDiscard = "return zengine_neovim.discard(...)";
inline constexpr const char* kUnsaved = "return zengine_neovim.unsaved()";
inline constexpr const char* kDocFacts = "return zengine_neovim.doc_facts()";
inline constexpr const char* kView = "return zengine_neovim.view(...)";
inline constexpr const char* kSelection = "return zengine_neovim.selection()";
inline constexpr const char* kDrop = "return zengine_neovim.drop(...)";
inline constexpr const char* kLanguage = "return zengine_neovim.language(...)";
inline constexpr const char* kLocation = "return zengine_neovim.location()";
inline constexpr const char* kLocate = "return zengine_neovim.locate(...)";

} // namespace zengine::neovim::lua

#endif // ZENGINE_NEOVIM_LUA_HPP
