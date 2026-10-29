"""The web simulator demo's pictures, as SVG (tools/web_demo_art.py renders
them in Chromium): each made-up game's scene, drawn in layers the way key
art is (a sky, far shapes lost in the haze, nearer ones darker and sharper,
a lit subject, light and grain over it all), and a screenshot's head-up
display for its kind of game.

Every scene is a function of a palette, random numbers seeded with the
game's name (the same picture at every build) and `v`, which picture of the
game: 0 its key art (the banner, and the icon's middle), 1… its
screenshots, other moments of it. Everything is drawn in a 1280 × 720 box.
Nothing of a real game: no name, character, logo or picture.
"""
import colorsys
import math

W, H = 1280, 720
FOCUS = [W / 2, H / 2, H]   # the scene's subject, set by each scene: x, y and the square around it


# ------------------------------------------------------------- colours

def hsv(h, s, v):
    r, g, b = colorsys.hsv_to_rgb(h % 1.0, max(0.0, min(1.0, s)), max(0.0, min(1.0, v)))
    return "#%02x%02x%02x" % (int(r * 255), int(g * 255), int(b * 255))


def rgb_of(c):
    if len(c) == 4:   # #abc
        c = "#" + "".join(ch * 2 for ch in c[1:])
    return tuple(int(c[i:i + 2], 16) for i in (1, 3, 5))


def mix(a, b, t):
    a, b = rgb_of(a), rgb_of(b)
    return "#%02x%02x%02x" % tuple(int(a[i] + (b[i] - a[i]) * t) for i in range(3))


class Palette:
    """A game's colours from its two hues: `a` its main one, `b` its accent."""

    def __init__(self, hues):
        self.a, self.b = hues
        self.main = hsv(self.a, 0.72, 0.92)
        self.accent = hsv(self.b, 0.78, 0.95)
        self.deep = hsv(self.a, 0.65, 0.20)
        self.light = hsv(self.b, 0.12, 1.0)
        # Which of a scene's moments a game's key art is: games of one kind differ.
        self.shift = int(self.a * 97 + self.b * 31) % 4


# ------------------------------------------------------------- shapes

def path_through(points, closed=False):
    """A smooth path through `points` (Catmull-Rom as cubic Béziers)."""
    if len(points) < 2:
        return ""
    pts = list(points)
    d = f"M{pts[0][0]:.1f},{pts[0][1]:.1f}"
    for i in range(len(pts) - 1):
        p0 = pts[i - 1] if i > 0 else pts[i]
        p1, p2 = pts[i], pts[i + 1]
        p3 = pts[i + 2] if i + 2 < len(pts) else p2
        c1 = (p1[0] + (p2[0] - p0[0]) / 6, p1[1] + (p2[1] - p0[1]) / 6)
        c2 = (p2[0] - (p3[0] - p1[0]) / 6, p2[1] - (p3[1] - p1[1]) / 6)
        d += f" C{c1[0]:.1f},{c1[1]:.1f} {c2[0]:.1f},{c2[1]:.1f} {p2[0]:.1f},{p2[1]:.1f}"
    return d + (" Z" if closed else "")


def ridge(rnd, base, amp, fill, peaks=7, sharp=0.0, bottom=H, extra=""):
    """A row of hills (sharp 0) or mountains (sharp 1) across the picture."""
    pts = []
    n = peaks * 2 + 2
    for i in range(n + 1):
        x = -60 + (W + 120) * i / n
        hi = (i % 2 == 0)
        y = base - amp * (0.35 + 0.65 * rnd.random()) * (1.0 if hi else 0.35 + 0.3 * rnd.random())
        pts.append((x, y))
    if sharp > 0.5:
        d = "M" + " L".join(f"{x:.1f},{y:.1f}" for x, y in pts)
    else:
        d = path_through(pts)
    return f'<path d="{d} L{W + 60},{bottom} L-60,{bottom} Z" fill="{fill}" {extra}/>'


def disc(x, y, r, fill, extra=""):
    return f'<circle cx="{x:.1f}" cy="{y:.1f}" r="{r:.1f}" fill="{fill}" {extra}/>'


def ell(x, y, rx, ry, fill, extra=""):
    return f'<ellipse cx="{x:.1f}" cy="{y:.1f}" rx="{rx:.1f}" ry="{ry:.1f}" fill="{fill}" {extra}/>'


def poly(pts, fill, extra=""):
    return f'<polygon points="{" ".join(f"{x:.1f},{y:.1f}" for x, y in pts)}" fill="{fill}" {extra}/>'


def rect(x, y, w, h, fill, rx=0, extra=""):
    return f'<rect x="{x:.1f}" y="{y:.1f}" width="{w:.1f}" height="{h:.1f}" rx="{rx:.1f}" fill="{fill}" {extra}/>'


def cloud(x, y, s, fill, opacity=1.0, blur=""):
    puffs = ((0, 0, 1.0), (0.95, 0.18, 0.78), (-0.95, 0.22, 0.72), (0.42, -0.42, 0.72), (-0.4, -0.3, 0.6), (1.7, 0.35, 0.5),
             (-1.65, 0.38, 0.45))
    body = "".join(disc(x + dx * s, y + dy * s, r * s, fill) for dx, dy, r in puffs)
    body += rect(x - 2.0 * s, y + 0.1 * s, 4.0 * s, 0.75 * s, fill, rx=0.35 * s)
    shade = ell(x, y + 0.62 * s, 2.0 * s, 0.25 * s, "#000", 'opacity="0.06"')
    return f'<g opacity="{opacity}" {blur}>{body}{shade}</g>'


def stars(rnd, n, top=0.6, colour="#ffffff"):
    out = []
    for _ in range(n):
        x, y = rnd.random() * W, rnd.random() * H * top
        r = rnd.choice((0.7, 0.9, 1.1, 1.4, 1.8))
        out.append(disc(x, y, r, colour, f'opacity="{0.45 + 0.55 * rnd.random():.2f}"'))
        if r > 1.5:
            out.append(disc(x, y, r * 4, colour, 'opacity="0.18" filter="url(#b2)"'))
    return "".join(out)


def bokeh(rnd, n, colour, size=(6, 26), area=(0, 0, W, H), opacity=(0.15, 0.5)):
    x0, y0, x1, y1 = area
    return "".join(
        disc(x0 + rnd.random() * (x1 - x0), y0 + rnd.random() * (y1 - y0), size[0] + rnd.random() * (size[1] - size[0]), colour,
             f'opacity="{opacity[0] + rnd.random() * (opacity[1] - opacity[0]):.2f}" filter="url(#b1)"')
        for _ in range(n))


def sparks(rnd, n, colour, area=(0, 0, W, H), size=(2, 5)):
    """Little glowing motes (fireflies, embers, magic)."""
    x0, y0, x1, y1 = area
    out = []
    for _ in range(n):
        x, y, r = x0 + rnd.random() * (x1 - x0), y0 + rnd.random() * (y1 - y0), size[0] + rnd.random() * (size[1] - size[0])
        out.append(disc(x, y, r * 3.5, colour, 'opacity="0.35" filter="url(#b2)"') + disc(x, y, r, "#fffbe8"))
    return "".join(out)


def sun(x, y, r, core="#fff8e1", halo="#ffd27a", strength=0.9):
    return (disc(x, y, r * 5, halo, f'opacity="{0.35 * strength:.2f}" filter="url(#b3)"')
            + disc(x, y, r * 2.2, halo, f'opacity="{0.55 * strength:.2f}" filter="url(#b2)"')
            + disc(x, y, r, core))


def rays(x, y, colour, n=7, spread=1.4, length=1300, angle=1.9, opacity=0.16):
    """Light shafts falling from (x, y)."""
    out = []
    for i in range(n):
        a = angle + spread * (i / max(1, n - 1) - 0.5)
        w = 0.035 + 0.03 * (i % 3)
        p1 = (x + math.cos(a - w) * length, y + math.sin(a - w) * length)
        p2 = (x + math.cos(a + w) * length, y + math.sin(a + w) * length)
        out.append(poly([(x, y), p1, p2], "url(#ray)", f'opacity="{opacity * (0.6 + 0.4 * (i % 2)):.2f}"'))
    return f'<g style="mix-blend-mode:screen" fill="{colour}">{"".join(out)}</g>'


def defs(p, sky_top, sky_low, haze, ray="#fff6d8"):
    """What every scene shares: blurs, grain, the sky and haze gradients, a vignette."""
    return f"""
<defs>
  <linearGradient id="sky" x1="0" y1="0" x2="0" y2="1">
    <stop offset="0" stop-color="{sky_top}"/><stop offset="1" stop-color="{sky_low}"/>
  </linearGradient>
  <linearGradient id="haze" x1="0" y1="0" x2="0" y2="1">
    <stop offset="0" stop-color="{haze}" stop-opacity="0"/><stop offset="1" stop-color="{haze}" stop-opacity="0.85"/>
  </linearGradient>
  <linearGradient id="ray" x1="0" y1="0" x2="1" y2="1">
    <stop offset="0" stop-color="{ray}" stop-opacity="1"/><stop offset="1" stop-color="{ray}" stop-opacity="0"/>
  </linearGradient>
  <linearGradient id="shine" x1="0" y1="0" x2="0" y2="1">
    <stop offset="0" stop-color="#ffffff" stop-opacity="0.55"/><stop offset="0.5" stop-color="#ffffff" stop-opacity="0"/>
  </linearGradient>
  <radialGradient id="vignette" cx="0.5" cy="0.48" r="0.75">
    <stop offset="0.55" stop-color="#000" stop-opacity="0"/><stop offset="1" stop-color="#000" stop-opacity="0.55"/>
  </radialGradient>
  <filter id="b1" x="-50%" y="-50%" width="200%" height="200%"><feGaussianBlur stdDeviation="3"/></filter>
  <filter id="b2" x="-50%" y="-50%" width="200%" height="200%"><feGaussianBlur stdDeviation="9"/></filter>
  <filter id="b3" x="-50%" y="-50%" width="200%" height="200%"><feGaussianBlur stdDeviation="28"/></filter>
  <filter id="far" x="-5%" y="-5%" width="110%" height="110%"><feGaussianBlur stdDeviation="1.6"/></filter>
  <filter id="shadow" x="-30%" y="-30%" width="160%" height="160%">
    <feDropShadow dx="0" dy="8" stdDeviation="9" flood-color="#000" flood-opacity="0.35"/>
  </filter>
  <filter id="grain" x="0" y="0" width="100%" height="100%">
    <feTurbulence type="fractalNoise" baseFrequency="0.85" numOctaves="2" seed="7" result="n"/>
    <feColorMatrix type="saturate" values="0" in="n" result="g"/>
    <feComponentTransfer in="g"><feFuncA type="table" tableValues="0 0.06"/></feComponentTransfer>
  </filter>
</defs>"""


def finish():
    """Over every scene: a vignette and a film's grain."""
    return (f'<rect width="{W}" height="{H}" fill="url(#vignette)"/>'
            f'<rect width="{W}" height="{H}" filter="url(#grain)" style="mix-blend-mode:overlay"/>')


# ------------------------------------------------------------- characters

def hero(x, y, s, cloak, scarf, flip=False, pose=0):
    """A small adventurer: a hooded cloak blown by the wind, a long scarf, a glint of eyes."""
    f = -1 if flip else 1
    cape = path_through([(x - 0.15 * s * f, y - 1.55 * s), (x - 0.75 * s * f, y - 0.6 * s), (x - 1.05 * s * f, y + 0.05 * s),
                         (x - 0.2 * s * f, y - 0.05 * s)])
    sc = path_through([(x + 0.05 * s * f, y - 1.25 * s), (x - 0.6 * s * f, y - 1.35 * s - pose * 0.1 * s),
                       (x - 1.25 * s * f, y - 1.15 * s), (x - 1.7 * s * f, y - 1.3 * s)])
    legs = (rect(x - 0.22 * s, y - 0.4 * s, 0.16 * s, 0.42 * s, "#2b2238", rx=0.06 * s)
            + rect(x + 0.06 * s, y - 0.4 * s, 0.16 * s, 0.42 * s, "#2b2238", rx=0.06 * s))
    body = poly([(x - 0.42 * s, y - 0.3 * s), (x + 0.42 * s, y - 0.3 * s), (x + 0.28 * s, y - 1.25 * s), (x - 0.28 * s, y - 1.25 * s)], cloak)
    hood = disc(x, y - 1.5 * s, 0.38 * s, cloak) + poly([(x - 0.3 * s * f, y - 1.75 * s), (x - 0.62 * s * f, y - 1.95 * s), (x - 0.2 * s * f, y - 1.4 * s)], cloak)
    face = disc(x + 0.08 * s * f, y - 1.47 * s, 0.24 * s, "#1c1626")
    eyes = disc(x + 0.02 * s * f, y - 1.5 * s, 0.045 * s, "#fff6c8") + disc(x + 0.17 * s * f, y - 1.5 * s, 0.045 * s, "#fff6c8")
    blade = f'<line x1="{x + 0.35 * s * f:.1f}" y1="{y - 0.7 * s:.1f}" x2="{x + 0.95 * s * f:.1f}" y2="{y - 1.45 * s:.1f}" stroke="#e9f3ff" stroke-width="{0.07 * s:.1f}" stroke-linecap="round"/>'
    glint = disc(x + 0.95 * s * f, y - 1.45 * s, 0.12 * s, "#ffffff", 'opacity="0.7" filter="url(#b1)"')
    shadow = ell(x, y + 0.02 * s, 0.6 * s, 0.1 * s, "#000", 'opacity="0.25"')
    return (shadow + f'<path d="{cape} Z" fill="{mix(cloak, "#000000", 0.35)}"/>' + legs + body
            + f'<path d="{sc}" fill="none" stroke="{scarf}" stroke-width="{0.16 * s:.1f}" stroke-linecap="round"/>'
            + hood + face + eyes + blade + glint)


def critter(x, y, s, body, belly, kind=0, flip=False):
    """A round little creature with big glossy eyes; `kind` changes its ears and tail."""
    f = -1 if flip else 1
    out = [ell(x, y + 0.92 * s, 0.85 * s, 0.14 * s, "#000", 'opacity="0.22"')]
    if kind == 0:   # a flame tail
        out.append(f'<path d="{path_through([(x - 0.7 * s * f, y + 0.3 * s), (x - 1.25 * s * f, y - 0.1 * s), (x - 1.15 * s * f, y - 0.75 * s)])}" '
                   f'stroke="{body}" stroke-width="{0.22 * s:.1f}" fill="none" stroke-linecap="round"/>')
        out.append(disc(x - 1.15 * s * f, y - 0.85 * s, 0.26 * s, "#ffb23e", 'filter="url(#b1)"'))
        out.append(disc(x - 1.15 * s * f, y - 0.88 * s, 0.14 * s, "#fff1b8"))
    elif kind == 1:   # a frosty crest
        for k in range(3):
            out.append(poly([(x - 0.3 * s + k * 0.3 * s, y - 0.8 * s), (x - 0.15 * s + k * 0.3 * s, y - 1.35 * s + k % 2 * 0.15 * s),
                             (x + k * 0.3 * s, y - 0.8 * s)], "#d9f4ff"))
    else:   # long ears
        pass
    for side in (-1, 1):
        out.append(poly([(x + side * 0.35 * s, y - 0.65 * s), (x + side * 0.85 * s, y - 1.35 * s), (x + side * 0.78 * s, y - 0.3 * s)], body))
        out.append(poly([(x + side * 0.45 * s, y - 0.6 * s), (x + side * 0.78 * s, y - 1.12 * s), (x + side * 0.72 * s, y - 0.45 * s)], belly))
    out.append(ell(x, y, 0.95 * s, 0.88 * s, body))
    out.append(ell(x, y + 0.3 * s, 0.6 * s, 0.5 * s, belly))
    out.append(ell(x - 0.25 * s, y - 0.45 * s, 0.4 * s, 0.22 * s, "#fff", 'opacity="0.25"'))
    for side in (-1, 1):
        ex = x + side * 0.36 * s + 0.08 * s * f
        out.append(ell(ex, y - 0.12 * s, 0.2 * s, 0.25 * s, "#1d1630"))
        out.append(disc(ex + 0.06 * s, y - 0.2 * s, 0.08 * s, "#ffffff"))
        out.append(disc(ex - 0.06 * s, y - 0.02 * s, 0.035 * s, "#ffffff"))
        out.append(ell(x + side * 0.6 * s, y + 0.15 * s, 0.13 * s, 0.07 * s, "#ff7d9a", 'opacity="0.6"'))
    out.append(f'<path d="M{x - 0.1 * s:.1f},{y + 0.18 * s:.1f} Q{x:.1f},{y + 0.28 * s:.1f} {x + 0.1 * s:.1f},{y + 0.18 * s:.1f}" '
               f'stroke="#1d1630" stroke-width="{0.04 * s:.1f}" fill="none" stroke-linecap="round"/>')
    return "".join(out)


def kart(x, y, s, body, driver, boost=False, tilt=0):
    """A kart seen from behind and a little above, its driver's helmet, its exhaust."""
    out = [ell(x, y + 0.42 * s, 1.35 * s, 0.22 * s, "#000", 'opacity="0.35" filter="url(#b1)"')]
    if boost:
        for side in (-1, 1):
            out.append(f'<path d="M{x + side * 0.5 * s:.1f},{y + 0.15 * s:.1f} q{side * 0.1 * s:.1f},{0.9 * s:.1f} {side * 0.05 * s:.1f},{1.6 * s:.1f}" '
                       f'stroke="#7fd8ff" stroke-width="{0.32 * s:.1f}" stroke-linecap="round" opacity="0.8" filter="url(#b1)"/>')
            out.append(ell(x + side * 0.5 * s, y + 0.35 * s, 0.14 * s, 0.24 * s, "#e8fbff"))
    out.append(f'<g transform="rotate({tilt} {x:.1f} {y:.1f})">')
    for side in (-1, 1):
        out.append(rect(x + side * 1.0 * s - 0.28 * s, y - 0.15 * s, 0.56 * s, 0.6 * s, "#1b1b22", rx=0.12 * s))
        out.append(rect(x + side * 1.0 * s - 0.2 * s, y - 0.08 * s, 0.4 * s, 0.1 * s, "#3a3a46", rx=0.04 * s))
    out.append(f'<path d="M{x - 1.0 * s:.1f},{y + 0.2 * s:.1f} L{x - 0.8 * s:.1f},{y - 0.45 * s:.1f} L{x + 0.8 * s:.1f},{y - 0.45 * s:.1f} '
               f'L{x + 1.0 * s:.1f},{y + 0.2 * s:.1f} Q{x:.1f},{y + 0.42 * s:.1f} {x - 1.0 * s:.1f},{y + 0.2 * s:.1f} Z" fill="{body}"/>')
    out.append(rect(x - 0.8 * s, y - 0.47 * s, 1.6 * s, 0.14 * s, "url(#shine)", rx=0.05 * s))
    out.append(rect(x - 1.15 * s, y - 0.62 * s, 2.3 * s, 0.16 * s, mix(body, "#000000", 0.35), rx=0.06 * s))
    out.append(rect(x - 0.35 * s, y + 0.02 * s, 0.7 * s, 0.14 * s, "#f5f5f5", rx=0.04 * s))
    out.append(disc(x, y - 0.8 * s, 0.42 * s, driver))
    out.append(ell(x, y - 0.86 * s, 0.32 * s, 0.13 * s, "#1c2433"))
    out.append(ell(x - 0.12 * s, y - 0.98 * s, 0.14 * s, 0.06 * s, "#ffffff", 'opacity="0.6"'))
    out.append("</g>")
    return "".join(out)


# ------------------------------------------------------------- the scenes

def scene_sky(p, rnd, v):
    """Islands floating over a sea of clouds, a hero on a cliff, a glider in the light."""
    t = [0.0, 0.3, 0.75, 0.15][v % 4]   # the time of day: 0 morning, 0.75 dusk
    top = mix("#3f86d6", "#2b2a6b", t)
    low = mix("#ffe9c2", "#ff9b6a", t)
    out = [defs(p, top, low, mix(low, "#ffffff", 0.3)), f'<rect width="{W}" height="{H}" fill="url(#sky)"/>']
    sx, sy = W * (0.72 - 0.3 * (v % 2)), H * (0.24 + 0.2 * t)
    out.append(sun(sx, sy, 42, halo=mix("#ffd890", "#ff8e5e", t)))
    out.append(rays(sx, sy, "#fff4d6", n=8, angle=1.75 + (0.5 if sx < W / 2 else 0), opacity=0.13))
    for k in range(6):
        out.append(cloud(rnd.random() * W, H * (0.1 + 0.25 * rnd.random()), 30 + 30 * rnd.random(), "#ffffff", 0.55, 'filter="url(#b1)"'))
    # Far islands, pale and blurred by the distance.
    for depth in (0.75, 0.45):
        tone = mix(hsv(p.a, 0.35, 0.55), low, depth)
        for _ in range(3):
            cx, cy, s = rnd.random() * W, H * (0.3 + 0.25 * rnd.random()), 40 + 50 * rnd.random() * (1 - depth)
            out.append(f'<g filter="url(#far)" opacity="0.95">{island(cx, cy, s, tone, mix(tone, hsv(0.3, 0.4, 0.7), 0.4), rnd)}</g>')
    # A sea of clouds below.
    out.append(f'<rect y="{H * 0.62:.1f}" width="{W}" height="{H * 0.4:.1f}" fill="{mix(low, "#ffffff", 0.55)}" opacity="0.9"/>')
    for k in range(9):
        out.append(cloud(W * k / 8 + rnd.random() * 60, H * (0.64 + 0.05 * rnd.random()), 55 + 20 * rnd.random(), mix(low, "#ffffff", 0.7)))
    # The near island and its hero.
    nx = W * (0.3 if v % 2 == 0 else 0.62)
    out.append(island(nx, H * 0.7, 210, hsv(p.a, 0.5, 0.45), hsv(0.29 + 0.05 * (p.a - 0.3), 0.62, 0.62), rnd, trees=True))
    FOCUS[:] = [nx + 60, H * 0.6, 300]
    out.append(hero(nx + 70, H * 0.68, 46, hsv(p.b, 0.55, 0.55), "#ff5a4f", flip=v % 2 == 1))
    # A glider, and birds.
    gx, gy = W * (0.55 + 0.15 * (v % 3)), H * 0.3
    out.append(poly([(gx - 46, gy), (gx + 46, gy), (gx, gy - 14)], "#5b3b2e") + f'<line x1="{gx:.1f}" y1="{gy:.1f}" x2="{gx:.1f}" y2="{gy + 26:.1f}" stroke="#3b2a22" stroke-width="3"/>')
    for k in range(5):
        bx, by = gx - 160 + 50 * k + 20 * rnd.random(), gy - 60 + 25 * rnd.random()
        out.append(f'<path d="M{bx - 9:.1f},{by:.1f} q5,-6 9,0 q4,-6 9,0" stroke="#3c3550" stroke-width="2.4" fill="none"/>')
    out.append(bokeh(rnd, 14, "#fff8e0", area=(0, H * 0.5, W, H)))
    return "".join(out)


def island(cx, cy, s, rock, grass, rnd, trees=False):
    """A floating island: a grassy top, a rock beneath it hanging in points, maybe trees and a waterfall."""
    under = path_through([(cx - s, cy), (cx - 0.7 * s, cy + 0.45 * s), (cx - 0.25 * s, cy + 1.05 * s), (cx + 0.05 * s, cy + 1.35 * s),
                          (cx + 0.3 * s, cy + 0.85 * s), (cx + 0.75 * s, cy + 0.4 * s), (cx + s, cy)])
    out = [f'<path d="{under} Z" fill="{rock}"/>',
           f'<path d="{path_through([(cx - 0.6 * s, cy + 0.3 * s), (cx - 0.1 * s, cy + 0.9 * s), (cx + 0.05 * s, cy + 1.3 * s)])}" '
           f'stroke="{mix(rock, "#000000", 0.3)}" stroke-width="{max(2, s * 0.05):.1f}" fill="none" opacity="0.6"/>',
           ell(cx, cy, s * 1.02, s * 0.2, grass), ell(cx - 0.2 * s, cy - 0.06 * s, s * 0.7, s * 0.09, mix(grass, "#ffffff", 0.25))]
    if trees:
        for k in range(4):
            tx = cx - 0.75 * s + k * 0.42 * s + rnd.random() * 0.1 * s
            th = s * (0.35 + 0.2 * rnd.random())
            out.append(rect(tx - 0.03 * s, cy - th * 0.55, 0.06 * s, th * 0.6, "#5a3d2b"))
            out.append(disc(tx, cy - th * 0.75, th * 0.42, mix(grass, "#000000", 0.25)))
            out.append(disc(tx - th * 0.12, cy - th * 0.85, th * 0.25, mix(grass, "#ffffff", 0.15)))
        wx = cx + 0.55 * s
        out.append(rect(wx, cy, 0.08 * s, 1.1 * s, "#dff3ff", rx=0.04 * s, extra='opacity="0.8" filter="url(#b1)"'))
    return "".join(out)


def scene_kart(p, rnd, v):
    """A sunny circuit curving to the horizon, karts racing, confetti and a finish arch."""
    out = [defs(p, "#3d9cf0", "#cdeeff", "#e9f8ff")]
    out.append(f'<rect width="{W}" height="{H}" fill="url(#sky)"/>')
    out.append(sun(W * 0.82, H * 0.14, 34))
    for k in range(4):
        out.append(cloud(rnd.random() * W, H * (0.08 + 0.15 * rnd.random()), 28 + 22 * rnd.random(), "#ffffff", 0.9))
    out.append(ridge(rnd, H * 0.44, 70, "#8fc6d9", peaks=5, extra='filter="url(#far)"'))
    out.append(ridge(rnd, H * 0.5, 40, "#5fb36a", peaks=8))
    out.append(rect(0, H * 0.48, W, H * 0.52, "#4fa85a"))
    # The track: a sweeping curve to the horizon, kerbs on both sides.
    bend = [-0.25, 0.2, -0.1, 0.3][v % 4]
    centre = [(W * (0.5 + bend * (1 - t) ** 2 * 0.9 - bend * 0.15), H * 0.48 + H * 0.56 * t) for t in [i / 12 for i in range(13)]]
    left = [(x - (20 + 560 * ((y - H * 0.48) / (H * 0.56)) ** 1.3), y) for x, y in centre]
    right = [(x + (20 + 560 * ((y - H * 0.48) / (H * 0.56)) ** 1.3), y) for x, y in centre]
    road = "M" + " L".join(f"{x:.1f},{y:.1f}" for x, y in left) + " L" + " L".join(f"{x:.1f},{y:.1f}" for x, y in reversed(right)) + " Z"
    out.append(f'<path d="{road}" fill="#545a6b"/>')
    for side in (left, right):
        for i in range(len(side) - 1):
            colour = "#e63946" if i % 2 else "#f8f8f8"
            (x0, y0), (x1, y1) = side[i], side[i + 1]
            w = 4 + 26 * i / len(side)
            out.append(f'<line x1="{x0:.1f}" y1="{y0:.1f}" x2="{x1:.1f}" y2="{y1:.1f}" stroke="{colour}" stroke-width="{w:.1f}"/>')
    for i in range(1, len(centre) - 1, 2):
        (x0, y0), (x1, y1) = centre[i], centre[i + 1]
        out.append(f'<line x1="{x0:.1f}" y1="{y0:.1f}" x2="{x1:.1f}" y2="{y1:.1f}" stroke="#f4f1e0" stroke-width="{2 + 8 * i / 12:.1f}" opacity="0.8"/>')
    # Item boxes floating above the track.
    for k in range(3):
        bx, by = centre[4][0] - 90 + 90 * k, centre[4][1] - 40
        out.append(f'<g transform="rotate(15 {bx:.1f} {by:.1f})">' + rect(bx - 16, by - 16, 32, 32, "#ffd84d", rx=6, extra='opacity="0.92"')
                   + f'<text x="{bx:.1f}" y="{by + 9:.1f}" font-size="26" font-family="Sora" fill="#fff" text-anchor="middle">?</text></g>')
    # The finish arch far away.
    ax, ay = centre[1]
    out.append(rect(ax - 120, ay - 90, 14, 90, "#e5e5ea") + rect(ax + 106, ay - 90, 14, 90, "#e5e5ea"))
    for i in range(12):
        for j in range(2):
            out.append(rect(ax - 120 + i * 20, ay - 104 + j * 10, 20, 10, "#111" if (i + j) % 2 else "#fff"))
    # The karts, the nearest biggest.
    colours = [p.main, p.accent, hsv(p.a + 0.33, 0.8, 0.9), hsv(p.a + 0.55, 0.7, 0.95)]
    for k, t in enumerate([0.32, 0.5, 0.86]):
        i = int(t * 12)
        cx, cy = centre[i]
        cx += (rnd.random() - 0.5) * 300 * t
        out.append(kart(cx, cy - 10, 110 * t ** 1.15, colours[(k + v) % 4], colours[(k + 2 + v) % 4], boost=(k == 2), tilt=(rnd.random() - 0.5) * 8))
        if k == 1:
            FOCUS[:] = [cx, cy - 20, 320]
    out.append(sparks(rnd, 22, "#ffe36e", area=(0, H * 0.2, W, H * 0.6), size=(1.5, 3)))
    return "".join(out)


def scene_critters(p, rnd, v):
    """A meadow (a volcano's foot, or a frozen valley) and three creatures, the nearest close up."""
    cold = 0.4 < p.a < 0.7
    if cold:
        out = [defs(p, "#6aa8e8", "#eaf6ff", "#f2fbff")]
    else:
        out = [defs(p, "#ff9b5e", "#ffe2b5", "#fff0d8")]
    out.append(f'<rect width="{W}" height="{H}" fill="url(#sky)"/>')
    if cold:
        out.append(sun(W * 0.25, H * 0.2, 30, core="#ffffff", halo="#cfeaff"))
        out.append(ridge(rnd, H * 0.5, 210, "#b7d3ee", peaks=4, sharp=1, extra='filter="url(#far)"'))
        out.append(ridge(rnd, H * 0.58, 120, "#e8f3ff", peaks=6, sharp=1))
        ground = "#f4fbff"
    else:
        vx = W * (0.7 - 0.3 * (v % 2))
        out.append(poly([(vx - 330, H * 0.58), (vx - 60, H * 0.16), (vx + 60, H * 0.16), (vx + 330, H * 0.58)], "#6b3d3a", 'filter="url(#far)"'))
        out.append(ell(vx, H * 0.16, 64, 14, "#ffb347") + disc(vx, H * 0.12, 70, "#ff8a3d", 'opacity="0.55" filter="url(#b3)"'))
        out.append(f'<path d="M{vx + 10:.1f},{H * 0.16:.1f} q-30,80 -10,160" stroke="#ff9c45" stroke-width="10" fill="none" opacity="0.8" filter="url(#b1)"/>')
        for k in range(5):
            out.append(cloud(vx - 40 + 30 * k, H * 0.06 - 15 * k, 26 + 6 * k, "#6e5a5a", 0.35, 'filter="url(#b1)"'))
        out.append(ridge(rnd, H * 0.6, 70, "#8db863", peaks=6))
        ground = "#7cc057"
    out.append(rect(0, H * 0.62, W, H * 0.4, ground))
    for k in range(40):
        gx, gy = rnd.random() * W, H * (0.64 + 0.36 * rnd.random())
        tuft = mix(ground, "#000000", 0.18)
        out.append(f'<path d="M{gx:.1f},{gy:.1f} l-5,-14 M{gx:.1f},{gy:.1f} l1,-17 M{gx:.1f},{gy:.1f} l6,-13" stroke="{tuft}" stroke-width="2.4" stroke-linecap="round"/>')
    if not cold:
        for k in range(14):
            fx, fy = rnd.random() * W, H * (0.66 + 0.3 * rnd.random())
            out.append(disc(fx, fy, 5, rnd.choice(["#fff3a3", "#ffffff", "#ffb3c7"])))
    # The creatures: two behind, one near.
    shades = [(hsv(p.a + 0.02 * k, 0.65, 0.95), hsv(p.b, 0.25, 1.0)) for k in range(3)]
    kind = 1 if cold else 0
    out.append(critter(W * 0.22, H * 0.66, 34, *shades[1], kind=kind))
    out.append(critter(W * 0.8, H * 0.7, 40, hsv(p.a + 0.45, 0.55, 0.9), "#fff8e6", kind=2, flip=True))
    near_x = W * (0.5 + 0.12 * ((v % 3) - 1))
    FOCUS[:] = [near_x, H * 0.68, 430]
    out.append(critter(near_x, H * 0.76, 110, *shades[0], kind=kind))
    if cold:
        out.append(sparks(rnd, 40, "#ffffff", area=(0, 0, W, H), size=(1.5, 3.5)))
    else:
        out.append(sparks(rnd, 16, "#ffcf6b", area=(0, H * 0.3, W, H * 0.8)))
    return "".join(out)


def scene_village(p, rnd, v):
    """A harbour village at golden hour: houses up a hill, boats, the sea glittering."""
    out = [defs(p, "#6cb7e8", "#ffe1b8", "#fff2dc")]
    out.append(f'<rect width="{W}" height="{H}" fill="url(#sky)"/>')
    out.append(sun(W * 0.2, H * 0.32, 38))
    out.append(rays(W * 0.2, H * 0.32, "#fff1d0", n=6, angle=0.4, opacity=0.1))
    for k in range(4):
        out.append(cloud(rnd.random() * W, H * (0.08 + 0.2 * rnd.random()), 26 + 20 * rnd.random(), "#ffffff", 0.8, 'filter="url(#b1)"'))
    out.append(ridge(rnd, H * 0.5, 80, "#9ec4cf", peaks=4, extra='filter="url(#far)"'))
    # The hill and its houses.
    out.append(f'<path d="{path_through([(W * 0.35, H * 0.62), (W * 0.55, H * 0.4), (W * 0.75, H * 0.33), (W * 1.05, H * 0.36)])} L{W + 10},{H} L{W * 0.35:.1f},{H} Z" fill="#7aa86a"/>')
    roofs = ["#d9534f", "#f0ad4e", "#5bc0de", "#e7717d", "#8e7cc3", p.main, p.accent]
    for k in range(16):
        t = k / 15
        hx = W * (0.42 + 0.56 * t) + rnd.random() * 20
        hy = H * (0.6 - 0.25 * math.sin(t * 2.6)) + rnd.random() * 30
        s = 34 + rnd.random() * 18
        wall = rnd.choice(["#fff6e8", "#ffeacc", "#f3f0ff", "#e8fff4", "#fff0f0"])
        out.append(rect(hx - s / 2, hy - s * 0.8, s, s * 0.8, wall, extra='filter="url(#shadow)"'))
        out.append(poly([(hx - s * 0.62, hy - s * 0.78), (hx, hy - s * 1.35), (hx + s * 0.62, hy - s * 0.78)], rnd.choice(roofs)))
        out.append(rect(hx - s * 0.12, hy - s * 0.42, s * 0.24, s * 0.42, "#6b4a3a"))
        out.append(rect(hx + s * 0.18, hy - s * 0.66, s * 0.18, s * 0.16, "#ffd66b", extra='opacity="0.9"'))
    FOCUS[:] = [W * 0.68, H * 0.5, 560]
    # The quay and the sea.
    out.append(rect(0, H * 0.66, W * 0.6, H * 0.04, "#c2a27d"))
    sea_y = H * 0.68
    out.append(f'<rect y="{sea_y:.1f}" width="{W}" height="{H - sea_y:.1f}" fill="{mix("#2f86b8", "#163d63", 0.3)}"/>')
    for k in range(70):
        gx, gy = rnd.random() * W, sea_y + rnd.random() * (H - sea_y)
        out.append(rect(gx, gy, 8 + 30 * rnd.random(), 2.5, "#ffffff", extra=f'opacity="{0.25 + 0.5 * rnd.random():.2f}"'))
    out.append(ell(W * 0.2, sea_y + 40, 120, 30, "#ffe7b0", 'opacity="0.5" filter="url(#b2)"'))
    # Boats.
    for k in range(3):
        bx, by = W * (0.12 + 0.25 * k + 0.05 * v), sea_y + 50 + 40 * k
        s = 40 + 25 * k
        out.append(f'<path d="M{bx - s:.1f},{by:.1f} L{bx + s:.1f},{by:.1f} L{bx + s * 0.7:.1f},{by + s * 0.35:.1f} L{bx - s * 0.7:.1f},{by + s * 0.35:.1f} Z" fill="{roofs[(k + v) % 7]}"/>')
        out.append(f'<line x1="{bx:.1f}" y1="{by:.1f}" x2="{bx:.1f}" y2="{by - s * 1.3:.1f}" stroke="#5a4636" stroke-width="3"/>')
        out.append(poly([(bx + 3, by - s * 1.25), (bx + 3, by - s * 0.15), (bx + s * 0.8, by - s * 0.2)], "#fffaf0"))
    for k in range(4):
        gx, gy = W * (0.5 + 0.1 * k), H * (0.25 + 0.05 * k)
        out.append(f'<path d="M{gx - 9:.1f},{gy:.1f} q5,-6 9,0 q4,-6 9,0" stroke="#4b5266" stroke-width="2.4" fill="none"/>')
    return "".join(out)


def scene_farm(p, rnd, v):
    """Rolling fields in rows, a red barn, a windmill turning, sunflowers close up."""
    night = p.a > 0.6
    if night:
        out = [defs(p, "#1b2350", "#6b5aa8", "#8f84c9")]
    else:
        out = [defs(p, "#5fb4ef", "#fff1c4", "#fff6dc")]
    out.append(f'<rect width="{W}" height="{H}" fill="url(#sky)"/>')
    if night:
        out.append(stars(rnd, 160))
        out.append(disc(W * 0.78, H * 0.18, 46, "#fff7d6") + disc(W * 0.78, H * 0.18, 140, "#cdbfff", 'opacity="0.25" filter="url(#b3)"'))
    else:
        out.append(sun(W * 0.78, H * 0.18, 40))
        for k in range(4):
            out.append(cloud(rnd.random() * W, H * (0.1 + 0.15 * rnd.random()), 28 + 20 * rnd.random(), "#ffffff", 0.85))
    far = "#4e5d93" if night else "#9dc98a"
    mid = "#3c4c7d" if night else "#7cb95c"
    near = "#2c3a66" if night else "#5fa046"
    out.append(ridge(rnd, H * 0.5, 50, far, peaks=4, extra='filter="url(#far)"'))
    out.append(ridge(rnd, H * 0.58, 40, mid, peaks=5))
    # Field rows sweeping to the horizon.
    for k in range(14):
        y0 = H * 0.58 + k * k * 1.1 + k * 6
        colour = mix(near, "#d9c35a" if not night else "#6f5fa8", 0.25 if k % 2 else 0.0)
        out.append(f'<path d="M-20,{y0:.1f} Q{W / 2:.1f},{y0 - 30:.1f} {W + 20},{y0:.1f} L{W + 20},{H} L-20,{H} Z" fill="{colour}"/>')
    # The barn and the windmill.
    bx, by = W * (0.28 + 0.4 * (v % 2)), H * 0.56
    out.append(rect(bx - 60, by - 70, 120, 70, "#c0392b", extra='filter="url(#shadow)"'))
    out.append(poly([(bx - 70, by - 68), (bx, by - 120), (bx + 70, by - 68)], "#922b21"))
    out.append(rect(bx - 22, by - 44, 44, 44, "#f5e6d3") + f'<path d="M{bx - 22},{by - 44} L{bx + 22},{by} M{bx + 22},{by - 44} L{bx - 22},{by}" stroke="#c0392b" stroke-width="4"/>')
    if night:
        out.append(rect(bx + 30, by - 60, 16, 14, "#ffd66b") + disc(bx + 38, by - 53, 30, "#ffd66b", 'opacity="0.35" filter="url(#b2)"'))
    mx, my = bx + (200 if v % 2 == 0 else -220), by - 10
    out.append(poly([(mx - 24, my), (mx - 12, my - 130), (mx + 12, my - 130), (mx + 24, my)], "#f2efe6"))
    angle = 20 + 25 * v
    for k in range(4):
        a = math.radians(angle + 90 * k)
        ex, ey = mx + math.cos(a) * 90, my - 130 + math.sin(a) * 90
        out.append(f'<line x1="{mx:.1f}" y1="{my - 130:.1f}" x2="{ex:.1f}" y2="{ey:.1f}" stroke="#7a5b45" stroke-width="10" stroke-linecap="round"/>')
    out.append(disc(mx, my - 130, 9, "#5a4636"))
    FOCUS[:] = [(bx + mx) / 2, by - 40, 560]
    # Flowers close up, out of focus a little.
    for k in range(9):
        fx = W * (k / 8) + rnd.random() * 30
        fy = H * (0.92 + 0.06 * rnd.random())
        s = 26 + 16 * rnd.random()
        out.append(f'<line x1="{fx:.1f}" y1="{fy:.1f}" x2="{fx:.1f}" y2="{H + 10}" stroke="#3e7d32" stroke-width="6"/>')
        petal = "#f6c945" if not night else "#c7b6ff"
        for j in range(10):
            a = j / 10 * math.tau
            out.append(ell(fx + math.cos(a) * s * 0.7, fy + math.sin(a) * s * 0.7, s * 0.42, s * 0.18, petal,
                           f'transform="rotate({math.degrees(a):.0f} {fx + math.cos(a) * s * 0.7:.1f} {fy + math.sin(a) * s * 0.7:.1f})"'))
        out.append(disc(fx, fy, s * 0.45, "#6b3f1d"))
    if night:
        out.append(sparks(rnd, 30, "#fff3a0", area=(0, H * 0.5, W, H)))
    else:
        out.append(bokeh(rnd, 10, "#fffbe6", area=(0, H * 0.4, W, H)))
    return "".join(out)


def scene_dungeon(p, rnd, v):
    """A torch-lit stone corridor going down, embers, a warrior's silhouette before a glowing gate."""
    out = [defs(p, "#1a0f14", "#3a1a1a", "#2a1212")]
    out.append(f'<rect width="{W}" height="{H}" fill="url(#sky)"/>')
    vx, vy = W * (0.5 + 0.08 * (v - 1)), H * 0.46
    # The corridor's walls as receding arches.
    for k in range(9, 0, -1):
        t = k / 9
        w, h = 90 + 900 * (1 - t) ** 1.4, 120 + 700 * (1 - t) ** 1.4
        stone = mix("#5a3a32", "#120a0c", t)
        out.append(f'<path d="M{vx - w:.1f},{vy + h * 0.55:.1f} L{vx - w:.1f},{vy - h * 0.2:.1f} Q{vx:.1f},{vy - h * 0.75:.1f} {vx + w:.1f},{vy - h * 0.2:.1f} '
                   f'L{vx + w:.1f},{vy + h * 0.55:.1f} L{vx + w * 1.4:.1f},{vy + h * 0.55:.1f} L{vx + w * 1.4:.1f},{vy - h:.1f} L{vx - w * 1.4:.1f},{vy - h:.1f} L{vx - w * 1.4:.1f},{vy + h * 0.55:.1f} Z" fill="{stone}"/>')
    # The gate's glow at the end.
    out.append(ell(vx, vy, 70, 95, "#ff6a2b", 'opacity="0.85" filter="url(#b2)"'))
    out.append(ell(vx, vy, 30, 50, "#ffd27a"))
    out.append(rays(vx, vy, "#ff9a4d", n=9, spread=2.8, angle=1.57, opacity=0.12, length=900))
    # The floor.
    out.append(poly([(vx - 50, vy + 60), (vx + 50, vy + 60), (W + 200, H), (-200, H)], "#2b1a18"))
    for k in range(8):
        y = vy + 60 + (H - vy - 60) * (k / 8) ** 1.6
        out.append(f'<line x1="0" y1="{y:.1f}" x2="{W}" y2="{y:.1f}" stroke="#000" stroke-width="2" opacity="0.3"/>')
    # Torches on the walls.
    for side in (-1, 1):
        for k, t in enumerate((0.55, 0.3)):
            tx, ty = vx + side * (180 + 520 * (1 - t)), vy - 40 - 120 * (1 - t)
            out.append(rect(tx - 5, ty, 10, 40, "#3b2a22"))
            out.append(disc(tx, ty - 6, 60, "#ff8a3d", 'opacity="0.45" filter="url(#b3)"'))
            out.append(ell(tx, ty - 10, 10, 18, "#ffb347") + ell(tx, ty - 6, 5, 10, "#fff1b8"))
    # The hero, with a long shadow towards us.
    hx = vx + (rnd.random() - 0.5) * 120
    out.append(f'<path d="M{hx - 20:.1f},{vy + 200:.1f} L{hx + 20:.1f},{vy + 200:.1f} L{hx + 140:.1f},{H} L{hx - 140:.1f},{H} Z" fill="#000" opacity="0.35" filter="url(#b1)"/>')
    FOCUS[:] = [hx, vy + 140, 380]
    out.append(hero(hx, vy + 200, 60, "#241517", "#c0392b"))
    out.append(sparks(rnd, 50, "#ff8a3d", area=(0, H * 0.1, W, H), size=(1.2, 3)))
    return "".join(out)


def scene_caves(p, rnd, v):
    """A cavern of glowing crystals and hanging lanterns, a small figure crossing a bridge of light."""
    out = [defs(p, mix(hsv(p.a, 0.7, 0.18), "#000000", 0.2), hsv(p.b, 0.6, 0.35), hsv(p.b, 0.5, 0.5))]
    out.append(f'<rect width="{W}" height="{H}" fill="url(#sky)"/>')
    glow_c = hsv(p.b, 0.55, 1.0)
    # Cave walls: stalactites from above, rocks from below.
    for layer, (tone, blur) in enumerate(((mix(hsv(p.a, 0.6, 0.3), "#000", 0.2), 'filter="url(#far)"'), (hsv(p.a, 0.6, 0.14), ""))):
        pts = [(-20, -10)]
        for k in range(18):
            x = W * k / 17
            pts += [(x - 25, 0), (x, 60 + 140 * rnd.random() * (1 - 0.4 * layer)), (x + 25, 0)]
        pts.append((W + 20, -10))
        out.append(poly(pts, tone, blur))
        out.append(ridge(rnd, H * (0.86 + 0.06 * layer), 70 + 40 * layer, tone, peaks=7, sharp=1, extra=blur))
    # Crystals.
    for k in range(14):
        cx, cy = rnd.random() * W, H * (0.72 + 0.25 * rnd.random())
        s = 20 + 45 * rnd.random()
        c = hsv(p.b + 0.05 * rnd.random(), 0.55, 1.0)
        out.append(disc(cx, cy - s, s * 1.4, c, 'opacity="0.35" filter="url(#b2)"'))
        out.append(poly([(cx - s * 0.3, cy), (cx - s * 0.15, cy - s * 1.6), (cx + s * 0.05, cy - s * 2.0), (cx + s * 0.3, cy - s * 1.4), (cx + s * 0.25, cy)], c, 'opacity="0.95"'))
        out.append(poly([(cx - s * 0.15, cy - s * 1.6), (cx + s * 0.05, cy - s * 2.0), (cx, cy)], "#ffffff", 'opacity="0.35"'))
    # Hanging lanterns.
    for k in range(6):
        lx, ly = W * (0.1 + 0.16 * k) + rnd.random() * 30, H * (0.15 + 0.2 * rnd.random())
        out.append(f'<line x1="{lx:.1f}" y1="0" x2="{lx:.1f}" y2="{ly:.1f}" stroke="#1a1420" stroke-width="2"/>')
        out.append(disc(lx, ly + 18, 50, "#ffc76b", 'opacity="0.45" filter="url(#b3)"'))
        out.append(rect(lx - 11, ly, 22, 30, "#ffd27a", rx=6) + rect(lx - 13, ly - 4, 26, 6, "#3a2a22", rx=2))
    # A bridge of light and its walker.
    by = H * (0.55 + 0.05 * (v % 2))
    out.append(f'<path d="M-20,{by + 40:.1f} Q{W / 2:.1f},{by - 30:.1f} {W + 20},{by + 40:.1f}" stroke="{glow_c}" stroke-width="10" fill="none" opacity="0.9" filter="url(#b1)"/>')
    out.append(f'<path d="M-20,{by + 40:.1f} Q{W / 2:.1f},{by - 30:.1f} {W + 20},{by + 40:.1f}" stroke="#ffffff" stroke-width="3" fill="none"/>')
    hx = W * (0.4 + 0.12 * v)
    FOCUS[:] = [hx + 10, by - 30, 280]
    out.append(hero(hx, by + 2, 38, hsv(p.a, 0.4, 0.3), "#ffd27a"))
    out.append(disc(hx + 30, by - 50, 14, "#ffe7a8") + disc(hx + 30, by - 50, 60, "#ffd27a", 'opacity="0.5" filter="url(#b2)"'))
    out.append(sparks(rnd, 45, glow_c, area=(0, 0, W, H), size=(1, 3)))
    return "".join(out)


def scene_party(p, rnd, v):
    """A stage under spotlights, confetti, balloons, little round players jumping."""
    out = [defs(p, hsv(p.a, 0.8, 0.35), hsv(p.b, 0.7, 0.75), hsv(p.b, 0.5, 0.9))]
    out.append(f'<rect width="{W}" height="{H}" fill="url(#sky)"/>')
    # Sunburst behind the stage.
    cx, cy = W / 2, H * 0.72
    for k in range(24):
        a0, a1 = math.tau * k / 24, math.tau * (k + 0.5) / 24
        out.append(poly([(cx, cy), (cx + math.cos(a0) * 1400, cy + math.sin(a0) * 1400), (cx + math.cos(a1) * 1400, cy + math.sin(a1) * 1400)],
                        "#ffffff", 'opacity="0.07"'))
    # Spotlights.
    for k, x in enumerate((W * 0.15, W * 0.85, W * 0.5)):
        out.append(poly([(x - 10, 0), (x + 10, 0), (cx + (x - cx) * 0.2 + 160, H * 0.8), (cx + (x - cx) * 0.2 - 160, H * 0.8)],
                        "#fff8d6", 'opacity="0.13" filter="url(#b1)"'))
    # The stage.
    out.append(ell(cx, H * 0.84, W * 0.55, 70, hsv(p.a, 0.6, 0.45)) + ell(cx, H * 0.82, W * 0.55, 60, hsv(p.a, 0.5, 0.7)))
    out.append(ell(cx, H * 0.8, W * 0.42, 40, "#ffffff", 'opacity="0.2"'))
    # Balloons.
    for k in range(10):
        bx, by = rnd.random() * W, H * (0.08 + 0.3 * rnd.random())
        c = hsv(rnd.random(), 0.7, 0.98)
        out.append(f'<path d="M{bx:.1f},{by + 34:.1f} q-8,30 4,60" stroke="#ffffff" stroke-width="1.5" fill="none" opacity="0.6"/>')
        out.append(ell(bx, by, 26, 32, c) + ell(bx - 8, by - 12, 7, 11, "#ffffff", 'opacity="0.5"'))
    # The players: jelly-round characters mid-jump.
    for k in range(5):
        px = W * (0.2 + 0.15 * k)
        py = H * (0.72 - 0.12 * ((k + v) % 2))
        out.append(critter(px, py, 42, hsv(p.a + 0.2 * k, 0.6, 0.98), "#ffffff", kind=2))
    FOCUS[:] = [W * 0.5, H * 0.6, 560]
    # Confetti.
    for k in range(120):
        x, y = rnd.random() * W, rnd.random() * H
        c = hsv(rnd.random(), 0.7, 1.0)
        out.append(rect(x, y, 6, 12, c, extra=f'transform="rotate({rnd.random() * 180:.0f} {x:.1f} {y:.1f})" opacity="0.9"'))
    return "".join(out)


def scene_kitchen(p, rnd, v):
    """A kitchen in orbit: a round station, a planet below, ingredients floating, a chef in a bubble helmet."""
    out = [defs(p, "#070b1f", "#1b1f4a", "#2b2f6a")]
    out.append(f'<rect width="{W}" height="{H}" fill="url(#sky)"/>')
    out.append(stars(rnd, 220, top=1.0))
    # The planet.
    out.append(disc(W * 0.2, H * 1.15, 520, hsv(p.b, 0.5, 0.65)) + disc(W * 0.2, H * 1.15, 540, hsv(p.b, 0.4, 0.9), 'opacity="0.3" filter="url(#b2)"'))
    for k in range(5):
        out.append(ell(W * 0.2, H * 1.15 - 300 + 70 * k, 500, 18, "#ffffff", 'opacity="0.12"'))
    # The station: a ring and a dome with a lit counter.
    sx, sy = W * 0.62, H * 0.48
    out.append(ell(sx, sy, 330, 80, "none", f'stroke="{hsv(p.a, 0.3, 0.85)}" stroke-width="22"'))
    out.append(disc(sx, sy, 170, "#d9e3f2") + disc(sx, sy, 150, "#24305e"))
    out.append(disc(sx, sy, 150, "#8fd0ff", 'opacity="0.18"'))
    out.append(rect(sx - 120, sy + 30, 240, 40, "#e8edf5", rx=10) + rect(sx - 120, sy + 26, 240, 8, p.main, rx=4))
    for k in range(3):
        out.append(disc(sx - 70 + 70 * k, sy + 20, 22, "#3a3f55") + ell(sx - 70 + 70 * k, sy + 10, 26, 8, "#ff8a3d", 'opacity="0.6" filter="url(#b1)"'))
    out.append(ell(sx - 40, sy - 120, 80, 30, "#ffffff", 'opacity="0.18"'))
    # Floating ingredients.
    foods = [("#ff5a4f", "#3a9e4a"), ("#ffd23f", "#ffd23f"), ("#8fd14f", "#5a9e2f"), ("#ff9f43", "#3a9e4a"), ("#f8f1e5", "#e0c79a")]
    for k in range(12):
        fx, fy = rnd.random() * W, rnd.random() * H * 0.9
        a, b = rnd.choice(foods)
        r = 10 + 14 * rnd.random()
        out.append(disc(fx, fy, r, a) + ell(fx, fy - r * 0.9, r * 0.3, r * 0.18, b) + disc(fx - r * 0.3, fy - r * 0.3, r * 0.25, "#ffffff", 'opacity="0.4"'))
    FOCUS[:] = [W * 0.5, H * 0.45, 640]
    # The chef, floating.
    cx, cy = W * (0.3 + 0.1 * v), H * 0.35
    out.append(disc(cx, cy, 46, "#cfe8ff", 'opacity="0.35"') + disc(cx, cy, 46, "none", 'stroke="#ffffff" stroke-width="3" opacity="0.7"'))
    out.append(disc(cx, cy + 4, 26, "#ffd7b5") + disc(cx - 9, cy, 4, "#2b2238") + disc(cx + 9, cy, 4, "#2b2238"))
    out.append(rect(cx - 22, cy - 40, 44, 20, "#ffffff", rx=10) + disc(cx - 12, cy - 44, 12, "#ffffff") + disc(cx + 10, cy - 46, 14, "#ffffff"))
    out.append(rect(cx - 30, cy + 44, 60, 60, "#ffffff", rx=16) + rect(cx - 30, cy + 64, 60, 8, p.main))
    return "".join(out)


def scene_lighthouse(p, rnd, v):
    """A lighthouse on a cliff at blue hour, its beam over a calm sea, a small boat."""
    out = [defs(p, "#1d2f63", "#f0a3a0", "#c7b8d8")]
    out.append(f'<rect width="{W}" height="{H}" fill="url(#sky)"/>')
    out.append(stars(rnd, 90, top=0.45))
    out.append(disc(W * 0.86, H * 0.16, 30, "#fff6d8") + disc(W * 0.86, H * 0.16, 100, "#fff2c0", 'opacity="0.2" filter="url(#b3)"'))
    sea = H * 0.62
    out.append(ridge(rnd, sea, 40, "#4b5d8f", peaks=3, extra='filter="url(#far)"'))
    out.append(f'<rect y="{sea:.1f}" width="{W}" height="{H - sea:.1f}" fill="#22325e"/>')
    for k in range(60):
        gx, gy = rnd.random() * W, sea + (H - sea) * rnd.random() ** 1.5
        out.append(rect(gx, gy, 10 + 40 * rnd.random(), 2, "#c9d6ff", extra=f'opacity="{0.2 + 0.4 * rnd.random():.2f}"'))
    # The cliff.
    lx = W * (0.25 if v % 2 == 0 else 0.72)
    out.append(f'<path d="{path_through([(lx - 340, H + 10), (lx - 230, sea - 20), (lx - 120, sea - 120), (lx + 60, sea - 140), (lx + 180, sea - 60), (lx + 300, H + 10)])} Z" fill="#1a2440"/>')
    # The lighthouse, white and red.
    by = sea - 135
    for k in range(5):
        y0 = by - k * 44
        w0, w1 = 46 - k * 4, 42 - k * 4
        out.append(poly([(lx - w0, y0), (lx + w0, y0), (lx + w1, y0 - 44), (lx - w1, y0 - 44)], "#f4f1ea" if k % 2 == 0 else "#d64545"))
    top = by - 220
    out.append(rect(lx - 30, top - 40, 60, 40, "#fff2b3") + poly([(lx - 38, top - 40), (lx, top - 75), (lx + 38, top - 40)], "#2a2f45"))
    out.append(rect(lx - 40, top, 80, 8, "#2a2f45"))
    FOCUS[:] = [lx, top + 90, 470]
    # The beam.
    a = [-0.25, 0.35, -0.05, 0.2][v % 4] + (math.pi if lx > W / 2 else 0)
    bx, byy = lx + math.cos(a) * 1500, top - 20 + math.sin(a) * 1500
    out.append(f'<path d="M{lx:.1f},{top - 20:.1f} L{bx:.1f},{byy - 140:.1f} L{bx:.1f},{byy + 140:.1f} Z" fill="url(#ray)" opacity="0.45" filter="url(#b1)" style="mix-blend-mode:screen"/>')
    out.append(disc(lx, top - 20, 70, "#fff3b0", 'opacity="0.6" filter="url(#b2)"'))
    # A little boat with a lamp.
    bx2 = W * (0.62 if lx < W / 2 else 0.25)
    out.append(f'<path d="M{bx2 - 40:.1f},{sea + 60:.1f} L{bx2 + 40:.1f},{sea + 60:.1f} L{bx2 + 28:.1f},{sea + 76:.1f} L{bx2 - 28:.1f},{sea + 76:.1f} Z" fill="#3a2a22"/>')
    out.append(disc(bx2 + 20, sea + 50, 5, "#ffd27a") + disc(bx2 + 20, sea + 50, 26, "#ffd27a", 'opacity="0.4" filter="url(#b2)"'))
    out.append(rect(bx2 - 60, sea + 80, 120, 4, "#ffd27a", extra='opacity="0.3" filter="url(#b1)"'))
    return "".join(out)


def scene_desert(p, rnd, v):
    """Dunes at sunset, a fennec fox with huge ears on a crest, a ruin, wind-blown sand."""
    out = [defs(p, "#ff8a5c", "#ffe0a3", "#ffd9a0")]
    out.append(f'<rect width="{W}" height="{H}" fill="url(#sky)"/>')
    out.append(sun(W * 0.68, H * 0.42, 70, core="#fff2c6", halo="#ffb066"))
    for depth, colour in ((0.45, "#f0a76a"), (0.55, "#e38e55"), (0.68, "#c9733f"), (0.82, "#a85a33")):
        pts = [(-50, H * depth + 30)]
        for k in range(6):
            pts.append((W * k / 5 + 80 * rnd.random(), H * depth - 40 * rnd.random() + 20 * (k % 2)))
        pts.append((W + 50, H * depth + 20))
        out.append(f'<path d="{path_through(pts)} L{W + 50},{H} L-50,{H} Z" fill="{colour}"/>')
        out.append(f'<path d="{path_through(pts)}" stroke="#ffe2b8" stroke-width="2" fill="none" opacity="0.45"/>')
    # A ruined arch far away.
    rx = W * (0.25 + 0.4 * (v % 2))
    out.append(f'<g filter="url(#far)" opacity="0.8">{rect(rx - 60, H * 0.36, 22, 90, "#c0784c")}{rect(rx + 38, H * 0.34, 22, 110, "#c0784c")}'
               f'<path d="M{rx - 60:.1f},{H * 0.36:.1f} Q{rx:.1f},{H * 0.26:.1f} {rx + 60:.1f},{H * 0.34:.1f}" stroke="#c0784c" stroke-width="20" fill="none"/></g>')
    # The fennec.
    fx, fy = W * (0.4 + 0.15 * (v % 3)), H * 0.66
    body = "#f4c38b"
    out.append(ell(fx, fy, 46, 30, body) + disc(fx + 40, fy - 30, 26, body))
    for side in (-1, 1):
        out.append(poly([(fx + 30 + side * 12, fy - 46), (fx + 40 + side * 34, fy - 120), (fx + 50 + side * 6, fy - 48)], body))
        out.append(poly([(fx + 34 + side * 12, fy - 50), (fx + 40 + side * 28, fy - 104), (fx + 46 + side * 6, fy - 52)], "#ffb3a0"))
    out.append(disc(fx + 50, fy - 34, 4, "#2b1a14") + disc(fx + 64, fy - 26, 4, "#2b1a14"))
    out.append(f'<path d="M{fx - 44:.1f},{fy:.1f} q-60,-10 -70,-60" stroke="{body}" stroke-width="24" fill="none" stroke-linecap="round"/>')
    out.append(disc(fx - 112, fy - 62, 14, "#fff3e0"))
    for side in (-1, 1):
        out.append(rect(fx + side * 22 - 6, fy + 18, 12, 26, body, rx=5))
    out.append(ell(fx - 10, fy - 10, 30, 10, "#ffffff", 'opacity="0.25"'))
    FOCUS[:] = [fx + 10, fy - 30, 380]
    # Sand blown by the wind.
    for k in range(40):
        x, y = rnd.random() * W, H * (0.5 + 0.5 * rnd.random())
        out.append(f'<line x1="{x:.1f}" y1="{y:.1f}" x2="{x + 40 + 60 * rnd.random():.1f}" y2="{y - 6:.1f}" stroke="#ffe7c2" stroke-width="2" opacity="0.4"/>')
    return "".join(out)


def scene_snow(p, rnd, v):
    """A snowy slope under a cold blue sky, pine trees, a racer carving a spray of snow."""
    out = [defs(p, "#3a8fe0", "#e6f4ff", "#ffffff")]
    out.append(f'<rect width="{W}" height="{H}" fill="url(#sky)"/>')
    out.append(sun(W * 0.82, H * 0.14, 28, core="#ffffff", halo="#d6efff"))
    out.append(ridge(rnd, H * 0.4, 220, "#a9c8e8", peaks=4, sharp=1, extra='filter="url(#far)"'))
    out.append(ridge(rnd, H * 0.48, 140, "#d8e8f7", peaks=5, sharp=1))
    # The slope.
    out.append(f'<path d="M-20,{H * 0.48:.1f} Q{W * 0.5:.1f},{H * 0.4:.1f} {W + 20},{H * 0.62:.1f} L{W + 20},{H} L-20,{H} Z" fill="#f6fbff"/>')
    out.append(f'<path d="M-20,{H * 0.48:.1f} Q{W * 0.5:.1f},{H * 0.4:.1f} {W + 20},{H * 0.62:.1f} L{W + 20},{H * 0.7:.1f} Q{W * 0.5:.1f},{H * 0.5:.1f} -20,{H * 0.6:.1f} Z" fill="#dcebfa" opacity="0.7"/>')
    # Pines.
    for k in range(18):
        tx = rnd.random() * W
        ty = H * (0.5 + 0.45 * rnd.random())
        s = 20 + 60 * ((ty - H * 0.5) / (H * 0.45))
        dark = mix("#2f5d50", "#9fb9c7", max(0.0, 0.7 - (ty - H * 0.5) / (H * 0.45)))
        for j in range(3):
            out.append(poly([(tx - s * (0.9 - 0.2 * j), ty - s * 0.6 * j), (tx, ty - s * (1.1 + 0.6 * j)), (tx + s * (0.9 - 0.2 * j), ty - s * 0.6 * j)], dark))
            out.append(poly([(tx - s * (0.6 - 0.15 * j), ty - s * (0.15 + 0.6 * j)), (tx, ty - s * (1.1 + 0.6 * j)), (tx + s * (0.2 - 0.05 * j), ty - s * (0.4 + 0.6 * j))], "#ffffff", 'opacity="0.75"'))
    # Ski tracks and the racer.
    rx, ry = W * (0.45 + 0.1 * v), H * 0.66
    out.append(f'<path d="M{rx:.1f},{ry:.1f} C{rx - 120:.1f},{ry - 60:.1f} {rx - 60:.1f},{ry - 150:.1f} {rx - 240:.1f},{ry - 220:.1f}" stroke="#bcd3ea" stroke-width="6" fill="none"/>')
    for k in range(30):
        a = -2.4 + rnd.random() * 1.2
        d = 20 + 90 * rnd.random()
        out.append(disc(rx + math.cos(a) * d, ry + 10 + math.sin(a) * d * 0.6, 3 + 6 * rnd.random(), "#ffffff", 'opacity="0.85"'))
    out.append(f'<line x1="{rx - 50:.1f}" y1="{ry + 12:.1f}" x2="{rx + 50:.1f}" y2="{ry - 2:.1f}" stroke="#2b2238" stroke-width="7" stroke-linecap="round"/>')
    out.append(f'<g transform="rotate(-18 {rx:.1f} {ry:.1f})">{rect(rx - 14, ry - 70, 28, 64, p.main, rx=12)}{disc(rx, ry - 84, 17, p.accent)}'
               f'{rect(rx - 13, ry - 92, 26, 9, "#1c2433", rx=4)}{rect(rx - 12, ry - 40, 24, 36, mix(p.main, "#000000", 0.3), rx=8)}</g>')
    FOCUS[:] = [rx, ry - 50, 380]
    out.append(stars(rnd, 70, top=1.0))
    return "".join(out)


def scene_paper(p, rnd, v):
    """Kingdoms cut out of paper: layered hills with soft shadows, castles and banners, a paper sun."""
    tones = [hsv(p.a + 0.08 * k, 0.32 + 0.06 * k, 0.98 - 0.09 * k) for k in range(6)]
    out = [defs(p, "#fdf3e1", "#f6dcc2", "#fff6e8")]
    out.append(f'<rect width="{W}" height="{H}" fill="url(#sky)"/>')
    # The paper's fibres.
    out.append(f'<rect width="{W}" height="{H}" filter="url(#grain)" opacity="0.8"/>')
    out.append(disc(W * 0.78, H * 0.22, 70, "#ffb86b", 'filter="url(#shadow)"') + disc(W * 0.78, H * 0.22, 50, "#ffd08f"))
    for k in range(3):
        out.append(f'<g filter="url(#shadow)">{cloud(W * (0.15 + 0.3 * k), H * (0.15 + 0.05 * k), 30, "#ffffff")}</g>')
    for k, tone in enumerate(tones):
        base = H * (0.42 + 0.1 * k)
        out.append(f'<g filter="url(#shadow)">{ridge(rnd, base, 60 - 5 * k, tone, peaks=4 + k % 3)}</g>')
        if k in (1, 3):
            cx = W * (0.25 + 0.5 * ((k + v) % 2)) + rnd.random() * 60
            cy = base - 40
            colour = mix(tone, "#ffffff", 0.35)
            castle = rect(cx - 50, cy - 60, 100, 60, colour)
            for t in (-1, 1):
                castle += rect(cx + t * 50 - 16, cy - 100, 32, 100, colour) + poly([(cx + t * 50 - 22, cy - 100), (cx + t * 50, cy - 140), (cx + t * 50 + 22, cy - 100)], "#d9534f")
                castle += f'<line x1="{cx + t * 50:.1f}" y1="{cy - 140:.1f}" x2="{cx + t * 50:.1f}" y2="{cy - 175:.1f}" stroke="#6b4a3a" stroke-width="3"/>'
                castle += poly([(cx + t * 50, cy - 175), (cx + t * 50 + 30, cy - 166), (cx + t * 50, cy - 157)], p.main)
            castle += rect(cx - 14, cy - 34, 28, 34, "#6b4a3a", rx=12)
            out.append(f'<g filter="url(#shadow)">{castle}</g>')
    FOCUS[:] = [W * 0.5, H * 0.55, 640]
    # Paper soldiers marching in the front.
    for k in range(7):
        sx, sy = W * (0.1 + 0.12 * k), H * 0.94
        out.append(f'<g filter="url(#shadow)">{rect(sx - 12, sy - 46, 24, 40, p.main if k % 2 else p.accent, rx=6)}{disc(sx, sy - 58, 13, "#ffe2c6")}'
                   f'{poly([(sx - 14, sy - 64), (sx, sy - 86), (sx + 14, sy - 64)], "#4a4a5a")}</g>')
    return "".join(out)


def scene_tactics(p, rnd, v):
    """A floating board of tiles under a night sky in bloom, units on it, a glowing selected tile."""
    out = [defs(p, "#0e0b24", hsv(p.a, 0.6, 0.38), hsv(p.b, 0.5, 0.6))]
    out.append(f'<rect width="{W}" height="{H}" fill="url(#sky)"/>')
    out.append(stars(rnd, 160))
    out.append(disc(W * 0.2, H * 0.2, 36, "#fdf2ff") + disc(W * 0.2, H * 0.2, 120, hsv(p.b, 0.4, 1.0), 'opacity="0.25" filter="url(#b3)"'))
    # The board, isometric.
    cx, cy, tw, th = W * 0.5, H * 0.2, 104, 52
    n = 7
    for i in range(n):
        for j in range(n):
            x = cx + (i - j) * tw / 2
            y = cy + (i + j) * th / 2
            lift = 10 * ((i * 3 + j * 5) % 3)
            top = mix(hsv(p.a, 0.4, 0.55), "#87a96b", 0.5 if (i + j) % 3 else 0.2)
            out.append(poly([(x, y - lift), (x + tw / 2, y + th / 2 - lift), (x, y + th - lift), (x - tw / 2, y + th / 2 - lift)], top))
            out.append(poly([(x - tw / 2, y + th / 2 - lift), (x, y + th - lift), (x, y + th + 26), (x - tw / 2, y + th / 2 + 26)], mix(top, "#000000", 0.35)))
            out.append(poly([(x + tw / 2, y + th / 2 - lift), (x, y + th - lift), (x, y + th + 26), (x + tw / 2, y + th / 2 + 26)], mix(top, "#000000", 0.5)))
    # The selected tile and a path.
    si, sj = 3 + v % 3, 4
    x, y = cx + (si - sj) * tw / 2, cy + (si + sj) * th / 2
    out.append(poly([(x, y), (x + tw / 2, y + th / 2), (x, y + th), (x - tw / 2, y + th / 2)], hsv(p.b, 0.6, 1.0), 'opacity="0.7"'))
    out.append(disc(x, y + th / 2, 50, hsv(p.b, 0.6, 1.0), 'opacity="0.4" filter="url(#b2)"'))
    # Units: banners and little figures.
    for k, (i, j) in enumerate(((1, 2), (2, 5), (5, 1), (6, 6), (si, sj))):
        ux, uy = cx + (i - j) * tw / 2, cy + (i + j) * th / 2 + th / 2
        colour = p.main if k < 3 else p.accent
        out.append(ell(ux, uy, 16, 6, "#000", 'opacity="0.35"') + rect(ux - 9, uy - 34, 18, 30, colour, rx=6) + disc(ux, uy - 42, 9, "#ffe2c6"))
        out.append(f'<line x1="{ux + 12:.1f}" y1="{uy - 4:.1f}" x2="{ux + 12:.1f}" y2="{uy - 60:.1f}" stroke="#d9d2c0" stroke-width="2"/>' + poly([(ux + 12, uy - 60), (ux + 30, uy - 54), (ux + 12, uy - 48)], colour))
    FOCUS[:] = [cx, cy + th * 3.5, 600]
    # Petals drifting.
    for k in range(40):
        x, y = rnd.random() * W, rnd.random() * H
        out.append(ell(x, y, 6, 3, hsv(p.b, 0.35, 1.0), f'opacity="0.8" transform="rotate({rnd.random() * 180:.0f} {x:.1f} {y:.1f})"'))
    return "".join(out)


def scene_ink(p, rnd, v):
    """A city plaza splashed with two inks, squid-like kids leaping, drops flying."""
    a, b = hsv(p.a, 0.85, 0.95), hsv(p.b, 0.85, 0.95)
    out = [defs(p, "#5ec8ff", "#e9f7ff", "#ffffff")]
    out.append(f'<rect width="{W}" height="{H}" fill="url(#sky)"/>')
    for k in range(7):
        bx, bw, bh = W * k / 6 - 40, 140 + 60 * rnd.random(), H * (0.3 + 0.25 * rnd.random())
        out.append(rect(bx, H * 0.62 - bh, bw, bh, mix("#c7d3e6", "#8695b3", rnd.random()), extra='filter="url(#far)"'))
        for r in range(int(bh / 40)):
            for c in range(3):
                out.append(rect(bx + 20 + c * 40, H * 0.62 - bh + 16 + r * 40, 20, 18, "#e8f4ff", extra='opacity="0.6"'))
    out.append(rect(0, H * 0.6, W, H * 0.4, "#a6a9b6"))
    for k in range(10):
        y = H * 0.6 + k * k * 3
        out.append(f'<line x1="0" y1="{y:.1f}" x2="{W}" y2="{y:.1f}" stroke="#8b8e9c" stroke-width="2"/>')
    # Ink splats on the ground and walls.
    for k in range(16):
        c = a if k % 2 else b
        x, y = rnd.random() * W, H * (0.4 + 0.6 * rnd.random())
        r = 30 + 70 * rnd.random()
        blob = [(x + math.cos(t / 12 * math.tau) * r * (0.6 + 0.6 * rnd.random()), y + math.sin(t / 12 * math.tau) * r * 0.5 * (0.6 + 0.6 * rnd.random())) for t in range(12)]
        out.append(f'<path d="{path_through(blob + blob[:3])}" fill="{c}" opacity="0.92"/>')
        for d in range(4):
            out.append(disc(x + (rnd.random() - 0.5) * r * 3, y + (rnd.random() - 0.5) * r, 6 + 10 * rnd.random(), c))
    # The squad: rounded kids with tentacle hair.
    for k in range(3):
        kx, ky = W * (0.25 + 0.25 * k), H * (0.62 - 0.12 * ((k + v) % 2))
        c = a if k != 1 else b
        out.append(ell(kx, H * 0.86, 40, 10, "#000", 'opacity="0.2"'))
        out.append(rect(kx - 22, ky, 44, 60, "#2f2f3a", rx=16) + disc(kx, ky - 12, 30, "#ffd7b5"))
        for t in range(4):
            out.append(f'<path d="M{kx - 24 + t * 16:.1f},{ky - 34:.1f} q{-8 + t * 4},{-30} {-2 + t * 3},{-48}" stroke="{c}" stroke-width="13" stroke-linecap="round" fill="none"/>')
        out.append(rect(kx - 22, ky - 22, 44, 12, "#1c1c24", rx=6) + disc(kx - 9, ky - 16, 4, "#fff") + disc(kx + 9, ky - 16, 4, "#fff"))
        out.append(rect(kx + 18, ky + 14, 44, 16, c, rx=6) + rect(kx + 50, ky + 10, 16, 24, "#ffffff", rx=4))
    FOCUS[:] = [W * 0.5, H * 0.55, 600]
    for k in range(60):
        x, y = rnd.random() * W, rnd.random() * H * 0.8
        out.append(disc(x, y, 3 + 7 * rnd.random(), a if k % 2 else b))
    return "".join(out)


SHADOW = 'opacity="0.3"'


def scene_arena(p, rnd, v):
    """A floating arena under a cosmic sky, fighters clashing in a burst of energy."""
    a, b = hsv(p.a, 0.8, 1.0), hsv(p.b, 0.8, 1.0)
    out = [defs(p, "#120b2e", "#3b1f6b", "#5b3a9e")]
    out.append(f'<rect width="{W}" height="{H}" fill="url(#sky)"/>')
    out.append(stars(rnd, 200))
    for k in range(3):
        out.append(ell(W * (0.2 + 0.3 * k), H * (0.25 + 0.1 * k), 260, 70, hsv(0.75 + 0.08 * k, 0.6, 0.8), 'opacity="0.25" filter="url(#b3)"'))
    # The platform.
    px, py = W / 2, H * 0.7
    out.append(ell(px, py + 40, 520, 70, "#2a1f4a") + ell(px, py, 520, 60, "#8a7cc0") + ell(px, py - 6, 500, 50, "#b9aee6"))
    out.append(poly([(px - 360, py + 40), (px + 360, py + 40), (px + 120, py + 230), (px - 120, py + 230)], "#2a1f4a"))
    out.append(ell(px, py - 6, 380, 34, "none", 'stroke="#ffffff" stroke-width="3" opacity="0.4"'))
    FOCUS[:] = [px + 40 * (v - 1), H * 0.55, 560]
    # The clash.
    cx, cy = px + 40 * (v - 1), py - 110
    out.append(disc(cx, cy, 120, "#ffffff", 'opacity="0.5" filter="url(#b3)"'))
    for k in range(14):
        ang = math.tau * k / 14
        out.append(poly([(cx, cy), (cx + math.cos(ang - 0.05) * 260, cy + math.sin(ang - 0.05) * 260), (cx + math.cos(ang + 0.05) * 260, cy + math.sin(ang + 0.05) * 260)],
                        "#fff4c2", 'opacity="0.35"'))
    out.append(disc(cx, cy, 34, "#ffffff"))
    for side, colour in ((-1, a), (1, b)):
        fx = cx + side * 150
        out.append(f'<g transform="translate({fx:.1f} {cy + 30:.1f}) scale({-side} 1)">'
                   f'{ell(0, 80, 46, 10, "#000", SHADOW)}'
                   f'{rect(-26, -10, 52, 70, colour, rx=18)}{disc(0, -38, 30, "#ffe0c7")}{rect(-30, -66, 60, 22, mix(colour, "#000000", 0.4), rx=10)}'
                   f'{rect(22, 0, 70, 20, colour, rx=10)}{disc(96, 10, 16, "#ffe0c7")}'
                   f'{rect(-22, 56, 16, 30, "#2b2238", rx=6)}{rect(6, 56, 16, 30, "#2b2238", rx=6)}</g>')
        out.append(sparks(rnd, 12, colour, area=(fx - 120, cy - 120, fx + 120, cy + 120)))
    return "".join(out)


def scene_space(p, rnd, v):
    """Little planets to hop between, a ringed giant, a hero mid-leap with a trail of stars."""
    out = [defs(p, "#0b0a2a", hsv(p.b, 0.6, 0.45), hsv(p.b, 0.4, 0.6))]
    out.append(f'<rect width="{W}" height="{H}" fill="url(#sky)"/>')
    out.append(stars(rnd, 260, top=1.0))
    out.append(ell(W * 0.6, H * 0.4, 500, 160, hsv(p.b + 0.1, 0.5, 0.8), 'opacity="0.18" filter="url(#b3)"'))
    # The ringed giant.
    gx, gy = W * (0.8 - 0.5 * (v % 2)), H * 0.26
    out.append(disc(gx, gy, 110, hsv(p.a + 0.1, 0.5, 0.85)))
    for k in range(4):
        out.append(ell(gx, gy - 60 + 30 * k, 105 - 10 * abs(k - 1.5), 9, "#ffffff", 'opacity="0.15"'))
    out.append(ell(gx, gy, 190, 32, "none", f'stroke="{hsv(p.b, 0.3, 1.0)}" stroke-width="12" opacity="0.8" transform="rotate(-12 {gx:.1f} {gy:.1f})"'))
    out.append(disc(gx - 40, gy - 40, 60, "#ffffff", 'opacity="0.18"'))
    # Small planets with grass, houses and trees.
    hops = [(W * 0.22, H * 0.68, 120), (W * 0.58, H * 0.8, 90), (W * 0.86, H * 0.66, 70)]
    for k, (x, y, r) in enumerate(hops):
        out.append(disc(x, y, r * 1.2, hsv(p.a + 0.3 * k, 0.4, 0.9), 'opacity="0.25" filter="url(#b2)"'))
        out.append(disc(x, y, r, hsv(p.a + 0.3 * k, 0.55, 0.75)))
        out.append(f'<path d="M{x - r:.1f},{y:.1f} A{r:.1f},{r:.1f} 0 0 1 {x + r:.1f},{y:.1f}" fill="{hsv(0.3, 0.6, 0.75)}"/>')
        out.append(disc(x - r * 0.35, y - r * 0.4, r * 0.3, "#ffffff", 'opacity="0.2"'))
        out.append(rect(x - 6, y - r - 30, 12, 30, "#6b4a3a") + disc(x, y - r - 40, 22, hsv(0.33, 0.6, 0.6)))
    FOCUS[:] = [W * (0.4 + 0.08 * v) - 30, H * 0.5, 320]
    # The hero leaping, a trail of stars behind.
    hx, hy = W * (0.4 + 0.08 * v), H * 0.45
    for k in range(9):
        out.append(disc(hx - 40 - k * 28, hy + 40 + k * 18, 6 - k * 0.5, "#fff3a3", 'opacity="0.9"'))
    out.append(f'<g transform="rotate(-20 {hx:.1f} {hy:.1f})">{hero(hx, hy + 60, 40, p.accent, "#ffd23f")}</g>')
    out.append(sparks(rnd, 20, "#ffffff"))
    return "".join(out)


def scene_summit(p, rnd, v):
    """A tall mountain at dawn, a climber on a ledge, strawberries of light, wind-blown snow."""
    out = [defs(p, hsv(p.a, 0.6, 0.35), "#ffb4a8", "#ffd2c8")]
    out.append(f'<rect width="{W}" height="{H}" fill="url(#sky)"/>')
    out.append(stars(rnd, 70, top=0.35))
    out.append(sun(W * 0.15, H * 0.62, 50, core="#fff0e6", halo="#ff9d8a"))
    out.append(ridge(rnd, H * 0.7, 120, "#8a6fa8", peaks=5, sharp=1, extra='filter="url(#far)"'))
    # The mountain.
    mx = W * (0.62 + 0.08 * (v % 2))
    face = [(mx - 520, H + 10), (mx - 260, H * 0.5), (mx - 90, H * 0.22), (mx, H * 0.05), (mx + 110, H * 0.25), (mx + 300, H * 0.55), (mx + 520, H + 10)]
    out.append(poly(face, "#4a3b66"))
    out.append(poly([(mx, H * 0.05), (mx + 110, H * 0.25), (mx + 300, H * 0.55), (mx + 520, H + 10), (mx + 40, H + 10)], "#382c50"))
    out.append(poly([(mx - 90, H * 0.22), (mx, H * 0.05), (mx + 110, H * 0.25), (mx + 40, H * 0.2), (mx - 20, H * 0.27)], "#ffffff"))
    # Ledges and the climber.
    for k in range(6):
        lx, ly = mx - 240 + 80 * k + 40 * rnd.random(), H * (0.75 - 0.08 * k)
        out.append(rect(lx, ly, 70, 10, "#6e5a8f", rx=4))
    cx, cy = mx - 160 + 60 * v, H * 0.67 - 30 * v
    FOCUS[:] = [cx, cy - 40, 330]
    out.append(hero(cx, cy, 34, "#e0485a", "#ffd23f"))
    for k in range(3):
        sx, sy = mx - 300 + 200 * k, H * (0.4 + 0.12 * k)
        out.append(disc(sx, sy, 22, "#ff6b8a", 'opacity="0.45" filter="url(#b2)"') + disc(sx, sy, 9, "#ff5577") + poly([(sx - 6, sy - 8), (sx, sy - 16), (sx + 6, sy - 8)], "#4caf50"))
    for k in range(80):
        x, y = rnd.random() * W, rnd.random() * H
        out.append(f'<line x1="{x:.1f}" y1="{y:.1f}" x2="{x + 18:.1f}" y2="{y + 4:.1f}" stroke="#ffffff" stroke-width="2" opacity="{0.3 + 0.5 * rnd.random():.2f}"/>')
    return "".join(out)


def scene_garden(p, rnd, v):
    """A garden at night: glowing moonflowers, fireflies, a greenhouse lit from inside, the Milky Way."""
    out = [defs(p, "#0b1033", hsv(p.a, 0.5, 0.55), hsv(p.b, 0.4, 0.7))]
    out.append(f'<rect width="{W}" height="{H}" fill="url(#sky)"/>')
    out.append(f'<path d="M-50,{H * 0.45:.1f} Q{W * 0.5:.1f},{H * 0.05:.1f} {W + 50},{H * 0.2:.1f}" stroke="#bfc8ff" stroke-width="120" fill="none" opacity="0.12" filter="url(#b3)"/>')
    out.append(stars(rnd, 260, top=0.75))
    out.append(disc(W * 0.85, H * 0.18, 34, "#fff8e1") + disc(W * 0.85, H * 0.18, 120, "#e8e2ff", 'opacity="0.2" filter="url(#b3)"'))
    out.append(ridge(rnd, H * 0.62, 60, hsv(p.a, 0.5, 0.22), peaks=4, extra='filter="url(#far)"'))
    out.append(rect(0, H * 0.66, W, H * 0.34, hsv(p.a, 0.45, 0.24)))
    # The greenhouse.
    gx, gy = W * (0.68 - 0.36 * (v % 2)), H * 0.64
    out.append(disc(gx, gy - 60, 180, "#ffd98a", 'opacity="0.3" filter="url(#b3)"'))
    out.append(f'<path d="M{gx - 120:.1f},{gy:.1f} L{gx - 120:.1f},{gy - 80:.1f} Q{gx:.1f},{gy - 190:.1f} {gx + 120:.1f},{gy - 80:.1f} L{gx + 120:.1f},{gy:.1f} Z" fill="#ffe6a8" opacity="0.85"/>')
    for k in range(5):
        x = gx - 120 + 60 * k
        out.append(f'<line x1="{x:.1f}" y1="{gy:.1f}" x2="{x:.1f}" y2="{gy - 80 - 30 * math.sin(k / 4 * math.pi):.1f}" stroke="#4a3f5c" stroke-width="5"/>')
    out.append(f'<path d="M{gx - 120:.1f},{gy - 80:.1f} Q{gx:.1f},{gy - 190:.1f} {gx + 120:.1f},{gy - 80:.1f}" stroke="#4a3f5c" stroke-width="6" fill="none"/>')
    FOCUS[:] = [gx, gy - 50, 500]
    # Moonflowers, glowing.
    glow_c = hsv(p.b, 0.4, 1.0)
    for k in range(22):
        fx, fy = rnd.random() * W, H * (0.7 + 0.28 * rnd.random())
        s = 8 + 16 * ((fy - H * 0.7) / (H * 0.28))
        out.append(f'<line x1="{fx:.1f}" y1="{fy:.1f}" x2="{fx + 4:.1f}" y2="{fy + s * 3:.1f}" stroke="#2f5d50" stroke-width="{s * 0.2:.1f}"/>')
        out.append(disc(fx, fy, s * 2.6, glow_c, 'opacity="0.3" filter="url(#b2)"'))
        for j in range(6):
            a = j / 6 * math.tau
            out.append(ell(fx + math.cos(a) * s * 0.6, fy + math.sin(a) * s * 0.6, s * 0.55, s * 0.3, "#f6f2ff",
                           f'transform="rotate({math.degrees(a):.0f} {fx + math.cos(a) * s * 0.6:.1f} {fy + math.sin(a) * s * 0.6:.1f})"'))
        out.append(disc(fx, fy, s * 0.3, glow_c))
    out.append(sparks(rnd, 45, "#fff3a0", area=(0, H * 0.3, W, H)))
    return "".join(out)


def scene_canyon(p, rnd, v):
    """A red canyon at noon, mesas in the haze, a rally car sliding in a cloud of dust."""
    out = [defs(p, "#4aa3e8", "#ffe2b0", "#ffd9a8")]
    out.append(f'<rect width="{W}" height="{H}" fill="url(#sky)"/>')
    out.append(sun(W * 0.85, H * 0.15, 36))
    for k in range(3):
        depth = 0.35 + 0.12 * k
        tone = mix("#c86a3c", "#ffd9a8", 0.6 - 0.25 * k)
        for m in range(3):
            x0 = rnd.random() * W
            top = H * (depth - 0.08 * rnd.random())
            w0 = 140 + 200 * rnd.random()
            out.append(poly([(x0 - w0 * 0.6, H * (depth + 0.25)), (x0 - w0 * 0.45, top), (x0 + w0 * 0.45, top), (x0 + w0 * 0.6, H * (depth + 0.25))],
                            tone, 'filter="url(#far)"' if k == 0 else ""))
            for s in range(3):
                y = top + (H * (depth + 0.25) - top) * (s + 1) / 4
                out.append(f'<line x1="{x0 - w0 * 0.5:.1f}" y1="{y:.1f}" x2="{x0 + w0 * 0.5:.1f}" y2="{y:.1f}" stroke="{mix(tone, "#000000", 0.15)}" stroke-width="5"/>')
    out.append(rect(0, H * 0.62, W, H * 0.4, "#e3a26a"))
    # The dirt road.
    road = [(W * (0.5 + 0.28 * math.sin(t * 2.4 + v)), H * (0.62 + 0.4 * t)) for t in [i / 20 for i in range(21)]]
    for i in range(len(road) - 1):
        out.append(f'<line x1="{road[i][0]:.1f}" y1="{road[i][1]:.1f}" x2="{road[i + 1][0]:.1f}" y2="{road[i + 1][1]:.1f}" '
                   f'stroke="#c78452" stroke-width="{8 + 120 * i / 20:.1f}" stroke-linecap="round"/>')
    # The car and its dust.
    cx, cy = road[7]
    for k in range(14):
        out.append(disc(cx - 60 - 22 * k, cy + 10 - 5 * k, 18 + 6 * k, "#f2d3ac", f'opacity="{0.75 - 0.045 * k:.2f}" filter="url(#b1)"'))
    out.append(f'<g transform="rotate(-8 {cx:.1f} {cy:.1f})" filter="url(#shadow)">'
               f'{rect(cx - 70, cy - 26, 140, 40, p.main, rx=14)}{poly([(cx - 34, cy - 26), (cx - 18, cy - 54), (cx + 36, cy - 54), (cx + 52, cy - 26)], "#2f3d55")}'
               f'{rect(cx - 70, cy - 10, 140, 8, "#ffffff")}{disc(cx - 42, cy + 16, 17, "#1b1b22")}{disc(cx + 42, cy + 16, 17, "#1b1b22")}'
               f'{disc(cx - 42, cy + 16, 7, "#9aa3b5")}{disc(cx + 42, cy + 16, 7, "#9aa3b5")}{rect(cx - 64, cy - 22, 50, 6, "url(#shine)")}</g>')
    FOCUS[:] = [cx - 30, cy - 10, 320]
    out.append(f'<text x="{cx + 2:.1f}" y="{cy - 6:.1f}" font-family="Sora" font-size="18" fill="#ffffff" text-anchor="middle" transform="rotate(-8 {cx:.1f} {cy:.1f})">07</text>')
    return "".join(out)


SCENES = {
    "sky": scene_sky, "kart": scene_kart, "critters": scene_critters, "village": scene_village,
    "farm": scene_farm, "dungeon": scene_dungeon, "caves": scene_caves, "party": scene_party,
    "kitchen": scene_kitchen, "lighthouse": scene_lighthouse, "desert": scene_desert, "snow": scene_snow,
    "paper": scene_paper, "tactics": scene_tactics, "ink": scene_ink, "arena": scene_arena,
    "space": scene_space, "summit": scene_summit, "garden": scene_garden, "canyon": scene_canyon,
}


def svg(art, p, rnd, v, aspect="wide"):
    """The scene as an SVG document; `aspect` square crops its middle (the icon)."""
    FOCUS[:] = [W / 2, H / 2, H]
    body = SCENES[art](p, rnd, v + p.shift) + finish()
    if aspect == "wide":
        box = f"0 0 {W} {H}"
    else:
        # The subject in the upper part: the name goes under it.
        x, y, size = FOCUS
        size = min(size, H)
        left = min(max(x - size / 2, 0), W - size)
        top = min(max(y - size * 0.3, 0), H - size)
        box = f"{left:.0f} {top:.0f} {size:.0f} {size:.0f}"
    return f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="{box}" preserveAspectRatio="xMidYMid slice" width="100%" height="100%">{body}</svg>'


# ------------------------------------------------------------- head-up displays

def hud(kind, name, seed, v):
    """A screenshot's head-up display as HTML over the scene, for its kind of game."""
    score = (sum(map(ord, name)) * 37 % 9000 + 1000) * (v + 1)
    hearts = "".join(f'<i class="heart{" empty" if k >= 3 + (v % 2) else ""}"></i>' for k in range(5))
    if kind in ("kart", "canyon", "snow"):
        place = ["2<sup>nd</sup>", "1<sup>st</sup>", "4<sup>th</sup>", "3<sup>rd</sup>"][v % 4]
        return (f'<div class="hud race"><div class="place">{place}</div>'
                f'<div class="lap">LAP <b>{min(3, v + 1)}</b>/3</div>'
                f'<div class="time">0{v}:{(seed * 7 + v * 13) % 60:02d}.{(seed * 31 + v) % 100:02d}</div>'
                f'<div class="speed"><b>{128 + 17 * v}</b> km/h<div class="gauge"><span style="width:{62 + 9 * v}%"></span></div></div>'
                f'<div class="map"><svg viewBox="0 0 100 100"><path d="M20,70 C10,30 40,10 60,25 S90,60 70,80 S30,95 20,70" stroke="#fff" stroke-width="6" fill="none"/>'
                f'<circle cx="{40 + 10 * v}" cy="{20 + 15 * v}" r="6" fill="#ffd23f"/></svg></div></div>')
    if kind in ("critters",):
        return (f'<div class="hud battle"><div class="card foe"><b>Wild critter</b><span>Lv {12 + v}</span><div class="bar"><span style="width:{40 + 10 * v}%"></span></div></div>'
                f'<div class="card mine"><b>Partner</b><span>Lv {15 + v}</span><div class="bar ok"><span style="width:{85 - 10 * v}%"></span></div><small>{48 - 6 * v}/56</small></div>'
                f'<div class="menu"><div class="sel">Fight</div><div>Bag</div><div>Team</div><div>Run</div></div></div>')
    if kind in ("arena", "ink"):
        if kind == "ink":
            return (f'<div class="hud ink"><div class="timer">{2 - v % 2}:{(seed * 11 + v * 7) % 60:02d}</div>'
                    f'<div class="split"><span style="width:{48 + 6 * v}%"></span></div>'
                    f'<div class="tank"><span style="height:{70 - 15 * v}%"></span></div><div class="pts">{score % 2000}p</div></div>')
        return (f'<div class="hud fight"><div class="fighter l"><b>P1</b><div class="pct">{12 + 34 * v}<small>%</small></div></div>'
                f'<div class="fighter r"><b>P2</b><div class="pct">{88 - 17 * v}<small>%</small></div></div><div class="clock">{3 - v % 3}:{(seed * 13) % 60:02d}</div></div>')
    if kind in ("farm", "village", "garden", "kitchen"):
        days = ["Spring", "Summer", "Autumn", "Winter"]
        return (f'<div class="hud life"><div class="date">{days[(seed + v) % 4]} {3 + 4 * v} · {["Mon", "Wed", "Fri", "Sun"][v % 4]}</div>'
                f'<div class="clock">{6 + 3 * v}:{(seed * 7) % 6}0</div><div class="gold">{(score * 3) % 90000:,} g</div>'
                f'<div class="bar"><div class="slot sel"></div><div class="slot"></div><div class="slot"></div><div class="slot"></div><div class="slot"></div><div class="slot"></div></div></div>')
    if kind in ("tactics", "paper"):
        return (f'<div class="hud tactics"><div class="turn">Turn {v + 2}</div><div class="phase">Your move</div>'
                f'<div class="unit"><b>Captain</b><div class="bar ok"><span style="width:{90 - 15 * v}%"></span></div><small>MOV 4 · ATK 12</small></div></div>')
    # Adventures and platformers.
    return (f'<div class="hud adventure"><div class="hearts">{hearts}</div><div class="stamina"><span style="height:{60 + 10 * v}%"></span></div>'
            f'<div class="coins">⬤ {score % 999:03d}</div><div class="map"><svg viewBox="0 0 100 100"><circle cx="50" cy="50" r="46" fill="#000" opacity="0.35"/>'
            f'<path d="M50,30 L58,58 L50,52 L42,58 Z" fill="#ffd23f"/></svg></div>'
            f'<div class="quest"><small>Quest</small>{["Find the lost lantern", "Reach the summit", "Open the old gate", "Ask the keeper"][v % 4]}</div></div>')


HUD_CSS = """
.hud{position:absolute;inset:0;font-family:Sora,sans-serif;color:#fff;text-shadow:0 2px 4px rgba(0,0,0,.6)}
.hud .map{position:absolute;right:28px;bottom:28px;width:150px;height:150px;border-radius:50%;background:rgba(10,14,30,.45);border:3px solid rgba(255,255,255,.85)}
.hud .map svg{width:100%;height:100%}
.race .place{position:absolute;left:34px;bottom:22px;font-size:110px;font-weight:700;color:#ffd23f;-webkit-text-stroke:6px #2b1a40;paint-order:stroke fill}
.race .place sup{font-size:42px}
.race .lap{position:absolute;left:36px;top:26px;font-size:30px}.race .lap b{font-size:44px}
.race .time{position:absolute;left:36px;top:84px;font-size:26px;opacity:.9}
.race .speed{position:absolute;right:200px;bottom:40px;font-size:24px;text-align:right}.race .speed b{font-size:44px}
.gauge{width:220px;height:12px;border-radius:6px;background:rgba(0,0,0,.4);margin-top:6px;overflow:hidden}.gauge span{display:block;height:100%;background:linear-gradient(90deg,#4fd1ff,#ffd23f,#ff5a4f)}
.battle .card{position:absolute;padding:14px 22px;background:rgba(255,255,255,.92);color:#2b2238;border-radius:16px;font-size:22px;text-shadow:none;width:300px;box-shadow:0 6px 18px rgba(0,0,0,.3)}
.battle .card span{float:right}.battle .foe{left:40px;top:34px}.battle .mine{right:40px;bottom:150px}
.battle .card small{display:block;text-align:right;font-size:18px}
.bar{height:12px;border-radius:6px;background:#3a3550;margin-top:10px;overflow:hidden}.bar span{display:block;height:100%;background:#ffb23e}.bar.ok span{background:#4cd97b}
.battle .menu{position:absolute;right:40px;bottom:30px;display:flex;gap:10px}
.battle .menu div{background:rgba(20,20,40,.75);padding:12px 22px;border-radius:12px;font-size:22px}.battle .menu .sel{background:#ff5a4f}
.fight .fighter{position:absolute;bottom:26px;width:230px;padding:10px 20px;border-radius:18px;background:rgba(0,0,0,.45)}
.fight .l{left:330px;border-bottom:6px solid #ff5a4f}.fight .r{right:330px;border-bottom:6px solid #4fa8ff}
.fight .pct{font-size:62px;font-weight:700}.fight .pct small{font-size:28px}.fight .clock{position:absolute;right:36px;top:26px;font-size:40px}
.ink .timer{position:absolute;left:50%;top:22px;transform:translateX(-50%);font-size:44px}
.ink .split{position:absolute;left:50%;top:84px;transform:translateX(-50%);width:420px;height:16px;border-radius:8px;background:#ff3fa4;overflow:hidden}
.ink .split span{display:block;height:100%;background:#3fffc2}
.ink .tank{position:absolute;right:40px;bottom:40px;width:46px;height:150px;border:4px solid #fff;border-radius:16px;overflow:hidden;display:flex;align-items:flex-end}
.ink .tank span{display:block;width:100%;background:#3fffc2}.ink .pts{position:absolute;right:110px;bottom:44px;font-size:40px}
.life .date{position:absolute;right:30px;top:24px;background:rgba(255,248,232,.92);color:#5a3d2b;text-shadow:none;padding:10px 20px;border-radius:14px;font-size:24px}
.life .clock{position:absolute;right:30px;top:84px;font-size:30px}.life .gold{position:absolute;right:30px;top:126px;font-size:26px;color:#ffd23f}
.life .bar{position:absolute;left:50%;bottom:26px;transform:translateX(-50%);display:flex;gap:10px;height:auto;background:none}
.life .slot{width:64px;height:64px;border-radius:12px;background:rgba(255,248,232,.85);border:3px solid rgba(90,61,43,.6)}.life .slot.sel{border-color:#ffd23f;box-shadow:0 0 0 3px #ffd23f}
.tactics .turn{position:absolute;left:36px;top:26px;font-size:40px}.tactics .phase{position:absolute;left:38px;top:80px;font-size:22px;color:#ffd23f}
.tactics .unit{position:absolute;left:36px;bottom:30px;width:300px;padding:14px 20px;background:rgba(20,16,40,.7);border-radius:16px;font-size:24px}
.tactics .unit small{display:block;margin-top:8px;font-size:18px;opacity:.85}
.adventure .hearts{position:absolute;left:30px;top:26px;display:flex;gap:8px}
.heart{width:34px;height:30px;background:#ff4f6d;clip-path:path('M17 30 L2 14 A8 8 0 0 1 17 5 A8 8 0 0 1 32 14 Z');display:block;filter:drop-shadow(0 2px 2px rgba(0,0,0,.5))}
.heart.empty{background:rgba(255,255,255,.35)}
.adventure .stamina{position:absolute;left:34px;top:72px;width:20px;height:90px;border-radius:10px;background:rgba(0,0,0,.35);display:flex;align-items:flex-end;overflow:hidden}
.adventure .stamina span{display:block;width:100%;background:#7dff9a}
.adventure .coins{position:absolute;right:36px;top:24px;font-size:34px;color:#ffd23f}
.adventure .quest{position:absolute;right:36px;top:80px;font-size:22px;text-align:right}.adventure .quest small{display:block;color:#ffd23f;font-size:16px;letter-spacing:.15em;text-transform:uppercase}
"""


HUD_OF = {
    "kart": "kart", "canyon": "canyon", "snow": "snow", "critters": "critters", "arena": "arena", "ink": "ink",
    "farm": "farm", "village": "village", "garden": "garden", "kitchen": "kitchen", "tactics": "tactics", "paper": "paper",
}
