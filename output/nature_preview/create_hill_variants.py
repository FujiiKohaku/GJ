"""Generate closed, smooth-shaded background hills in the game's model resources."""
from pathlib import Path
import math
import numpy as np

destination = Path(__file__).resolve().parents[2] / 'resources' / 'Models' / 'Nature'
destination.mkdir(parents=True, exist_ok=True)
variants = [('hill_low', 5.0, 1.2, 2.5, 0.25),
            ('hill_tall', 3.0, 3.1, 2.2, 0.45),
            ('hill_ridge', 6.0, 2.0, 2.8, -1.1)]
for name, rx, height, rz, lean in variants:
    segments, rows = 48, 20
    vertices, faces = [], []
    for row in range(rows):
        phi = math.pi * row / (2 * rows)
        for col in range(segments):
            theta = 2 * math.pi * col / segments
            vertices.append((rx * math.cos(phi) * math.cos(theta) + lean * math.sin(phi)**2,
                             height * math.sin(phi), rz * math.cos(phi) * math.sin(theta)))
    top, bottom = len(vertices), len(vertices) + 1
    vertices.extend([(lean, height, 0), (0, 0, 0)])
    for row in range(rows - 1):
        for col in range(segments):
            a = row * segments + col
            b = row * segments + (col + 1) % segments
            faces.extend([[a, b + segments, b], [a, a + segments, b + segments]])
    for col in range(segments):
        a, b = (rows - 1) * segments + col, (rows - 1) * segments + (col + 1) % segments
        faces.append([a, top, b])
    curved_count = len(faces)
    for col in range(segments):
        faces.append([col, (col + 1) % segments, bottom])
    points = np.array(vertices)
    normals = np.zeros_like(points)
    edges = {}
    for k, f in enumerate(faces):
        a, b, c = points[f]
        normal = np.cross(b - a, c - a)
        assert np.linalg.norm(normal) > 1e-9
        if k < curved_count:
            for index in f:
                normals[index] += normal
        for a, b in zip(f, f[1:] + f[:1]):
            edge = tuple(sorted((a, b)))
            edges[edge] = edges.get(edge, 0) + 1
    assert set(edges.values()) == {2}
    assert sum(np.dot(points[a], np.cross(points[b], points[c])) for a, b, c in faces) > 0
    normals[bottom] = (0, -1, 0)
    normals /= np.linalg.norm(normals, axis=1)[:, None]
    assert np.isfinite(normals).all()
    lines = ['# Y-up, meters; smooth background hill', 'mtllib hills.mtl', 'o ' + name]
    lines += ['v ' + ' '.join(f'{v:.6f}' for v in p) for p in points]
    lines += ['vt 0.5 0.5']
    lines += ['vn ' + ' '.join(f'{v:.6f}' for v in n) for n in normals]
    lines += ['usemtl hill_grass', 's 1']
    for k, face in enumerate(faces):
        lines.append('f ' + ' '.join(f'{i + 1}/1/{i + 1 if k < curved_count else bottom + 1}' for i in face))
    (destination / (name + '.obj')).write_text('\n'.join(lines) + '\n', encoding='utf-8')
    print(name, len(faces), 'triangles; closed geometry and normals verified')
(destination / 'hills.mtl').write_text('newmtl hill_grass\nKd 0.43 0.66 0.32\nKs 0 0 0\nmap_Kd ../../Textures/white.png\n', encoding='utf-8')
