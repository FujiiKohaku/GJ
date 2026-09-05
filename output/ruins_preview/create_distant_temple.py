"""A standalone distant temple model and two views; no game installation."""
from pathlib import Path
import random
import numpy as np
from PIL import Image, ImageDraw, ImageFont
from create_models import Mesh, render

ROOT = Path(__file__).resolve().parent
temple = Mesh()
random.seed(81)

# Broad, shallow stage-like structure: readable from a side-scrolling camera.
temple.stone((0, 0.18, 0), (22, 0.36, 3.8), 0.035)
temple.stone((0, 0.47, 0), (21.4, 0.20, 3.25), 0.025)
xs = [-9, -6, -3, 0, 3, 6, 9]
heights = [5.2, 5.2, 5.2, 2.6, 5.2, 5.2, 3.5]
for x, height in zip(xs, heights):
    temple.stone((x, 0.74, 0), (1.40, 0.32, 1.50), 0.025)
    temple.stone((x, 0.908 + height / 2, 0), (0.90, height, 1.04), 0.02)
    temple.stone((x, 0.908 + height + 0.118, 0), (1.40, 0.22, 1.45), 0.025)

# Incomplete entablature leaves a large gap above the broken central column.
temple.stone((-6, 6.53, 0), (7.75, 0.36, 1.55), 0.025)
temple.stone((-6.25, 6.85, 0), (8.10, 0.264, 1.80), 0.025)
temple.stone((4.55, 6.53, 0), (4.28, 0.36, 1.55), 0.025)
temple.stone((4.30, 6.85, 0), (4.05, 0.264, 1.80), 0.025)

# A broken pediment built as a solid extruded polygon, not a texture.
def stone_prism(outline, depth):
    points = [(x, y, z) for z in (-depth / 2, depth / 2) for x, y in outline]
    n = len(outline)
    faces = []
    for i in range(1, n - 1):
        faces.extend(([0, i + 1, i], [n, n + i, n + i + 1]))
    for i in range(n):
        j = (i + 1) % n
        faces.extend(([i, j, n + j], [i, n + j, n + i]))
    pts = np.array(points)
    if sum(np.dot(pts[a], np.cross(pts[b], pts[c])) for a, b, c in faces) < 0:
        faces = [f[::-1] for f in faces]
    start, group = len(temple.vertices), len(temple.groups)
    temple.vertices.extend(points)
    temple.groups.append(pts)
    temple.faces.extend([[start + i for i in f] for f in faces])
    temple.materials.extend([(2, group)] * len(faces))

stone_prism([(-10.30, 7.0), (-2.75, 7.0), (-3.1, 8.15), (-4.0, 8.55)], 1.35)
stone_prism([(2.30, 7.0), (6.2, 7.0), (2.8, 8.42)], 1.35)
# A sparse rear row suggests the depth of a large building.
for x, height in [(-7.5, 4.5), (-1.5, 3.1), (4.5, 4.5), (7.5, 2.0)]:
    temple.stone((x, 0.58 + height / 2, -1.02), (0.72, height, 0.74), 0.018)
for x, width, height in [(-8.3, 1.6, 0.45), (-0.6, 2.3, 0.50), (1.3, 1.0, 0.76), (8.1, 2.2, 0.42)]:
    temple.stone((x, 0.575 + height / 2, 0.92), (width, height, 0.76), 0.04, 0.13)

vertices = np.array(temple.vertices)
edges = {}
for f in temple.faces:
    a, b, c = vertices[f]
    assert np.linalg.norm(np.cross(b - a, c - a)) > 1e-8
    for i, j in zip(f, f[1:] + f[:1]):
        key = tuple(sorted((i, j)))
        edges[key] = edges.get(key, 0) + 1
assert set(edges.values()) == {2}
temple.save('ruin_distant_temple')

sheet = Image.new('RGB', (1800, 1340), '#e5e5df')
draw = ImageDraw.Draw(sheet)
font = 'C:/Windows/Fonts/segoeui.ttf'
title = ImageFont.truetype(font, 34)
label = ImageFont.truetype(font, 24)
small = ImageFont.truetype(font, 19)
draw.text((65, 32), 'STAGE 01 / DISTANT TEMPLE', font=title, fill='#333831')
draw.text((65, 85), 'MODEL PREVIEW  /  BROKEN PEDIMENT + COLONNADE', font=small, fill='#73776e')
perspective = render(temple, 1700, 650, azimuth=0.25, elevation=0.25)
sheet.paste(perspective, (50, 124))
draw.text((65, 780), 'DISTANT VIEW / SIDE-SCROLLING CAMERA', font=label, fill='#333831')
front = render(temple, 1700, 450, azimuth=0.0, elevation=0.02)
# Atmospheric wash is a preview of distance treatment, not baked into the mesh.
front = Image.blend(front, Image.new('RGB', front.size, '#e5e5df'), 0.63)
sheet.paste(front, (50, 826))
draw.text((65, 1294), 'Same mesh below, with reduced contrast to keep gameplay readable. No game placement.', font=small, fill='#73776e')
sheet.save(ROOT / 'distant_temple_preview.png')
print('Distant temple:', len(temple.vertices), 'vertices,', len(temple.faces), 'triangles; closed meshes verified')
