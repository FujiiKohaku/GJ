"""Regenerate the stage archive OBJ/MTL/PNG assets using only Python's stdlib.

Run from any directory. Assets are checked in; Python is not needed at game runtime.
Coordinates match the existing upright stage-selection book (bottom y=-3).
"""
from pathlib import Path
import random
import struct
import zlib

OUT = Path(__file__).resolve().parents[1] / "resources/Models/StageSelectBook"
PALETTE = [
    (38, 30, 28), (78, 46, 31), (118, 76, 43), (176, 133, 65),
    (36, 57, 57), (87, 42, 39), (48, 59, 79), (113, 84, 46),
    (164, 151, 119), (24, 32, 37), (61, 48, 37), (213, 173, 98),
]


def write_palette():
    # Three shade rows give the unlit model depth without extra scene lights.
    width, height = len(PALETTE) * 16, 48
    pixels = bytearray()
    for y in range(height):
        pixels.append(0)
        shade = (0.65, 0.85, 1.0)[y // 16]
        for x in range(width):
            pixels.extend(round(c * shade) for c in PALETTE[x // 16])

    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))

    data = b"\x89PNG\r\n\x1a\n"
    data += chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
    data += chunk(b"IDAT", zlib.compress(pixels)) + chunk(b"IEND", b"")
    (OUT / "ArchivePalette.png").write_bytes(data)


class Mesh:
    def __init__(self):
        self.lines = ["mtllib ArchiveRoom.mtl", "o ArchiveRoom", "usemtl ArchivePalette"]
        self.vertices = 0
        self.faces = 0

    def box(self, center, size, color):
        x, y, z = center
        w, h, d = (s / 2 for s in size)
        points = [(x-w,y-h,z-d), (x+w,y-h,z-d), (x+w,y+h,z-d), (x-w,y+h,z-d),
                  (x-w,y-h,z+d), (x+w,y-h,z+d), (x+w,y+h,z+d), (x-w,y+h,z+d)]
        # Outward winding in OBJ space; Assimp handles engine handedness.
        faces = [(0,3,2,1), (4,5,6,7), (0,1,5,4), (3,7,6,2), (0,4,7,3), (1,2,6,5)]
        normals = [(0,0,-1), (0,0,1), (0,-1,0), (0,1,0), (-1,0,0), (1,0,0)]
        for face, normal, shade in zip(faces, normals, (2,0,0,2,0,1)):
            u, v = (color + 0.5) / len(PALETTE), 1 - (shade + 0.5) / 3
            for index in face:
                p = points[index]
                self.lines += [f"v {p[0]:.5f} {p[1]:.5f} {p[2]:.5f}",
                               f"vt {u:.6f} {v:.6f}", f"vn {normal[0]} {normal[1]} {normal[2]}"]
            indices = [self.vertices + i + 1 for i in range(4)]
            self.lines.append("f " + " ".join(f"{i}/{i}/{i}" for i in indices))
            self.vertices += 4
            self.faces += 1


def generate():
    OUT.mkdir(parents=True, exist_ok=True)
    mesh = Mesh()
    box = mesh.box
    rng = random.Random(731)
    # Dark panelled back wall, stone/wood floor, and a carpet leading to the book.
    box((0, 1, 9), (38, 24, 0.4), 9)
    box((0, -7.25, 0), (38, 0.5, 38), 0)
    for x in range(-18, 19, 3):
        box((x, 1, 8.65), (0.12, 24, 0.12), 10)
    box((0, -6.97, -7), (9, 0.04, 19), 4)
    for x in (-4.25, 4.25):
        box((x, -6.94, -7), (0.08, 0.025, 19), 3)
    # Side bookcases: structural posts, shelves, individually sized books and gilt spines.
    for cx in (-9, 9):
        box((cx, -0.5, 7.2), (6.4, 13, 0.35), 0)
        for dx in (-3.2, 3.2):
            box((cx+dx, -0.5, 5.8), (0.38, 13, 3.2), 1)
            box((cx+dx, -0.5, 4.15), (0.12, 12.5, 0.08), 3)
        for y in (-6.8, -3.6, -0.4, 2.8, 6):
            box((cx, y, 5.8), (6.8, 0.24, 3.4), 2)
            box((cx, y, 4.05), (6.8, 0.07, 0.1), 3)
        box((cx, 6.35, 5.8), (7.1, 0.4, 3.6), 1)
        for floor in (-6.68, -3.48, -0.28, 2.92):
            left = cx - 2.95
            while left < cx + 2.65:
                width = rng.uniform(0.3, 0.58)
                height = rng.uniform(1.65, 2.7)
                bx = left + width/2
                z = rng.uniform(4.85, 5.12)
                box((bx, floor+height/2, z), (width, height, 1.3), rng.choice((4,5,6,7)))
                for dy in (0.22, height-0.22):
                    box((bx, floor+dy, z-0.66), (width*0.84, 0.055, 0.025), 3)
                left += width + 0.08
    # Tiered central plinth. Top meets the existing book's bottom edge at y=-3.
    box((0, -6.75, 1), (10.8, 0.5, 5.6), 1)
    box((0, -6.43, 1), (10.3, 0.14, 5.2), 3)
    box((0, -4.9, 1), (8.3, 2.9, 3.8), 1)
    box((0, -3.28, 0.6), (10.6, 0.56, 4.8), 2)
    box((0, -3.62, 0.6), (10.3, 0.10, 4.7), 3)
    for x in (-3.7, 3.7):
        box((x, -4.85, -0.94), (0.22, 2.6, 0.1), 3)
    box((0, -4.75, -0.94), (2.1, 0.65, 0.12), 3)
    box((0, -4.75, -1.02), (1.85, 0.43, 0.05), 0)
    # Back support and a low retaining lip keep the upright book visually grounded.
    box((0, -1.5, 0.9), (7.5, 3.0, 0.35), 1)
    box((0, -2.94, -0.02), (9.6, 0.12, 0.45), 3)
    write_palette()
    (OUT / "ArchiveRoom.mtl").write_text(
        "newmtl ArchivePalette\nKa 1 1 1\nKd 1 1 1\nillum 1\nmap_Kd ArchivePalette.png\n", encoding="utf-8")
    (OUT / "ArchiveRoom.obj").write_text("\n".join(mesh.lines) + "\n", encoding="utf-8")
    print(f"Generated archive: {mesh.vertices} vertices, {mesh.faces * 2} triangles in {OUT}")


if __name__ == "__main__":
    generate()
