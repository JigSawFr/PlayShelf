// The web simulator's page logic that touches neither the page nor PlayShelf
// (web/README.md): which of the console's languages a browser's is, the URL
// a set of settings makes, the PLAYSHELF_SIM_* variables and config.json
// PlayShelf starts with, the steps a link replays, the database that keeps
// its data folder. simulator.js uses it as
// `SimLogic`; tests/web_page/ tests it with Node
// (`node --test tests/web_page/*.test.js`).
// `known` is params.json's entries by name (a Map).
'use strict';

(function (root) {
  // The console's languages, as PlayShelf's config.json names them, in their own words.
  const LANGUAGES = [
    ['en-US', 'English'], ['fr', 'Français'], ['fr-CA', 'Français (Canada)'], ['de', 'Deutsch'],
    ['es', 'Español'], ['es-419', 'Español (Latinoamérica)'], ['it', 'Italiano'], ['nl', 'Nederlands'],
    ['pt', 'Português'], ['pt-BR', 'Português (Brasil)'], ['ru', 'Русский'], ['ja', '日本語'],
    ['ko', '한국어'], ['zh-Hans', '简体中文'], ['zh-Hant', '繁體中文'],
  ];
  // Each language's flag in the header's menu, as on the website
  // (boreale-site's FLAGS; twemoji-flags.woff2 draws them the same
  // everywhere, Windows included): the country of the console's language,
  // Mexico for Latin America's Spanish, Taiwan for Traditional Chinese.
  const FLAGS = {
    'en-US': 'US', fr: 'FR', 'fr-CA': 'CA', de: 'DE', es: 'ES', 'es-419': 'MX', it: 'IT', nl: 'NL',
    pt: 'PT', 'pt-BR': 'BR', ru: 'RU', ja: 'JP', ko: 'KR', 'zh-Hans': 'CN', 'zh-Hant': 'TW',
  };
  // A language's flag: its country's two regional indicator letters.
  const flag = (code) => [...(FLAGS[code] || '')].map((c) => String.fromCodePoint(0x1F1E6 + c.charCodeAt(0) - 65)).join('');
  // A setting's name in the URL (lower case, digits, _).
  const NAME = /^[a-z0-9_]+$/;
  // A flag is off with these (core/sim_env.h's sim_env_flag() reads the same).
  const FALSY = new Set(['', '0', 'false', 'off', 'no']);
  const isOn = (value) => value != null && !FALSY.has(String(value).toLowerCase());

  // A language tag (the URL's, or the browser's) to one of the console's; null when none fits.
  function consoleLanguage(tag) {
    const t = String(tag || '').toLowerCase();
    const exact = LANGUAGES.find(([code]) => code.toLowerCase() === t);
    if (exact) return exact[0];
    const [base, region = ''] = t.split('-');
    switch (base) {
      case 'en': return 'en-US';
      case 'fr': return region === 'ca' ? 'fr-CA' : 'fr';
      case 'es': return !region || region === 'es' ? 'es' : 'es-419';
      case 'pt': return region === 'br' ? 'pt-BR' : 'pt';
      case 'zh': return ['tw', 'hk', 'mo', 'hant'].includes(region) || t.includes('hant') ? 'zh-Hant' : 'zh-Hans';
      case 'de': case 'it': case 'nl': case 'ru': case 'ja': case 'ko': return base;
      default: return null;
    }
  }

  // The first of `tags` (the browser's, in order) the console has; English without one.
  function firstConsoleLanguage(tags) {
    for (const tag of tags) {
      const code = consoleLanguage(tag);
      if (code) return code;
    }
    return 'en-US';
  }

  // The settings a query string sets, names in lower case; the others left out.
  function readQuery(search) {
    const out = new Map();
    for (const [k, v] of new URLSearchParams(search)) {
      const name = k.toLowerCase();
      if (NAME.test(name)) out.set(name, v);
    }
    return out;
  }

  // Only what differs from the defaults goes in the URL, in the panel's
  // order, then the settings the panel does not know, by name.
  function toQuery(values, known) {
    const q = new URLSearchParams();
    const names = [...known.keys(), ...[...values.keys()].filter((n) => !known.has(n)).sort()];
    for (const name of names) {
      if (!values.has(name)) continue;
      let v = values.get(name);
      if (known.get(name)?.type === 'bool') {
        if (!isOn(v)) continue;
        v = '1';
      }
      if (v === '' || v == null) continue;
      // A visit's UTM tags (it came from another of our sites) stay out of the
      // links the page makes: the first change of the URL drops them.
      if (name.startsWith('utm_')) continue;
      q.set(name, v);
    }
    const s = q.toString();
    return s ? `?${s}` : '';
  }

  // The settings whose value differs between `a` and `b`, as their URLs have them.
  function changedNames(a, b, known) {
    const qa = new URLSearchParams(toQuery(a, known)), qb = new URLSearchParams(toQuery(b, known));
    return [...new Set([...qa.keys(), ...qb.keys()])].filter((n) => qa.get(n) !== qb.get(n));
  }

  // A setting as its PLAYSHELF_SIM_* variable has it: '' for none.
  function envValue(p, value) {
    if (value == null || value === '') return '';
    if (p?.type === 'bool') return isOn(value) ? '1' : '';
    return p?.values?.[value] ?? value;
  }
  const envName = (name) => `PLAYSHELF_SIM_${name.toUpperCase()}`;

  // The variables and the config.json PlayShelf starts with: the page's
  // language and theme, whatever else `values` sets.
  function startEnvironment(values, known, language, theme) {
    const env = { SDL_EMSCRIPTEN_KEYBOARD_ELEMENT: '#canvas' };
    const config = { language, theme };
    for (const [name, value] of values) {
      if (name === 'lang' || name === 'theme') continue;
      const p = known.get(name);
      if (p?.page) continue;
      if (p?.config) { if (value) config[p.config] = value; continue; }
      const v = envValue(p, value);
      if (v !== '') env[envName(name)] = v;
    }
    return { env, config };
  }

  // ---- Steps ----------------------------------------------------------
  //
  // One step per dot: a button (up, down, left, right, a, b, x, y, l, r, zl,
  // zr, plus, minus), `*N` to press it N times; `wN` waits N tenths of a
  // second; `tX,Y` touches the screen (1280x720); `s:name=value` changes a
  // live setting (its value URL-encoded, dots too); `say:key` shows the
  // page's text `tour_<key>` (the tour).

  const STEP_BUTTONS = ['up', 'down', 'left', 'right', 'a', 'b', 'x', 'y', 'l', 'r', 'zl', 'zr', 'plus', 'minus'];

  // `text` as a list of steps: { button, times }, { wait } (ms), { touch: [x, y] },
  // { set: [name, value] } (value '' to remove it), { say }. Throws on a
  // step it does not know, before any is played.
  function parseSteps(text) {
    return String(text ?? '').split('.').filter(Boolean).map((step) => {
      let m;
      if ((m = /^([a-z]+)(?:\*(\d+))?$/.exec(step)) && STEP_BUTTONS.includes(m[1])) return { button: m[1], times: Number(m[2] || 1), step };
      if ((m = /^w(\d+)$/.exec(step))) return { wait: Number(m[1]) * 100, step };
      if ((m = /^t(\d+),(\d+)$/.exec(step))) return { touch: [Number(m[1]), Number(m[2])], step };
      if ((m = /^s:([a-z0-9_]+)=(.*)$/.exec(step))) {
        let value;
        try { value = decodeURIComponent(m[2]); } catch { value = null; }
        if (value != null) return { set: [m[1], value], step };
      }
      if ((m = /^say:([a-z_]+)$/.exec(step))) return { say: m[1], step };
      throw new Error(`steps: no such step "${step}"`);
    });
  }

  // A live change as a step: its value escaped, its dots too.
  const stepValue = (name, value) => `${name}=${encodeURIComponent(value ?? '').replace(/\./g, '%2E')}`;

  // The steps as a string: the same button pressed in a row, once with *N.
  function stepsText(list) {
    const out = [];
    for (const step of list) {
      const last = out[out.length - 1];
      const m = last && /^([a-z]+)(?:\*(\d+))?$/.exec(last);
      if (m && m[1] === step && STEP_BUTTONS.includes(step)) out[out.length - 1] = `${step}*${Number(m[2] || 1) + 1}`;
      else out.push(step);
    }
    return out.join('.');
  }

  // Links out (the rule "liens sortants anonymes"): our own sites
  // (boreale.dev and its subdomains, the repositories of github.com/JigSawFr/)
  // get the origin alone, any other site nothing. A fork's repository, the
  // profile page or a look-alike is another site.
  function ownSite(href) {
    let u;
    try { u = new URL(href); } catch { return false; }
    const host = u.hostname.toLowerCase();
    return u.protocol === 'https:' && (host === 'boreale.dev' || host.endsWith('.boreale.dev') ||
      (host === 'github.com' && u.pathname.toLowerCase().startsWith('/jigsawfr/')));
  }

  // The attributes a link to `href` takes: none on the page itself.
  function linkOutAttributes(href) {
    if (!/^https?:/i.test(href)) return {};
    return ownSite(href) ? { referrerpolicy: 'strict-origin' } : { rel: 'noreferrer' };
  }

  // The IndexedDB database that keeps the data folder (`keep`): one per
  // deployment, by the page's folder (a release's, main's, a pull
  // request's), so that one never reads or writes another's files, newer or
  // older, on the same origin.
  function keptDatabase(data, pathname) {
    const folder = String(pathname || '/').replace(/[^/]*$/, '');
    return `${data}@${folder.startsWith('/') ? folder : `/${folder}`}`;
  }

  const SimLogic = {
    LANGUAGES, FLAGS, flag, FALSY, STEP_BUTTONS, isOn, consoleLanguage, firstConsoleLanguage, readQuery, toQuery, changedNames,
    envValue, envName, startEnvironment, parseSteps, stepValue, stepsText, ownSite, linkOutAttributes, keptDatabase,
  };
  if (typeof module === 'object' && module.exports) module.exports = SimLogic;
  else root.SimLogic = SimLogic;
})(typeof self !== 'undefined' ? self : this);
