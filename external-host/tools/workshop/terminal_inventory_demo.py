# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
"""Capture actual Terminal messages, then reuse a reply field in a command preset."""
import json
from hand import Hand
from demo_setup import Measured, layout
from workshop_steps import picture, moment, chord_moments


def run(ctx):
    measured = Measured(ctx)
    hand = Hand(measured, ctx.inputs['link'])
    label = ctx.inputs['label']
    ctx.check(label.isascii() and label.isalnum() and len(label) <= 40, 'use a short ASCII word for label')
    inv, info = ('zengine.inventory-pane', 'inventory'), ('zengine.info', 'info')
    comp, term = ('zengine.composer', 'compose'), ('zengine.terminal', 'terminal')
    preset = ('zengine.info', 'info.2')

    def entries():
        return hand.ask('zengine.inventory', 'InventoryList', {})['entries']

    def read(entry):
        return hand.ask('zengine.inventory', 'InventoryRead', {'reference': entry['reference']})

    def latest(prefix):
        # The newest value the Terminal shows saying `prefix`: its row is named entry:<observation>.
        return hand.last_part(*term, 'entry:', prefix)

    def command(line):
        hand.click(hand.row(*term, '>    Tab:'))
        hand.text(line); hand.key('enter')

    def named_drop(row, name):
        before = entries()
        hand.drag(row, hand.first(*inv), 400)
        hand.last_view(hand.words(*term), 'last-terminal.json')
        hand.row(*inv, 'Name:')
        hand.text(name); hand.key('enter')
        added = [e for e in entries() if e['reference'] not in [x['reference'] for x in before]]
        ctx.check(len(added) == 1 and added[0]['label'] == name, 'capture did not store and name one independent item')
        return read(added[0])

    def rename_selected(row, name):
        # Existing context action, with fresh coordinates; no direct data-owner rename shortcut.
        hand.inject([moment(measured, 'PointerButton', button=3, pressed=p,
                            x=row['x'], y=row['y'], space=row['space']) for p in (True, False)])
        hand.inject(chord_moments(measured, 'down', repeat=2)); hand.key('enter')
        hand.text(name); hand.key('enter')

    ctx.check(not any(e['label'].startswith(label) for e in entries()), 'choose a fresh label')

    def desk(views):
        # The desk's Compose gives its place to a second Info view, once one is open: the preset is
        # finished there, field to field, while the first view holds the answer it is filled from.
        setup = layout('presets')
        panes = setup['fields']['panes']
        kept = (inv[0], info[0], 'zengine.demo') + ((comp[0],) if views else ())
        panes[:] = [p for p in panes if p['provider'] in kept]
        for p in panes:
            if p['provider'] == inv[0]:
                p['place']['y'] = str(24 * 12); p['height']['amount'] = str(15 * 12)
            if p['provider'] == comp[0]:
                p['provider'], p['pane'] = preset
        panes.append({'provider': term[0], 'pane': term[1],
                      'place': {'mode': 'pixels', 'x': '12', 'y': '0'},
                      'width': {'mode': 'pixels', 'amount': str(50 * 12)},
                      'height': {'mode': 'pixels', 'amount': str(22 * 12)}, 'front': str(len(panes)),
                      'settings': []})
        for index, pane in enumerate(panes):
            pane['front'] = str(index)
        hand.ask('zengine.workshop', 'SetupApplyRequested', {'setup': json.dumps(setup)}, settle=True)

    ctx.step('lay the Terminal out beside Inventory and Info, and open a second Info view')
    desk(False)
    # Info offers a value view the first time one is opened, so the desk can name it only then;
    # the new view opens in front, wherever Workshop finds it room, until the desk places it.
    hand.click(hand.first(*info)); hand.key('ctrl+n')
    hand.row(*preset, 'New view')
    desk(True)

    ctx.step('run an existing Terminal query and capture its real typed answer')
    command('ask @zengine.editor-switch EditorSwitchStatusRequested 1')
    reply = named_drop(latest('v EditorSwitchAnswered'), label + ' answer')
    ctx.produce('answer.bin', reply['pair'])
    ctx.step('capture an authored message without replaying it')
    command('send @zengine.powers FindPowers 2 text=' + label)
    original = named_drop(latest('^ FindPowers'), label + ' command')
    ctx.produce('command.bin', original['pair'])
    picture(ctx, ctx.inputs['link'], 'captured')

    ctx.step('make an incomplete preset from the captured command')
    hand.drag(hand.row(*inv, label + ' command', scroll=True), hand.first(*info), 400)
    hand.key('ctrl+b'); hand.click(hand.row(*info, 'text:')); hand.key('ctrl+u')
    hand.row(*info, 'text: absent')
    before = entries(); hand.key('ctrl+s')
    added = [e for e in entries() if e['reference'] not in [x['reference'] for x in before]]
    ctx.check(len(added) == 1, 'preset save did not create one entry')
    # Rename the new preset using its visible row and ordinary context action.
    rename_selected(hand.row(*inv, added[0]['label'] + ' : ', scroll=True), label + ' preset')
    incomplete = read(added[0])['pair']

    ctx.step('link the preset in the second view and fill it from the saved answer, field to field')
    hand.click(hand.row(*inv, label + ' preset', scroll=True)); hand.key('ctrl+l')
    hand.click(hand.first(*preset))
    hand.row(*preset, 'PRESET LINKED')
    hand.drag(hand.row(*inv, label + ' answer', scroll=True), hand.first(*info), 400)
    hand.row(*info, 'active:')
    hand.field(*preset, 'text')
    hand.drag(hand.field(*info, 'active'), hand.field(*preset, 'text'), 500)
    hand.row(*preset, '*text:')
    picture(ctx, ctx.inputs['link'], 'refilled')
    hand.control(*preset, 'Save')
    hand.row(*preset, 'saved rev')
    ctx.check(read(added[0])['pair'] != incomplete, 'the filled preset was not stored')
    ctx.check(read(original)['pair'] == original['pair'], 'reusing the command changed its stored source')
    ctx.check(read(reply)['pair'] == reply['pair'], 'reusing the answer changed its stored source')
    ctx.produce('result.json', json.dumps({'command': original['reference'], 'answer': reply['reference'],
        'preset': added[0]['reference'], 'entries': entries(), 'request_calls': dict(measured.calls),
        'outcomes': dict(measured.outcomes)}, indent=2).encode())
    hand.close()
    return 'Captured actual authored/reply data, named independent copies, and filled an incomplete preset from the reply in a second Info view.'
