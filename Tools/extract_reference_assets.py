#!/usr/bin/env python3
"""
Extracts artwork from Design/ui-reference.png into Resources/ (fixed file names used by the UI):
  Resources/Logo/logo.png, logo@2x.png      grunge logo with alpha
  Resources/Photos/halfcut_1..8.jpg         the 8 polaroid photos of the reference (thread / pins retouched)
  Resources/Textures/background.png         black grain tile (mirrored -> seamless)
Requires Pillow (pip install pillow). Re-run after changing the reference.
"""
import os
import random
from PIL import Image, ImageFilter, ImageChops

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REF = os.path.join(ROOT, "Design", "ui-reference.png")

# reference pixel coordinates (1586 x 992 image)
LOGO_BOX = (58, 8, 790, 151)
PHOTO_BOXES = [
    (267, 299, 456, 463), (490, 299, 671, 463), (707, 299, 885, 463), (923, 299, 1100, 463),
    (268, 550, 454, 703), (491, 550, 670, 703), (707, 550, 885, 703), (923, 550, 1101, 703),
]
BG_BOX = (0, 800, 212, 992)
BASE_LOGO_WIDTH = 460          # logo width on the 1000 x 625 canvas


def extract_logo(img):
    crop = img.crop(LOGO_BOX).convert("RGB")
    w, h = crop.size
    out = Image.new("RGBA", (w, h))
    src, dst = crop.load(), out.load()
    for y in range(h):
        for x in range(w):
            r, g, b = src[x, y]
            m = max(r, g, b)
            a = max(0.0, min(1.0, (m - 55) / 70.0))
            dst[x, y] = (r, g, b, int(a * 255))
    # trim transparent borders
    bbox = out.getbbox()
    out = out.crop(bbox)
    scale2 = (BASE_LOGO_WIDTH * 2) / out.width
    big = out.resize((BASE_LOGO_WIDTH * 2, round(out.height * scale2)), Image.LANCZOS)
    small = out.resize((BASE_LOGO_WIDTH, round(out.height * BASE_LOGO_WIDTH / out.width)), Image.LANCZOS)
    os.makedirs(os.path.join(ROOT, "Resources", "Logo"), exist_ok=True)
    big.save(os.path.join(ROOT, "Resources", "Logo", "logo@2x.png"))
    small.save(os.path.join(ROOT, "Resources", "Logo", "logo.png"))
    print("logo", small.size, big.size)


def inpaint(photo, mask, iterations=200):
    """Diffusion inpainting of masked pixels (grayscale photo), then re-adds film grain."""
    w, h = photo.size
    p = photo.load()
    m = mask.load()
    vals = [[float(p[x, y]) for x in range(w)] for y in range(h)]
    holes = [(x, y) for y in range(h) for x in range(w) if m[x, y]]
    # initial guess: nearest unmasked value in the same column
    for x, y in holes:
        for d in range(1, h):
            for yy in (y - d, y + d):
                if 0 <= yy < h and not m[x, yy]:
                    vals[y][x] = float(p[x, yy])
                    break
            else:
                continue
            break
    for _ in range(iterations):
        for x, y in holes:
            s, n = 0.0, 0
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                xx, yy = x + dx, y + dy
                if 0 <= xx < w and 0 <= yy < h:
                    s += vals[yy][xx]
                    n += 1
            vals[y][x] = s / n
    rnd = random.Random(5)
    for x, y in holes:
        p[x, y] = int(max(0, min(255, vals[y][x] + rnd.gauss(0, 3.5))))
    return photo


def extract_photos(img):
    rgb = img.convert("RGB")
    for i, box in enumerate(PHOTO_BOXES, start=1):
        crop = rgb.crop(box)
        w, h = crop.size
        gray = crop.convert("L")
        mask = Image.new("1", (w, h), 0)
        src, mk, gp = crop.load(), mask.load(), gray.load()
        for y in range(h):
            for x in range(w):
                r, g, b = src[x, y]
                red = r > g + 22 and r > b + 22                  # thread, pins, red frame bleed
                tag = y < 40 and i in (1, 2) and gp[x, y] > 150 and 80 < x < 140 if i == 1 else False
                if i == 2 and y < 40 and 80 < x < 135 and gp[x, y] > 150:
                    tag = True
                if red or tag:
                    mk[x, y] = 1
        # tags A / B are paper rectangles: mask their full box
        if i == 1:
            for y in range(0, 36):
                for x in range(88, 128):
                    mk[x, y] = 1
        if i == 2:
            for y in range(0, 36):
                for x in range(90, 130):
                    mk[x, y] = 1
        mask = mask.filter(ImageFilter.MaxFilter(3))
        fixed = inpaint(gray, mask)
        out = fixed.resize((512, round(512 * h / w)), Image.LANCZOS).convert("RGB")
        path = os.path.join(ROOT, "Resources", "Photos", f"halfcut_{i}.jpg")
        out.save(path, quality=90)
        print("photo", path, out.size, "retouched px:", mask.histogram()[-1])


def extract_background(img):
    crop = img.crop(BG_BOX).convert("RGB")
    w, h = crop.size
    tile = Image.new("RGB", (w * 2, h * 2))
    tile.paste(crop, (0, 0))
    tile.paste(crop.transpose(Image.FLIP_LEFT_RIGHT), (w, 0))
    tile.paste(crop.transpose(Image.FLIP_TOP_BOTTOM), (0, h))
    tile.paste(crop.transpose(Image.ROTATE_180), (w, h))
    os.makedirs(os.path.join(ROOT, "Resources", "Textures"), exist_ok=True)
    tile.save(os.path.join(ROOT, "Resources", "Textures", "background.png"))
    print("background", tile.size)


if __name__ == "__main__":
    ref = Image.open(REF)
    extract_logo(ref)
    extract_photos(ref)
    extract_background(ref)
