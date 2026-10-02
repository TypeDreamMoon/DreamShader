"""-game driver of the Custom Pass demo level: one screenshot per pipeline on its own, then all of them together, then
the showcase pipelines, some of them as a burst of frames.

Build-PassDemo.ps1 -Shots runs it as  UnrealEditor.exe <host> /Game/PassDemo/L_PassDemo -game ... -ExecCmds="py <this>".
It activates what the level cannot hold -- CP_UIBackdrop and four of the showcase pipelines through the API, the cone
into list Enemies -- turns r.CustomDepth to 3 for CP_Tagged (the host does not set it), waits for every shader, then
walks SCENARIOS: each sets r.DreamPass.DisablePipelines to everything but its own pipelines, and
r.DreamPass.Visualize, runs its `before` hook, lets the picture settle and takes a HighResShot -- first the frames of
its burst, if it has one, each after the burst's `frame` hook and `every` seconds, then its shot -- and runs its
`after` hook. At the end it exports two of the exported render targets, prints DreamPass.Dump into the log and quits.

The pictures and TakeShots.txt go to DREAMSHADER_PASSDEMO_SHOTS, or <host>/Saved/PassDemo/Shots; a burst's frames go
to its frames/ folder, as <scenario>_NN.png.
"""
import os
import traceback

import unreal

SHOTS = os.environ.get('DREAMSHADER_PASSDEMO_SHOTS') or os.path.join(unreal.Paths.project_saved_dir(), 'PassDemo', 'Shots')
SHOTS = os.path.abspath(SHOTS)
FRAMES = os.path.join(SHOTS, 'frames')
SUMMARY = os.path.join(SHOTS, 'TakeShots.txt')
DEMO = '/Game/PassDemo/'

for folder in (SHOTS, FRAMES):
    os.makedirs(folder, exist_ok=True)
    for name in os.listdir(folder):
        if name.lower().endswith('.png'):
            os.remove(os.path.join(folder, name))
with open(SUMMARY, 'w', encoding='utf-8') as f:
    f.write('')

ORIGINAL = ['CP_Highlight', 'CP_XRay', 'CP_Tagged', 'CP_Scanner', 'CP_Retro', 'CP_UIBackdrop', 'CP_WindField']
# The showcase. CP_LivingWall has a volume; the others are added through the API in setup(), each with a handle for
# its parameters.
THROUGH_THE_API = ['CP_Shockwave', 'CP_Comic', 'CP_Matrix', 'CP_GodRays']
ALL = ORIGINAL + ['CP_LivingWall'] + THROUGH_THE_API

# CP_Scanner's pulse: its front is at frac(world time * Speed / Range) * Range from the camera (CP_Scanner.dsp), with
# the pipeline's default Speed and Range -- the level's volume overrides the colour only. A shot of it waits for a phase.
SCANNER_SPEED = 1500.0
SCANNER_RANGE = 4000.0

state = {'phase': 'find', 'ticks': 0, 'world': None, 'wait_until': 0.0, 'index': 0, 'shot': None,
         'shot_deadline': 0.0, 'keep': [], 'handle': None, 'handles': {}, 'queue': [], 'sun_rotation': None}


def log(message):
    unreal.log('[PassDemo] ' + message)
    with open(SUMMARY, 'a', encoding='utf-8') as f:
        f.write(message + '\n')


def console(command):
    unreal.SystemLibrary.execute_console_command(state['world'], command)


def now():
    return unreal.GameplayStatics.get_real_time_seconds(state['world'])


# ------------------------------------------------------------------------------------------------ scenario hooks

def blast(radius):
    """CP_Shockwave's front, where gameplay would put it."""
    unreal.DreamPassBlueprintLibrary.set_float_parameter(state['world'], state['handles']['CP_Shockwave'], 'Radius', radius)


def blast_frame(index, count):
    # Fast at first and slower as it grows, as a blast does.
    t = index / float(max(count - 1, 1))
    blast(40.0 + 1300.0 * (1.0 - (1.0 - t) ** 2))


def frame_rate(limit):
    """At most this many frames a second (0: no limit). CP_LivingWall steps per frame, not per second: fewer frames
    spread its growth over more of a burst."""
    console('t.MaxFPS %d' % limit)


def slow_time(dilation):
    """World time -- DP_Time -- at this rate, so that a burst's frames are close together in it."""
    unreal.GameplayStatics.set_global_time_dilation(state['world'], dilation)


def sunset(on):
    """The sun low behind the wall, and the colonnade there to stand in front of it; or the noon sun again."""
    world = state['world']
    for actor in unreal.GameplayStatics.get_all_actors_with_tag(world, 'GodRayColonnade'):
        actor.set_actor_hidden_in_game(not on)
    for sun in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.DirectionalLight):
        if on:
            state['sun_rotation'] = sun.get_actor_rotation()
            sun.set_actor_rotation(unreal.Rotator(roll=0.0, pitch=-6.0, yaw=180.0), False)
        elif state['sun_rotation'] is not None:
            sun.set_actor_rotation(state['sun_rotation'], False)


# name, the pipelines that run, r.DreamPass.Visualize, seconds to settle before the shot, CP_Scanner phase to wait for,
# hooks: before / after (no arguments) and burst = {frames, every, frame(index, count)}
SCENARIOS = [
    ('00_none', [], '', 1.0, None, {}),
    ('01_highlight', ['CP_Highlight'], '', 1.5, None, {}),
    ('02_xray', ['CP_XRay'], '', 1.0, None, {}),
    ('03_tagged', ['CP_Tagged'], '', 1.0, None, {}),
    ('04_scanner_near', ['CP_Scanner'], '', 0.5, 0.27, {}),
    ('05_scanner_far', ['CP_Scanner'], '', 0.5, 0.45, {}),
    ('06_retro', ['CP_Retro'], '', 1.0, None, {}),
    ('07_backdrop', ['CP_UIBackdrop'], 'CP_UIBackdrop.Quarter', 1.0, None, {}),
    ('08_wind', ['CP_WindField'], 'CP_WindField.Wind', 1.5, None, {}),
    ('09_highlight_mask', ['CP_Highlight'], 'CP_Highlight.Mask', 1.0, None, {}),
    ('10_all', ORIGINAL, '', 1.5, 0.27, {}),
    # The coral grows from its spores over the burst, at 12 frames a second; the shot is the grown wall.
    ('11_living_wall', ['CP_LivingWall'], '', 0.2, None,
     {'before': lambda: frame_rate(12), 'burst': {'frames': 16, 'every': 0.6}, 'after': lambda: frame_rate(0)}),
    ('12_shockwave', ['CP_Shockwave'], '', 0.5, None, {'before': lambda: blast(360.0)}),
    ('13_shockwave_blast', ['CP_Shockwave'], '', 0.3, None,
     {'burst': {'frames': 20, 'every': 0.1, 'frame': blast_frame}, 'after': lambda: blast(360.0)}),
    ('14_comic', ['CP_Comic'], '', 1.5, None, {}),
    ('15_matrix', ['CP_Matrix'], '', 1.0, None,
     {'before': lambda: slow_time(0.06), 'burst': {'frames': 16, 'every': 0.0}, 'after': lambda: slow_time(1.0)}),
    ('16_sunset', [], '', 4.0, None, {'before': lambda: sunset(True)}),
    ('17_god_rays', ['CP_GodRays'], '', 1.5, None, {'after': lambda: sunset(False)}),
]


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
    for name in THROUGH_THE_API:
        pipeline = unreal.load_asset(DEMO + name)
        state['keep'].append(pipeline)
        state['handles'][name] = unreal.DreamPassBlueprintLibrary.add_pipeline(world, pipeline, 0.0, [])
    log('%s activated through the API' % ', '.join(THROUGH_THE_API))
    listed = unreal.GameplayStatics.get_all_actors_with_tag(world, 'DemoEnemyList')
    for actor in listed:
        unreal.DreamPassBlueprintLibrary.add_to_list(world, 'Enemies', actor.static_mesh_component)
    log('list Enemies: %d primitive(s)' % len(listed))


def scanner_phase():
    seconds = unreal.GameplayStatics.get_time_seconds(state['world'])
    cycles = seconds * SCANNER_SPEED / SCANNER_RANGE
    return cycles - int(cycles)


def start_scenario():
    name, enabled, visualize, settle, _, hooks = SCENARIOS[state['index']]
    disabled = [p for p in ALL if p not in enabled]
    # A console variable set to nothing only prints its value: 'None' disables no pipeline, and a visualize target that
    # names no running pipeline draws nothing.
    console('r.DreamPass.DisablePipelines ' + (','.join(disabled) or 'None'))
    console('r.DreamPass.Visualize ' + (visualize or 'Off.Off'))
    if 'before' in hooks:
        hooks['before']()
    # The shots to take: the burst's frames, then the scenario's own. Each is (path, seconds to wait, frame hook).
    queue = []
    burst = hooks.get('burst')
    if burst:
        for index in range(burst['frames']):
            frame = burst.get('frame')
            prepare = (lambda i=index, f=frame, n=burst['frames']: f(i, n)) if frame else None
            queue.append((os.path.join(FRAMES, '%s_%02d.png' % (name, index)), settle if index == 0 else burst['every'], prepare))
    queue.append((os.path.join(SHOTS, name + '.png'), settle if not burst else burst['every'], None))
    state['queue'] = queue
    next_shot()


def next_shot():
    path, wait, prepare = state['queue'].pop(0)
    if prepare:
        prepare()
    state['shot'] = path.replace('\\', '/')
    state['wait_until'] = now() + wait
    state['phase'] = 'settle'


def take_shot():
    state['shot_deadline'] = now() + 15.0
    # Quoted: the value ends at the first space otherwise, and a project path often has one.
    console('HighResShot 1 filename="%s"' % state['shot'])
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
            # The shot lands a frame or two after the request: a window of 4% of the period (about 0.1 s) is wide
            # enough for a slow frame rate and narrow enough to put the ring where it was meant to be.
            target = SCENARIOS[state['index']][4]
            if target is not None and not (target <= scanner_phase() < target + 0.04):
                return
            take_shot()
        elif phase == 'shooting':
            if os.path.exists(state['shot']):
                log('shot ' + os.path.relpath(state['shot'], SHOTS).replace('\\', '/'))
            elif now() < state['shot_deadline']:
                return
            else:
                log('shot %s did not appear' % os.path.basename(state['shot']))
            if state['queue']:
                next_shot()
                return
            hooks = SCENARIOS[state['index']][5]
            if 'after' in hooks:
                hooks['after']()
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
