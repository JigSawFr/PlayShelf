// PlayShelf's website (site/README.md): the aurora of the home page, the
// language menu of the header, and the home page's first visit in the
// browser's language. One dark look, Boréale's, as Aurore's and boreale.dev.
'use strict';

// The aurora behind the home page's header and hero (canvas#sky), boreale.dev's
// own (boreale/site/assets/site.js): three soft curtains in the brand's
// colours under a few stars; they move only while the sky is in view, the
// page visible and the visitor has not asked for less motion. Without
// script, site.css's gradient stands in.
(function () {
  const canvas = document.getElementById('sky');
  if (!canvas || !canvas.getContext) return;
  const ctx = canvas.getContext('2d');
  // The curtains are drawn here at a quarter of the size, then laid on the
  // sky blurred and scaled up in one go: blurring each strip would be slow.
  const glow = document.createElement('canvas');
  const gtx = glow.getContext('2d');
  const still = window.matchMedia('(prefers-reduced-motion: reduce)');
  const curtains = [
    { rgb: [0, 255, 204], y: 0.34, amp: 0.08, freq: 1.2, speed: 0.00012, height: 0.42, alpha: 0.34 },
    { rgb: [51, 187, 255], y: 0.26, amp: 0.06, freq: 1.7, speed: -0.00009, height: 0.34, alpha: 0.26 },
    { rgb: [138, 92, 255], y: 0.18, amp: 0.05, freq: 0.9, speed: 0.00007, height: 0.3, alpha: 0.26 },
  ];
  let w = 0, h = 0, stars = [];

  function size() {
    const ratio = Math.min(window.devicePixelRatio || 1, 2);
    w = canvas.clientWidth; h = canvas.clientHeight;
    canvas.width = Math.max(1, Math.round(w * ratio));
    canvas.height = Math.max(1, Math.round(h * ratio));
    ctx.setTransform(ratio, 0, 0, ratio, 0, 0);
    glow.width = Math.max(1, Math.round(w / 4));
    glow.height = Math.max(1, Math.round(h / 4));
    let seed = 7; // the same stars at each load
    const random = () => (seed = (seed * 16807) % 2147483647) / 2147483647;
    stars = Array.from({ length: Math.round((w * h) / 2600) }, () => ({
      x: random() * w, y: random() * h * 0.75, r: random() * 1.1 + 0.2, a: random() * 0.6 + 0.2,
    }));
  }

  function draw(t) {
    const gw = glow.width, gh = glow.height;
    gtx.globalCompositeOperation = 'source-over';
    gtx.clearRect(0, 0, gw, gh);
    gtx.globalCompositeOperation = 'lighter';
    const step = Math.max(2, gw / 120);
    for (const c of curtains) {
      const [r, g, b] = c.rgb;
      const phase = t * c.speed;
      for (let x = -step; x < gw + step; x += step) {
        const u = x / gw;
        const base = gh * (c.y + c.amp * Math.sin((u * c.freq + phase) * 2 * Math.PI) + 0.03 * Math.sin(u * 17 + phase * 20));
        const length = gh * c.height * (0.6 + 0.4 * Math.sin(u * 9 + phase * 11));
        const flicker = 0.65 + 0.35 * Math.sin(u * 31 + phase * 40);
        const fill = gtx.createLinearGradient(0, base - length, 0, base);
        fill.addColorStop(0, `rgba(${r}, ${g}, ${b}, 0)`);
        fill.addColorStop(0.75, `rgba(${r}, ${g}, ${b}, ${c.alpha * flicker * 0.6})`);
        fill.addColorStop(1, `rgba(${r}, ${g}, ${b}, ${c.alpha * flicker})`);
        gtx.fillStyle = fill;
        gtx.fillRect(x, base - length, step + 1, length);
      }
    }

    ctx.fillStyle = '#0b1424';
    ctx.fillRect(0, 0, w, h);
    ctx.filter = 'blur(8px)'; // where the browser can; scaling up softens it anyway
    ctx.drawImage(glow, 0, 0, w, h);
    ctx.filter = 'none';
    for (const s of stars) {
      ctx.fillStyle = `rgba(232, 238, 247, ${s.a})`;
      ctx.beginPath(); ctx.arc(s.x, s.y, s.r, 0, 2 * Math.PI); ctx.fill();
    }
  }

  // The curtains drift slowly: 30 frames a second is plenty, and half the
  // work on a 60 or 120Hz screen. Nothing is drawn while the sky is scrolled
  // out of view, the page hidden, or less motion asked.
  const FRAME = 1000 / 30;
  let frame = 0, last = 0, seen = true;
  function loop(t) {
    if (t - last >= FRAME - 1) { last = t; draw(t); }
    frame = requestAnimationFrame(loop);
  }
  function start() {
    cancelAnimationFrame(frame);
    if (still.matches || document.hidden || !seen) draw(last);
    else frame = requestAnimationFrame(loop);
  }

  size();
  start();
  let resizing = 0; // once per frame, however many resize events come
  window.addEventListener('resize', () => {
    cancelAnimationFrame(resizing);
    resizing = requestAnimationFrame(() => { size(); draw(last); });
  });
  document.addEventListener('visibilitychange', start);
  still.addEventListener('change', start);
  if ('IntersectionObserver' in window) {
    new IntersectionObserver((entries) => {
      seen = entries[entries.length - 1].isIntersecting;
      start();
    }).observe(canvas);
  }
})();

(function () {
  const store = {
    get: (k) => { try { return localStorage.getItem(k); } catch { return null; } },
    set: (k, v) => { try { localStorage.setItem(k, v); } catch { /* storage refused */ } },
  };

  // The way back to the top, far down any page.
  const toTop = document.querySelector('.to-top');
  let ticking = false;
  const scrolled = () => {
    ticking = false;
    toTop.hidden = scrollY < innerHeight * 2;
  };
  addEventListener('scroll', () => { if (!ticking) { ticking = true; requestAnimationFrame(scrolled); } }, { passive: true });

  // The header's links on a phone, a row to scroll: its edge fades while
  // there are more of them to scroll to.
  const nav = document.querySelector('.nav');
  const fade = () => nav.classList.toggle('more', nav.scrollLeft + nav.clientWidth < nav.scrollWidth - 2);
  nav.addEventListener('scroll', fade, { passive: true });
  addEventListener('resize', fade);
  fade();

  // The gallery on a phone, a row to scroll: a dot for each screen, the one
  // in view lit.
  const gallery = document.querySelector('.gallery');
  if (gallery && 'IntersectionObserver' in window) {
    const figures = [...gallery.querySelectorAll(':scope > figure:not(.gallery-stage)')];
    const dots = document.createElement('div');
    dots.className = 'gallery-dots';
    dots.setAttribute('aria-hidden', 'true');
    dots.append(...figures.map(() => document.createElement('span')));
    gallery.after(dots);
    const sync = () => { dots.hidden = gallery.scrollWidth <= gallery.clientWidth; };
    addEventListener('resize', sync);
    sync();
    const seen = new IntersectionObserver((entries) => {
      for (const e of entries) {
        if (e.isIntersecting) {
          for (const d of dots.children) d.classList.remove('on');
          dots.children[figures.indexOf(e.target)].classList.add('on');
        }
      }
    }, { root: gallery, threshold: 0.6 });
    for (const f of figures) seen.observe(f);
  }

  // A table wider than its column scrolls on its own: reachable with the
  // keyboard (Tab, then the arrows) while it does.
  const tables = [...document.querySelectorAll('.table')];
  const scrollable = () => {
    for (const t of tables) {
      if (t.scrollWidth > t.clientWidth + 1) t.tabIndex = 0;
      else t.removeAttribute('tabindex');
    }
  };
  if (tables.length) {
    addEventListener('resize', scrollable);
    scrollable();
  }

  // What the simulator downloads, on its buttons: "· 4 MB" in the page's
  // language (build_site.py's simulator_size()).
  for (const size of document.querySelectorAll('.size[data-bytes]')) {
    try {
      const mb = Math.max(1, Math.round(Number(size.dataset.bytes) / 1e6));
      size.textContent = `· ${new Intl.NumberFormat(document.documentElement.lang, { style: 'unit', unit: 'megabyte' }).format(mb)}`;
    } catch { /* no size then */ }
  }

  // Tabs: the panels' names become buttons, one panel shown at a time;
  // without this script, every panel under its name.
  for (const tabs of document.querySelectorAll('.tabs')) {
    const panels = [...tabs.querySelectorAll(':scope > .tab-panel')];
    const list = document.createElement('div');
    list.className = 'tab-list';
    list.setAttribute('role', 'tablist');
    const buttons = panels.map((panel, i) => {
      const title = panel.querySelector('.tab-title');
      const button = document.createElement('button');
      button.type = 'button';
      button.id = `${panel.id}-tab`;
      button.textContent = title.textContent;
      button.setAttribute('role', 'tab');
      button.setAttribute('aria-controls', panel.id);
      panel.setAttribute('role', 'tabpanel');
      panel.setAttribute('aria-labelledby', button.id);
      title.hidden = true;
      button.addEventListener('click', () => select(i, false));
      return button;
    });
    const select = (n, focus) => {
      buttons.forEach((b, i) => {
        b.setAttribute('aria-selected', String(i === n));
        b.tabIndex = i === n ? 0 : -1;
        panels[i].hidden = i !== n;
      });
      if (focus) buttons[n].focus();
    };
    // The arrows from one tab to the next, as for any tab list.
    list.addEventListener('keydown', (e) => {
      const i = buttons.indexOf(document.activeElement);
      if (e.key === 'ArrowRight') select((i + 1) % buttons.length, true);
      if (e.key === 'ArrowLeft') select((i - 1 + buttons.length) % buttons.length, true);
    });
    list.append(...buttons);
    tabs.prepend(list);
    select(0, false);
  }

  // Dates (a release's) as the page's language writes them.
  for (const time of document.querySelectorAll('time[datetime]')) {
    const date = new Date(`${time.dateTime}T12:00:00`);
    if (!Number.isNaN(date.getTime())) {
      try {
        time.textContent = new Intl.DateTimeFormat(document.documentElement.lang, { dateStyle: 'long' }).format(date);
      } catch { /* the ISO date stays */ }
    }
  }

  // A link to a question or an older release opens it; a translated page's
  // section answers to its English anchor too (data-en: the app's links).
  const reveal = () => {
    let target = null, english = false;
    try {
      const id = location.hash ? decodeURIComponent(location.hash.slice(1)) : '';
      target = id ? document.getElementById(id) : null;
      if (id && !target) {
        target = document.querySelector(`[data-en="${CSS.escape(id)}"]`);
        english = !!target;
      }
    } catch { /* a bad address */ }
    const folded = target?.closest('details');
    const opened = folded && !folded.open;
    if (opened) folded.open = true;
    if (opened || english) target.scrollIntoView();
  };
  addEventListener('hashchange', reveal);
  reveal();

  // A screenshot larger (the gallery's, the documentation's): the others of
  // its row with the arrows or a swipe, down or Esc to close.
  const box = document.querySelector('.lightbox');
  if (box && typeof box.showModal === 'function') {
    const big = box.querySelector('img');
    const caption = box.querySelector('figcaption');
    const back = box.querySelector('.lightbox-prev');
    const forward = box.querySelector('.lightbox-next');
    let group = [];
    let at = 0;
    const show = () => {
      const figure = group[at];
      // The screenshot shown (a page could have more than one image).
      const img = [...figure.querySelectorAll('img')].find((i) => getComputedStyle(i).display !== 'none');
      big.src = img.currentSrc || img.src;
      big.alt = img.alt;
      caption.textContent = figure.querySelector('figcaption')?.textContent || '';
      back.hidden = at === 0;
      forward.hidden = at === group.length - 1;
    };
    const step = (by) => { if (group[at + by]) { at += by; show(); } };
    const open = (figure) => {
      // The gallery's large screen is the one chosen in its strip.
      if (figure.classList.contains('gallery-stage')) figure = figure.parentElement.querySelector(':scope > figure.on');
      group = [...figure.parentElement.children].filter((f) => f.tagName === 'FIGURE' && !f.classList.contains('gallery-stage'));
      at = group.indexOf(figure);
      show();
      box.showModal();
    };
    document.addEventListener('click', (e) => {
      const figure = e.target.closest('.gallery figure, .shots figure');
      if (!figure || !e.target.closest('a, img') || e.metaKey || e.ctrlKey || e.shiftKey) return;
      e.preventDefault();
      open(figure);
    });
    // The documentation's screenshots are not links: a key opens them too.
    for (const img of document.querySelectorAll('.shots img')) {
      img.tabIndex = 0;
      img.setAttribute('role', 'button');
      img.addEventListener('keydown', (e) => {
        if (e.key === 'Enter' || e.key === ' ') { e.preventDefault(); open(img.closest('figure')); }
      });
    }
    back.addEventListener('click', () => step(-1));
    forward.addEventListener('click', () => step(1));
    box.querySelector('.lightbox-close').addEventListener('click', () => box.close());
    // A touch beside the screenshot closes it.
    box.addEventListener('click', (e) => { if (e.target === box || e.target.tagName === 'FIGURE') box.close(); });
    box.addEventListener('keydown', (e) => {
      if (e.key === 'ArrowLeft') step(-1);
      if (e.key === 'ArrowRight') step(1);
    });
    let touch = null;
    box.addEventListener('touchstart', (e) => { touch = e.touches.length === 1 ? e.touches[0] : null; }, { passive: true });
    box.addEventListener('touchend', (e) => {
      if (!touch) return;
      const dx = e.changedTouches[0].clientX - touch.clientX;
      const dy = e.changedTouches[0].clientY - touch.clientY;
      touch = null;
      if (Math.abs(dx) > 50 && Math.abs(dx) > Math.abs(dy)) step(dx < 0 ? 1 : -1);
      else if (dy > 80) box.close();
    });
  }

  // A copy button on each block of code (a path, a config.json).
  const prose = document.querySelector('.prose[data-copy]');
  if (prose && navigator.clipboard) {
    for (const pre of prose.querySelectorAll('pre')) {
      const button = document.createElement('button');
      button.type = 'button';
      button.className = 'copy';
      button.textContent = prose.dataset.copy;
      button.addEventListener('click', () => {
        navigator.clipboard.writeText(pre.querySelector('code')?.textContent ?? pre.textContent).then(() => {
          button.textContent = prose.dataset.copied;
          setTimeout(() => { button.textContent = prose.dataset.copy; }, 1500);
        }, () => {});
      });
      pre.classList.add('has-copy');
      pre.append(button);
    }
  }

  // Printed: every folded part open (the questions, a page's details).
  addEventListener('beforeprint', () => {
    for (const d of document.querySelectorAll('details:not(.toc-mobile)')) d.open = true;
  });

  // The changelog: what changed since the version typed (a console's), the
  // releases after it shown and open, the others hidden.
  const since = document.querySelector('.since');
  if (since) {
    const field = since.querySelector('input');
    const result = since.querySelector('.since-result');
    const parse = (v) => (/^\s*v?(\d{1,3})(?:\.(\d{1,3}))?(?:\.(\d{1,3}))?\s*$/i.exec(v) || []).slice(1).map((n) => Number(n || 0));
    const order = ([a, b, c]) => a * 1e6 + b * 1e3 + c;
    const releases = [...document.querySelectorAll('[data-version]')];
    const older = document.getElementById('older');
    const latest = Math.max(...releases.map((r) => order(parse(r.dataset.version))));
    field.addEventListener('input', () => {
      const v = parse(field.value);
      const known = v.length && order(v) <= latest;
      let after = 0;
      for (const r of releases) {
        const newer = !known || order(parse(r.dataset.version)) > order(v);
        r.hidden = !newer;
        if (newer && known) after++;
        if (r.tagName === 'DETAILS') r.open = Boolean(known && newer);
      }
      if (older) older.hidden = Boolean(known) && !releases.some((r) => r.tagName === 'DETAILS' && !r.hidden);
      result.textContent = !field.value.trim() ? '' : !known ? since.dataset.unknown
        : after ? since.dataset.many.replace('{n}', after) : since.dataset.latest;
    });
    since.hidden = false;
  }

  // On a phone, the documentation's pages are a row to scroll: this one in view.
  const current = document.querySelector('.doc-nav [aria-current]');
  const pages = current?.closest('.doc-list');
  if (pages && pages.scrollWidth > pages.clientWidth) {
    pages.scrollLeft = current.offsetLeft - (pages.clientWidth - current.offsetWidth) / 2;
  }

  // The documentation's search: docs/search.json (the language's pages, a
  // section an entry), fetched on the first key. Every word typed must be in
  // the section; one in its heading counts more.
  const search = document.querySelector('.doc-search');
  if (search) {
    const input = search.querySelector('input');
    const list = search.querySelector('.doc-results');
    const indexUrl = new URL(input.dataset.index, location.href);
    let index = null;
    const fold = (s) => s.toLowerCase().normalize('NFD').replace(/[\u0300-\u036f]/g, '');
    // A text with the words found in it marked, accents or not.
    const marked = (tag, text, words) => {
      const el = document.createElement(tag);
      let folded = '';
      const from = [];
      for (let i = 0; i < text.length; i++) {
        const f = fold(text[i]);
        folded += f;
        for (let k = 0; k < f.length; k++) from.push(i);
      }
      const spans = [];
      for (const w of words) {
        for (let at = folded.indexOf(w); at >= 0; at = folded.indexOf(w, at + w.length)) {
          spans.push([from[at], from[at + w.length - 1] + 1]);
        }
      }
      spans.sort((a, b) => a[0] - b[0]);
      let done = 0;
      for (const [start, end] of spans) {
        if (start < done) continue;
        el.append(text.slice(done, start));
        const m = document.createElement('mark');
        m.textContent = text.slice(start, end);
        el.append(m);
        done = end;
      }
      el.append(text.slice(done));
      return el;
    };
    const show = () => {
      const words = fold(input.value).split(/\s+/).filter(Boolean);
      list.replaceChildren();
      list.hidden = !words.length || !index;
      if (list.hidden) return;
      const hits = [];
      for (const e of index) {
        const heading = fold(`${e.p} ${e.h}`);
        const text = fold(e.t);
        if (!words.every((w) => heading.includes(w) || text.includes(w))) continue;
        hits.push([words.reduce((n, w) => n + (heading.includes(w) ? 3 : 1), 0), e, text.indexOf(words[0])]);
      }
      hits.sort((a, b) => b[0] - a[0]);
      for (const [, e, at] of hits.slice(0, 8)) {
        const a = document.createElement('a');
        a.href = new URL(`../${e.u}`, indexUrl).href;
        const title = marked('strong', e.h ? `${e.p} › ${e.h}` : e.p, words);
        const from = Math.max(0, at - 40);
        const extract = marked('span', (from ? '…' : '') + e.t.slice(from, from + 120) + '…', words);
        a.append(title, extract);
        const li = document.createElement('li');
        li.append(a);
        list.append(li);
      }
      if (!hits.length) {
        const li = document.createElement('li');
        li.className = 'none';
        li.textContent = input.dataset.none;
        list.append(li);
      }
    };
    input.addEventListener('input', () => {
      if (index) return show();
      fetch(indexUrl).then((r) => r.json()).then((data) => { index = data; show(); }).catch(() => {});
    });
    input.addEventListener('keydown', (e) => {
      if (e.key === 'Escape') { input.value = ''; show(); }
      if (e.key === 'Enter') list.querySelector('a')?.click();
      if (e.key === 'ArrowDown') { e.preventDefault(); list.querySelector('a')?.focus(); }
    });
    // Up and down through the results, back to the box from the first.
    list.addEventListener('keydown', (e) => {
      const links = [...list.querySelectorAll('a')];
      const i = links.indexOf(document.activeElement);
      if (i < 0) return;
      if (e.key === 'ArrowDown' && links[i + 1]) { e.preventDefault(); links[i + 1].focus(); }
      if (e.key === 'ArrowUp') { e.preventDefault(); (links[i - 1] || input).focus(); }
      if (e.key === 'Escape') { input.focus(); }
    });
    // Closed when one leaves it (a phone shows the results over the page).
    document.addEventListener('click', (e) => { if (!search.contains(e.target)) list.hidden = true; });
    input.addEventListener('focus', () => { if (input.value.trim() && index) show(); });
    // / searches, as on many sites, unless one is typing somewhere.
    document.addEventListener('keydown', (e) => {
      if (e.key !== '/' || e.metaKey || e.ctrlKey || e.altKey) return;
      if (e.target.closest('input, textarea, select, [contenteditable]')) return;
      e.preventDefault();
      input.focus();
    });
    search.hidden = false;
  }

  // The header's languages (a popover): the one chosen is kept, and the
  // page opens at the same place in it.
  const languages = [...document.querySelectorAll('#languages a')];
  for (const link of languages) {
    link.addEventListener('click', (e) => {
      if (e.metaKey || e.ctrlKey || e.shiftKey) return;
      e.preventDefault();
      store.set('pg-site-lang', link.dataset.lang);
      location.href = link.getAttribute('href') + location.hash;
    });
  }

  const options = languages.map((a) => [a.dataset.lang, a.getAttribute('href')]);
  const find = (code) => options.find(([c]) => c.toLowerCase() === code.toLowerCase());
  // The visitor's language: the one chosen before, else the browser's when
  // the site has it (null: English).
  const visitor = () => {
    const chosen = store.get('pg-site-lang');
    if (chosen) return find(chosen);
    for (const tag of navigator.languages || [navigator.language]) {
      const t = tag.toLowerCase();
      const [base, region = ''] = t.split('-');
      let code = null;
      if (base === 'en') return null;
      if (base === 'fr') code = region === 'ca' ? 'fr-CA' : 'fr';
      else if (base === 'es') code = !region || region === 'es' ? 'es' : 'es-419';
      else if (base === 'pt') code = region === 'br' ? 'pt-BR' : 'pt';
      else if (base === 'zh') code = ['tw', 'hk', 'mo', 'hant'].includes(region) || t.includes('hant') ? 'zh-Hant' : 'zh-Hans';
      else if (['de', 'it', 'nl', 'ru', 'ja', 'ko'].includes(base)) code = base;
      if (code) return find(code);
    }
    return null;
  };

  // The page not found (404.html, at any address): its text in the language
  // of the address' folder (/fr/…), else the visitor's.
  if (document.body.classList.contains('not-found-page')) {
    const folder = options.find(([c, v]) => c !== 'en-US' && location.pathname.toLowerCase().startsWith(new URL(v, location.href).pathname.toLowerCase()));
    const code = (folder || visitor() || ['en-US'])[0];
    const block = document.querySelector(`.not-found[data-lang="${code}"]`);
    if (block) {
      for (const b of document.querySelectorAll('.not-found')) b.hidden = b !== block;
      document.documentElement.lang = block.lang;
      document.title = `${block.querySelector('h1').textContent} · PlayShelf`;
    }
    return;
  }

  // The English home page (the site's root), on a first visit from elsewhere:
  // the browser's language when the site has it, unless one was chosen before.
  const html = document.documentElement;
  if (html.dataset.lang !== 'en-US' || !document.body.classList.contains('home')) return;
  if (document.referrer && new URL(document.referrer).origin === location.origin) return;
  const target = visitor();
  if (target && target[0] !== 'en-US') location.replace(target[1] + location.hash);
})();

// Boréale's site switcher: the menu of Boréale's sites opens under the name
// beside its button, wherever the header is. The same block in boreale.dev's,
// PlayGuard's, PlayShelf's and Aurore's site.js; boreale's is the reference.
// Without popovers (an old browser), the name still leads home.
(function () {
  const menu = document.getElementById('sites');
  const anchor = menu && menu.closest('.switcher');
  if (!anchor || !menu.showPopover) return;
  menu.addEventListener('beforetoggle', (event) => {
    if (event.newState !== 'open') return;
    const box = anchor.getBoundingClientRect();
    const screen = document.documentElement.clientWidth;
    const width = Math.min(400, screen - 24); // site.css's width
    menu.style.top = `${box.bottom + window.scrollY + 6}px`;
    menu.style.left = `${Math.max(12, Math.min(box.left, screen - width - 12)) + window.scrollX}px`;
  });
})();

// The header's languages: their menu opens under its button, its right edge
// on the button's.
(function () {
  const menu = document.getElementById('languages');
  const button = document.querySelector('.lang-button');
  if (!menu || !button || !menu.showPopover) return;
  menu.addEventListener('beforetoggle', (event) => {
    if (event.newState !== 'open') return;
    const box = button.getBoundingClientRect();
    const screen = document.documentElement.clientWidth;
    menu.style.top = `${box.bottom + window.scrollY + 6}px`;
    menu.style.right = `${Math.max(12, screen - box.right)}px`;
    // The language in use in view, in a long list on a short screen.
    requestAnimationFrame(() => menu.querySelector('[aria-current]')?.scrollIntoView({ block: 'nearest' }));
  });
})();

// The documentation: once the header is scrolled away, a slim bar at the top
// (the way home, the page's name, the search, the light reading); and the
// light reading itself, kept as ps-reading (reading.js sets it before the
// first paint).
(function () {
  const root = document.documentElement;
  const toggles = [...document.querySelectorAll('.reading-toggle')];
  const shown = () => toggles.forEach((b) => b.setAttribute('aria-pressed', String(root.dataset.reading === 'light')));
  for (const b of toggles) {
    b.addEventListener('click', () => {
      const light = root.dataset.reading !== 'light';
      if (light) root.dataset.reading = 'light'; else delete root.dataset.reading;
      try { localStorage.setItem('ps-reading', light ? 'light' : 'night'); } catch { /* storage refused */ }
      shown();
    });
  }
  shown();

  const bar = document.querySelector('.doc-bar');
  const sky = document.querySelector('.sky');
  if (!bar || !sky || !('IntersectionObserver' in window)) return;
  bar.hidden = false;
  new IntersectionObserver((entries) => {
    bar.classList.toggle('on', !entries[entries.length - 1].isIntersecting);
  }).observe(sky);
  bar.querySelector('.doc-bar-search').addEventListener('click', () => {
    const input = document.querySelector('.doc-search input');
    if (!input) return;
    window.scrollTo({ top: 0 });
    input.focus();
  });
})();

// The showcase (site.css's block of the same name): the cards' edge lit
// where the pointer is, the home page's sections rising into view once, and
// the screens on a wide screen, the one chosen large over a strip of the others.
(function () {
  for (const card of document.querySelectorAll('.card')) {
    card.addEventListener('pointermove', (e) => {
      const box = card.getBoundingClientRect();
      card.style.setProperty('--mx', `${e.clientX - box.left}px`);
      card.style.setProperty('--my', `${e.clientY - box.top}px`);
    });
  }

  const still = matchMedia('(prefers-reduced-motion: reduce)').matches;
  if (!still && 'IntersectionObserver' in window) {
    const parts = document.querySelectorAll('.home main > .section, .home main > .band');
    const seen = new IntersectionObserver((entries) => {
      for (const e of entries) {
        if (e.isIntersecting) { e.target.classList.add('in'); seen.unobserve(e.target); }
      }
    }, { rootMargin: '0px 0px -12% 0px' });
    for (const part of parts) {
      // Already in view at load (a link to a section): shown at once.
      if (part.getBoundingClientRect().top < innerHeight * 0.88) continue;
      part.classList.add('reveal');
      [...part.querySelectorAll('.card')].forEach((card, i) => card.style.setProperty('--i', i % 5));
      seen.observe(part);
    }
  }

  const gallery = document.querySelector('.gallery');
  if (!gallery) return;
  const figures = [...gallery.querySelectorAll(':scope > figure')];
  if (figures.length < 2) return;
  const stage = document.createElement('figure');
  stage.className = 'gallery-stage';
  const link = document.createElement('a');
  const img = document.createElement('img');
  img.width = 1280;
  img.height = 720;
  link.append(img);
  const caption = document.createElement('figcaption');
  const name = document.createElement('strong');
  const what = document.createElement('span');
  caption.append(name, what);
  stage.append(link, caption);
  const choose = (figure) => {
    const shot = figure.querySelector('img');
    for (const f of figures) f.classList.toggle('on', f === figure);
    link.href = figure.querySelector('a').href;
    img.src = shot.currentSrc || shot.src;
    img.alt = shot.alt;
    name.textContent = figure.querySelector('figcaption')?.textContent || '';
    // The alt says the screen's name first: once is enough.
    const rest = shot.alt.startsWith(name.textContent) ? shot.alt.slice(name.textContent.length).replace(/^\s*[:：]\s*/, '') : shot.alt;
    what.textContent = rest.charAt(0).toUpperCase() + rest.slice(1);
  };
  choose(figures[0]);
  gallery.style.setProperty('--count', figures.length);
  gallery.prepend(stage);
  gallery.classList.add('staged');
  const wide = matchMedia('(min-width: 861px)');
  // On a wide screen, a screen of the strip comes up large; elsewhere (the
  // row of a phone) it opens full screen, as before.
  gallery.addEventListener('click', (e) => {
    const figure = e.target.closest('figure');
    if (!wide.matches || !figure || figure === stage || e.metaKey || e.ctrlKey || e.shiftKey) return;
    e.preventDefault();
    e.stopPropagation();
    choose(figure);
  });
})();
