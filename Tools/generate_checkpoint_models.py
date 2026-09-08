from pathlib import Path
import math


ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "resources" / "Models" / "Checkpoint"
OUT.mkdir(parents=True, exist_ok=True)


def add_cylinder(vertices, faces, y_min, y_max, radius, sides):
    start = len(vertices) + 1
    for y in (y_min, y_max):
        for index in range(sides):
            angle = math.tau * index / sides
            vertices.append((math.cos(angle) * radius, y, math.sin(angle) * radius))

    faces.append(tuple(start + index for index in reversed(range(sides))))
    faces.append(tuple(start + sides + index for index in range(sides)))
    for index in range(sides):
        next_index = (index + 1) % sides
        faces.append((
            start + index,
            start + next_index,
            start + sides + next_index,
            start + sides + index,
        ))


def add_sphere(vertices, faces, center_y, radius, rings, sides):
    start = len(vertices) + 1
    vertices.append((0.0, center_y + radius, 0.0))
    for ring in range(1, rings):
        latitude = math.pi * ring / rings
        ring_radius = math.sin(latitude) * radius
        y = center_y + math.cos(latitude) * radius
        for side in range(sides):
            longitude = math.tau * side / sides
            vertices.append((
                math.cos(longitude) * ring_radius,
                y,
                math.sin(longitude) * ring_radius,
            ))
    bottom = len(vertices) + 1
    vertices.append((0.0, center_y - radius, 0.0))

    first_ring = start + 1
    for side in range(sides):
        next_side = (side + 1) % sides
        faces.append((start, first_ring + next_side, first_ring + side))

    for ring in range(rings - 2):
        current = first_ring + ring * sides
        following = current + sides
        for side in range(sides):
            next_side = (side + 1) % sides
            faces.append((current + side, current + next_side,
                          following + next_side, following + side))

    last_ring = first_ring + (rings - 2) * sides
    for side in range(sides):
        next_side = (side + 1) % sides
        faces.append((last_ring + side, last_ring + next_side, bottom))


def add_box(vertices, faces, x_min, x_max, y_min, y_max, z_min, z_max):
    start = len(vertices) + 1
    vertices.extend([
        (x_min, y_min, z_min), (x_max, y_min, z_min),
        (x_max, y_max, z_min), (x_min, y_max, z_min),
        (x_min, y_min, z_max), (x_max, y_min, z_max),
        (x_max, y_max, z_max), (x_min, y_max, z_max),
    ])
    for face in ((0, 3, 2, 1), (4, 5, 6, 7), (0, 4, 7, 3),
                 (1, 2, 6, 5), (3, 7, 6, 2), (0, 1, 5, 4)):
        faces.append(tuple(start + index for index in face))


def write_obj(filename, object_name, vertices, faces):
    lines = [
        "mtllib Checkpoint.mtl",
        "o " + object_name,
        "usemtl CheckpointWhite",
    ]
    for x, y, z in vertices:
        lines.append(f"v {x:.6f} {y:.6f} {z:.6f}")

    normals = []
    for face in faces:
        a = vertices[face[0] - 1]
        b = vertices[face[1] - 1]
        c = vertices[face[2] - 1]
        ab = (b[0] - a[0], b[1] - a[1], b[2] - a[2])
        ac = (c[0] - a[0], c[1] - a[1], c[2] - a[2])
        normal = (
            ab[1] * ac[2] - ab[2] * ac[1],
            ab[2] * ac[0] - ab[0] * ac[2],
            ab[0] * ac[1] - ab[1] * ac[0],
        )
        length = math.sqrt(sum(value * value for value in normal))
        if length == 0.0:
            length = 1.0
        normals.append(tuple(value / length for value in normal))

    for x, y, z in normals:
        lines.append(f"vn {x:.6f} {y:.6f} {z:.6f}")
    for normal_index, face in enumerate(faces, 1):
        refs = []
        for vertex_index in face:
            refs.append(f"{vertex_index}//{normal_index}")
        lines.append("f " + " ".join(refs))

    (OUT / filename).write_text("\n".join(lines) + "\n", encoding="utf-8")


material = """newmtl CheckpointWhite
Ns 80.0
Ka 0.2 0.2 0.2
Kd 1.0 1.0 1.0
Ks 0.25 0.25 0.25
d 1.0
illum 2
map_Kd ../../Textures/white.png
"""
(OUT / "Checkpoint.mtl").write_text(material, encoding="utf-8")

stand_vertices = []
stand_faces = []
add_cylinder(stand_vertices, stand_faces, -0.62, -0.48, 0.58, 12)
add_cylinder(stand_vertices, stand_faces, -0.48, -0.30, 0.43, 12)
add_cylinder(stand_vertices, stand_faces, -0.30, 0.70, 0.075, 12)
add_cylinder(stand_vertices, stand_faces, 0.58, 0.68, 0.13, 12)
add_sphere(stand_vertices, stand_faces, 0.80, 0.11, 6, 12)
write_obj("CheckpointStand.obj", "CheckpointStand", stand_vertices, stand_faces)

segment_vertices = []
segment_faces = []
add_box(segment_vertices, segment_faces, -0.5, 0.5, -0.5, 0.5, -0.035, 0.035)
write_obj("CheckpointFlagSegment.obj", "CheckpointFlagSegment",
          segment_vertices, segment_faces)

print(OUT)
