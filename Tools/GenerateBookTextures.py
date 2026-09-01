"""Generate deterministic printed pages and leather textures (requires Pillow).
Run from any directory. Generated PNGs are runtime assets; Python is not required by GJ.
"""
from pathlib import Path
import math
import random
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'resources/Models/StageSelectBook'
FONT = ROOT / 'resources/Fonts/NotoSansJP/NotoSansJP-Variable.ttf'


def font(size):
    return ImageFont.truetype(str(FONT), size)


def page(kind):
    rng = random.Random(281 + kind)
    image = Image.new('RGB', (768, 1024))
    pixels = image.load()
    for y in range(1024):
        for x in range(768):
            grain = rng.uniform(-3, 3)
            edge = min(x, 767-x, y, 1023-y)
            age = 12 * math.exp(-edge / 28)
            pixels[x, y] = tuple(int(c + grain - age) for c in (231, 219, 190))
    d = ImageDraw.Draw(image)
    ink = (104, 84, 60)
    faint = (151, 130, 99)
    d.rectangle((44, 44, 724, 980), outline=faint, width=2)
    d.text((76, 68), 'THE STAGE ARCHIVE', font=font(18), fill=faint)
    titles = ['I. THE JOURNEY', 'II. ATLAS OF WORLDS', 'III. HERBARIUM',
              'IV. CELESTIAL CHART', 'V. THE OLD GATE', 'VI. FIELD NOTES',
              'VII. TIDE AND CURRENT', 'VIII. THE CLOCKWORK']
    d.text((76, 115), titles[kind], font=font(29), fill=ink)
    d.line((76, 168, 690, 168), fill=faint, width=2)
    if kind == 0:
        lines = [
            'Beyond the shelves, a thousand roads await.',
            'Each leaf remembers a world once travelled.',
            'Follow the river past the quiet hills,',
            'where the old stone gate guards the valley.',
            '', 'NOTES FROM THE EXPLORER', '',
            'Look for a path where the light gathers.',
            'The smallest step begins the longest story.',
        ]
        for i, line in enumerate(lines):
            d.text((76, 200+i*34), line, font=font(18), fill=ink)
        for x, h in ((160,100),(300,145),(450,120),(580,85)):
            d.polygon([(x-65,800),(x,800-h),(x+80,800)], outline=faint, width=3)
            for j in range(5):
                d.line((x-j*8,800-h+j*16,x+20-j*4,790), fill=faint)
        d.text((180, 855), 'PLATE 01 - THE DISTANT RIDGE', font=font(16), fill=faint)
    elif kind == 1:
        # A hand-drafted map with rivers, contours, a route, and a compass rose.
        for j in range(5):
            points = []
            for n in range(81):
                a = n * math.tau / 80
                r = 190 + j*13 + 22*math.sin(a*3) + 13*math.cos(a*7)
                points.append((385+math.cos(a)*r, 495+math.sin(a)*r*1.1))
            d.line(points, fill=faint, width=2)
        river = [(365+30*math.sin(y/55), y) for y in range(250,780,4)]
        d.line(river, fill=(110,135,128), width=6)
        route = [(155,680),(255,580),(420,540),(510,385),(590,310)]
        d.line(route, fill=ink, width=2)
        for i,(x,y) in enumerate(route):
            d.ellipse((x-7,y-7,x+7,y+7), fill=ink)
            d.text((x+10,y-24), f'{i+1:02}', font=font(18), fill=ink)
        for x,y in ((200,340),(530,600),(460,275)):
            d.polygon([(x-24,y+20),(x,y-25),(x+24,y+20)], outline=ink, width=2)
        d.line((635,800,635,890), fill=ink, width=2)
        d.line((600,845,670,845), fill=ink, width=2)
        d.polygon([(635,800),(626,843),(644,843)], fill=ink)
        d.text((628,772),'N',font=font(18),fill=ink)
        d.text((85,905),'A route is a promise, not a boundary.',font=font(18),fill=ink)
    elif kind == 2:
        for x in (210, 530):
            d.line((x,800,x-25,290),fill=ink,width=3)
            for j in range(6):
                y=350+j*65
                d.ellipse((x-100,y-50,x-10,y+15),outline=faint,width=3)
                d.ellipse((x,y-80,x+100,y-15),outline=faint,width=3)
        d.text((90,850),'Specimens gathered along the forest road.',font=font(19),fill=ink)
    elif kind == 3:
        for r in (90,180,260):
            d.ellipse((384-r,520-r,384+r,520+r),outline=faint,width=2)
        stars=[(rng.randint(120,650),rng.randint(260,790)) for _ in range(30)]
        for x,y in stars:
            d.ellipse((x-3,y-3,x+3,y+3),fill=ink)
        d.line(stars[:9],fill=ink,width=2)
        d.text((115,860),'The northern lights guide the traveller.',font=font(19),fill=ink)
    elif kind == 4:
        d.rectangle((190,400,580,790),outline=ink,width=4)
        d.arc((190,220,580,620),180,360,fill=ink,width=4)
        d.rectangle((285,460,480,790),outline=faint,width=3)
        for y in range(430,790,45):
            d.line((190,y,275,y),fill=faint,width=2)
            d.line((490,y,580,y),fill=faint,width=2)
        d.text((120,860),'Elevation of the forgotten eastern gate.',font=font(19),fill=ink)
    elif kind == 5:
        notes=['DAY 01 - ARRIVAL','The valley is quiet before sunrise.',
               'A narrow bridge crosses the northern river.','',
               'DAY 02 - THE PASS','We found old markings beneath the arch.',
               'Three lanterns still burn along the path.','',
               'DAY 03 - RETURN','Keep the map dry. Follow the morning light.']
        for i,line in enumerate(notes):
            d.text((80,230+i*49),line,font=font(20),fill=ink)
        d.rectangle((440,790,650,860),outline=faint,width=2)
        d.text((458,807),'RECORDED',font=font(23),fill=faint)
    elif kind == 6:
        for row in range(10):
            pts=[(x,300+row*44+18*math.sin(x/70+row*.5)) for x in range(90,690,5)]
            d.line(pts,fill=(104,131,126),width=2)
        d.polygon([(380,420),(340,580),(470,580)],outline=ink,width=3)
        d.line((380,405,380,650),fill=ink,width=3)
        d.line((300,630,480,630,450,665,330,665,300,630),fill=ink,width=3)
        d.text((105,850),'Soundings taken beyond the southern cape.',font=font(19),fill=ink)
    else:
        for cx,cy,r in ((295,425,130),(500,610,110),(240,730,70)):
            for radius in (r,r*.68,r*.22):
                d.ellipse((cx-radius,cy-radius,cx+radius,cy+radius),outline=ink,width=3)
            for n in range(16):
                a=n*math.tau/16
                d.line((cx+math.cos(a)*r*.8,cy+math.sin(a)*r*.8,
                        cx+math.cos(a)*r*1.13,cy+math.sin(a)*r*1.13),fill=faint,width=5)
        d.text((100,885),'Mechanism of the archive door - plate VIII',font=font(18),fill=ink)
    d.text((370, 942), f'{kind+1:02}', font=font(20), fill=ink)
    return image


def generate():
    OUT.mkdir(parents=True, exist_ok=True)
    atlas = Image.new('RGB', (768*8,1024))
    for kind in range(8):
        atlas.paste(page(kind),(768*kind,0))
    atlas.save(OUT/'PrintedPages.png')
    rng = random.Random(113)
    leather = Image.new('RGB',(768,1024))
    leather.putdata([tuple(max(0,int(c+rng.uniform(-5,5))) for c in (67,31,22)) for _ in range(768*1024)])
    d = ImageDraw.Draw(leather)
    gold = (169,126,60)
    for inset in (36,43,62):
        d.rounded_rectangle((inset,inset,767-inset,1023-inset),radius=12,outline=gold,width=2)
    for x,y in ((85,85),(682,85),(85,938),(682,938)):
        d.ellipse((x-15,y-15,x+15,y+15),outline=gold,width=3)
    d.text((190,380),'THE STAGE',font=font(40),fill=gold)
    d.text((224,440),'ARCHIVE',font=font(40),fill=gold)
    d.polygon([(384,545),(440,610),(384,675),(328,610)],outline=gold,width=3)
    leather.save(OUT/'BookLeather.png')
    print('Generated PrintedPages.png and BookLeather.png')


if __name__ == '__main__':
    generate()
