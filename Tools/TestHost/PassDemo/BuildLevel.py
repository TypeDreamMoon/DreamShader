"""Builds /Game/PassDemo/L_PassDemo, the Custom Pass demo level, in an editor commandlet (-run=pythonscript).

Build-PassDemo.ps1 runs it after compiling Sources/. Every pipeline gets something to act on, and most get their
activation here:

  CP_Highlight   unbound volume    the orange cube and the blue sphere at the wall's edge carry layer Highlight
  CP_XRay        camera component  Scope = ViewTarget; the red cylinder behind the wall has layer XRay, the red cone
                                   joins list Enemies at run time (TakeShots.py; it carries the tag DemoEnemyList)
  CP_Tagged      unbound volume    the gold cube has custom stencil 5, and its own material writes the pass's data
  CP_Scanner     unbound volume    with a PulseColor override (magenta), so the override shows
  CP_Retro       unbound volume    BlendWeight 0.5, so the weight shows
  CP_UIBackdrop  the API, at run time (TakeShots.py); the monitor panel shows its export
  CP_WindField   the project settings, from the command line (Build-PassDemo.ps1); the grass and the wind panel read
                 its export

Lines starting with [PassDemo] go to the log. An error raises, which fails the commandlet.
"""
import unreal

MAP_PATH = '/Game/PassDemo/L_PassDemo'
DEMO = '/Game/PassDemo/'

CUBE = '/Engine/BasicShapes/Cube.Cube'
SPHERE = '/Engine/BasicShapes/Sphere.Sphere'
CYLINDER = '/Engine/BasicShapes/Cylinder.Cylinder'
CONE = '/Engine/BasicShapes/Cone.Cone'
PLANE = '/Engine/BasicShapes/Plane.Plane'

actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
subobjects = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)


def log(message):
    unreal.log('[PassDemo] ' + message)


def load(path):
    asset = unreal.load_asset(path)
    if asset is None:
        raise RuntimeError('missing asset ' + path + ' -- compile Sources/ first')
    return asset


def spawn(cls, location, rotation=(0.0, 0.0, 0.0), label=None):
    """rotation is (roll, pitch, yaw) in degrees."""
    actor = actors.spawn_actor_from_class(cls, unreal.Vector(*location), unreal.Rotator(roll=rotation[0], pitch=rotation[1], yaw=rotation[2]))
    if actor is None:
        raise RuntimeError('could not spawn %s' % cls)
    if label:
        actor.set_actor_label(label)
    return actor


def mesh_actor(label, mesh, material, location, scale=(1.0, 1.0, 1.0), rotation=(0.0, 0.0, 0.0)):
    actor = spawn(unreal.StaticMeshActor, location, rotation, label)
    component = actor.static_mesh_component
    component.set_static_mesh(load(mesh))
    component.set_material(0, load(material))
    actor.set_actor_scale3d(unreal.Vector(*scale))
    return actor


def add_component(actor, cls):
    """An instance component, saved with the level (what the details panel's Add button makes)."""
    handles = subobjects.k2_gather_subobject_data_for_instance(actor)
    params = unreal.AddNewSubobjectParams(parent_handle=handles[0], new_class=cls, blueprint_context=None)
    handle, fail = subobjects.add_new_subobject(params)
    if not fail.is_empty():
        raise RuntimeError('could not add %s to %s: %s' % (cls, actor.get_actor_label(), fail))
    data = unreal.SubobjectDataBlueprintFunctionLibrary.get_data(handle)
    return unreal.SubobjectDataBlueprintFunctionLibrary.get_object(data)


def volume(label, pipeline, location, weight=1.0, overrides=None):
    actor = spawn(unreal.DreamPassVolume, location, label=label)
    actor.set_editor_property('pipeline', load(DEMO + pipeline))
    actor.set_editor_property('unbound', True)
    actor.set_editor_property('blend_weight', weight)
    if overrides:
        actor.set_editor_property('overrides', overrides)
    return actor


if assets.does_asset_exist(MAP_PATH):
    assets.delete_asset(MAP_PATH)
world = unreal.EditorLoadingAndSavingUtils.new_blank_map(False)

# Light, sky, atmosphere: all movable; the project has no static lighting.
sun = spawn(unreal.DirectionalLight, (0.0, 0.0, 800.0), (0.0, -38.0, -125.0), 'Sun')
sun.light_component.set_editor_property('atmosphere_sun_light', True)
sun.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
spawn(unreal.SkyAtmosphere, (0.0, 0.0, 0.0), label='SkyAtmosphere')
sky = spawn(unreal.SkyLight, (0.0, 0.0, 600.0), label='SkyLight')
sky.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
sky.light_component.set_editor_property('real_time_capture', True)

# The floor and the wall.
mesh_actor('Ground', PLANE, DEMO + 'M_DemoGround', (0.0, 0.0, 0.0), (60.0, 60.0, 1.0))
mesh_actor('Wall', CUBE, DEMO + 'MI_DemoWall', (250.0, 150.0, 150.0), (0.3, 7.0, 3.0))

# CP_Highlight: one in front, one straddling the wall's left edge, whose outline shows through the wall.
hero = mesh_actor('HighlightCube', CUBE, DEMO + 'MI_DemoOrange', (-50.0, -250.0, 50.0), rotation=(0.0, 0.0, 25.0))
ball = mesh_actor('HighlightSphere', SPHERE, DEMO + 'MI_DemoBlue', (330.0, -200.0, 60.0), (1.2, 1.2, 1.2))
for actor in (hero, ball):
    add_component(actor, unreal.DreamPassLayerComponent).set_editor_property('layers', ['Highlight'])

# CP_XRay: behind the wall -- one by layer, one by list.
enemy = mesh_actor('XRayCylinder', CYLINDER, DEMO + 'MI_DemoRed', (420.0, 200.0, 75.0), (0.8, 0.8, 1.5))
add_component(enemy, unreal.DreamPassLayerComponent).set_editor_property('layers', ['XRay'])
listed = mesh_actor('XRayCone', CONE, DEMO + 'MI_DemoRed', (420.0, 420.0, 60.0), (1.0, 1.0, 1.2))
listed.set_editor_property('tags', ['DemoEnemyList'])

# CP_Tagged: custom stencil 5; its own material carries UE.DreamPassOutput.
tagged = mesh_actor('TaggedCube', CUBE, DEMO + 'M_DemoTagged', (-50.0, 250.0, 60.0), (1.2, 1.2, 1.2), rotation=(0.0, 0.0, -20.0))
tagged.static_mesh_component.set_editor_property('render_custom_depth', True)
tagged.static_mesh_component.set_editor_property('custom_depth_stencil_value', 5)

# Props, for the scanner and the backdrop to pass over.
mesh_actor('PropCylinder', CYLINDER, DEMO + 'MI_DemoWhite', (100.0, -600.0, 75.0), (0.7, 0.7, 1.5))
mesh_actor('PropCube', CUBE, DEMO + 'MI_DemoWhite', (700.0, -350.0, 100.0), (2.0, 2.0, 2.0), rotation=(0.0, 0.0, 15.0))
mesh_actor('PropSphere', SPHERE, DEMO + 'MI_DemoWhite', (900.0, 600.0, 120.0), (2.4, 2.4, 2.4))

# CP_WindField: grass blades bent by the exported field, and a panel that shows the field itself.
for i in range(4):
    for j in range(3):
        mesh_actor('Grass_%d_%d' % (i, j), CUBE, DEMO + 'M_DemoFoliage', (-380.0 + 60.0 * i, -470.0 + 50.0 * j, 75.0), (0.06, 0.06, 1.5))
mesh_actor('WindPanel', PLANE, DEMO + 'M_DemoWindView', (350.0, 700.0, 180.0), (2.5, 2.5, 1.0), rotation=(0.0, 90.0, 0.0))

# CP_UIBackdrop: a monitor showing the export.
mesh_actor('BackdropMonitor', PLANE, DEMO + 'M_DemoFrosted', (150.0, -560.0, 190.0), (2.0, 2.0, 1.0), rotation=(0.0, 90.0, 0.0))

# The default pawn spawns behind the camera, out of its view.
spawn(unreal.PlayerStart, (-1200.0, 0.0, 120.0), label='PlayerStart')

# The camera the player looks through; CP_XRay rides on it.
camera = spawn(unreal.CameraActor, (-650.0, 0.0, 280.0), (0.0, -14.0, 0.0), 'DemoCamera')
camera.set_editor_property('auto_activate_for_player', unreal.AutoReceiveInput.PLAYER0)
xray = add_component(camera, unreal.DreamPassComponent)
xray.set_editor_property('pipeline', load(DEMO + 'CP_XRay'))
xray.set_editor_property('scope', unreal.DreamPassComponentScope.VIEW_TARGET)

volume('Volume_Highlight', 'CP_Highlight', (0.0, 0.0, 50.0))
volume('Volume_Tagged', 'CP_Tagged', (0.0, 100.0, 50.0))
magenta = unreal.DreamPassBlueprintLibrary.make_color_value(unreal.LinearColor(1.0, 0.25, 0.85, 1.0))
volume('Volume_Scanner', 'CP_Scanner', (0.0, 200.0, 50.0), overrides=[unreal.DreamPassParameterOverride(name='PulseColor', value=magenta)])
volume('Volume_Retro', 'CP_Retro', (0.0, 300.0, 50.0), weight=0.5)

if not unreal.EditorLoadingAndSavingUtils.save_map(world, MAP_PATH):
    raise RuntimeError('could not save ' + MAP_PATH)
log('saved ' + MAP_PATH)
