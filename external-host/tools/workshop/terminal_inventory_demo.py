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

    def entries():
        return hand.ask('zengine.inventory', 'InventoryList', {})['entries']

    def read(entry):
        return hand.ask('zengine.inventory', 'InventoryRead', {'reference': entry['reference']})

    def latest(prefix):
        rows = [r for r in hand.view(*term)['rows'] if r['text'].startswith(prefix)]
        ctx.check(bool(rows), 'latest terminal entry is not visible: ' + prefix)
        return rows[-1]

    def command(line):
        hand.click(hand.row(*term, '>    Tab:'))
        hand.text(line); hand.key('enter')

    def named_drop(row, name):
        before = entries()
        hand.drag(row, hand.view(*inv)['rows'][0], 400)
        ctx.produce('last-terminal.json', json.dumps(hand.view(*term).fields, indent=2).encode())
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
    ctx.step('select the real skin as the receiving command target')
    hand.click(hand.row('zengine.introspection', 'loaded', 'zengine-skin-', scroll=True))
    hand.click(hand.row(*comp, 'SurfaceText v1', scroll=True))
    setup = layout('presets')
    panes = setup['fields']['panes']
    panes[:] = [p for p in panes if p['provider'] in (inv[0], info[0], comp[0], 'zengine.demo')]
    for p in panes:
        if p['provider'] == inv[0]:
            p['place']['y'] = str(26 * 48); p['height']['amount'] = str(15 * 48)
    panes.append({'provider': term[0], 'pane': term[1],
                  'place': {'mode': 'subcells', 'x': '48', 'y': str(2 * 48)},
                  'width': {'mode': 'subcells', 'amount': str(50 * 48)},
                  'height': {'mode': 'subcells', 'amount': str(22 * 48)}, 'front': str(len(panes))})
    for index, pane in enumerate(panes):
        pane['front'] = str(index)
    hand.ask('zengine.workshop', 'SetupApplyRequested', {'setup': json.dumps(setup)}, settle=True)

    ctx.step('run an existing Terminal query and capture its real typed answer')
    command('ask @zengine.editor-switch EditorSwitchStatusRequested 1')
    reply = named_drop(latest('v EditorSwitchAnswered'), label + ' answer')
    ctx.produce('answer.bin', reply['pair'])
    ctx.step('capture an authored message without replaying it')
    command('send @zengine.skin SurfaceText 1 slot=score text=' + label)
    original = named_drop(latest('^ SurfaceText'), label + ' command')
    ctx.produce('command.bin', original['pair'])
    picture(ctx, ctx.inputs['link'], 'captured')

    ctx.step('make an incomplete preset from the captured command')
    hand.drag(hand.row(*inv, label + ' command', scroll=True), hand.view(*info)['rows'][0], 400)
    hand.key('ctrl+b'); hand.click(hand.row(*info, 'text:')); hand.key('ctrl+u')
    hand.row(*info, 'text: absent (required)')
    before = entries(); hand.key('ctrl+s')
    added = [e for e in entries() if e['reference'] not in [x['reference'] for x in before]]
    ctx.check(len(added) == 1, 'preset save did not create one entry')
    # Rename the new preset using its visible row and ordinary context action.
    rename_selected(hand.row(*inv, added[0]['label'] + ' : ', scroll=True), label + ' preset')
    hand.drag(hand.row(*inv, label + ' preset', scroll=True), hand.view(*comp)['rows'][0], 400)
    hand.row(*comp, 'text:')

    ctx.step('fill the preset from the saved answer through Info')
    hand.drag(hand.row(*inv, label + ' answer', scroll=True), hand.view(*info)['rows'][0], 400)
    field = hand.row(*info, 'active:')
    hand.click(field); hand.key('ctrl+g')
    hand.click(hand.row(*comp, 'text:'))
    hand.row(*comp, 'Copied data into form')
    ctx.check(read(original)['pair'] == original['pair'], 'reusing the command changed its stored source')
    ctx.check(read(reply)['pair'] == reply['pair'], 'reusing the answer changed its stored source')
    picture(ctx, ctx.inputs['link'], 'refilled')
    # This guest has inventory/capture power, not SurfaceText execution power. Submission must
    # refuse without silently borrowing the Terminal participant's grant.
    hand.key('ctrl+enter')
    hand.row(*comp, 'no authority')
    ctx.produce('result.json', json.dumps({'command': original['reference'], 'answer': reply['reference'],
        'preset': added[0]['reference'], 'entries': entries(), 'request_calls': dict(measured.calls),
        'outcomes': dict(measured.outcomes)}, indent=2).encode())
    hand.close()
    return 'Captured actual authored/reply data, named independent copies, refilled a preset, and refused execution without current authority.'
