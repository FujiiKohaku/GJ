from pathlib import Path
import math


ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "resources" / "Models" / "Spring"
OUT.mkdir(parents=True, exist_ok=True)


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


def add_helix(vertices, faces, radius, tube_radius, y_min, y_max,
              turns, path_segments, tube_segments):
    start = len(vertices) + 1
    angle_step = math.tau * turns / path_segments
    y_step = (y_max - y_min) / path_segments

    for path_index in range(path_segments + 1):
        angle = angle_step * path_index
        cosine = math.cos(angle)
        sine = math.sin(angle)
        center = (
            cosine * radius,
            y_min + y_step * path_index,
            sine * radius,
        )
        tangent = (
            -sine * radius * angle_step,
            y_step,
            cosine * radius * angle_step,
        )
        tangent_length = math.sqrt(sum(value * value for value in tangent))
        tangent = tuple(value / tangent_length for value in tangent)
        radial = (cosine, 0.0, sine)
        second = (
            tangent[1] * radial[2] - tangent[2] * radial[1],
            tangent[2] * radial[0] - tangent[0] * radial[2],
            tangent[0] * radial[1] - tangent[1] * radial[0],
        )
        second_length = math.sqrt(sum(value * value for value in second))
        second = tuple(value / second_length for value in second)

        for tube_index in range(tube_segments):
            tube_angle = math.tau * tube_index / tube_segments
            radial_amount = math.cos(tube_angle) * tube_radius
            second_amount = math.sin(tube_angle) * tube_radius
            vertices.append((
                center[0] + radial[0] * radial_amount + second[0] * second_amount,
                center[1] + radial[1] * radial_amount + second[1] * second_amount,
                center[2] + radial[2] * radial_amount + second[2] * second_amount,
            ))

    for path_index in range(path_segments):
        current = start + path_index * tube_segments
        following = current + tube_segments
        for tube_index in range(tube_segments):
            next_tube = (tube_index + 1) % tube_segments
            faces.append((
                current + tube_index,
                current + next_tube,
                following + next_tube,
                following + tube_index,
            ))


def write_obj(vertices, faces):
    lines = [
        "mtllib Spring.mtl",
        "o Spring",
        "usemtl SpringWhite",
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
        references = []
        for vertex_index in face:
            references.append(f"{vertex_index}//{normal_index}")
        lines.append("f " + " ".join(references))

    (OUT / "Spring.obj").write_text(
        "\n".join(lines) + "\n", encoding="utf-8")


vertices = []
faces = []
add_box(vertices, faces, -0.46, 0.46, -0.50, -0.38, -0.46, 0.46)
add_box(vertices, faces, -0.46, 0.46, 0.38, 0.50, -0.46, 0.46)
add_helix(vertices, faces, 0.28, 0.065, -0.36, 0.36, 3.5, 70, 8)
write_obj(vertices, faces)

material = """newmtl SpringWhite
Ns 96.0
Ka 0.2 0.2 0.2
Kd 1.0 1.0 1.0
Ks 0.35 0.35 0.35
d 1.0
illum 2
map_Kd ../../Textures/white.png
"""
(OUT / "Spring.mtl").write_text(material, encoding="utf-8")

print(OUT / "Spring.obj")
