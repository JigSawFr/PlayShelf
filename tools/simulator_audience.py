#!/usr/bin/env python3
"""Counts the web simulator's visits as the website's (site/README.md, "The
audience"): adds Umami's tracker, served by the site (../assets/umami.js), to
the published simulator's page, and lets that page send to Umami
(connect-src). Run on the site being published, after the simulator is copied
in; a pull request's preview (SITE_NOINDEX=1) is left without it, as its
pages are, and the tracker only counts on the site's own address.

    python3 tools/simulator_audience.py <site-dir>/simulator
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import build_site  # noqa: E402


def main() -> None:
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    if build_site.NOINDEX:
        print("simulator_audience: a preview, no tracker")
        return
    page = Path(sys.argv[1]) / "index.html"
    html = page.read_text(encoding="utf-8")
    csp = "connect-src 'self';"
    tag = (f'  <script src="../assets/umami.js" defer data-website-id="{build_site.UMAMI_WEBSITE}" '
           f'data-domains="{build_site.UMAMI_DOMAIN}"></script>\n</head>')
    if html.count(csp) != 1 or html.count("</head>") != 1:
        sys.exit(f"simulator_audience: {page} has not the one connect-src and </head> expected")
    html = html.replace(csp, "connect-src 'self' https://gateway.umami.is;").replace("</head>", tag)
    page.write_text(html, encoding="utf-8")
    print(f"simulator_audience: {page}, counted on {build_site.UMAMI_DOMAIN}")


if __name__ == "__main__":
    main()
