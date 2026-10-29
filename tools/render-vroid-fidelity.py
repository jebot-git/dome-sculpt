#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Offline Blender QA, never part of the native application runtime.

Run Blender --background --factory-startup --python this_file -- [artifact_dir].
Compare standard glTF unlit source/export appearance, not full MToon parity.
Generate the native GLBs with DOME_VROID_SCENE_FIXTURE before running this tool.
"""
from pathlib import Path
import sys

import bpy
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[1]
arguments = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
ARTIFACTS = Path(arguments[0]).resolve() if arguments else ROOT / 'artifacts'
ARTIFACTS.mkdir(parents=True, exist_ok=True)


def import_model(gender, native):
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete(use_global=False)
    suffix = 'native' if native else 'source'
    path = ARTIFACTS / f'vroid-{gender}-{suffix}.glb'
    if native:
        if not path.is_file():
            raise FileNotFoundError('Generate the actual native Scene export: ' + str(path))
    else:
        source = ROOT / 'assets/humanoid/vroid' / f'base-{gender}.vrm'
        path.write_bytes(source.read_bytes())
    bpy.ops.import_scene.gltf(filepath=str(path))
    points = []
    depsgraph = bpy.context.evaluated_depsgraph_get()
    for obj in bpy.data.objects:
        # The importer creates hidden armature bone widgets in glTF_not_exported.
        if obj.type != 'MESH' or not obj.visible_get():
            continue
        evaluated = obj.evaluated_get(depsgraph)
        mesh = evaluated.to_mesh()
        points.extend(obj.matrix_world @ vertex.co for vertex in mesh.vertices)
        evaluated.to_mesh_clear()
    floor = min(point.z for point in points)
    top = max(point.z for point in points)
    return floor, top


def aim(obj, target):
    obj.rotation_euler = (Vector(target) - obj.location).to_track_quat('-Z', 'Y').to_euler()


for gender in ['female', 'male']:
    for native in [False, True]:
        floor, top = import_model(gender, native)
        height = top - floor
        scene = bpy.context.scene
        scene.render.engine = 'CYCLES'
        scene.cycles.samples = 64
        scene.cycles.seed = 7
        scene.world.color = (.16, .16, .16)
        # Rotate reference cameras/lights with the VRM0 -> native proper Y turn.
        sign = -1 if native else 1
        lights = [((3 * sign, 4 * sign, 4), 700, 4),
                  ((-3 * sign, 2 * sign, 2), 450, 3),
                  ((0, -3 * sign, 4), 900, 3)]
        for location, power, size in lights:
            bpy.ops.object.light_add(type='AREA', location=location)
            light = bpy.context.object
            light.data.energy = power
            light.data.size = size
            aim(light, (0, 0, 1))
        bpy.ops.object.camera_add(location=(-sign * .15, sign * 3, top - .07))
        camera = bpy.context.object
        aim(camera, (0, sign * .08, top - height * .105))
        camera.data.type = 'ORTHO'
        camera.data.ortho_scale = height * .34
        scene.camera = camera
        scene.render.resolution_x = 640
        scene.render.resolution_y = 640
        scene.render.resolution_percentage = 100
        scene.render.image_settings.file_format = 'PNG'
        prefix = f'vroid-{gender}-' + ('native' if native else 'source')
        scene.render.filepath = str(ARTIFACTS / (prefix + '-face.png'))
        bpy.ops.render.render(write_still=True)
        camera.location = (-sign * 2, sign * 5, top * .98)
        aim(camera, (0, 0, floor + height * .52))
        camera.data.ortho_scale = height * 1.18
        scene.render.resolution_y = 840
        scene.render.filepath = str(ARTIFACTS / (prefix + '-body.png'))
        bpy.ops.render.render(write_still=True)
