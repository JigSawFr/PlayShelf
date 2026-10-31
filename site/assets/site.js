// PlayShelf's website (site/README.md): the lightbox, the search, the copy
// buttons. The aurora, the header's row of links, the cards' glow and the
// reveal are boreale-site's chrome.js. The language (kept from the header's
// menu, a first visit in the browser's, the page not found's text) is its
// lang.js, where the header's two menus open its boreale.js, loaded beside it.
'use strict';

(function () {
  // The way back to the top, far down any page.
  const toTop = document.querySelector('.to-top');
  let ticking = false;
  const scrolled = () => {
    ticking = false;
    toTop.hidden = scrollY < innerHeight * 2;
  };
  addEventListener('scroll', () => { if (!ticking) { ticking = true; requestAnimationFrame(scrolled); } }, { passive: true });

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

// The showcase (site.css's block of the same name): the screens on a wide
// screen, the one chosen large over a strip of the others.
(function () {
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
