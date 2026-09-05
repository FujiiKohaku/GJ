"""Generate a reusable closed low-poly rock for the ruin ground."""
from pathlib import Path
import math
import numpy as np

destination = Path(__file__).resolve().parents[2] / 'resources' / 'Models' / 'Nature'
segments, rings = 12, 5
vertices = [(0.0, 0.78, 0.0)]
for ring in range(1, rings):
    phi = math.pi * ring / rings
    for segment in range(segments):
        theta = 2.0 * math.pi * segment / segments
        wobble = 1.0 + 0.13 * math.sin(theta * 3.0 + ring) + 0.07 * math.cos(theta * 5.0)
        vertices.append((1.05 * math.sin(phi) * math.cos(theta) * wobble,
                         0.42 + 0.48 * math.cos(phi),
                         0.78 * math.sin(phi) * math.sin(theta) * wobble))
bottom = len(vertices)
vertices.append((0.0, 0.0, 0.0))
faces = []
for segment in range(segments):
    faces.append([0, 1 + segment, 1 + (segment + 1) % segments])
for ring in range(rings - 2):
    start = 1 + ring * segments
    next_start = start + segments
    for segment in range(segments):
        next_segment = (segment + 1) % segments
        faces.extend([[start + segment, next_start + segment, next_start + next_segment],
                      [start + segment, next_start + next_segment, start + next_segment]])
last = 1 + (rings - 2) * segments
for segment in range(segments):
    faces.append([last + segment, bottom, last + (segment + 1) % segments])
points = np.array(vertices)
edges = {}
for face in faces:
    for a, b in zip(face, face[1:] + face[:1]):
        edge = tuple(sorted((a, b)))
        edges[edge] = edges.get(edge, 0) + 1
assert set(edges.values()) == {2}
volume = sum(np.dot(points[a], np.cross(points[b], points[c])) for a, b, c in faces)
if volume < 0:
    faces = [face[::-1] for face in faces]
lines = ['# Y-up low-poly ground rock', 'mtllib nature.mtl', 'o ground_rock']
lines += ['v ' + ' '.join(f'{value:.6f}' for value in point) for point in points]
lines.append('vt 0.5 0.5')
for face in faces:
    a, b, c = points[face]
    normal = np.cross(b - a, c - a)
    normal /= np.linalg.norm(normal)
    lines.append('vn ' + ' '.join(f'{value:.6f}' for value in normal))
lines += ['usemtl green_0', 's off']
for normal_index, face in enumerate(faces, 1):
    lines.append('f ' + ' '.join(f'{index + 1}/1/{normal_index}' for index in face))
(destination / 'ground_rock.obj').write_text('\n'.join(lines) + '\n', encoding='utf-8')
print('ground_rock:', len(vertices), 'vertices,', len(faces), 'triangles; closed mesh verified')
