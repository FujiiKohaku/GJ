from pathlib import Path
import math
import random

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "resources" / "Models" / "ClearMeadow"
OUT.mkdir(parents=True, exist_ok=True)

MTL = """newmtl WhiteMaterial
Kd 1.0 1.0 1.0
Ka 0.1 0.1 0.1
map_Kd ../../Textures/white.png
"""
(OUT / "WhiteMaterial.mtl").write_text(MTL, encoding="utf-8")


def add_box(vertices, faces, center, size):
    cx, cy, cz = center
    sx, sy, sz = (v * 0.5 for v in size)
    start = len(vertices) + 1
    vertices.extend([
        (cx-sx, cy-sy, cz-sz), (cx+sx, cy-sy, cz-sz),
        (cx+sx, cy+sy, cz-sz), (cx-sx, cy+sy, cz-sz),
        (cx-sx, cy-sy, cz+sz), (cx+sx, cy-sy, cz+sz),
        (cx+sx, cy+sy, cz+sz), (cx-sx, cy+sy, cz+sz),
    ])
    quads = [(0,3,2,1), (4,5,6,7), (0,4,7,3),
             (1,2,6,5), (3,7,6,2), (0,1,5,4)]
    faces.extend(tuple(start+i for i in quad) for quad in quads)


def add_cone(vertices, faces, center, radius, height, sides):
    cx, cy, cz = center
    base = len(vertices) + 1
    for i in range(sides):
        angle = math.tau * i / sides
        vertices.append((cx + math.cos(angle)*radius, cy, cz + math.sin(angle)*radius))
    tip = len(vertices) + 1
    vertices.append((cx, cy + height, cz))
    faces.append(tuple(base+i for i in range(sides)))
    for i in range(sides):
        faces.append((base+(i+1)%sides, base+i, tip))


def add_tapered_branch(vertices, faces, start_point, end_point, start_radius, end_radius, sides=7):
    sx, sy, sz = start_point
    ex, ey, ez = end_point
    dx, dy, dz = ex-sx, ey-sy, ez-sz
    length = math.sqrt(dx*dx + dy*dy + dz*dz)
    ux, uy, uz = dx/length, dy/length, dz/length
    helper = (0.0, 1.0, 0.0) if abs(uy) < 0.9 else (1.0, 0.0, 0.0)
    ax = uy*helper[2] - uz*helper[1]
    ay = uz*helper[0] - ux*helper[2]
    az = ux*helper[1] - uy*helper[0]
    al = math.sqrt(ax*ax + ay*ay + az*az)
    ax, ay, az = ax/al, ay/al, az/al
    bx, by, bz = uy*az-uz*ay, uz*ax-ux*az, ux*ay-uy*ax
    base = len(vertices) + 1
    for px, py, pz, radius in ((sx,sy,sz,start_radius),(ex,ey,ez,end_radius)):
        for i in range(sides):
            angle = math.tau*i/sides
            rx = (ax*math.cos(angle)+bx*math.sin(angle))*radius
            ry = (ay*math.cos(angle)+by*math.sin(angle))*radius
            rz = (az*math.cos(angle)+bz*math.sin(angle))*radius
            vertices.append((px+rx,py+ry,pz+rz))
    faces.append(tuple(base+i for i in reversed(range(sides))))
    faces.append(tuple(base+sides+i for i in range(sides)))
    for i in range(sides):
        faces.append((base+i, base+(i+1)%sides,
                      base+sides+(i+1)%sides, base+sides+i))


def add_low_poly_crown(vertices, faces, center, radius, seed):
    rng = random.Random(seed)
    cx, cy, cz = center
    rings, segments = 3, 9
    base = len(vertices) + 1
    vertices.append((cx, cy+radius*(0.88+rng.random()*0.12), cz))
    for ring in range(1, rings+1):
        phi = math.pi*ring/(rings+1)
        for segment in range(segments):
            theta = math.tau*segment/segments + rng.uniform(-0.13,0.13)
            variation = radius*rng.uniform(0.82,1.12)
            vertices.append((cx+math.sin(phi)*math.cos(theta)*variation,
                             cy+math.cos(phi)*variation,
                             cz+math.sin(phi)*math.sin(theta)*variation))
    bottom = len(vertices) + 1
    vertices.append((cx, cy-radius*(0.82+rng.random()*0.12), cz))
    for segment in range(segments):
        faces.append((base, base+1+(segment+1)%segments, base+1+segment))
    for ring in range(rings-1):
        a = base+1+ring*segments
        b = a+segments
        for segment in range(segments):
            nxt = (segment+1)%segments
            if (segment+ring)%2:
                faces.extend([(a+segment,a+nxt,b+segment),(a+nxt,b+nxt,b+segment)])
            else:
                faces.extend([(a+segment,b+nxt,b+segment),(a+segment,a+nxt,b+nxt)])
    last = base+1+(rings-1)*segments
    for segment in range(segments):
        faces.append((last+segment,last+(segment+1)%segments,bottom))


def write_obj(name, vertices, faces):
    lines = ["mtllib WhiteMaterial.mtl", "usemtl WhiteMaterial"]
    lines += [f"v {x:.6f} {y:.6f} {z:.6f}" for x, y, z in vertices]
    normals = []
    for face in faces:
        a, b, c = (vertices[index-1] for index in face[:3])
        ab = (b[0]-a[0], b[1]-a[1], b[2]-a[2])
        ac = (c[0]-a[0], c[1]-a[1], c[2]-a[2])
        normal = (ab[1]*ac[2]-ab[2]*ac[1],
                  ab[2]*ac[0]-ab[0]*ac[2],
                  ab[0]*ac[1]-ab[1]*ac[0])
        length = math.sqrt(sum(value*value for value in normal)) or 1.0
        normals.append(tuple(value/length for value in normal))
    lines += [f"vn {x:.6f} {y:.6f} {z:.6f}" for x, y, z in normals]
    lines += ["f " + " ".join(f"{index}//{face_index}" for index in face)
              for face_index, face in enumerate(faces, 1)]
    (OUT / name).write_text("\n".join(lines) + "\n", encoding="utf-8")


trunk_v, trunk_f = [], []
add_tapered_branch(trunk_v,trunk_f,(0,0,0),(0.28,2.2,0.05),0.48,0.36)
add_tapered_branch(trunk_v,trunk_f,(0.28,2.2,0.05),(-0.05,4.15,0.12),0.36,0.23)
add_tapered_branch(trunk_v,trunk_f,(-0.05,4.15,0.12),(-1.85,5.15,0.18),0.25,0.12)
add_tapered_branch(trunk_v,trunk_f,(-0.05,4.15,0.12),(1.70,5.35,-0.12),0.27,0.13)
add_tapered_branch(trunk_v,trunk_f,(0.05,3.55,0.10),(0.20,5.70,0.20),0.24,0.11)
add_tapered_branch(trunk_v,trunk_f,(0.28,2.6,0.05),(1.05,4.45,0.65),0.25,0.10)
write_obj("MeadowTreeTrunk.obj", trunk_v, trunk_f)

leaf_v, leaf_f = [], []
clusters = [
    (-2.25,5.35,0.05,1.65),(-1.05,5.95,-0.25,1.75),(0.35,6.25,0.05,1.85),
    (1.75,5.85,-0.15,1.70),(2.65,5.20,0.15,1.42),(-2.75,4.65,0.20,1.35),
    (-1.35,4.65,0.45,1.55),(0.25,4.90,0.55,1.60),(1.65,4.65,0.50,1.48),
    (-0.45,5.45,-0.95,1.30),(1.05,5.35,-0.90,1.32)
]
for index,(x,y,z,radius) in enumerate(clusters):
    add_low_poly_crown(leaf_v,leaf_f,(x,y,z),radius,100+index)
write_obj("MeadowTreeCanopy.obj", leaf_v, leaf_f)

mountain_v, mountain_f = [], []
add_cone(mountain_v, mountain_f, (0, 0, 0), 5.0, 7.0, 7)
add_cone(mountain_v, mountain_f, (-3.8, 0, 1.0), 3.5, 4.8, 7)
add_cone(mountain_v, mountain_f, (4.0, 0, 1.5), 3.8, 5.3, 7)
write_obj("MeadowMountain.obj", mountain_v, mountain_f)

print(OUT)
