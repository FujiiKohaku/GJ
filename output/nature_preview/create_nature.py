"""Generate models; send the preview as base64 on stdout without saving an image."""
import ast
import base64
import io
import math
import random
from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent
# Reuse only definitions: importing the old module would save its previews.
source = ROOT.parent / 'ruins_preview' / 'create_models.py'
tree = ast.parse(source.read_text(encoding='utf-8'))
definitions = [node for node in tree.body if isinstance(node, (ast.Import, ast.ImportFrom, ast.FunctionDef, ast.ClassDef))]
scope = {'__file__': str(source)}
exec(compile(ast.Module(body=definitions, type_ignores=[]), str(source), 'exec'), scope)
colors = [(0.43, 0.66, 0.32), (0.34, 0.59, 0.23), (0.53, 0.74, 0.34), (0.39, 0.63, 0.25)]
scope['PALETTE'] = colors
scope['rng'] = random.Random(91)
Mesh, render = scope['Mesh'], scope['render']


def add(mesh, points, faces, material):
    points = np.array(points, dtype=float)
    volume = sum(np.dot(points[a], np.cross(points[b], points[c])) for a, b, c in faces)
    if volume < 0:
        faces = [f[::-1] for f in faces]
    offset, group = len(mesh.vertices), len(mesh.groups)
    mesh.vertices.extend(points.tolist())
    mesh.groups.append(points)
    mesh.faces.extend([[offset + i for i in f] for f in faces])
    mesh.materials.extend([(material, group)] * len(faces))


hill = Mesh()
segments, rows = 48, 18
points = []
for row in range(rows):
    angle = (math.pi / 2) * row / rows
    for i in range(segments):
        theta = 2 * math.pi * i / segments
        radius = math.cos(angle)
        points.append((3.9 * radius * math.cos(theta) + 0.3 * math.sin(angle),
                       2.0 * math.sin(angle), 2.25 * radius * math.sin(theta)))
top, bottom = len(points), len(points) + 1
points += [(0.3, 2.0, 0), (0, 0, 0)]
faces = []
for row in range(rows - 1):
    for i in range(segments):
        j = (i + 1) % segments
        a, b, c, d = row * segments + i, row * segments + j, (row + 1) * segments + j, (row + 1) * segments + i
        faces += [[a, b, c], [a, c, d]]
for i in range(segments):
    j = (i + 1) % segments
    faces += [[(rows - 1) * segments + i, (rows - 1) * segments + j, top], [i, bottom, j]]
add(hill, points, faces, 0)

grass = Mesh()
rng = random.Random(95)
for blade in range(13):
    theta = blade * 2.39996
    distance = 0.10 + 0.30 * (blade % 4) / 3
    origin = np.array([math.cos(theta) * distance, 0.018, math.sin(theta) * distance])
    direction = np.array([math.cos(theta), 0, math.sin(theta)])
    side = np.array([-math.sin(theta), 0, math.cos(theta)])
    height = rng.uniform(0.68, 1.15)
    lean = rng.uniform(0.28, 0.60)
    width = rng.uniform(0.10, 0.16)
    points, faces = [], []
    steps = 7
    for row in range(steps):
        t = row / steps
        center = origin + direction * (lean * t * t) + np.array([0, height * t, 0])
        w = width * (0.4 + 0.95 * math.sin(math.pi * t)) * (1 - t * 0.35)
        thickness = 0.018 * (1 - t * 0.65)
        points.extend([center + side * w, center + direction * thickness,
                       center - side * w, center - direction * thickness])
    points.append(origin + direction * lean + np.array([0, height, 0]))
    tip = len(points) - 1
    faces.extend([[0, 2, 1], [0, 3, 2]])
    for row in range(steps - 1):
        for edge in range(4):
            a, b = row * 4 + edge, row * 4 + (edge + 1) % 4
            faces.extend([[a, b, b + 4], [a, b + 4, a + 4]])
    for edge in range(4):
        faces.append([(steps - 1) * 4 + edge, (steps - 1) * 4 + (edge + 1) % 4, tip])
    add(grass, points, faces, 1 + blade % 3)


def save(mesh, name, smooth):
    vertices = np.array(mesh.vertices)
    normals = np.zeros_like(vertices)
    face_normals = []
    edge_counts = {}
    for f in mesh.faces:
        a, b, c = vertices[f]
        normal = np.cross(b - a, c - a)
        assert np.linalg.norm(normal) > 1e-9
        for i in f:
            normals[i] += normal
        face_normals.append(normal / np.linalg.norm(normal))
        for a, b in zip(f, f[1:] + f[:1]):
            edge = tuple(sorted((a, b)))
            edge_counts[edge] = edge_counts.get(edge, 0) + 1
    assert set(edge_counts.values()) == {2}
    if smooth:
        # Analytic ellipsoid surface normals keep the hemisphere seam clean.
        normals = np.column_stack(((vertices[:, 0] - 0.15 * vertices[:, 1]) / 3.9**2,
                                   vertices[:, 1] / 2.0**2,
                                   vertices[:, 2] / 2.25**2))
        normals[-1] = (0, -1, 0)
        normals /= np.linalg.norm(normals, axis=1)[:, None]
    else:
        normals = np.array(face_normals)
    lines = ['# Y up, meters; preview-only model', 'mtllib nature.mtl', 'o ' + name]
    lines += ['v ' + ' '.join(f'{x:.6f}' for x in p) for p in vertices]
    lines += ['vt 0.5 0.5']
    lines += ['vn ' + ' '.join(f'{x:.6f}' for x in n) for n in normals]
    last = None
    for k, (face, (mat, group)) in enumerate(zip(mesh.faces, mesh.materials)):
        if group != last:
            lines += [f'g part_{group}', f'usemtl green_{mat}']
            last = group
        lines += ['f ' + ' '.join(f'{i + 1}/1/{i + 1 if smooth else k + 1}' for i in face)]
    (ROOT / (name + '.obj')).write_text('\n'.join(lines) + '\n', encoding='utf-8')


save(hill, 'rounded_hill', True)
save(grass, 'grass_clump', False)
(ROOT / 'nature.mtl').write_text('\n'.join(f'newmtl green_{i}\nKd {r} {g} {b}\nKs 0 0 0\nmap_Kd ../../resources/Textures/white.png\n' for i, (r, g, b) in enumerate(colors)), encoding='utf-8')
sheet = Image.new('RGB', (1400, 800), '#e5e5df')
draw = ImageDraw.Draw(sheet)
font = ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf', 29)
small = ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf', 21)
draw.text((45, 26), 'STAGE 01 / SOFT LANDSCAPE', font=font, fill='#333831')
for model, x, label in [(hill, 0, '01 / ROUNDED HILL'), (grass, 700, '02 / GRASS CLUMP')]:
    preview = render(model, width=1000, height=940, azimuth=0.25)
    sheet.paste(preview.resize((700, 658), Image.Resampling.LANCZOS), (x, 83))
    draw.text((x + 44, 745), label, font=small, fill='#333831')
buf = io.BytesIO()
sheet.save(buf, format='PNG')
print(base64.b64encode(buf.getvalue()).decode('ascii'))
