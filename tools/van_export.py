#!/usr/bin/env python3
#
#	@(#)van_export.py	1.0
#
# Blender exporter.  Writes model.obj and model.van from the SAME mesh in
# one run, which is the only way the two can be guaranteed to agree about
# what vertex number 417 is.
#
#	blender scene.blend --background --python tools/van_export.py -- \
#		--object Guard --out data/guard --start 1 --end 30 --fps 30
#
# Or paste it into Blender's text editor and press Run Script; with no
# arguments it exports the active object over the scene frame range.
#
# WHAT IT DOES NOT DO, and why:
#
#   * no bones.  The armature does its job in Blender and the result is
#     baked into vertex positions.  That is the whole point of the format:
#     the engine never learns what a skeleton is.
#   * no modifiers that change the vertex COUNT (subdivision, mirror,
#     array, decimate).  Apply them first - Ctrl+A - or the frames stop
#     lining up with the .obj and the script refuses to write.
#     Armature, shape key, lattice and hook modifiers are fine: they move
#     vertices without adding any.
#   * no scene units.  What you see in Blender is what the engine gets.
#
# Blender is Z up and the engine is Y up, so every position and normal
# goes through flip(): (x, y, z) becomes (x, z, -y).

import os
import struct
import sys

try:
    import bpy
except ImportError:
    print("van_export: this script runs inside Blender")
    raise SystemExit(1)


def flip(v):
    return (v[0], v[2], -v[1])


def parse_args():
    argv = sys.argv
    if "--" in argv:
        argv = argv[argv.index("--") + 1:]
    else:
        argv = []

    args = {
        "object": None,
        "out": "model",
        "start": None,
        "end": None,
        "fps": None,
        "normals": True,
    }
    i = 0
    while i < len(argv):
        if argv[i] == "--object":
            args["object"] = argv[i + 1]
            i += 2
        elif argv[i] == "--out":
            args["out"] = argv[i + 1]
            i += 2
        elif argv[i] == "--start":
            args["start"] = int(argv[i + 1])
            i += 2
        elif argv[i] == "--end":
            args["end"] = int(argv[i + 1])
            i += 2
        elif argv[i] == "--fps":
            args["fps"] = int(argv[i + 1])
            i += 2
        elif argv[i] == "--no-normals":
            args["normals"] = False
            i += 1
        else:
            i += 1
    return args


def write_obj(obj, path):
    """The .obj carries the topology, the UVs and the vertex ORDER.

    One "v" line per Blender vertex, in Blender's own order, so a .van
    frame is indexed by exactly that.  UVs are per loop, because a seam
    needs two of them for one position - the engine folds the duplicates
    back together at load time and remembers where each came from.
    """
    mesh = obj.data
    mesh.calc_loop_triangles()
    uv_layer = mesh.uv_layers.active

    with open(path, "w") as f:
        f.write("# written by van_export.py from %s\n" % obj.name)
        f.write("# %d vertices, Y up\n" % len(mesh.vertices))

        for v in mesh.vertices:
            p = flip(v.co)
            f.write("v %.6f %.6f %.6f\n" % p)

        if uv_layer is not None:
            for loop_index in range(len(mesh.loops)):
                uv = uv_layer.data[loop_index].uv
                f.write("vt %.6f %.6f\n" % (uv[0], uv[1]))
        else:
            f.write("vt 0.000000 0.000000\n")

        for v in mesh.vertices:
            n = flip(v.normal)
            f.write("vn %.6f %.6f %.6f\n" % n)

        material = None
        for tri in mesh.loop_triangles:
            name = "default"
            if mesh.materials and tri.material_index < len(mesh.materials):
                slot = mesh.materials[tri.material_index]
                if slot is not None:
                    name = slot.name
            if name != material:
                f.write("usemtl %s\n" % name)
                material = name

            corners = []
            for k in range(3):
                vertex = tri.vertices[k] + 1
                if uv_layer is not None:
                    uv = tri.loops[k] + 1
                else:
                    uv = 1
                corners.append("%d/%d/%d" % (vertex, uv, vertex))
            f.write("f %s %s %s\n" % tuple(corners))

    return len(mesh.vertices)


def write_van(obj, path, nverts, start, end, fps, with_normals):
    """One frame is every vertex position, in Blender's vertex order."""
    scene = bpy.context.scene
    depsgraph = bpy.context.evaluated_depsgraph_get()

    frames = []
    for frame in range(start, end + 1):
        scene.frame_set(frame)
        depsgraph = bpy.context.evaluated_depsgraph_get()
        evaluated = obj.evaluated_get(depsgraph)
        mesh = evaluated.to_mesh()

        if len(mesh.vertices) != nverts:
            evaluated.to_mesh_clear()
            raise SystemExit(
                "van_export: frame %d has %d vertices, the mesh has %d.\n"
                "A modifier is changing the vertex count - apply it first."
                % (frame, len(mesh.vertices), nverts))

        row = []
        for v in mesh.vertices:
            row.extend(flip(v.co))
            if with_normals:
                row.extend(flip(v.normal))
        frames.append(row)
        evaluated.to_mesh_clear()

    flags = 1 if with_normals else 0
    with open(path, "wb") as f:
        f.write(b"VAN1")
        f.write(struct.pack("<iiiii", nverts, len(frames), fps, flags, 0))
        for row in frames:
            f.write(struct.pack("<%df" % len(row), *row))

    return len(frames)


def main():
    args = parse_args()
    scene = bpy.context.scene

    if args["object"] is not None:
        obj = bpy.data.objects.get(args["object"])
        if obj is None:
            raise SystemExit("van_export: no object named %s"
                             % args["object"])
    else:
        obj = bpy.context.active_object
        if obj is None:
            raise SystemExit("van_export: select a mesh first")

    if obj.type != "MESH":
        raise SystemExit("van_export: %s is not a mesh" % obj.name)

    start = args["start"] if args["start"] is not None else scene.frame_start
    end = args["end"] if args["end"] is not None else scene.frame_end
    fps = args["fps"] if args["fps"] is not None else scene.render.fps

    base = args["out"]
    directory = os.path.dirname(base)
    if directory and not os.path.isdir(directory):
        os.makedirs(directory)

    nverts = write_obj(obj, base + ".obj")
    nframes = write_van(obj, base + ".van", nverts, start, end, fps,
                        args["normals"])

    size = os.path.getsize(base + ".van")
    print("van_export: %s.obj  %d vertices" % (base, nverts))
    print("van_export: %s.van  %d frames at %d fps, %d KB"
          % (base, nframes, fps, size // 1024))
    if size > 4 * 1024 * 1024:
        print("van_export: that is large.  Vertex animation costs "
              "nverts * nframes * 24 bytes; cut the frame rate before "
              "you cut the model.")


main()
