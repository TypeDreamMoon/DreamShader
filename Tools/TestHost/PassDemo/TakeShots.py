"""-game driver of the Custom Pass demo level: one screenshot per pipeline on its own, then all of them together.

Build-PassDemo.ps1 -Shots runs it as  UnrealEditor.exe <host> /Game/PassDemo/L_PassDemo -game ... -ExecCmds="py <this>".
It activates what the level cannot hold -- CP_UIBackdrop through the API, the cone into list Enemies -- turns
r.CustomDepth to 3 for CP_Tagged (the host does not set it), waits for every shader, then walks SCENARIOS: each sets
r.DreamPass.DisablePipelines to everything but its own pipelines, and r.DreamPass.Visualize, lets the picture
settle and takes a HighResShot. At the end it exports two of the exported render targets, prints DreamPass.Dump into
the log and quits.

The pictures and TakeShots.txt go to DREAMSHADER_PASSDEMO_SHOTS, or <host>/Saved/PassDemo/Shots.
"""
import os
import traceback

import unreal

SHOTS = os.environ.get('DREAMSHADER_PASSDEMO_SHOTS') or os.path.join(unreal.Paths.project_saved_dir(), 'PassDemo', 'Shots')
SHOTS = os.path.abspath(SHOTS)
SUMMARY = os.path.join(SHOTS, 'TakeShots.txt')
DEMO = '/Game/PassDemo/'

os.makedirs(SHOTS, exist_ok=True)
for name in os.listdir(SHOTS):
    if name.lower().endswith('.png'):
        os.remove(os.path.join(SHOTS, name))
with open(SUMMARY, 'w', encoding='utf-8') as f:
    f.write('')

ALL = ['CP_Highlight', 'CP_XRay', 'CP_Tagged', 'CP_Scanner', 'CP_Retro', 'CP_UIBackdrop', 'CP_WindField']

# name, the pipelines that run, r.DreamPass.Visualize, seconds to settle before the shot
SCENARIOS = [
    ('00_none', [], '', 1.0),
    ('01_highlight', ['CP_Highlight'], '', 1.5),
    ('02_xray', ['CP_XRay'], '', 1.0),
    ('03_tagged', ['CP_Tagged'], '', 1.0),
    ('04_scanner_a', ['CP_Scanner'], '', 0.5),
    ('05_scanner_b', ['CP_Scanner'], '', 0.9),
    ('06_retro', ['CP_Retro'], '', 1.0),
    ('07_backdrop', ['CP_UIBackdrop'], 'CP_UIBackdrop.Quarter', 1.0),
    ('08_wind', ['CP_WindField'], 'CP_WindField.Wind', 1.5),
    ('09_highlight_mask', ['CP_Highlight'], 'CP_Highlight.Mask', 1.0),
    ('10_all', ALL, '', 1.5),
]

state = {'phase': 'find', 'ticks': 0, 'world': None, 'wait_until': 0.0, 'index': 0, 'shot': None,
         'shot_deadline': 0.0, 'keep': [], 'handle': None}


def log(message):
    unreal.log('[PassDemo] ' + message)
    with open(SUMMARY, 'a', encoding='utf-8') as f:
        f.write(message + '\n')


def console(command):
    unreal.SystemLibrary.execute_console_command(state['world'], command)


def now():
    return unreal.GameplayStatics.get_real_time_seconds(state['world'])


def find_world():
    for world in unreal.ObjectIterator(unreal.World):
        try:
            if world.get_name() == 'L_PassDemo' and unreal.GameplayStatics.get_player_controller(world, 0):
                return world
        except Exception:  # noqa: BLE001
            continue
    return None


def setup():
    world = state['world']
    console('r.CustomDepth 3')
    backdrop = unreal.load_asset(DEMO + 'CP_UIBackdrop')
    state['keep'].append(backdrop)
    state['handle'] = unreal.DreamPassBlueprintLibrary.add_pipeline(world, backdrop, 0.0, [])
    log('CP_UIBackdrop activated through the API')
    listed = unreal.GameplayStatics.get_all_actors_with_tag(world, 'DemoEnemyList')
    for actor in listed:
        unreal.DreamPassBlueprintLibrary.add_to_list(world, 'Enemies', actor.static_mesh_component)
    log('list Enemies: %d primitive(s)' % len(listed))


def start_scenario():
    _, enabled, visualize, settle = SCENARIOS[state['index']]
    disabled = [p for p in ALL if p not in enabled]
    # A console variable set to nothing only prints its value: 'None' disables no pipeline, and a visualize target that
    # names no running pipeline draws nothing.
    console('r.DreamPass.DisablePipelines ' + (','.join(disabled) or 'None'))
    console('r.DreamPass.Visualize ' + (visualize or 'Off.Off'))
    state['wait_until'] = now() + settle
    state['phase'] = 'settle'


def take_shot():
    name = SCENARIOS[state['index']][0]
    path = os.path.join(SHOTS, name + '.png').replace('\\', '/')
    state['shot'] = path
    state['shot_deadline'] = now() + 15.0
    # Quoted: the value ends at the first space otherwise, and a project path often has one.
    console('HighResShot 1 filename="%s"' % path)
    state['phase'] = 'shooting'


def finish():
    world = state['world']
    console('r.DreamPass.DisablePipelines None')
    console('r.DreamPass.Visualize Off.Off')
    for pipeline, buffer in (('CP_UIBackdrop', 'Quarter'), ('CP_Highlight', 'Blurred')):
        target = unreal.DreamPassBlueprintLibrary.get_export_target(unreal.load_asset(DEMO + pipeline), buffer)
        if target is None:
            log('%s.%s: no export target' % (pipeline, buffer))
            continue
        log('%s.%s: export target %dx%d' % (pipeline, buffer, target.size_x, target.size_y))
        unreal.RenderingLibrary.export_render_target(world, target, SHOTS.replace('\\', '/'), 'export_%s_%s.png' % (pipeline, buffer))
    console('DreamPass.Dump')
    log('done')
    unreal.SystemLibrary.quit_game(world, None, unreal.QuitPreference.QUIT, False)


def on_tick(delta):
    state['ticks'] += 1
    try:
        phase = state['phase']
        if phase == 'find':
            state['world'] = find_world()
            if state['world'] is None:
                if state['ticks'] > 3000:
                    log('no demo world after %d ticks; quitting' % state['ticks'])
                    unreal.SystemLibrary.quit_editor()
                return
            setup()
            state['wait_until'] = now() + 2.0
            state['phase'] = 'warm'
        elif phase == 'warm':
            # Every pipeline on for a moment, so each pass material and mesh pass shader is asked for; then wait for them.
            if now() < state['wait_until']:
                return
            unreal.AutomationLibrary.finish_loading_before_screenshot()
            log('shaders ready')
            console('r.DreamPass.DisablePipelines ' + ','.join(ALL))
            state['wait_until'] = now() + 3.0
            state['phase'] = 'adapt'
        elif phase == 'adapt':
            if now() < state['wait_until']:
                return
            start_scenario()
        elif phase == 'settle':
            if now() < state['wait_until']:
                return
            take_shot()
        elif phase == 'shooting':
            if os.path.exists(state['shot']):
                log('shot ' + os.path.basename(state['shot']))
            elif now() < state['shot_deadline']:
                return
            else:
                log('shot %s did not appear' % os.path.basename(state['shot']))
            state['index'] += 1
            if state['index'] >= len(SCENARIOS):
                state['phase'] = 'done'
                finish()
            else:
                start_scenario()
    except Exception:  # noqa: BLE001
        log('driver error\n' + traceback.format_exc())
        state['phase'] = 'done'
        try:
            unreal.SystemLibrary.quit_game(state['world'], None, unreal.QuitPreference.QUIT, False)
        except Exception:  # noqa: BLE001
            unreal.SystemLibrary.quit_editor()


state['tick_handle'] = unreal.register_slate_post_tick_callback(on_tick)
