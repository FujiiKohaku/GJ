from pathlib import Path
import math


ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "resources" / "Models" / "Cannon"
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


def add_cylinder_z(vertices, faces, center_x, center_y, center_z,
                   radius, depth, segments):
    start = len(vertices) + 1
    for z in (center_z - depth * 0.5, center_z + depth * 0.5):
        for index in range(segments):
            angle = math.tau * index / segments
            vertices.append((center_x + math.cos(angle) * radius,
                             center_y + math.sin(angle) * radius, z))
    for index in range(segments):
        next_index = (index + 1) % segments
        faces.append((start + index, start + next_index,
                      start + segments + next_index, start + segments + index))
    faces.append(tuple(start + index for index in reversed(range(segments))))
    faces.append(tuple(start + segments + index for index in range(segments)))


def add_barrel(vertices, faces, start_point, end_point,
               start_radius, end_radius, segments):
    axis_x = end_point[0] - start_point[0]
    axis_y = end_point[1] - start_point[1]
    length = math.sqrt(axis_x * axis_x + axis_y * axis_y)
    normal_x = -axis_y / length
    normal_y = axis_x / length
    start = len(vertices) + 1
    for point, radius in ((start_point, start_radius), (end_point, end_radius)):
        for index in range(segments):
            angle = math.tau * index / segments
            radial = math.cos(angle) * radius
            z = math.sin(angle) * radius
            vertices.append((point[0] + normal_x * radial,
                             point[1] + normal_y * radial,
                             point[2] + z))
    for index in range(segments):
        next_index = (index + 1) % segments
        faces.append((start + index, start + next_index,
                      start + segments + next_index, start + segments + index))
    faces.append(tuple(start + index for index in reversed(range(segments))))
    faces.append(tuple(start + segments + index for index in range(segments)))


def write_obj(vertices, faces):
    lines = ["mtllib Cannon.mtl", "o Cannon", "usemtl CannonWhite"]
    for x, y, z in vertices:
        lines.append(f"v {x:.6f} {y:.6f} {z:.6f}")
    normals = []
    for face in faces:
        a = vertices[face[0] - 1]
        b = vertices[face[1] - 1]
        c = vertices[face[2] - 1]
        ab = (b[0] - a[0], b[1] - a[1], b[2] - a[2])
        ac = (c[0] - a[0], c[1] - a[1], c[2] - a[2])
        normal = (ab[1] * ac[2] - ab[2] * ac[1],
                  ab[2] * ac[0] - ab[0] * ac[2],
                  ab[0] * ac[1] - ab[1] * ac[0])
        normal_length = math.sqrt(sum(value * value for value in normal))
        if normal_length == 0.0:
            normal_length = 1.0
        normals.append(tuple(value / normal_length for value in normal))
    for x, y, z in normals:
        lines.append(f"vn {x:.6f} {y:.6f} {z:.6f}")
    for normal_index, face in enumerate(faces, 1):
        references = [f"{vertex_index}//{normal_index}" for vertex_index in face]
        lines.append("f " + " ".join(references))
    (OUT / "Cannon.obj").write_text("\n".join(lines) + "\n", encoding="utf-8")


vertices = []
faces = []
add_box(vertices, faces, -0.48, 0.48, -0.50, -0.34, -0.45, 0.45)
add_cylinder_z(vertices, faces, -0.22, -0.23, 0.0, 0.22, 0.62, 16)
add_cylinder_z(vertices, faces, 0.22, -0.23, 0.0, 0.22, 0.62, 16)
add_barrel(vertices, faces, (-0.20, -0.10, 0.0), (0.34, 0.35, 0.0),
           0.18, 0.15, 16)
add_barrel(vertices, faces, (0.30, 0.32, 0.0), (0.45, 0.45, 0.0),
           0.20, 0.20, 16)
write_obj(vertices, faces)

(OUT / "Cannon.mtl").write_text("""newmtl CannonWhite
Ns 96.0
Ka 0.2 0.2 0.2
Kd 1.0 1.0 1.0
Ks 0.35 0.35 0.35
d 1.0
illum 2
map_Kd ../../Textures/white.png
""", encoding="utf-8")

print(OUT / "Cannon.obj")
