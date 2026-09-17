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
  group = group, callback = function() notify('zengine_doc', doc_facts()) end })
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

} // namespace zengine::neovim::lua

#endif // ZENGINE_NEOVIM_LUA_HPP
