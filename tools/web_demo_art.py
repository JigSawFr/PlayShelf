#!/usr/bin/env python3
"""The web simulator's demo (web/README.md), the part tools/web_demo.cpp
leaves: each made-up game's pictures, what Nlib API says of it and a title
database, from the same table (tools/web_demo_games.json), into the demo's
folder:

- icons/<id>.jpg: its icon, 256 × 256 (the catalogue's; console/control/
  for the games on the console at a first start, and nlib/<id>/icon.jpg);
- nlib/<id>/banner.jpg, 854 × 480, and screen<n>.jpg, 1280 × 720: its
  banner and screenshots, as Nlib API serves a game's;
- nlib/<id>/details.en.json and details.fr.json: its details, in Nlib API's
  shape (titledb/nlib.cpp reads them);
- titledb/index.json: every game of the table, as NXMeta's Tinfoil index
  (titledb/list.cpp): names, studios, releases, the coming ones among them.

Every picture is drawn here, the same at every run: a scene of its kind
(SCENES: an open sky of islands, a kart track, a harbour village…) in the
game's colours, and on the icon its name in Sora
(resources/font, OFL). Nothing of a real game: no name, character, logo or
picture. Needs Pillow.

Usage: tools/web_demo_art.py <games.json> <demo-dir>
"""
import colorsys
import datetime
import json
import math
import os
import random
import sys

from PIL import Image, ImageDraw, ImageFilter, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FONT = os.path.join(ROOT, "resources", "font", "sora_bold_latin.ttf")
SS = 2                      # drawn this many times larger, then reduced: smooth edges
QUALITY = 78                # JPEG
SCREENS = 3                 # screenshots a game


def tid(index):
    """The title id of the table's game `index` (from 0), as web_demo.cpp makes it."""
    return 0x0100DE0000000000 | ((index + 1) << 16)


def rgb(h, s, v):
    r, g, b = colorsys.hsv_to_rgb(h % 1.0, max(0.0, min(1.0, s)), max(0.0, min(1.0, v)))
    return (int(r * 255), int(g * 255), int(b * 255))


def mix(a, b, t):
    return tuple(int(a[i] + (b[i] - a[i]) * t) for i in range(3))


class Palette:
    """A game's colours, from its two hues."""

    def __init__(self, hues):
        a, b = hues
        self.a, self.b = a, b
        self.sky_top = rgb(a, 0.55, 0.45)
        self.sky_low = rgb(b, 0.35, 0.95)
        self.main = rgb(a, 0.70, 0.90)
        self.second = rgb(b, 0.75, 0.85)
        self.dark = rgb(a, 0.60, 0.22)
        self.light = rgb(b, 0.15, 1.0)


# ------------------------------------------------------------- drawing

def gradient(img, top, bottom, y0=0, y1=None):
    w, h = img.size
    y1 = h if y1 is None else y1
    d = ImageDraw.Draw(img)
    for y in range(y0, y1):
        d.line([(0, y), (w, y)], fill=mix(top, bottom, (y - y0) / max(1, y1 - y0 - 1)))


def ridge(d, w, h, base, amp, color, rnd, step=None, jag=0.4):
    """A row of hills or mountains: a polygon from the left edge to the right."""
    step = step or w // 9
    pts = [(0, h)]
    x = 0
    phase = rnd.random() * 6
    while x <= w + step:
        y = base - amp * (0.5 + 0.5 * math.sin(x / w * 5 + phase)) - amp * jag * rnd.random()
        pts.append((x, y))
        x += step * (0.6 + rnd.random() * 0.8)
    pts.append((w, h))
    d.polygon(pts, fill=color)


def stars(d, w, h, rnd, n, color=(255, 255, 255), top=1.0):
    for _ in range(n):
        x, y = rnd.random() * w, rnd.random() * h * top
        r = rnd.choice((1, 1, 1, 2, 2, 3)) * SS
        d.ellipse([x - r, y - r, x + r, y + r], fill=color)


def disc(d, x, y, r, color, outline=None, width=0):
    d.ellipse([x - r, y - r, x + r, y + r], fill=color, outline=outline, width=width)


def cloud(d, x, y, s, color):
    for dx, dy, r in ((0, 0, 1.0), (0.9, 0.15, 0.8), (-0.9, 0.2, 0.75), (0.4, -0.45, 0.7)):
        disc(d, x + dx * s, y + dy * s, r * s, color)


def glow(img, x, y, r, color, strength=0.7):
    layer = Image.new("RGB", img.size, (0, 0, 0))
    ImageDraw.Draw(layer).ellipse([x - r, y - r, x + r, y + r], fill=color)
    layer = layer.filter(ImageFilter.GaussianBlur(r * 0.6))
    return Image.blend(img, Image.composite(layer, img, layer.convert("L")), strength)


# ------------------------------------------------------------- the scenes
# Each draws a whole picture of w × h (already SS times larger) for a game:
# `p` its palette, `rnd` its random numbers (the same at every run), `v`
# which picture of it (0: the banner and icon, 1…: the screenshots).

def scene_sky(img, p, rnd, v):
    w, h = img.size
    gradient(img, rgb(0.56 + (p.a - 0.3) * 0.3, 0.55, 0.9), rgb(p.b, 0.18, 1.0))
    d = ImageDraw.Draw(img)
    disc(d, w * 0.8, h * 0.22, h * 0.09, (255, 244, 214))
    for i in range(5):
        cloud(d, rnd.random() * w, h * (0.15 + rnd.random() * 0.4), h * 0.05, (255, 255, 255))
    # Floating islands: a green top, a rock below.
    for i in range(4 + v):
        cx, cy, s = rnd.random() * w, h * (0.25 + rnd.random() * 0.45), h * (0.05 + rnd.random() * 0.08)
        d.polygon([(cx - s * 2, cy), (cx + s * 2, cy), (cx + s * 0.4, cy + s * 2.2), (cx - s * 0.6, cy + s * 1.6)], fill=rgb(p.a, 0.35, 0.45))
        d.ellipse([cx - s * 2.1, cy - s * 0.5, cx + s * 2.1, cy + s * 0.5], fill=rgb(0.30, 0.6, 0.65))
        d.polygon([(cx - s * 0.3, cy - s * 0.2), (cx, cy - s * 1.4), (cx + s * 0.3, cy - s * 0.2)], fill=rgb(0.33, 0.6, 0.4))
    ridge(d, w, h, h * 0.92, h * 0.12, rgb(p.a, 0.45, 0.55), rnd)
    # A glider, far away.
    gx, gy = w * (0.3 + 0.1 * v), h * 0.38
    d.polygon([(gx - h * 0.06, gy), (gx + h * 0.06, gy), (gx, gy - h * 0.02)], fill=(80, 60, 50))
    d.line([(gx, gy), (gx, gy + h * 0.035)], fill=(60, 40, 30), width=2 * SS)


def scene_kart(img, p, rnd, v):
    w, h = img.size
    gradient(img, rgb(0.58, 0.5, 0.95), rgb(0.55, 0.15, 1.0), 0, int(h * 0.45))
    d = ImageDraw.Draw(img)
    ridge(d, w, h, h * 0.5, h * 0.1, rgb(0.33, 0.5, 0.6), rnd)
    d.rectangle([0, h * 0.45, w, h], fill=rgb(0.30, 0.55, 0.65))
    # The track, to the horizon, with its kerbs.
    vx = w * (0.5 + 0.1 * (v - 1))
    d.polygon([(vx - w * 0.02, h * 0.45), (vx + w * 0.02, h * 0.45), (w * 0.95, h), (w * 0.05, h)], fill=(70, 72, 84))
    for i in range(14):
        t0, t1 = (i / 14) ** 1.6, ((i + 0.5) / 14) ** 1.6
        for side in (-1, 1):
            x0 = vx + side * (w * 0.02 + (w * 0.45) * t0)
            x1 = vx + side * (w * 0.02 + (w * 0.45) * t1)
            d.line([(x0, h * 0.45 + h * 0.55 * t0), (x1, h * 0.45 + h * 0.55 * t1)],
                   fill=(230, 40, 40) if i % 2 else (250, 250, 250), width=int(SS * (2 + 10 * t0)))
    # Karts: a body, a driver's helmet, four wheels.
    colours = [p.main, p.second, rgb(p.a + 0.33, 0.8, 0.9), rgb(p.a + 0.6, 0.7, 0.95)]
    for k in range(3 + (v % 2)):
        t = 0.35 + 0.18 * k + 0.05 * rnd.random()
        s = h * 0.09 * t * 1.6
        x = vx + (rnd.random() - 0.5) * w * 0.5 * t
        y = h * 0.45 + h * 0.55 * t
        c = colours[k % len(colours)]
        for wx in (-1, 1):
            d.ellipse([x + wx * s * 1.1 - s * 0.35, y - s * 0.1, x + wx * s * 1.1 + s * 0.35, y + s * 0.6], fill=(25, 25, 30))
        d.rounded_rectangle([x - s * 1.3, y - s * 0.6, x + s * 1.3, y + s * 0.35], radius=s * 0.3, fill=c)
        disc(d, x, y - s * 0.8, s * 0.42, (250, 250, 250))
        d.rectangle([x - s * 0.35, y - s * 0.85, x + s * 0.35, y - s * 0.7], fill=(40, 40, 60))
    # A chequered banner across the track.
    bw = w * 0.3
    for i in range(16):
        for j in range(2):
            d.rectangle([vx - bw / 2 + i * bw / 16, h * 0.30 + j * h * 0.02, vx - bw / 2 + (i + 1) * bw / 16, h * 0.32 + j * h * 0.02],
                        fill=(20, 20, 20) if (i + j) % 2 else (245, 245, 245))


def scene_critters(img, p, rnd, v):
    w, h = img.size
    cold = p.b > 0.4 and p.b < 0.7
    gradient(img, rgb(0.58, 0.45, 0.95), rgb(0.55, 0.1, 1.0) if cold else rgb(0.1, 0.25, 1.0))
    d = ImageDraw.Draw(img)
    if cold:
        stars(d, w, h, rnd, 60, (255, 255, 255))
    else:
        d.polygon([(w * 0.55, h * 0.55), (w * 0.72, h * 0.18), (w * 0.9, h * 0.55)], fill=rgb(0.02, 0.5, 0.45))
        d.polygon([(w * 0.69, h * 0.24), (w * 0.72, h * 0.18), (w * 0.75, h * 0.24)], fill=(255, 120, 40))
    ridge(d, w, h, h * 0.62, h * 0.08, (235, 245, 255) if cold else rgb(0.28, 0.55, 0.7), rnd)
    d.rectangle([0, h * 0.7, w, h], fill=(225, 238, 250) if cold else rgb(0.30, 0.6, 0.6))
    # Critters: round bodies, ears, big eyes.
    for k in range(3 + v % 2):
        x, y = w * (0.18 + 0.22 * k + 0.05 * rnd.random()), h * (0.72 + 0.08 * rnd.random())
        s = h * (0.08 + 0.04 * rnd.random())
        c = rgb(p.a + k * 0.17, 0.65, 0.95)
        for side in (-1, 1):
            d.polygon([(x + side * s * 0.3, y - s * 0.7), (x + side * s * 0.95, y - s * 1.5), (x + side * s * 0.85, y - s * 0.4)], fill=c)
        disc(d, x, y, s, c)
        disc(d, x, y + s * 0.3, s * 0.55, mix(c, (255, 255, 255), 0.5))
        for side in (-1, 1):
            disc(d, x + side * s * 0.38, y - s * 0.2, s * 0.2, (255, 255, 255))
            disc(d, x + side * s * 0.38, y - s * 0.17, s * 0.11, (30, 30, 40))
        d.arc([x - s * 0.2, y - s * 0.05, x + s * 0.2, y + s * 0.2], 20, 160, fill=(40, 30, 30), width=SS * 2)


def scene_village(img, p, rnd, v):
    w, h = img.size
    gradient(img, rgb(0.08 + 0.5 * (v % 2), 0.4, 0.98), rgb(0.12, 0.2, 1.0), 0, int(h * 0.6))
    d = ImageDraw.Draw(img)
    gradient(img, rgb(0.55, 0.6, 0.85), rgb(0.55, 0.7, 0.55), int(h * 0.6), h)
    d = ImageDraw.Draw(img)
    for i in range(6):
        d.line([(rnd.random() * w, h * (0.65 + 0.3 * rnd.random())), (rnd.random() * w, h * (0.65 + 0.3 * rnd.random()))],
               fill=(255, 255, 255), width=SS)
    # A quay, houses with their roofs, round trees, a boat.
    d.rectangle([0, h * 0.55, w * 0.75, h * 0.62], fill=(170, 140, 110))
    x = w * 0.03
    while x < w * 0.7:
        bw, bh = w * (0.06 + 0.04 * rnd.random()), h * (0.12 + 0.08 * rnd.random())
        wall = rgb(rnd.random(), 0.25, 0.98)
        d.rectangle([x, h * 0.55 - bh, x + bw, h * 0.55], fill=wall)
        d.polygon([(x - bw * 0.1, h * 0.55 - bh), (x + bw / 2, h * 0.55 - bh - bh * 0.6), (x + bw * 1.1, h * 0.55 - bh)],
                  fill=rgb(p.a + rnd.random() * 0.1, 0.65, 0.75))
        d.rectangle([x + bw * 0.35, h * 0.55 - bh * 0.45, x + bw * 0.65, h * 0.55], fill=(110, 80, 60))
        x += bw * 1.35
        if rnd.random() < 0.4:
            disc(d, x - bw * 0.15, h * 0.53 - bh * 0.3, bh * 0.35, rgb(0.3, 0.6, 0.55))
    bx, by = w * 0.84, h * 0.72
    d.polygon([(bx - w * 0.07, by), (bx + w * 0.07, by), (bx + w * 0.05, by + h * 0.05), (bx - w * 0.05, by + h * 0.05)], fill=p.main)
    d.line([(bx, by), (bx, by - h * 0.16)], fill=(90, 70, 50), width=SS * 3)
    d.polygon([(bx + SS * 3, by - h * 0.15), (bx + w * 0.06, by - h * 0.03), (bx + SS * 3, by - h * 0.03)], fill=(250, 250, 245))


def scene_farm(img, p, rnd, v):
    w, h = img.size
    gradient(img, rgb(0.55, 0.45, 0.98), rgb(0.12, 0.25, 1.0), 0, int(h * 0.5))
    d = ImageDraw.Draw(img)
    disc(d, w * 0.15, h * 0.18, h * 0.08, (255, 226, 120))
    ridge(d, w, h, h * 0.52, h * 0.08, rgb(0.27, 0.45, 0.65), rnd)
    d.rectangle([0, h * 0.5, w, h], fill=rgb(0.25, 0.55, 0.62))
    # Rows of crops to the horizon.
    for i in range(12):
        y = h * 0.55 + (h * 0.45) * (i / 12) ** 1.3
        d.line([(0, y), (w, y)], fill=rgb(p.a, 0.6, 0.45), width=int(SS * (2 + i)))
        for j in range(int(30 - i * 1.5)):
            x = (j + 0.5 * (i % 2)) * w / (30 - i * 1.5)
            disc(d, x, y - SS * (2 + i), SS * (1.5 + i * 0.7), rgb(p.b, 0.75, 0.95))
    # A barn.
    bx, by, bw, bh = w * 0.62, h * 0.5, w * 0.16, h * 0.17
    d.rectangle([bx, by - bh, bx + bw, by], fill=(190, 50, 45))
    d.polygon([(bx - bw * 0.08, by - bh), (bx + bw / 2, by - bh * 1.6), (bx + bw * 1.08, by - bh)], fill=(120, 40, 35))
    d.rectangle([bx + bw * 0.35, by - bh * 0.6, bx + bw * 0.65, by], fill=(245, 235, 220))
    d.line([(bx + bw * 0.35, by - bh * 0.6), (bx + bw * 0.65, by)], fill=(190, 50, 45), width=SS * 3)
    d.line([(bx + bw * 0.65, by - bh * 0.6), (bx + bw * 0.35, by)], fill=(190, 50, 45), width=SS * 3)


def scene_dungeon(img, p, rnd, v):
    w, h = img.size
    img.paste(rgb(p.a, 0.6, 0.12), [0, 0, w, h])
    d = ImageDraw.Draw(img)
    # A corridor of arches, darker deeper in.
    for i in range(7, 0, -1):
        t = i / 7
        aw, ah = w * (0.15 + 0.75 * t), h * (0.2 + 0.75 * t)
        c = mix(rgb(p.a, 0.55, 0.35), rgb(p.a, 0.6, 0.08), 1 - t)
        d.rectangle([w / 2 - aw / 2, h * 0.95 - ah, w / 2 + aw / 2, h], fill=c)
        d.pieslice([w / 2 - aw / 2, h * 0.95 - ah - aw / 2, w / 2 + aw / 2, h * 0.95 - ah + aw / 2], 180, 360, fill=c)
        inner = mix(c, (0, 0, 0), 0.35)
        d.rectangle([w / 2 - aw * 0.42, h * 0.95 - ah * 0.95, w / 2 + aw * 0.42, h], fill=inner)
    for side in (-1, 1):
        x, y = w / 2 + side * w * 0.3, h * 0.45
        d.rectangle([x - SS * 3, y, x + SS * 3, y + h * 0.08], fill=(60, 40, 30))
        img = glow(img, x, y - h * 0.02, h * 0.08, (255, 140, 40), 0.9)
        d = ImageDraw.Draw(img)
        d.polygon([(x - h * 0.015, y), (x, y - h * 0.05), (x + h * 0.015, y)], fill=(255, 200, 80))
    for _ in range(60):
        x, y = rnd.random() * w, rnd.random() * h
        disc(d, x, y, SS * rnd.choice((1, 1, 2)), (255, 120 + int(rnd.random() * 100), 40))
    # The hero, a silhouette with a sword.
    hx, hy = w * (0.5 + 0.05 * (v - 1)), h * 0.88
    d.polygon([(hx - h * 0.04, hy), (hx + h * 0.04, hy), (hx + h * 0.025, hy - h * 0.14), (hx - h * 0.025, hy - h * 0.14)], fill=(20, 10, 15))
    disc(d, hx, hy - h * 0.17, h * 0.03, (20, 10, 15))
    d.line([(hx + h * 0.03, hy - h * 0.1), (hx + h * 0.12, hy - h * 0.2)], fill=(230, 230, 240), width=SS * 3)
    return img


def scene_caves(img, p, rnd, v):
    w, h = img.size
    gradient(img, rgb(p.a, 0.7, 0.18), rgb(p.b, 0.6, 0.35))
    d = ImageDraw.Draw(img)
    # Stalactites, platforms, lanterns glowing.
    x = 0
    while x < w:
        s = w * (0.02 + 0.03 * rnd.random())
        d.polygon([(x, 0), (x + s, 0), (x + s / 2, h * (0.08 + 0.15 * rnd.random()))], fill=rgb(p.a, 0.6, 0.12))
        x += s
    ridge(d, w, h, h * 0.95, h * 0.1, rgb(p.a, 0.6, 0.1), rnd)
    for i in range(5):
        px, py, pw = w * (0.1 + 0.18 * i), h * (0.45 + 0.3 * rnd.random()), w * (0.08 + 0.06 * rnd.random())
        d.rounded_rectangle([px, py, px + pw, py + h * 0.03], radius=SS * 4, fill=rgb(p.b, 0.4, 0.55))
        if i % 2 == v % 2:
            img = glow(img, px + pw / 2, py - h * 0.06, h * 0.06, (255, 210, 120), 0.8)
            d = ImageDraw.Draw(img)
            d.rectangle([px + pw / 2 - SS * 6, py - h * 0.08, px + pw / 2 + SS * 6, py - h * 0.04], fill=(255, 230, 160))
    for _ in range(30):
        disc(d, rnd.random() * w, rnd.random() * h, SS * 2, rgb(p.b, 0.3, 1.0))
    hx, hy = w * 0.32, h * 0.6
    d.ellipse([hx - h * 0.025, hy - h * 0.06, hx + h * 0.025, hy], fill=(15, 12, 25))
    disc(d, hx + h * 0.008, hy - h * 0.045, SS * 3, (255, 240, 200))
    return img


def scene_party(img, p, rnd, v):
    w, h = img.size
    gradient(img, p.main, p.second)
    d = ImageDraw.Draw(img)
    # Rays from the stage, confetti, balloons.
    cx, cy = w / 2, h * 1.1
    for i in range(18):
        a0, a1 = math.pi + i * math.pi / 18, math.pi + (i + 0.5) * math.pi / 18
        d.polygon([(cx, cy), (cx + math.cos(a0) * w, cy + math.sin(a0) * w), (cx + math.cos(a1) * w, cy + math.sin(a1) * w)],
                  fill=mix(p.main, (255, 255, 255), 0.25))
    for _ in range(140):
        x, y, s = rnd.random() * w, rnd.random() * h, SS * (3 + rnd.random() * 5)
        d.rectangle([x, y, x + s, y + s * 0.5], fill=rgb(rnd.random(), 0.8, 1.0))
    for k in range(6):
        x, y, r = w * (0.08 + 0.17 * k), h * (0.25 + 0.2 * rnd.random()), h * 0.06
        d.line([(x, y + r), (x + SS * 6, y + r * 3.5)], fill=(250, 250, 250), width=SS)
        d.ellipse([x - r * 0.85, y - r, x + r * 0.85, y + r], fill=rgb(p.a + k * 0.15, 0.75, 1.0))
    d.rounded_rectangle([w * 0.2, h * 0.8, w * 0.8, h * 1.05], radius=h * 0.05, fill=p.dark)
    for k in range(4):
        x = w * (0.3 + 0.13 * k)
        disc(d, x, h * 0.74, h * 0.045, rgb(p.a + k * 0.25, 0.7, 1.0))
        d.rounded_rectangle([x - h * 0.03, h * 0.76, x + h * 0.03, h * 0.82], radius=SS * 4, fill=rgb(p.a + k * 0.25, 0.7, 0.8))


def scene_kitchen(img, p, rnd, v):
    w, h = img.size
    gradient(img, (10, 12, 40), (40, 25, 70))
    d = ImageDraw.Draw(img)
    stars(d, w, h, rnd, 160)
    disc(d, w * 0.85, h * 1.1, h * 0.5, rgb(0.58, 0.5, 0.8))
    # A ring station, pots and plates floating.
    cx, cy = w * 0.42, h * 0.52
    d.ellipse([cx - w * 0.3, cy - h * 0.28, cx + w * 0.3, cy + h * 0.28], outline=(200, 205, 220), width=SS * 18)
    d.rounded_rectangle([cx - w * 0.12, cy - h * 0.1, cx + w * 0.12, cy + h * 0.12], radius=SS * 16, fill=(220, 225, 235))
    d.rectangle([cx - w * 0.1, cy - h * 0.02, cx + w * 0.1, cy + h * 0.02], fill=p.main)
    for k in range(7):
        a = k / 7 * math.tau + v
        x, y = cx + math.cos(a) * w * 0.3, cy + math.sin(a) * h * 0.28
        if k % 2:
            d.rounded_rectangle([x - h * 0.04, y - h * 0.03, x + h * 0.04, y + h * 0.04], radius=SS * 6, fill=(90, 90, 100))
            d.rectangle([x - h * 0.055, y - h * 0.03, x + h * 0.055, y - h * 0.018], fill=(120, 120, 130))
        else:
            disc(d, x, y, h * 0.035, (250, 250, 250))
            disc(d, x, y, h * 0.02, rgb(p.a + k * 0.1, 0.7, 0.95))


def scene_lighthouse(img, p, rnd, v):
    w, h = img.size
    gradient(img, (20, 28, 60), rgb(p.b, 0.4, 0.55), 0, int(h * 0.62))
    d = ImageDraw.Draw(img)
    stars(d, w, h, rnd, 70, top=0.5)
    gradient(img, (25, 40, 70), (10, 18, 35), int(h * 0.62), h)
    d = ImageDraw.Draw(img)
    for i in range(20):
        y = h * (0.64 + 0.36 * rnd.random())
        d.line([(rnd.random() * w, y), (rnd.random() * w, y)], fill=(70, 90, 130), width=SS)
    # The rock, the tower and its beam.
    d.polygon([(w * 0.55, h * 0.66), (w * 0.62, h * 0.55), (w * 0.78, h * 0.53), (w * 0.86, h * 0.66)], fill=(30, 30, 40))
    tx = w * 0.7
    d.polygon([(tx - w * 0.035, h * 0.55), (tx + w * 0.035, h * 0.55), (tx + w * 0.022, h * 0.2), (tx - w * 0.022, h * 0.2)], fill=(235, 230, 225))
    for k in range(3):
        y = h * (0.28 + 0.09 * k)
        d.rectangle([tx - w * 0.03, y, tx + w * 0.03, y + h * 0.035], fill=(190, 50, 50))
    d.rectangle([tx - w * 0.025, h * 0.14, tx + w * 0.025, h * 0.2], fill=(255, 230, 150) if v != 1 else (60, 60, 70))
    d.polygon([(tx - w * 0.03, h * 0.14), (tx, h * 0.1), (tx + w * 0.03, h * 0.14)], fill=(40, 40, 50))
    if v != 1:
        d.polygon([(tx, h * 0.17), (0, h * 0.05), (0, h * 0.3)], fill=mix((255, 240, 180), (40, 50, 80), 0.6))
        img = glow(img, tx, h * 0.17, h * 0.06, (255, 230, 150), 0.9)
    return img


def scene_desert(img, p, rnd, v):
    w, h = img.size
    gradient(img, rgb(0.07, 0.55, 1.0), rgb(0.12, 0.25, 1.0), 0, int(h * 0.6))
    d = ImageDraw.Draw(img)
    disc(d, w * 0.75, h * 0.25, h * 0.12, (255, 245, 210))
    for i, (base, c) in enumerate(((0.6, 0.5), (0.72, 0.6), (0.86, 0.7))):
        ridge(d, w, h, h * base, h * 0.1, rgb(0.08, c * 0.7, 0.95 - i * 0.1), rnd, step=w // 4, jag=0.1)
    # An oasis's palm, a fox's silhouette with its big ears.
    px, py = w * 0.2, h * 0.68
    d.line([(px, py), (px + w * 0.02, py - h * 0.2)], fill=(110, 80, 50), width=SS * 6)
    for a in range(6):
        ang = -math.pi / 2 + (a - 2.5) * 0.5
        d.line([(px + w * 0.02, py - h * 0.2), (px + w * 0.02 + math.cos(ang) * h * 0.12, py - h * 0.2 + math.sin(ang) * h * 0.06 + h * 0.03)],
               fill=(60, 140, 70), width=SS * 8)
    fx, fy, s = w * (0.55 + 0.05 * v), h * 0.8, h * 0.07
    c = (230, 140, 60)
    d.ellipse([fx - s, fy - s * 0.6, fx + s, fy + s * 0.5], fill=c)
    disc(d, fx + s * 0.9, fy - s * 0.8, s * 0.5, c)
    for side in (-1, 1):
        d.polygon([(fx + s * 0.9 + side * s * 0.15, fy - s * 1.1), (fx + s * 0.9 + side * s * 0.55, fy - s * 2.1), (fx + s * 0.9 + side * s * 0.6, fy - s * 0.9)], fill=c)
    d.polygon([(fx - s, fy - s * 0.2), (fx - s * 2, fy - s * 0.9), (fx - s * 1.7, fy + s * 0.1)], fill=(245, 235, 220))


def scene_snow(img, p, rnd, v):
    w, h = img.size
    gradient(img, rgb(0.58, 0.55, 0.9), rgb(0.55, 0.1, 1.0))
    d = ImageDraw.Draw(img)
    for i in range(3):
        ridge(d, w, h, h * (0.45 + 0.1 * i), h * 0.25, rgb(0.6, 0.12 - 0.03 * i, 0.85 + 0.05 * i), rnd, jag=0.6)
    # The run, a rider leaving a trail, flags.
    d.polygon([(w * 0.1, h), (w * 0.45, h * 0.4), (w * 0.55, h * 0.4), (w * 0.95, h)], fill=(250, 252, 255))
    pts = [(w * (0.5 + 0.08 * math.sin(t * 3 + v)), h * (0.4 + 0.6 * t)) for t in [i / 20 for i in range(21)]]
    d.line(pts, fill=(200, 215, 235), width=SS * 4)
    rx, ry = pts[12]
    d.line([(rx - h * 0.04, ry), (rx + h * 0.04, ry + h * 0.01)], fill=p.main, width=SS * 6)
    d.polygon([(rx - h * 0.012, ry), (rx + h * 0.012, ry), (rx + h * 0.008, ry - h * 0.06), (rx - h * 0.008, ry - h * 0.06)], fill=p.second)
    disc(d, rx, ry - h * 0.075, h * 0.016, (40, 40, 50))
    for k in range(6):
        t = k / 6
        for side in (-1, 1):
            x = w * 0.5 + side * w * (0.06 + 0.4 * t)
            y = h * (0.4 + 0.6 * t)
            d.line([(x, y), (x, y - h * 0.05 * (0.3 + t))], fill=(80, 80, 80), width=SS * 2)
            d.polygon([(x, y - h * 0.05 * (0.3 + t)), (x + h * 0.03 * (0.3 + t), y - h * 0.04 * (0.3 + t)), (x, y - h * 0.03 * (0.3 + t))],
                      fill=(230, 50, 50) if k % 2 else (40, 90, 220))


def scene_paper(img, p, rnd, v):
    w, h = img.size
    gradient(img, (150, 110, 80), (110, 75, 50))
    d = ImageDraw.Draw(img)
    # A sheet on a desk, a map drawn on it, paper castles, folded boats.
    d.polygon([(w * 0.08, h * 0.12), (w * 0.92, h * 0.08), (w * 0.95, h * 0.92), (w * 0.05, h * 0.95)], fill=(245, 238, 222))
    for _ in range(6):
        cx, cy = w * (0.15 + 0.7 * rnd.random()), h * (0.2 + 0.6 * rnd.random())
        pts = [(cx + math.cos(a) * w * 0.08 * (0.6 + 0.5 * rnd.random()), cy + math.sin(a) * h * 0.1 * (0.6 + 0.5 * rnd.random()))
               for a in [i / 9 * math.tau for i in range(9)]]
        d.polygon(pts, outline=(120, 140, 170), fill=(225, 232, 240), width=SS * 2)
    for k in range(3):
        x, y, s = w * (0.25 + 0.25 * k), h * (0.55 + 0.1 * rnd.random()), h * 0.08
        c = (p.main, p.second, rgb(p.a + 0.5, 0.6, 0.8))[k]
        d.rectangle([x - s, y - s, x + s, y + s * 0.5], fill=c)
        for t in range(3):
            d.rectangle([x - s + t * s * 0.8, y - s * 1.3, x - s + t * s * 0.8 + s * 0.4, y - s], fill=c)
        d.polygon([(x + s * 0.8, y - s), (x + s * 0.8, y - s * 2.2), (x + s * 1.4, y - s * 1.9)], fill=(250, 250, 250))
    for _ in range(4):
        x, y = w * (0.15 + 0.7 * rnd.random()), h * (0.75 + 0.1 * rnd.random())
        d.polygon([(x - h * 0.05, y), (x + h * 0.05, y), (x, y - h * 0.06)], fill=(250, 250, 250), outline=(180, 170, 160))
    d.line([(w * 0.1, h * 0.3), (w * 0.9, h * 0.7)], fill=(60, 60, 120), width=SS * 2)


def scene_tactics(img, p, rnd, v):
    w, h = img.size
    gradient(img, rgb(p.a, 0.6, 0.18), rgb(p.b, 0.5, 0.35))
    d = ImageDraw.Draw(img)
    stars(d, w, h, rnd, 60, top=0.35)
    # An isometric board of tiles, flowers blooming on a few.
    n, tw, th = 8, w * 0.06, h * 0.06
    ox, oy = w * 0.5, h * 0.25
    for i in range(n):
        for j in range(n):
            x, y = ox + (i - j) * tw, oy + (i + j) * th
            c = rgb(0.32 if (i + j) % 2 else 0.30, 0.45, 0.45 + 0.1 * rnd.random())
            d.polygon([(x, y), (x + tw, y + th), (x, y + 2 * th), (x - tw, y + th)], fill=c, outline=rgb(0.3, 0.4, 0.25))
            if rnd.random() < 0.15:
                for a in range(5):
                    ang = a / 5 * math.tau
                    disc(d, x + math.cos(ang) * th * 0.35, y + th + math.sin(ang) * th * 0.2, th * 0.2, p.second)
                disc(d, x, y + th, th * 0.15, (255, 230, 120))
    for k in range(3):
        i, j = rnd.randrange(n), rnd.randrange(n)
        x, y = ox + (i - j) * tw, oy + (i + j) * th
        c = p.main if k < 2 else (40, 30, 50)
        d.polygon([(x - tw * 0.3, y + th), (x + tw * 0.3, y + th), (x + tw * 0.2, y - th * 0.8), (x - tw * 0.2, y - th * 0.8)], fill=c)
        disc(d, x, y - th * 1.1, tw * 0.22, (240, 220, 200))


def scene_ink(img, p, rnd, v):
    w, h = img.size
    gradient(img, (230, 232, 240), (180, 185, 200))
    d = ImageDraw.Draw(img)
    # A town square under splats of the two teams' ink.
    for k in range(5):
        bx = w * (0.05 + 0.2 * k)
        d.rectangle([bx, h * 0.15, bx + w * 0.15, h * 0.62], fill=(150, 155, 170))
        for r in range(4):
            for c in range(2):
                d.rectangle([bx + w * (0.02 + 0.065 * c), h * (0.2 + 0.1 * r), bx + w * (0.06 + 0.065 * c), h * (0.26 + 0.1 * r)], fill=(90, 100, 130))
    d.rectangle([0, h * 0.62, w, h], fill=(120, 125, 140))
    for team, c in ((0, p.main), (1, p.second)):
        for _ in range(9):
            x, y, r = rnd.random() * w, h * (0.3 + 0.7 * rnd.random()), h * (0.04 + 0.08 * rnd.random())
            disc(d, x, y, r, c)
            for _ in range(7):
                a = rnd.random() * math.tau
                disc(d, x + math.cos(a) * r * 1.3, y + math.sin(a) * r * 1.3, r * 0.25, c)
    for k in range(2):
        x, y, c = w * (0.3 + 0.4 * k), h * 0.78, (p.main, p.second)[k]
        d.ellipse([x - h * 0.05, y - h * 0.12, x + h * 0.05, y], fill=c)
        disc(d, x, y - h * 0.14, h * 0.045, c)
        d.rectangle([x - h * 0.035, y - h * 0.16, x + h * 0.035, y - h * 0.13], fill=(30, 30, 40))
        xa, xb = sorted((x + h * 0.04 * (1 - 2 * k), x + h * 0.11 * (1 - 2 * k)))
        d.rectangle([xa, y - h * 0.08, xb, y - h * 0.06], fill=(60, 60, 70))


def scene_arena(img, p, rnd, v):
    w, h = img.size
    gradient(img, p.dark, rgb(p.b, 0.8, 0.6))
    d = ImageDraw.Draw(img)
    cx, cy = w / 2, h * 0.45
    for i in range(36):
        a0 = i / 36 * math.tau
        d.polygon([(cx, cy), (cx + math.cos(a0) * w, cy + math.sin(a0) * w), (cx + math.cos(a0 + 0.06) * w, cy + math.sin(a0 + 0.06) * w)],
                  fill=mix(p.dark, (255, 255, 255), 0.15))
    # A floating stage, two fighters, a spark between them.
    d.polygon([(w * 0.15, h * 0.72), (w * 0.85, h * 0.72), (w * 0.72, h * 0.86), (w * 0.28, h * 0.86)], fill=(70, 70, 90))
    d.rectangle([w * 0.15, h * 0.7, w * 0.85, h * 0.73], fill=(200, 200, 220))
    for k, side in enumerate((-1, 1)):
        x, y, c = cx + side * w * 0.14, h * 0.7, (p.main, p.second)[k]
        d.polygon([(x - h * 0.04, y), (x + h * 0.04, y), (x + h * 0.03, y - h * 0.16), (x - h * 0.03, y - h * 0.16)], fill=c)
        disc(d, x, y - h * 0.2, h * 0.04, mix(c, (255, 255, 255), 0.4))
        d.line([(x, y - h * 0.12), (x - side * h * 0.1, y - h * 0.18)], fill=c, width=SS * 10)
    for i in range(10):
        a = i / 10 * math.tau
        d.line([(cx, h * 0.52), (cx + math.cos(a) * h * 0.08, h * 0.52 + math.sin(a) * h * 0.08)], fill=(255, 240, 150), width=SS * 4)
    img2 = glow(img, cx, h * 0.52, h * 0.06, (255, 240, 180), 0.8)
    return img2


def scene_space(img, p, rnd, v):
    w, h = img.size
    gradient(img, (8, 10, 35), rgb(p.b, 0.6, 0.35))
    d = ImageDraw.Draw(img)
    stars(d, w, h, rnd, 220)
    # Tiny planets, each its own colour, rings on one, a hopping hero.
    for k in range(5):
        x, y, r = w * (0.1 + 0.2 * k), h * (0.3 + 0.45 * rnd.random()), h * (0.06 + 0.1 * rnd.random())
        c = rgb(p.a + k * 0.2, 0.6, 0.9)
        disc(d, x, y, r, c)
        d.chord([x - r, y - r, x + r, y + r], 200, 340, fill=mix(c, (255, 255, 255), 0.35))
        if k == 2:
            d.ellipse([x - r * 1.8, y - r * 0.35, x + r * 1.8, y + r * 0.35], outline=(240, 220, 160), width=SS * 4)
    hx, hy = w * (0.45 + 0.05 * v), h * 0.2
    d.ellipse([hx - h * 0.025, hy - h * 0.03, hx + h * 0.025, hy + h * 0.04], fill=(230, 60, 60))
    disc(d, hx, hy - h * 0.045, h * 0.025, (255, 220, 190))
    d.chord([hx - h * 0.03, hy - h * 0.085, hx + h * 0.03, hy - h * 0.025], 180, 360, fill=(230, 60, 60))
    for k in range(5):
        a = k / 5 * math.tau - math.pi / 2
        disc(d, w * 0.85 + math.cos(a) * h * 0.04, h * 0.15 + math.sin(a) * h * 0.04, h * 0.02, (255, 225, 90))


def scene_summit(img, p, rnd, v):
    w, h = img.size
    gradient(img, rgb(p.a, 0.6, 0.35), rgb(p.b, 0.35, 0.95))
    d = ImageDraw.Draw(img)
    stars(d, w, h, rnd, 50, top=0.3)
    # A tall mountain of ledges, snow on top, a climber.
    d.polygon([(w * 0.2, h), (w * 0.55, h * 0.08), (w * 0.9, h)], fill=rgb(p.a, 0.4, 0.3))
    d.polygon([(w * 0.47, h * 0.22), (w * 0.55, h * 0.08), (w * 0.63, h * 0.22), (w * 0.58, h * 0.19), (w * 0.53, h * 0.24)], fill=(245, 248, 255))
    for k in range(9):
        y = h * (0.3 + 0.075 * k)
        x = w * (0.35 + 0.35 * rnd.random())
        d.rectangle([x, y, x + w * 0.08, y + h * 0.02], fill=rgb(p.b, 0.3, 0.75))
    for _ in range(40):
        x, y = rnd.random() * w, rnd.random() * h
        d.line([(x, y), (x + SS * 10, y + SS * 4)], fill=(255, 255, 255), width=SS)
    cx, cy = w * 0.6, h * (0.55 - 0.05 * v)
    d.rectangle([cx - h * 0.012, cy - h * 0.05, cx + h * 0.012, cy], fill=(200, 50, 70))
    disc(d, cx, cy - h * 0.065, h * 0.016, (255, 220, 200))
    d.polygon([(cx - h * 0.016, cy - h * 0.075), (cx - h * 0.06, cy - h * 0.06), (cx - h * 0.016, cy - h * 0.06)], fill=(230, 80, 120))


def scene_garden(img, p, rnd, v):
    w, h = img.size
    gradient(img, (15, 18, 50), rgb(p.b, 0.5, 0.45), 0, int(h * 0.7))
    d = ImageDraw.Draw(img)
    stars(d, w, h, rnd, 160, top=0.65)
    disc(d, w * 0.2, h * 0.2, h * 0.08, (250, 245, 220))
    disc(d, w * 0.2 + h * 0.03, h * 0.2 - h * 0.02, h * 0.07, (15, 18, 50))
    d.rectangle([0, h * 0.7, w, h], fill=(20, 45, 40))
    # Moonflowers on stems, fireflies.
    for _ in range(26):
        x, y = rnd.random() * w, h * (0.68 + 0.3 * rnd.random())
        s = h * (0.025 + 0.025 * rnd.random())
        d.line([(x, y), (x, y + s * 2.5)], fill=(60, 120, 80), width=SS * 2)
        c = rgb(p.a + rnd.random() * 0.2, 0.35, 1.0)
        for a in range(6):
            ang = a / 6 * math.tau
            disc(d, x + math.cos(ang) * s * 0.6, y + math.sin(ang) * s * 0.6, s * 0.45, c)
        disc(d, x, y, s * 0.35, (255, 240, 170))
    for _ in range(25):
        img = glow(img, rnd.random() * w, h * (0.35 + 0.5 * rnd.random()), SS * 8, (240, 255, 150), 0.5)
    return img


def scene_canyon(img, p, rnd, v):
    w, h = img.size
    gradient(img, rgb(0.06, 0.5, 1.0), rgb(0.1, 0.25, 1.0), 0, int(h * 0.55))
    d = ImageDraw.Draw(img)
    for i in range(4):
        x0 = w * (0.05 + 0.25 * i + 0.05 * rnd.random())
        top = h * (0.25 + 0.15 * rnd.random())
        d.polygon([(x0, h * 0.62), (x0 + w * 0.02, top), (x0 + w * 0.15, top), (x0 + w * 0.18, h * 0.62)], fill=rgb(0.04, 0.55, 0.6 + 0.1 * i % 2))
        for k in range(3):
            y = top + (h * 0.62 - top) * (k + 1) / 4
            d.line([(x0 + w * 0.01, y), (x0 + w * 0.17, y)], fill=rgb(0.04, 0.5, 0.5), width=SS * 3)
    d.rectangle([0, h * 0.6, w, h], fill=rgb(0.07, 0.45, 0.85))
    pts = [(w * (0.5 + 0.25 * math.sin(t * 2.5 + v)), h * (0.6 + 0.4 * t)) for t in [i / 24 for i in range(25)]]
    for i in range(len(pts) - 1):
        d.line([pts[i], pts[i + 1]], fill=rgb(0.06, 0.35, 0.55), width=int(SS * (6 + 50 * i / 24)))
    # A rally car and its dust.
    cx, cy = pts[14]
    for k in range(10):
        disc(d, cx - h * (0.06 + 0.02 * k), cy + h * 0.01 * (k % 3), h * (0.02 + 0.006 * k), (235, 205, 165))
    d.rounded_rectangle([cx - h * 0.07, cy - h * 0.04, cx + h * 0.07, cy + h * 0.02], radius=SS * 6, fill=p.main)
    d.polygon([(cx - h * 0.03, cy - h * 0.04), (cx - h * 0.015, cy - h * 0.07), (cx + h * 0.035, cy - h * 0.07), (cx + h * 0.05, cy - h * 0.04)], fill=(60, 80, 110))
    for side in (-1, 1):
        disc(d, cx + side * h * 0.045, cy + h * 0.022, h * 0.018, (25, 25, 30))


SCENES = {
    "sky": scene_sky, "kart": scene_kart, "critters": scene_critters, "village": scene_village,
    "farm": scene_farm, "dungeon": scene_dungeon, "caves": scene_caves, "party": scene_party,
    "kitchen": scene_kitchen, "lighthouse": scene_lighthouse, "desert": scene_desert, "snow": scene_snow,
    "paper": scene_paper, "tactics": scene_tactics, "ink": scene_ink, "arena": scene_arena,
    "space": scene_space, "summit": scene_summit, "garden": scene_garden, "canyon": scene_canyon,
}


def scene(game, index, w, h, v):
    """The game's scene `v`, w × h."""
    img = Image.new("RGB", (w * SS, h * SS))
    rnd = random.Random(f"{game['name']}/{v}")
    out = SCENES[game["art"]](img, Palette(game["hues"]), rnd, v)
    return out if out is not None else img


# ------------------------------------------------------------- the title

def title_on(img, name, size, box, align="left", shadow=True):
    """`name` in Sora, white with a soft shadow, fitted in `box` (x, y, w, h), its lines from the bottom."""
    d = ImageDraw.Draw(img)
    x, y, bw, bh = box
    words = name.replace(": ", ":\n").split("\n")
    lines = []
    for part in words:
        line = ""
        for word in part.split(" "):
            test = (line + " " + word).strip()
            if line and ImageFont.truetype(FONT, size).getlength(test) > bw:
                lines.append(line)
                line = word
            else:
                line = test
        lines.append(line)
    while size > 10:
        font = ImageFont.truetype(FONT, size)
        if max(font.getlength(l) for l in lines) <= bw and len(lines) * size * 1.15 <= bh:
            break
        size -= 2
    font = ImageFont.truetype(FONT, size)
    total = len(lines) * size * 1.15
    ty = y + bh - total
    layer = Image.new("L", img.size, 0)
    ld = ImageDraw.Draw(layer)
    for i, l in enumerate(lines):
        lx = x if align == "left" else x + (bw - font.getlength(l)) / 2
        ld.text((lx, ty + i * size * 1.15), l, font=font, fill=255)
    if shadow:
        blur = layer.filter(ImageFilter.GaussianBlur(size * 0.18))
        img.paste((0, 0, 0), (int(size * 0.06), int(size * 0.08)), Image.eval(blur, lambda a: int(a * 0.75)))
    img.paste((255, 255, 255), (0, 0), layer)


def shade(img, top=0.0, bottom=0.55):
    """Darker towards the bottom, for the title to read."""
    w, h = img.size
    mask = Image.new("L", (1, h))
    for y in range(h):
        t = y / (h - 1)
        mask.putpixel((0, y), int(255 * (top + (bottom - top) * t * t)))
    img.paste((0, 0, 0), (0, 0), mask.resize((w, h)))


def hud(img, game, v):
    """A screenshot's head-up display: hearts, a score, a little map."""
    w, h = img.size
    d = ImageDraw.Draw(img)
    for k in range(3 + v):
        x, y, r = w * 0.03 + k * h * 0.045, h * 0.05, h * 0.016
        c = (235, 60, 80) if k < 2 + v else (90, 90, 100)
        disc(d, x - r * 0.5, y, r * 0.6, c)
        disc(d, x + r * 0.5, y, r * 0.6, c)
        d.polygon([(x - r * 1.05, y + r * 0.15), (x + r * 1.05, y + r * 0.15), (x, y + r * 1.2)], fill=c)
    font = ImageFont.truetype(FONT, int(h * 0.04))
    score = f"{(sum(map(ord, game['name'])) * 37 % 9000 + 1000) * (v + 1):07d}"
    d.text((w * 0.97 - font.getlength(score), h * 0.03), score, font=font, fill=(255, 255, 255), stroke_width=SS * 2, stroke_fill=(20, 20, 30))
    m = h * 0.16
    d.rounded_rectangle([w * 0.97 - m, h * 0.95 - m, w * 0.97, h * 0.95], radius=m * 0.15, fill=(20, 24, 40), outline=(240, 240, 250), width=SS * 2)
    disc(d, w * 0.97 - m / 2, h * 0.95 - m / 2, m * 0.06, (255, 220, 80))


# ------------------------------------------------------------- the files

def save(img, path, size):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    img.resize(size, Image.LANCZOS).save(path, "JPEG", quality=QUALITY, optimize=True, progressive=True)


def release_of(game, today):
    if "days" in game:
        return (today + datetime.timedelta(days=game["days"])).isoformat()
    return game["date"]


def main():
    table, out = sys.argv[1], sys.argv[2]
    games = json.load(open(table, encoding="utf-8"))["games"]
    # The day of the build, as web_demo.cpp counts the table's `days` from it.
    today = datetime.datetime.now(datetime.timezone.utc).date()
    titledb = {}
    for i, g in enumerate(games):
        gid = f"{tid(i):016X}"
        release = release_of(g, today)
        # The pictures: the banner the scene, the icon a crop of it with the
        # name, the screenshots its other views.
        art = scene(g, i, 1280, 720, 0)
        # The banner bare: PlayShelf writes the name over it.
        save(art, f"{out}/nlib/{gid}/banner.jpg", (854, 480))
        side = 720 * SS
        icon = art.crop(((1280 * SS - side) // 2, 0, (1280 * SS + side) // 2, side))
        shade(icon, 0.0, 0.6)
        title_on(icon, g["name"], 104 * SS, (40 * SS, 250 * SS, 640 * SS, 430 * SS), align="center")
        save(icon, f"{out}/icons/{gid}.jpg", (256, 256))
        save(icon, f"{out}/nlib/{gid}/icon.jpg", (256, 256))
        if g.get("installed"):
            save(icon, f"{out}/console/control/{gid.lower()}.jpg", (256, 256))
        screens = SCREENS if g.get("on_server", True) else 2
        for v in range(1, screens + 1):
            shot = scene(g, i, 1280, 720, v)
            hud(shot, g, v)
            save(shot, f"{out}/nlib/{gid}/screen{v}.jpg", (1280, 720))
        # Nlib API's details, in English and French.
        for lang in ("en", "fr"):
            details = {
                "id": gid, "name": g["name"], "publisher": g["studio"], "developer": g["studio"],
                "releaseDate": release, "category": g["categories"],
                "intro": g["intro"][lang], "description": g["description"][lang],
                "screens": {"count": screens}, "numberOfPlayers": g["players"],
                "languages": ["en", "fr", "de", "es", "it", "ja"], "ratingContent": [], "isDemo": False,
            }
            with open(f"{out}/nlib/{gid}/details.{lang}.json", "w", encoding="utf-8") as f:
                json.dump(details, f, ensure_ascii=False)
        titledb[gid] = {"name": g["name"], "publisher": g["studio"], "releaseDate": release.replace("-", ""),
                        "version": 0, "size": (1 + i % 12) * 1_000_000_000}
    os.makedirs(f"{out}/titledb", exist_ok=True)
    with open(f"{out}/titledb/index.json", "w", encoding="utf-8") as f:
        json.dump({"titledb": titledb}, f, ensure_ascii=False)
    size = sum(os.path.getsize(os.path.join(r, n)) for r, _, ns in os.walk(out) for n in ns
               if n.endswith((".jpg", ".json")) and "/files" not in r)
    print(f"web_demo_art: {len(games)} games, {size // 1024} KB of pictures and answers, in {out}")


if __name__ == "__main__":
    main()
