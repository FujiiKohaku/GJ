"""Generate independent ruin OBJ assets and render their actual mesh with Python."""
from pathlib import Path
import itertools
import math
import random
import numpy as np
from PIL import Image, ImageDraw, ImageFont, ImageFilter

ROOT = Path(__file__).resolve().parent
rng = random.Random(31)
PALETTE = [(0.54, 0.53, 0.49), (0.61, 0.60, 0.55), (0.66, 0.65, 0.60),
           (0.57, 0.57, 0.53), (0.70, 0.68, 0.62)]


class Mesh:
    def __init__(self):
        self.vertices, self.faces, self.materials, self.groups = [], [], [], []

    def stone(self, center, size, bevel=0.09, rotate=0.0, tilt=0.0):
        half = np.array(size) / 2
        bevel = min(bevel, min(half) * 0.45)
        corners = list(itertools.product((-1, 1), repeat=3))
        local, lookup = [], {}
        for corner in corners:
            # Independently chipped corners, shared by all adjacent faces.
            cut = bevel * rng.uniform(0.7, 1.3)
            for axis in range(3):
                p = (half - cut) * np.array(corner)
                p[axis] = half[axis] * corner[axis]
                lookup[(corner, axis)] = len(local)
                local.append(p)
        faces = []
        for axis in range(3):
            others = [a for a in range(3) if a != axis]
            for sign in (-1, 1):
                ids = []
                for s, t in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
                    corner = [0, 0, 0]
                    corner[axis], corner[others[0]], corner[others[1]] = sign, s, t
                    ids.append(lookup[(tuple(corner), axis)])
                faces.append(ids)
        for a, b in itertools.combinations(range(3), 2):
            c = 3 - a - b
            for sa, sb in itertools.product((-1, 1), repeat=2):
                ends = []
                for sc in (-1, 1):
                    corner = [0, 0, 0]
                    corner[a], corner[b], corner[c] = sa, sb, sc
                    ends.append(tuple(corner))
                faces.append([lookup[(ends[0], a)], lookup[(ends[0], b)],
                              lookup[(ends[1], b)], lookup[(ends[1], a)]])
        for corner in corners:
            faces.append([lookup[(corner, axis)] for axis in range(3)])
        local = np.array(local)
        for face in faces:
            p = local[face]
            if np.dot(np.cross(p[1] - p[0], p[2] - p[0]), p.mean(axis=0)) < 0:
                face.reverse()
        cy, sy, cz, sz = math.cos(rotate), math.sin(rotate), math.cos(tilt), math.sin(tilt)
        rotation = np.array([[cy, 0, sy], [0, 1, 0], [-sy, 0, cy]]) @ np.array([[cz, -sz, 0], [sz, cz, 0], [0, 0, 1]])
        world = local @ rotation.T + center
        start = len(self.vertices)
        self.vertices.extend(world.tolist())
        material = rng.randrange(len(PALETTE))
        group = len(self.groups)
        self.groups.append(world)
        for face in faces:
            # Export explicit triangles so preview and imported mesh agree.
            for j in range(1, len(face) - 1):
                self.faces.append([start + face[0], start + face[j], start + face[j + 1]])
                self.materials.append((material, group))

    def save(self, name):
        lines = ['# Ruins concept asset; Y up; units: meters', 'mtllib ruins.mtl', f'o {name}']
        lines += ['v ' + ' '.join(f'{x:.6f}' for x in p) for p in self.vertices]
        points = np.array(self.vertices)
        for face in self.faces:
            a, b, c = points[face]
            normal = np.cross(b - a, c - a)
            normal /= np.linalg.norm(normal)
            lines.append('vn ' + ' '.join(f'{x:.6f}' for x in normal))
        last = None
        for normal_index, (face, (mat, group)) in enumerate(zip(self.faces, self.materials), 1):
            if last != group:
                lines += [f'g stone_{group:03}', f'usemtl stone_{mat}', 's off']
                last = group
            lines.append('f ' + ' '.join(f'{i + 1}//{normal_index}' for i in face))
        (ROOT / f'{name}.obj').write_text('\n'.join(lines) + '\n', encoding='utf-8')


pillar = Mesh()
pillar.stone((0, 0.17, 0), (1.58, 0.34, 1.48), 0.025)
pillar.stone((0, 0.438, 0), (1.30, 0.18, 1.22), 0.018)
for i in range(5):
    pillar.stone((rng.uniform(-0.004, 0.004), 0.837 + i * 0.61, 0),
                 (0.93 + rng.uniform(-0.008, 0.008), 0.602, 0.91), 0.018,
                 rng.uniform(-0.004, 0.004))
pillar.stone((0.005, 3.696, 0), (1.20, 0.22, 1.13), 0.018, 0.004)
pillar.stone((0.005, 3.964, 0), (1.46, 0.30, 1.35), 0.025, -0.004)
pillar.stone((-0.20, 4.197, 0.07), (0.77, 0.15, 0.83), 0.018, 0.06)
pillar.stone((1.08, 0.16, 0.30), (0.48, 0.30, 0.42), 0.10, 0.5)
pillar.stone((-0.87, 0.10, -0.62), (0.39, 0.20, 0.36), 0.08, -0.3)

wall = Mesh()
for row, count in enumerate((6, 6, 5, 4, 2)):
    for col in range(count):
        if row == 4 and col == 1:
            continue
        x = -2.75 + col * 1.02 + (0.22 if row % 2 else 0)
        wall.stone((x, 0.28 + row * 0.57, rng.uniform(-0.008, 0.008)),
                   (1.012, 0.562, rng.uniform(0.74, 0.77)), 0.018,
                   rng.uniform(-0.003, 0.003))
wall.stone((0.40, 1.99, 0.025), (0.68, 0.40, 0.68), 0.09, 0.07, -0.14)
for x, z, s in ((2.15, 0.66, 0.65), (1.45, 0.89, 0.5), (2.81, -0.11, 0.64),
                (0.38, 0.87, 0.38), (-2.4, 0.72, 0.31), (2.97, 0.77, 0.32)):
    wall.stone((x, s * 0.25, z), (s, s * 0.5, s * 0.75), s * 0.14,
               rng.uniform(-0.8, 0.8))

arch = Mesh()
for side in (-1, 1):
    x = side * 1.7
    arch.stone((x, 0.14, 0), (1.05, 0.28, 1.0), 0.025)
    for row in range(4):
        arch.stone((x, 0.562 + row * 0.55, 0), (0.70, 0.542, 0.82), 0.018)
    arch.stone((x, 2.368, 0), (0.90, 0.22, 0.94), 0.025)

# Wedge-shaped voussoirs: transform bevelled blocks into a segmented arch.
# Omit two crown stones to form an actual break through the silhouette.
for segment in range(12):
    if segment in (5, 6):
        continue
    start = len(arch.vertices)
    arch.stone((0, 0, 0), (1.0, 0.70, 0.82), 0.016)
    block = np.array(arch.vertices[start:])
    angle = (segment + 1 - (block[:, 0] + 0.5)) * math.pi / 12
    angle += (block[:, 0]) * 0.008  # Fine joints between adjacent wedges.
    radius = 1.70 + block[:, 1]
    block[:, 0] = radius * np.cos(angle)
    block[:, 1] = 2.49 + radius * np.sin(angle)
    arch.vertices[start:] = block.tolist()
    arch.groups[-1] = block
for x, z, size, rotation in [(-0.35, 0.36, (0.66, 0.26, 0.72), 0.35),
                            (0.49, 0.55, (0.56, 0.35, 0.61), -0.35),
                            (0.08, 1.03, (0.27, 0.16, 0.30), 0.50)]:
    arch.stone((x, size[1] / 2, z), size, 0.035, rotation)

fallen = Mesh()
fallen.stone((-2.13, 0.72, 0), (0.30, 1.44, 1.35), 0.025)
fallen.stone((-1.862, 0.72, 0), (0.22, 1.20, 1.13), 0.018)
for i in range(3):
    fallen.stone((-1.443 + i * 0.61, 0.465, 0), (0.602, 0.93, 0.91), 0.018)
# The far section has broken off and settled at an angle on the ground.
for i in range(2):
    fallen.stone((0.74 + i * 0.597, 0.465, -0.12 - i * 0.12),
                 (0.602, 0.93, 0.91), 0.018, 0.20)
fallen.stone((2.22, 0.17, 0.17), (1.48, 0.34, 1.38), 0.028, -0.19)
for x, z, s in [(0.29, 0.77, 0.28), (0.49, 0.58, 0.22), (-1.7, 0.92, 0.25)]:
    fallen.stone((x, s / 3, z), (s, s * 2 / 3, s * 0.8), 0.025, 0.45)

def turned_stone(mesh, profile, segments=20, material=2):
    """Revolve a closed radius/height profile, including solid axis endpoints."""
    points, rings, faces = [], [], []
    for radius, y in profile:
        if radius == 0:
            rings.append([len(points)])
            points.append((0, y, 0))
        else:
            ring = []
            for i in range(segments):
                angle = 2 * math.pi * i / segments
                ring.append(len(points))
                points.append((radius * math.cos(angle), y, radius * math.sin(angle)))
            rings.append(ring)
    for a, b in zip(rings, rings[1:] + rings[:1]):
        if len(a) == len(b) == 1:
            continue
        for i in range(segments):
            j = (i + 1) % segments
            if len(a) == 1:
                faces.append([a[0], b[i], b[j]])
            elif len(b) == 1:
                faces.append([a[i], b[0], a[j]])
            else:
                faces.extend(([a[i], b[i], b[j]], [a[i], b[j], a[j]]))
    pts = np.array(points)
    volume = sum(np.dot(pts[a], np.cross(pts[b], pts[c])) for a, b, c in faces)
    if volume < 0:
        faces = [f[::-1] for f in faces]
    offset, group = len(mesh.vertices), len(mesh.groups)
    mesh.vertices.extend(points)
    mesh.groups.append(pts)
    mesh.faces.extend([[offset + i for i in f] for f in faces])
    mesh.materials.extend([(material, group)] * len(faces))


def disk(mesh, radius, bottom, height, material=2, segments=20):
    bevel = 0.025
    turned_stone(mesh, [(0, bottom), (radius - bevel, bottom),
                       (radius, bottom + bevel), (radius, bottom + height - bevel),
                       (radius - bevel, bottom + height), (0, bottom + height)],
                 segments, material)


altar = Mesh()
disk(altar, 2.10, 0, 0.24, 0)
disk(altar, 1.82, 0.248, 0.24, 1)
disk(altar, 1.53, 0.496, 0.28, 2)
# A low ceremonial table rises from the concentric stepped platform.
disk(altar, 0.76, 0.784, 0.14, 0, 12)
disk(altar, 0.60, 0.932, 0.39, 1, 12)
disk(altar, 1.02, 1.329, 0.16, 2, 20)
for x, z, s in [(1.88, 1.0, 0.24), (-1.72, 1.38, 0.20)]:
    altar.stone((x, s / 3, z), (s, s * 2 / 3, s * 0.8), 0.018, 0.4)

fountain = Mesh()
disk(fountain, 2.14, 0, 0.20, 0)
# The profile returns along the inside wall to the dry basin floor.
turned_stone(fountain, [(0, 0.208), (1.96, 0.208), (2.00, 0.25),
                       (2.00, 0.70), (2.08, 0.73), (2.08, 0.83),
                       (2.04, 0.86), (1.73, 0.86), (1.69, 0.82),
                       (1.69, 0.74), (1.73, 0.70), (1.73, 0.34),
                       (0, 0.34)], 20, 1)
disk(fountain, 0.48, 0.348, 0.18, 0, 12)
disk(fountain, 0.30, 0.536, 0.87, 1, 12)
# Smaller empty upper bowl, visibly concave rather than a filled cylinder.
turned_stone(fountain, [(0, 1.414), (0.32, 1.414), (0.62, 1.66),
                       (0.86, 1.80), (0.86, 1.91), (0.82, 1.94),
                       (0.70, 1.94), (0.67, 1.86), (0.44, 1.70),
                       (0, 1.64)], 16, 2)
disk(fountain, 0.105, 1.648, 0.44, 0, 8)
for x, z, s in [(0.8, 0.65, 0.22), (-0.65, 0.9, 0.28), (0.25, 1.2, 0.16)]:
    fountain.stone((x, 0.34 + s / 3, z), (s, s * 2 / 3, s * 0.8), 0.02, 0.35)

for name, model in [('ruin_pillar', pillar), ('ruin_broken_wall', wall),
                    ('ruin_broken_arch', arch), ('ruin_fallen_pillar', fallen),
                    ('ruin_circular_altar', altar), ('ruin_dry_fountain', fountain)]:
    model.save(name)
    points = np.array(model.vertices)
    assert np.isfinite(points).all()
    assert min(min(f) for f in model.faces) >= 0
    assert max(max(f) for f in model.faces) < len(points)
    # Every stone is closed: each undirected edge belongs to two triangles.
    edges = {}
    for f in model.faces:
        assert np.linalg.norm(np.cross(points[f[1]] - points[f[0]], points[f[2]] - points[f[0]])) > 1e-8
        for a, b in zip(f, f[1:] + f[:1]):
            key = tuple(sorted((a, b)))
            edges[key] = edges.get(key, 0) + 1
    assert set(edges.values()) == {2}
    print(name, len(model.vertices), 'vertices', len(model.faces), 'triangles')

mtl = []
for i, color in enumerate(PALETTE):
    mtl += [f'newmtl stone_{i}', 'Kd ' + ' '.join(map(str, color)), 'Ka 0.15 0.15 0.15', 'Ks 0 0 0', 'Ns 1', '']
(ROOT / 'ruins.mtl').write_text('\n'.join(mtl), encoding='utf-8')


def render(mesh, width=1400, height=1250, azimuth=0.43, elevation=0.42):
    vertices = np.array(mesh.vertices)
    eye = np.array([math.sin(azimuth) * 0.91, elevation, math.cos(azimuth) * 0.91])
    eye /= np.linalg.norm(eye)
    right = np.cross([0, 1, 0], eye); right /= np.linalg.norm(right)
    up = np.cross(eye, right)
    basis = np.array([right, up, eye]).T
    projected = vertices @ basis
    low, high = projected[:, :2].min(axis=0), projected[:, :2].max(axis=0)
    scale = min(width * 0.79 / (high[0] - low[0]), height * 0.78 / (high[1] - low[1]))
    center = (low + high) / 2

    def screen(v):
        p = np.asarray(v) @ basis
        p[:, 0] = (p[:, 0] - center[0]) * scale + width / 2
        p[:, 1] = -(p[:, 1] - center[1]) * scale + height * 0.48
        return p

    canvas = Image.new('RGB', (width, height), (229, 229, 223))
    shadows = Image.new('RGBA', (width, height))
    painter = ImageDraw.Draw(shadows)
    for group in mesh.groups:
        ground = group.copy()
        ground[:, 0] += ground[:, 1] * 0.3
        ground[:, 2] -= ground[:, 1] * 0.18
        ground[:, 1] = 0
        pts = screen(ground)[:, :2]
        # Ground shadow footprint, sorted around its center.
        mid = pts.mean(axis=0)
        pts = pts[np.argsort(np.arctan2(pts[:, 1] - mid[1], pts[:, 0] - mid[0]))]
        painter.polygon([tuple(p) for p in pts], fill=(49, 47, 40, 45))
    canvas = Image.alpha_composite(canvas.convert('RGBA'), shadows.filter(ImageFilter.GaussianBlur(15))).convert('RGB')
    pixels = np.array(canvas)
    depth = np.full((height, width), -np.inf)
    points = screen(vertices)
    light = np.array([-0.55, 0.82, 0.68]); light /= np.linalg.norm(light)
    for face, (mat, group) in zip(mesh.faces, mesh.materials):
        tri = vertices[face]
        normal = np.cross(tri[1] - tri[0], tri[2] - tri[0]); normal /= np.linalg.norm(normal)
        if normal @ eye <= 0:
            continue
        p = points[face]
        xmin, ymin = np.maximum(np.floor(p[:, :2].min(axis=0)).astype(int), 0)
        xmax, ymax = np.minimum(np.ceil(p[:, :2].max(axis=0)).astype(int), [width - 1, height - 1])
        if xmax < xmin or ymax < ymin:
            continue
        xx, yy = np.meshgrid(np.arange(xmin, xmax + 1) + 0.5, np.arange(ymin, ymax + 1) + 0.5)
        a, b, c = p
        den = (b[1] - c[1]) * (a[0] - c[0]) + (c[0] - b[0]) * (a[1] - c[1])
        if abs(den) < 1e-9:
            continue
        wa = ((b[1] - c[1]) * (xx - c[0]) + (c[0] - b[0]) * (yy - c[1])) / den
        wb = ((c[1] - a[1]) * (xx - c[0]) + (a[0] - c[0]) * (yy - c[1])) / den
        wc = 1 - wa - wb
        z = wa * a[2] + wb * b[2] + wc * c[2]
        view = depth[ymin:ymax + 1, xmin:xmax + 1]
        mask = (wa >= -1e-8) & (wb >= -1e-8) & (wc >= -1e-8) & (z > view)
        brightness = 0.56 + 0.44 * max(0, normal @ light)
        color = np.clip(np.array(PALETTE[mat]) * brightness * 255, 0, 255).astype('uint8')
        pixels[ymin:ymax + 1, xmin:xmax + 1][mask] = color
        view[mask] = z[mask]
    return Image.fromarray(pixels)


sheet = Image.new('RGB', (1800, 1080), '#e5e5df')
draw = ImageDraw.Draw(sheet)
font_path = 'C:/Windows/Fonts/segoeui.ttf'
title = ImageFont.truetype(font_path, 36)
label = ImageFont.truetype(font_path, 25)
small = ImageFont.truetype(font_path, 19)
draw.text((62, 36), 'STAGE 01 / RUINS', font=title, fill='#333831')
draw.text((62, 88), 'PYTHON-GENERATED MESH STUDY  /  OBJ + MTL', font=small, fill='#73776e')
for model, name, x, heading, sub in [(pillar, 'pillar', 20, '01  /  STONE PILLAR', 'Stacked shaft / chipped cap / separate stones'),
                                   (wall, 'wall', 910, '02  /  BROKEN WALL', 'Staggered masonry / broken silhouette / rubble')]:
    preview = render(model).resize((870, 777), Image.Resampling.LANCZOS)
    preview.save(ROOT / f'{name}_preview.png')
    sheet.paste(preview, (x, 146))
    draw.text((x + 43, 938), heading, font=label, fill='#333831')
    draw.text((x + 43, 981), sub, font=small, fill='#73776e')
draw.line((897, 178, 897, 1010), fill='#cdcec6', width=2)
sheet.save(ROOT / 'ruins_preview.png')
sheet = Image.new('RGB', (1800, 1080), '#e5e5df')
draw = ImageDraw.Draw(sheet)
draw.text((62, 36), 'STAGE 01 / RUINS', font=title, fill='#333831')
draw.text((62, 88), 'PYTHON-GENERATED MESH STUDY  /  OBJ + MTL', font=small, fill='#73776e')
for model, name, x, heading, sub in [(arch, 'arch', 20, '03  /  BROKEN ARCH', 'Fitted wedge stones / missing crown / fallen fragments'),
                                   (fallen, 'fallen_pillar', 910, '04  /  FALLEN PILLAR', 'Tight joints / separated shaft / collapsed base')]:
    preview = render(model, azimuth=0.30).resize((870, 777), Image.Resampling.LANCZOS)
    preview.save(ROOT / f'{name}_preview.png')
    sheet.paste(preview, (x, 146))
    draw.text((x + 43, 938), heading, font=label, fill='#333831')
    draw.text((x + 43, 981), sub, font=small, fill='#73776e')
draw.line((897, 178, 897, 1010), fill='#cdcec6', width=2)
sheet.save(ROOT / 'arch_fallen_pillar_preview.png')
sheet = Image.new('RGB', (1800, 1080), '#e5e5df')
draw = ImageDraw.Draw(sheet)
draw.text((62, 36), 'STAGE 01 / RUINS', font=title, fill='#333831')
draw.text((62, 88), 'PYTHON-GENERATED MESH STUDY  /  OBJ + MTL', font=small, fill='#73776e')
for model, name, x, heading, sub in [(altar, 'altar', 20, '05  /  CIRCULAR ALTAR', 'Concentric steps / raised ceremonial stone table'),
                                   (fountain, 'fountain', 910, '06  /  DRY FOUNTAIN', 'Empty basin / shallow upper bowl / stone fragments')]:
    preview = render(model, azimuth=0.30).resize((870, 777), Image.Resampling.LANCZOS)
    preview.save(ROOT / f'{name}_preview.png')
    sheet.paste(preview, (x, 146))
    draw.text((x + 43, 938), heading, font=label, fill='#333831')
    draw.text((x + 43, 981), sub, font=small, fill='#73776e')
draw.line((897, 178, 897, 1010), fill='#cdcec6', width=2)
sheet.save(ROOT / 'altar_fountain_preview.png')
(ROOT / 'README.txt').write_text('Standalone preview assets. Not installed in the game.\nY-up, meter units. OBJ models share ruins.mtl; keep the MTL alongside all six OBJ files.\nEach stone is a named group. Flat-shaded bevelled geometry, no image textures.\nRun create_models.py with Python, NumPy and Pillow to regenerate.\nPreview images render the same triangles exported to OBJ.\n', encoding='utf-8')
