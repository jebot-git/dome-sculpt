#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Optional Blender QA; source-only meshes exclude hidden bone widgets.

Generate fixtures with DOME_VROID_GROOM_FIXTURE before running this script.
"""
import bpy, json
from pathlib import Path
from mathutils import Vector
from mathutils.bvhtree import BVHTree
import sys
args = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
ROOT = Path(args[0]).resolve() if args else Path(__file__).resolve().parents[1] / 'artifacts'
reports = []
for name in ['medium', 'long', 'fringe', 'posed', 'male']:
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete(use_global=False)
    bpy.data.orphans_purge(do_local_ids=True, do_linked_ids=True, do_recursive=True)
    bpy.ops.import_scene.gltf(filepath=str(ROOT / ('vroid-groom-' + name + '.glb')))
    hair = next((o for o in bpy.data.objects if o.type == 'MESH' and 'Original fitted groom' in o.name))
    body = [o for o in bpy.context.scene.objects if o.type == 'MESH' and o != hair and o.name.startswith('Fitted hair ')]
    deps = bpy.context.evaluated_depsgraph_get()
    vertices = []
    faces = []
    assert len(body) == 19, 'Expected nineteen actual authored source meshes'
    for obj in body:
        ev = obj.evaluated_get(deps)
        mesh = ev.to_mesh()
        offset = len(vertices)
        vertices += [obj.matrix_world @ v.co for v in mesh.vertices]
        faces += [[offset + i for i in p.vertices] for p in mesh.polygons]
        ev.to_mesh_clear()
    tree = BVHTree.FromPolygons(vertices, faces)
    ev = hair.evaluated_get(deps)
    mesh = ev.to_mesh()
    points = [hair.matrix_world @ v.co for v in mesh.vertices]
    ev.to_mesh_clear()
    direction = Vector((0.013, 0.021, 1)).normalized()
    inside = 0
    bidirectional = 0
    for p in points:
        loc, normal, index, distance = tree.ray_cast(p, direction, 10)
        if normal is not None and normal.dot(direction) > 0.001:
            inside += 1
            _, opposite, _, _ = tree.ray_cast(p, -direction, 10)
            if opposite is not None and opposite.dot(-direction) > 0.001:
                bidirectional += 1
    reports.append({'example': name, 'hair_vertices': len(points), 'inside_body_first_oriented_hit': inside, 'bidirectional_containment': bidirectional})
    print('GROOM_INTERSECTIONS', reports[-1])
    top = max((v.z for v in vertices))
    floor = min((v.z for v in vertices))
    height = top - floor
    scene = bpy.context.scene
    scene.render.engine = 'CYCLES'
    scene.cycles.samples = 32
    scene.world.color = (0.16, 0.16, 0.16)
    bpy.ops.object.camera_add(location=(0.15, -3, top - 0.07))
    camera = bpy.context.object
    camera.rotation_euler = (Vector((0, -0.08, top - height * 0.105)) - camera.location).to_track_quat('-Z', 'Y').to_euler()
    camera.data.type = 'ORTHO'
    camera.data.ortho_scale = height * 0.34
    scene.camera = camera
    scene.render.resolution_x = 768
    scene.render.resolution_y = 768
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = 'PNG'
    scene.render.filepath = str(ROOT / ('vroid-groom-' + name + '-face.png'))
    bpy.ops.render.render(write_still=True)
    camera.location = (2, -5, top * 0.98)
    camera.rotation_euler = (Vector((0, 0, floor + height * 0.52)) - camera.location).to_track_quat('-Z', 'Y').to_euler()
    camera.data.ortho_scale = height * 1.18
    scene.render.resolution_y = 900
    scene.render.filepath = str(ROOT / ('vroid-groom-' + name + '-body.png'))
    bpy.ops.render.render(write_still=True)
(ROOT / 'vroid-groom-intersections.json').write_text(json.dumps(reports, indent=2) + '\n')
