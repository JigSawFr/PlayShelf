#!/usr/bin/env python3
# Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
"""Draws the pictures of the example theme folder of docs/themes.md
(docs/examples/themes/nightfall): its backgrounds at night and at dawn, its
logo in each variant, the picture of a game without a cover
(playshelf/cover/placeholder), its thumbnail in the palette picker and the
faint pattern of its dialogs. Made here, so that the example has no
third-party picture.

    python3 tools/make_example_theme.py docs/examples/themes/nightfall

Needs Pillow and NumPy. The output is the same on every run (seeded).
"""
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

W, H = 1920, 1080


def gradient(stops, width, height):
    """A vertical gradient through `stops` [(at 0..1, (r, g, b)), ...]."""
    y = np.linspace(0.0, 1.0, height)
    at = [s[0] for s in stops]
    cols = [np.interp(y, at, [s[1][c] for s in stops]) for c in range(3)]
    column = np.stack(cols, axis=-1)[:, None, :]
    return np.repeat(column, width, axis=1)


def glow(width, height, cx, cy, radius, colour, strength):
    """A round soft light at cx, cy (fractions of the picture)."""
    y, x = np.mgrid[0:height, 0:width]
    d = np.hypot(x - cx * width, y - cy * height) / (radius * height)
    return (np.exp(-d * d) * strength)[..., None] * np.array(colour, float)[None, None, :]


def hills(img, rng, colour, base, amplitude, waves):
    """A soft band of hills along the bottom, `colour` with its alpha."""
    x = np.arange(W)
    line = np.full(W, base * H)
    for k, a in waves:
        line += amplitude * a * H * np.sin(x / W * 6.283 * k + rng.random() * 6.283)
    y = np.arange(H)[:, None]
    mask = np.clip((y - line[None, :]) / 3.0, 0.0, 1.0)   # 3 px of antialiasing
    a = colour[3] / 255.0
    rgb = np.array(colour[:3], float)[None, None, :]
    return img * (1 - mask[..., None] * a) + rgb * mask[..., None] * a


def stars(img, rng, count, top_share, brightness):
    ys = rng.integers(0, int(H * top_share), count)
    xs = rng.integers(0, W, count)
    b = rng.random(count) * brightness
    for x, y, v in zip(xs, ys, b):
        img[y, x] = np.minimum(255, img[y, x] + v)
        if v > brightness * 0.8:   # the bright ones a little larger
            for dx, dy in ((1, 0), (0, 1), (-1, 0), (0, -1)):
                if 0 <= y + dy < H and 0 <= x + dx < W:
                    img[y + dy, x + dx] = np.minimum(255, img[y + dy, x + dx] + v * 0.4)
    return img


def save_jpeg(arr, path):
    Image.fromarray(np.clip(arr, 0, 255).astype(np.uint8), "RGB").save(path, quality=86, optimize=True)


def backgrounds(out):
    rng = np.random.default_rng(7)
    night = gradient([(0.0, (10, 14, 38)), (0.55, (22, 30, 70)), (1.0, (34, 52, 86))], W, H)
    night += glow(W, H, 0.80, 0.22, 0.30, (150, 160, 255), 0.35)
    night += glow(W, H, 0.80, 0.22, 0.06, (230, 235, 255), 0.9)    # the moon
    night = stars(night, rng, 520, 0.7, 150)
    night = hills(night, rng, (24, 34, 66, 255), 0.78, 0.025, [(1.2, 1.0), (3.1, 0.4)])
    night = hills(night, rng, (14, 20, 44, 255), 0.86, 0.02, [(1.7, 1.0), (4.3, 0.5)])
    save_jpeg(night, out / "night.jpg")

    rng = np.random.default_rng(11)
    day = gradient([(0.0, (238, 232, 250)), (0.6, (250, 236, 228)), (1.0, (246, 226, 214))], W, H)
    day += glow(W, H, 0.78, 0.30, 0.35, (255, 214, 170), 0.25)
    day = hills(day, rng, (226, 214, 236, 255), 0.78, 0.025, [(1.2, 1.0), (3.1, 0.4)])
    day = hills(day, rng, (214, 200, 228, 255), 0.86, 0.02, [(1.7, 1.0), (4.3, 0.5)])
    save_jpeg(day, out / "day.jpg")


def logo(out, name, frame, moon, ink):
    """A crescent moon over a shelf of three books, drawn at 4x and reduced
    (smooth edges)."""
    s = 1024
    img = Image.new("RGBA", (s, s), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    d.rounded_rectangle((96, 96, s - 96, s - 96), 150, outline=frame, width=56)
    # The crescent: a disc, another one cut out of it
    d.ellipse((560, 220, 800, 460), fill=moon)
    d.ellipse((610, 190, 850, 430), fill=(0, 0, 0, 0))
    # The books and their shelf
    for x0, top in ((260, 470), (360, 420), (460, 500)):
        d.rounded_rectangle((x0, top, x0 + 80, 760), 18, fill=ink)
    d.rounded_rectangle((220, 770, 804, 820), 25, fill=frame)
    img.resize((256, 256), Image.LANCZOS).save(out / name, optimize=True)


def cover(out):
    """A game without a cover: a night sky in a square, a moon, a cartridge."""
    s = 512
    sky = gradient([(0.0, (30, 40, 92)), (1.0, (64, 52, 120))], s, s)
    y, x = np.mgrid[0:s, 0:s]
    d = np.hypot(x - 0.72 * s, y - 0.28 * s) / (0.18 * s)
    sky += (np.exp(-d * d) * 0.6)[..., None] * np.array([210, 210, 255.0])[None, None, :]
    rng = np.random.default_rng(3)
    for _ in range(60):
        px, py = rng.integers(0, s, 2)
        sky[py, px] = np.minimum(255, sky[py, px] + rng.random() * 160)
    img = Image.fromarray(np.clip(sky, 0, 255).astype(np.uint8), "RGB").convert("RGBA")
    big = Image.new("RGBA", (s * 2, s * 2), (0, 0, 0, 0))
    d = ImageDraw.Draw(big)
    d.ellipse((620, 170, 840, 390), fill=(240, 242, 255, 255))
    d.ellipse((670, 140, 890, 360), fill=(0, 0, 0, 0))
    # A cartridge, its label and its notch
    d.rounded_rectangle((330, 470, 694, 900), 40, fill=(236, 232, 248, 235))
    d.rounded_rectangle((380, 520, 644, 720), 22, fill=(120, 104, 200, 255))
    d.polygon([(330, 800), (380, 800), (330, 850)], fill=(0, 0, 0, 0))
    img.alpha_composite(big.resize((s, s), Image.LANCZOS))
    img.convert("RGB").save(out / "cover.png", optimize=True)


def preview(out):
    """The thumbnail in the palette picker: the night, the logo over it."""
    p = Image.open(out / "night.jpg").convert("RGBA")
    side = min(p.size)
    p = p.crop(((p.width - side) // 2, 0, (p.width + side) // 2, side)).resize((128, 128), Image.LANCZOS)
    mark = Image.open(out / "logo-dark.png").resize((84, 84), Image.LANCZOS)
    p.alpha_composite(mark, (22, 22))
    p.convert("RGB").save(out / "preview.png", optimize=True)


def pattern(out, name, colour):
    """A faint tile of small stars, for the dialogs (backdrop.dialog)."""
    rng = np.random.default_rng(5)
    img = Image.new("RGBA", (128, 128), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    for _ in range(14):
        x, y = rng.integers(4, 124, 2)
        r = 1 + int(rng.random() * 2)
        d.ellipse((x - r, y - r, x + r, y + r), fill=colour)
    img.filter(ImageFilter.GaussianBlur(0.6)).save(out / name, optimize=True)


def main():
    if len(sys.argv) != 2:
        print(__doc__)
        sys.exit(2)
    out = Path(sys.argv[1])
    out.mkdir(parents=True, exist_ok=True)
    backgrounds(out)
    logo(out, "logo-dark.png", (126, 140, 255, 255), (240, 242, 255, 255), (200, 206, 240, 255))
    logo(out, "logo-light.png", (92, 76, 190, 255), (232, 150, 60, 255), (70, 60, 140, 255))
    cover(out)
    preview(out)
    pattern(out, "stars-night.png", (220, 226, 255, 120))
    pattern(out, "stars-day.png", (110, 90, 190, 70))


if __name__ == "__main__":
    main()
