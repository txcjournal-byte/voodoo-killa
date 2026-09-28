#!/usr/bin/env python3
"""
Fills the polaroids of all categories with provisional photos derived from the 8 HALFCUT photos
(Resources/Photos/halfcut_1..8.jpg): every card gets its own crop, zoom, mirror, tilt, contrast and grain,
and each category uses all 8 motifs in a different order.

Files that already exist are kept unless --force is given, so real photos generated from prompts.md
simply replace these by using the same file names.
Requires Pillow.
"""
import os
import random
import sys
from PIL import Image, ImageEnhance, ImageFilter, ImageOps, ImageChops

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PHOTOS = os.path.join(ROOT, "Resources", "Photos")
CATEGORIES = ["deadtape", "glitchritval", "chopped", "backmask", "diabolvs", "cassettecvlt",
              "villainera", "aura", "toxic", "crashout", "lockin"]
OUT_W, OUT_H = 512, 448


def vignette(size, strength):
    w, h = size
    mask = Image.radial_gradient("L").resize((w, h))          # 0 in the centre, 255 at the edge
    return mask.point(lambda v: int(255 - v * strength))


def derive(src, rnd):
    img = src.convert("L")
    if rnd.random() < 0.5:
        img = ImageOps.mirror(img)

    # zoomed crop around a random focal point
    zoom = rnd.uniform(1.25, 1.9)
    w, h = img.size
    cw, ch = w / zoom, (w / zoom) * OUT_H / OUT_W
    ch = min(ch, h)
    x0 = rnd.uniform(0, w - cw)
    y0 = rnd.uniform(0, h - ch)
    img = img.crop((int(x0), int(y0), int(x0 + cw), int(y0 + ch)))

    # slight tilt
    angle = rnd.uniform(-5, 5)
    img = img.rotate(angle, resample=Image.BICUBIC, expand=False)
    tw, th = img.size
    m = int(max(tw, th) * 0.06)
    img = img.crop((m, m, tw - m, th - m)).resize((OUT_W, OUT_H), Image.LANCZOS)

    # tone: contrast / brightness / gamma
    img = ImageEnhance.Contrast(img).enhance(rnd.uniform(1.0, 1.5))
    img = ImageEnhance.Brightness(img).enhance(rnd.uniform(0.75, 1.1))
    gamma = rnd.uniform(0.85, 1.25)
    img = img.point(lambda v: int(255 * (v / 255) ** gamma))

    # heavy film grain
    noise = Image.effect_noise((OUT_W, OUT_H), rnd.uniform(18, 34)).filter(ImageFilter.GaussianBlur(0.5))
    img = ImageChops.add(img, noise, scale=1.0, offset=-128)

    # flash vignette
    dark = Image.new("L", img.size, 0)
    img = Image.composite(img, dark, vignette(img.size, rnd.uniform(0.35, 0.7)))
    return img.convert("RGB")


def main():
    force = "--force" in sys.argv
    sources = [Image.open(os.path.join(PHOTOS, f"halfcut_{i}.jpg")) for i in range(1, 9)]
    made = 0
    for ci, cat in enumerate(CATEGORIES):
        order = list(range(8))
        random.Random(1000 + ci).shuffle(order)
        for card in range(8):
            path = os.path.join(PHOTOS, f"{cat}_{card + 1}.jpg")
            if os.path.exists(path) and not force:
                continue
            rnd = random.Random(ci * 97 + card * 13 + 7)
            derive(sources[order[card]], rnd).save(path, quality=88)
            made += 1
    print(f"created {made} photos")


if __name__ == "__main__":
    main()
