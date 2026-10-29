#!/usr/bin/env python3
"""The simulators' demo (web/README.md), the part tools/web_demo.cpp
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
(tools/web_demo_scenes.py: an open sky of islands, a kart track, a harbour
village…) in the game's colours, as SVG rendered in Chromium; on the icon,
framed on the scene's subject, its name in Sora (resources/font, OFL) as a
logo; on the screenshots, a head-up display of its kind of game. Nothing of
a real game: no name, character, logo or picture. Needs Playwright (with
its Chromium, or CHROMIUM=<path> for another) and Pillow.

Usage: tools/web_demo_art.py <games.json> <demo-dir>
"""
import base64
import datetime
import io
import json
import os
import random
import sys

from PIL import Image

import web_demo_scenes as scenes

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FONT = os.path.join(ROOT, "resources", "font", "sora_bold_latin.ttf")
QUALITY = 78                # JPEG
SCREENS = 3                 # screenshots a game


def tid(index):
    """The title id of the table's game `index` (from 0), as web_demo.cpp makes it."""
    return 0x0100DE0000000000 | ((index + 1) << 16)


# ------------------------------------------------------------- the pages

PAGE = """<!doctype html><html><head><meta charset="utf-8"><style>
@font-face{{font-family:Sora;src:url(data:font/ttf;base64,{font}) format("truetype");font-weight:700}}
html,body{{margin:0;padding:0;background:#000;overflow:hidden}}
.stage{{position:relative;width:{w}px;height:{h}px;overflow:hidden}}
.stage>svg{{position:absolute;inset:0;width:100%;height:100%}}
.shade{{position:absolute;left:0;right:0;bottom:0;height:62%;background:linear-gradient(180deg,rgba(0,0,0,0) 0%,rgba(8,6,20,.55) 55%,rgba(8,6,20,.82) 100%)}}
.logo{{position:absolute;left:6%;right:6%;bottom:6.5%;text-align:center;font-family:Sora,sans-serif;font-weight:700;color:#fff}}
.kicker{{display:inline-block;font-size:{kick}px;letter-spacing:.18em;text-transform:uppercase;padding:.28em .8em .22em;border-radius:999px;
  background:{accent};color:#fff;text-shadow:0 1px 2px rgba(0,0,0,.35);margin-bottom:.45em;box-shadow:0 4px 14px rgba(0,0,0,.35)}}
.name{{font-size:{size}px;line-height:1.02;letter-spacing:-.01em;
  background:linear-gradient(180deg,#ffffff 0%,#ffffff 48%,{tint} 100%);-webkit-background-clip:text;background-clip:text;color:transparent;
  -webkit-text-stroke:{stroke}px rgba(20,12,40,.85);paint-order:stroke fill;filter:drop-shadow(0 {drop}px {blur}px rgba(0,0,0,.55))}}
{hud_css}
</style></head><body><div class="stage">{svg}{over}</div>
<script>
// The name as large as its box allows: smaller until it fits.
const n = document.querySelector('.name');
if (n) {{
  const box = document.querySelector('.logo');
  let size = parseFloat(getComputedStyle(n).fontSize);
  const wide = () => {{ const r = document.createRange(); r.selectNodeContents(n);
    return Math.max(...[...r.getClientRects()].map(c => c.width)); }};
  while (size > 12 && (wide() > box.clientWidth || n.getBoundingClientRect().height > {max_h})) {{
    size -= 2; n.style.fontSize = size + 'px';
  }}
}}
</script></body></html>"""


def font_data():
    with open(FONT, "rb") as f:
        return base64.b64encode(f.read()).decode()


def split_name(name):
    """A series' game: its series as a kicker over its own name."""
    if ": " in name:
        series, own = name.split(": ", 1)
        return series, own
    return "", name


def page(game, v, kind, w, h, font):
    """The HTML of one of the game's pictures: `kind` icon, banner or screen."""
    p = scenes.Palette(game["hues"])
    rnd = random.Random(f"{game['name']}/{v}")
    picture = scenes.svg(game["art"], p, rnd, v, "square" if kind == "icon" else "wide")
    over, hud_css = "", ""
    size = kick = stroke = drop = blur = max_h = 0
    if kind == "icon":
        series, own = split_name(game["name"])
        kicker = f'<div class="kicker">{series}</div><br>' if series else ""
        over = f'<div class="shade"></div><div class="logo">{kicker}<div class="name">{own}</div></div>'
        size, kick, stroke, drop, blur = int(w * 0.135), int(w * 0.042), max(2, w // 120), w // 100, w // 70
        max_h = int(h * (0.36 if series else 0.46))
    elif kind == "screen":
        over = scenes.hud(scenes.HUD_OF.get(game["art"], "adventure"), game["name"], sum(map(ord, game["name"])), v)
        hud_css = scenes.HUD_CSS
    return PAGE.format(font=font, w=w, h=h, svg=picture, over=over, hud_css=hud_css, size=size, kick=kick, stroke=stroke,
                       drop=drop, blur=blur, max_h=max_h, accent=p.accent, tint=p.light)


class Renderer:
    """Chromium, through Playwright: a page at a time, as a PNG."""

    def __init__(self):
        from playwright.sync_api import sync_playwright
        self.pw = sync_playwright().start()
        # CHROMIUM: a Chromium of one's own, when Playwright's is not installed.
        self.browser = self.pw.chromium.launch(executable_path=os.environ.get("CHROMIUM") or None)
        self.pages = {}

    def shot(self, html, w, h):
        if (w, h) not in self.pages:
            self.pages[(w, h)] = self.browser.new_page(viewport={"width": w, "height": h}, device_scale_factor=1)
        tab = self.pages[(w, h)]
        tab.set_content(html, wait_until="load")
        tab.evaluate("document.fonts.ready")
        return Image.open(io.BytesIO(tab.screenshot(clip={"x": 0, "y": 0, "width": w, "height": h}))).convert("RGB")

    def close(self):
        self.browser.close()
        self.pw.stop()


# ------------------------------------------------------------- the files

def save(img, path, size):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    if img.size != size:
        img = img.resize(size, Image.LANCZOS)
    img.save(path, "JPEG", quality=QUALITY, optimize=True, progressive=True)


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
    font = font_data()
    render = Renderer()
    for i, g in enumerate(games):
        gid = f"{tid(i):016X}"
        release = release_of(g, today)
        # The pictures: the banner the key art, bare (PlayShelf writes the
        # name over it), the icon its middle with the name, the screenshots
        # other moments of it with their game's head-up display.
        banner = render.shot(page(g, 0, "banner", 1280, 720, font), 1280, 720)
        save(banner, f"{out}/nlib/{gid}/banner.jpg", (854, 480))
        icon = render.shot(page(g, 0, "icon", 512, 512, font), 512, 512)
        save(icon, f"{out}/icons/{gid}.jpg", (256, 256))
        save(icon, f"{out}/nlib/{gid}/icon.jpg", (256, 256))
        if g.get("installed"):
            save(icon, f"{out}/console/control/{gid.lower()}.jpg", (256, 256))
        screens = SCREENS if g.get("on_server", True) else 2
        for v in range(1, screens + 1):
            shot = render.shot(page(g, v, "screen", 1280, 720, font), 1280, 720)
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
    render.close()
    os.makedirs(f"{out}/titledb", exist_ok=True)
    with open(f"{out}/titledb/index.json", "w", encoding="utf-8") as f:
        json.dump({"titledb": titledb}, f, ensure_ascii=False)
    size = sum(os.path.getsize(os.path.join(r, n)) for r, _, ns in os.walk(out) for n in ns
               if n.endswith((".jpg", ".json")) and "/files" not in r)
    print(f"web_demo_art: {len(games)} games, {size // 1024} KB of pictures and answers, in {out}")


if __name__ == "__main__":
    main()
