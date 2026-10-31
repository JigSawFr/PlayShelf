// The web simulator's page (web/README.md): reads the settings from the URL,
// shows them in the side panel (built from params.json), and starts PlayShelf
// (playshelf.js, Emscripten) with them: PLAYSHELF_SIM_* variables, and a
// config.json for the language, the theme and the first tab. Applying new
// settings reloads the page with them in the URL, so a link always opens the
// same state.
//
// The page speaks the console's languages (i18n/<language>.json): the
// browser's, unless the URL says `lang=`; its theme is dark, as PlayShelf on
// a console, or the one chosen in its header (Auto: the browser's) or set by
// `theme=`. PlayShelf takes both.
//
// While PlayShelf runs, the `live` settings change without a restart
// (source/sim/web_page.cpp), and with `keep` its data folder stays in the browser
// (IndexedDB). <body data-playshelf> says where it is: loading, running,
// ready (its first screen is up), error.
//
// Steps: what is pressed, touched and changed live is noted, with the time
// between; **Steps link** gives the settings PlayShelf started with and
// them (`rec=`), which a page opening it replays. The tour (`tour=1`) and
// the smoke test (tools/web_smoke.py, runSteps()) are steps too.
//
// What touches neither the page nor PlayShelf (the languages, the URL, the
// variables, the steps' format) is in sim_logic.js (`SimLogic`), loaded
// before this file and tested on its own (tests/web_page/).
'use strict';

const { LANGUAGES, isOn, keptDatabase } = SimLogic;

const THEMES = ['auto', 'light', 'dark'];
// The languages PlayShelf draws with the CJK font (cjk.otf, next to the page):
// waited for before it starts; fetched once it runs in the others.
const CJK = new Set(['ja', 'ko', 'zh-Hans', 'zh-Hant']);
const CJK_FILE = '/app/resources/font/cjk.otf';
// The data folder (source/util/paths.cpp), kept in IndexedDB with `keep`:
// in a database of this deployment's own (KEPT_DB, SimLogic.keptDatabase).
const DATA = '/playshelf_data';
const KEPT_DB = keptDatabase(DATA, location.pathname);
// config.json's schema (config::SCHEMA in source/util/config.hpp).
const CONFIG_SCHEMA = 1;
// Where a report goes without build.json (a local build).
const REPO = 'JigSawFr/PlayShelf';

function browserLanguage() {
  return SimLogic.firstConsoleLanguage(navigator.languages?.length ? navigator.languages : [navigator.language]);
}

// Aurore's ControllerButton values (library/include/aurore/core/input.hpp).
const BUTTON = { ZL: 0, L: 1, UP: 3, RIGHT: 4, DOWN: 5, LEFT: 6, MINUS: 7, PLUS: 9, Y: 11, B: 12, A: 13, X: 14, R: 15, ZR: 16 };
// The keys the page presses buttons for, by what they say (any layout); the
// arrows, Enter and Escape reach SDL directly.
const KEYS = {
  x: BUTTON.X, X: BUTTON.X, y: BUTTON.Y, Y: BUTTON.Y,
  l: BUTTON.L, r: BUTTON.R, L: BUTTON.ZL, R: BUTTON.ZR,
  '+': BUTTON.PLUS, p: BUTTON.PLUS, '-': BUTTON.MINUS, m: BUTTON.MINUS,
  Backspace: BUTTON.B,
};
const NAME = /^[a-z0-9_]+$/;

// As the documentation (pages.yml): simulator/ is the latest release's
// (main's while no release has one), simulator/dev/ main's, simulator/<tag>/
// a release's. The site one or two folders up.
const VERSION_DIR = /\/simulator\/(dev|v\d+\.\d+\.\d+)\/$/.exec(location.pathname)?.[1] ?? null;
const SITE_UP = VERSION_DIR ? '../../' : '../';

const $ = (id) => document.getElementById(id);
const canvas = $('canvas');
let manifest = { groups: [], params: [], presets: [] };
let known = new Map();          // name -> param
let current = new Map();        // what the URL says (applied)
let started = false;
let text = { ui: {}, groups: {}, params: {}, presets: {} };   // the page's language, English underneath
let language = 'en-US';         // the page's, and PlayShelf's
let consoleTheme = 'light';     // what PlayShelf was started with
let running = false;            // PlayShelf's runtime is up: ccall works
let version = '';               // the build's commit, for the files' URLs
let buildInfo = null;           // build.json, for a report
let startValues = new Map();    // the settings PlayShelf started with, for a steps link

// ---- Texts ------------------------------------------------------------

const t = (key) => text.ui[key] ?? key;
const paramText = (p, key) => text.params[p.name]?.[key] ?? '';
const optionText = (p, value) => text.params[p.name]?.options?.[value] ?? value;

async function loadTexts(code) {
  const get = async (c) => {
    try {
      const res = await fetch(`i18n/${c}.json`);
      return res.ok ? await res.json() : {};
    } catch { return {}; }
  };
  const [en, own] = await Promise.all([get('en-US'), code === 'en-US' ? {} : get(code)]);
  const merged = { ui: {}, groups: {}, params: {}, presets: {} };
  for (const part of Object.keys(merged)) {
    Object.assign(merged[part], en[part]);
    for (const [k, v] of Object.entries(own[part] ?? {})) {
      merged[part][k] = part === 'params' ? { ...merged.params[k], ...v, options: { ...merged.params[k]?.options, ...v.options } } : v;
    }
  }
  text = merged;
}

// ---- The theme ------------------------------------------------------------

const dark = matchMedia('(prefers-color-scheme: dark)');

function storedTheme() {
  try { return localStorage.getItem('ps-theme'); } catch { return null; }
}
// What the header chose: the URL's theme, else the one kept from an earlier
// visit, else dark (PlayShelf's own default).
const DEFAULT_THEME = 'dark';
function chosenTheme() {
  const fromUrl = current.get('theme');
  if (THEMES.includes(fromUrl)) return fromUrl;
  const kept = storedTheme();
  return THEMES.includes(kept) ? kept : DEFAULT_THEME;
}
const effectiveTheme = (choice) => (choice === 'auto' ? (dark.matches ? 'dark' : 'light') : choice);

function showTheme() {
  const effective = effectiveTheme(chosenTheme());
  // The choice (the header's icon on a phone), then the theme it gives.
  document.documentElement.dataset.themeChoice = chosenTheme();
  document.documentElement.dataset.theme = effective;
  barColour(effective);
  // Started with the other one (Auto following a change of the system's):
  // PlayShelf reads its theme at start-up, so offer to start it again.
  $('theme-notice').hidden = !started || effective === consoleTheme;
}

// The browser's bar (Android) and the home-screen app's, in the page's
// theme: the header's colour (simulator.css' --surface).
function barColour(theme) {
  document.querySelector('meta[name="theme-color"]')?.setAttribute('content', '#0e1a33');
}

function chooseTheme(choice) {
  try { localStorage.setItem('ps-theme', choice); } catch { /* storage refused */ }
  const values = new Map(current);
  if (choice === DEFAULT_THEME) values.delete('theme');
  else values.set('theme', choice);
  if (effectiveTheme(choice) === consoleTheme) {
    // PlayShelf already has it: only the URL changes.
    current = values;
    history.replaceState(null, '', `${location.pathname}${toQuery(values)}`);
    showTheme();
    markChanges();
    return;
  }
  apply(values);
}

// ---- The URL ------------------------------------------------------------

const readUrl = () => SimLogic.readQuery(location.search);
const toQuery = (values) => SimLogic.toQuery(values, known);

function linkFor(values) {
  return `${location.origin}${location.pathname}${toQuery(values)}`;
}

// ---- The panel ------------------------------------------------------------

function el(tag, attrs = {}, ...children) {
  const node = document.createElement(tag);
  // A link out says nothing of the visit, but to our own sites (sim_logic.js).
  if (tag === 'a') {
    const out = SimLogic.linkOutAttributes(attrs.href || '');
    attrs = { ...out, ...attrs, ...(out.rel && attrs.rel ? { rel: `${attrs.rel} ${out.rel}` } : {}) };
  }
  for (const [k, v] of Object.entries(attrs)) {
    if (k === 'class') node.className = v;
    else if (k === 'text') node.textContent = v;
    else if (v !== false && v != null) node.setAttribute(k, v === true ? '' : v);
  }
  for (const c of children) if (c != null) node.append(c);
  return node;
}

function toLocalInput(seconds) {
  const d = new Date(Number(seconds) * 1000);
  if (Number.isNaN(d.getTime())) return '';
  const pad = (n) => String(n).padStart(2, '0');
  return `${d.getFullYear()}-${pad(d.getMonth() + 1)}-${pad(d.getDate())}T${pad(d.getHours())}:${pad(d.getMinutes())}`;
}

function fieldFor(p, value) {
  const id = `p-${p.name}`;
  const label = paramText(p, 'label') || p.name;
  const help = paramText(p, 'help') ? el('p', { class: 'help', text: paramText(p, 'help') }) : null;
  if (p.type === 'bool') {
    const input = el('input', { type: 'checkbox', id, 'data-name': p.name });
    input.checked = isOn(value);
    return [el('label', { class: 'field check', for: id }, input, el('span', { text: label })), help];
  }
  if (p.type === 'multi') {
    const chosen = new Set(String(value ?? '').split(',').filter(Boolean));
    const box = el('div', { class: 'multi', 'data-multi': p.name });
    for (const o of p.options) {
      const input = el('input', { type: 'checkbox', value: o, 'data-of': p.name });
      input.checked = chosen.has(o);
      box.append(el('label', { class: 'field check' }, input, el('span', { text: optionText(p, o) })));
    }
    return [el('div', { class: 'field' }, el('span', { text: label })), box, help];
  }
  let input;
  if (p.type === 'select') {
    input = el('select', { id, 'data-name': p.name });
    for (const o of p.options) input.append(el('option', { value: o, text: optionText(p, o) }));
    if (value != null && !p.options.includes(value)) input.append(el('option', { value, text: value }));
    input.value = value ?? '';
  } else if (p.type === 'datetime') {
    input = el('input', { type: 'datetime-local', id, 'data-name': p.name, 'data-datetime': true });
    input.value = value ? toLocalInput(value) : '';
  } else {
    input = el('input', {
      type: p.type === 'number' ? 'number' : 'text', id, 'data-name': p.name,
      placeholder: p.placeholder ?? '', min: p.min, max: p.max, inputmode: p.type === 'number' ? 'numeric' : null,
      list: p.suggestions ? `${id}-list` : null,
    });
    input.value = value ?? '';
  }
  const list = p.suggestions ? el('datalist', { id: `${id}-list` }, ...p.suggestions.map((s) => el('option', { value: s }))) : null;
  return [el('label', { class: 'field', for: id }, el('span', { text: label }), input), list, help];
}

function buildForm(values) {
  const form = $('form');
  form.replaceChildren();
  for (const g of manifest.groups) {
    const params = manifest.params.filter((p) => p.group === g.id && p.panel !== false && (!p.page || p.panel === true));
    if (!params.length) continue;
    const set = params.filter((p) => values.has(p.name)).length;
    const details = el('details', { class: 'group', 'data-group': g.id },
      el('summary', {}, text.groups[g.id] ?? g.id, set ? el('span', { class: 'count', text: String(set) }) : null));
    if (g.id === 'app' || g.id === 'parental' || set) details.open = true;
    for (const p of params) for (const node of fieldFor(p, values.get(p.name))) if (node) details.append(node);
    form.append(details);
  }
  buildOther(values);
  markChanges();
}

function buildOther(values) {
  const box = $('other');
  box.replaceChildren();
  const others = [...values.entries()].filter(([n]) => !known.has(n));
  for (const [name, value] of others) {
    const input = el('input', { 'data-other': name, 'aria-label': name });
    input.value = value;
    const remove = el('button', { type: 'button', class: 'btn ghost', text: '×', title: t('remove'), 'aria-label': t('remove') });
    remove.addEventListener('click', () => { input.closest('.other-row').remove(); markChanges(); });
    box.append(el('div', { class: 'other-row' }, el('code', { text: name }), input, remove));
  }
  $('other-group').open = others.length > 0;
}

// What the panel says now, with the header's language and theme as the URL has them.
function readForm() {
  const out = new Map();
  for (const name of ['lang', 'theme']) if (current.has(name)) out.set(name, current.get(name));
  for (const input of $('form').querySelectorAll('[data-name]')) {
    const name = input.dataset.name;
    if (input.type === 'checkbox') { if (input.checked) out.set(name, '1'); continue; }
    let v = input.value.trim();
    if (input.dataset.datetime != null && v) {
      const ms = new Date(v).getTime();
      v = Number.isNaN(ms) ? '' : String(Math.round(ms / 1000));
    }
    if (v !== '') out.set(name, v);
  }
  for (const box of $('form').querySelectorAll('[data-multi]')) {
    const chosen = [...box.querySelectorAll('input:checked')].map((i) => i.value);
    if (chosen.length) out.set(box.dataset.multi, chosen.join(','));
  }
  for (const input of $('other').querySelectorAll('[data-other]')) out.set(input.dataset.other, input.value.trim());
  return out;
}

function markChanges() {
  const pending = toQuery(readForm()) !== toQuery(current);
  $('apply').classList.toggle('pending', pending);
  for (const input of $('form').querySelectorAll('[data-name]')) {
    const name = input.dataset.name;
    const field = input.closest('.field');
    if (field) field.classList.toggle('changed', current.has(name) || (input.type === 'checkbox' ? input.checked : input.value !== ''));
  }
}

// Only `live` settings changed, PlayShelf running: they reach it at once and
// the URL follows; otherwise the page reloads with them.
function apply(values) {
  const changed = SimLogic.changedNames(values, current, known).filter((n) => !known.get(n)?.page);
  if (running && changed.length && changed.every((n) => known.get(n)?.live)) {
    for (const name of changed) { setLive(name, values.get(name)); noteStep(`s:${SimLogic.stepValue(name, values.get(name))}`); }
    current = new Map(values);
    history.replaceState(null, '', linkFor(current));
    buildForm(current);
    toast(t('live_applied'));
    return;
  }
  const query = toQuery(values);
  if (query === location.search || (query === '' && location.search === '')) { location.reload(); return; }
  location.search = query;   // reloads: PlayShelf starts again from scratch
}

// The page's language and theme stay, and the `sticky` settings; the rest is what `keep` says.
function keepingHeader(keep) {
  return new Map([...current].filter(([n]) => n === 'lang' || n === 'theme' || known.get(n)?.sticky || keep(n)));
}

// A preset's datetime `@21:30`: today at that time, in the browser's time zone.
function presetValue(name, value) {
  const m = known.get(name)?.type === 'datetime' && /^@(\d\d):(\d\d)$/.exec(value);
  if (!m) return value;
  const d = new Date();
  d.setHours(Number(m[1]), Number(m[2]), 0, 0);
  return String(Math.round(d.getTime() / 1000));
}

function buildPresets() {
  const select = $('preset');
  select.append(el('option', { value: '', text: t('preset_none') }));
  for (const p of manifest.presets) select.append(el('option', { value: p.id, text: text.presets[p.id] ?? p.id }));
  select.addEventListener('change', () => {
    const preset = manifest.presets.find((p) => p.id === select.value);
    if (!preset) return;
    // The first tab stays too.
    const values = keepingHeader((n) => known.get(n)?.config);
    for (const [n, v] of Object.entries(preset.params)) values.set(n, presetValue(n, v));
    apply(values);
  });
}

// ---- PlayShelf ------------------------------------------------------------

let logLines = [];
function log(line0, error) {
  // eslint-disable-next-line no-control-regex
  const line = String(line0).replace(/\x1b\[[0-9;]*m/g, '');
  logLines.push([line, error]);
  if (logLines.length > 1000) logLines = logLines.slice(-800);
  const pre = $('log');
  if (pre.closest('[hidden]')) return;
  appendLog(pre, line, error);
}
function appendLog(pre, line, error) {
  const span = document.createElement('span');
  if (error || /\[ERROR\]/.test(line)) span.className = 'err';
  span.textContent = `${line}\n`;
  pre.append(span);
  pre.parentElement.scrollTop = pre.parentElement.scrollHeight;
}
function renderLog() {
  const pre = $('log');
  pre.replaceChildren();
  for (const [line, error] of logLines) appendLog(pre, line, error);
}

function showStatus(message) {
  $('status').textContent = message;
}

function showProgress(done, total) {
  const bar = $('progress');
  if (!bar || !total) return;
  bar.hidden = false;
  bar.max = total;
  bar.value = done;
}

let toastTimer = 0;
function toast(message) {
  const box = $('toast');
  box.textContent = message;
  box.hidden = false;
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => { box.hidden = true; }, 2500);
}

function showError(message) {
  running = false;   // nothing to call into any more: Apply reloads
  document.body.dataset.playshelf = 'error';
  const overlay = $('overlay');
  overlay.hidden = false;
  overlay.classList.add('error');
  const button = el('button', { type: 'button', class: 'btn primary', text: t('reload') });
  button.addEventListener('click', () => location.reload());
  overlay.replaceChildren(el('div', { class: 'spinner', 'aria-hidden': 'true' }), el('p', { text: message }), button);
}

// A variable changed while PlayShelf runs (source/sim/web_page.cpp).
function setLive(name, value) {
  window.Module.ccall('playshelf_web_setenv', 'number', ['string', 'string'], [SimLogic.envName(name), SimLogic.envValue(known.get(name), value)]);
  log(`live: ${name}=${value ?? ''}`);
}

// ---- Keeping the state (keep=1) ------------------------------------------------

let syncing = false;
let forgetting = false;
// The data folder to IndexedDB, at most one write at a time.
function saveData() {
  const FS = window.Module?.FS;
  if (!FS || syncing || forgetting) return;
  syncing = true;
  FS.syncfs(false, (e) => { syncing = false; if (e) log(`keep: ${e}`, true); });
}

// Before PlayShelf starts: the data folder from IndexedDB, then config.json
// with the page's language, theme and first tab over what it kept. On a
// first start PlayShelf adds the demo library server to servers.json, and
// its first games to the simulated console, itself (source/sim/sim_demo.hpp),
// unless `no_demo`.
function mountKept(m, config) {
  // Emscripten's IDBFS names the database after the mount point: this
  // deployment's own name in its place.
  const idbfs = m.IDBFS;
  const getDB = idbfs.getDB;
  idbfs.getDB = (name, callback) => getDB.call(idbfs, name === DATA ? KEPT_DB : name, callback);
  m.FS.mkdirTree(DATA);
  m.FS.mount(idbfs, {}, DATA);
  m.addRunDependency('keep');
  m.FS.syncfs(true, (e) => {
    if (e) log(`keep: ${e}`, true);
    let kept = {};
    try { kept = JSON.parse(m.FS.readFile(`${DATA}/config.json`, { encoding: 'utf8' })); } catch { /* first run */ }
    // A newer PlayShelf's file is left as it is (it reads the defaults).
    if (!(kept?.schema > CONFIG_SCHEMA)) {
      m.FS.writeFile(`${DATA}/config.json`, JSON.stringify({ ...kept, ...config, schema: CONFIG_SCHEMA }));
    }
    setInterval(saveData, 3000);
    addEventListener('pagehide', saveData);
    addEventListener('visibilitychange', () => { if (document.hidden) saveData(); });
    m.removeRunDependency('keep');
  });
}

function forget() {
  forgetting = true;
  const done = () => location.reload();
  const req = indexedDB.deleteDatabase(KEPT_DB);
  req.onsuccess = req.onerror = req.onblocked = done;
}

// ---- The CJK font ------------------------------------------------------------

async function fetchCjk() {
  const res = await fetch(`cjk.otf${version ? `?v=${version}` : ''}`);
  if (!res.ok) throw new Error(`cjk.otf: ${res.status}`);
  return new Uint8Array(await res.arrayBuffer());
}
function writeCjk(m, bytes) {
  m.FS.mkdirTree(CJK_FILE.slice(0, CJK_FILE.lastIndexOf('/')));
  m.FS.writeFile(CJK_FILE, bytes);
}
// In Japanese, Korean or Chinese, PlayShelf starts once the font is there.
function preloadCjk(m) {
  m.addRunDependency('cjk');
  fetchCjk().then((bytes) => writeCjk(m, bytes), (e) => log(String(e), true)).finally(() => m.removeRunDependency('cjk'));
}
// In the others, it comes behind the Latin font once PlayShelf runs.
async function laterCjk() {
  try {
    const bytes = await fetchCjk();
    writeCjk(window.Module, bytes);
    window.Module.ccall('playshelf_web_cjk_font', 'number', ['string'], [CJK_FILE]);
  } catch (e) {
    log(String(e), true);
  }
}

function start() {
  if (!self.crossOriginIsolated) {
    // sw.js reloads the page once its service worker is installed, which a
    // first visit on a slow connection can take a while to do. Without service workers (a private window in some
    // browsers) it never will; otherwise give it time before saying so.
    if (!('serviceWorker' in navigator) || !self.isSecureContext) { showError(t('no_isolation')); return; }
    setTimeout(() => showError(t('no_isolation')), 30000);
    return;
  }
  if (!document.createElement('canvas').getContext('webgl2')) { showError(t('no_webgl')); return; }

  // Screenshots read the canvas after the frame: keep its drawing buffer.
  const getContext = HTMLCanvasElement.prototype.getContext;
  HTMLCanvasElement.prototype.getContext = function (type, attrs) {
    if (this === canvas && /^webgl/.test(type)) attrs = { ...(attrs || {}), preserveDrawingBuffer: true };
    return getContext.call(this, type, attrs);
  };

  consoleTheme = effectiveTheme(chosenTheme());
  const { env, config } = SimLogic.startEnvironment(current, known, language, consoleTheme);
  // The demo server's packages (signed with the host tests' key) install as
  // a console's would, unless the URL says signed=0.
  if (!isOn(current.get('no_demo')) && !current.has('signed')) env.PLAYSHELF_SIM_SIGNED = '1';
  log(`settings: ${decodeURIComponent(toQuery(current)) || '(defaults)'}; language ${language}, theme ${consoleTheme}`);
  showStatus(t('downloading'));
  const keep = isOn(current.get('keep'));
  window.Module = {
    canvas,
    // GitHub Pages lets browsers keep files 10 minutes: after a push, the
    // commit in their URL keeps an old .wasm from meeting a new .js.
    locateFile: (path, prefix) => `${prefix}${path}${version ? `?v=${version}` : ''}`,
    print: (line) => {
      log(line);
      if (line.includes('start-up: first screen ready') && document.body.dataset.playshelf !== 'ready') {
        document.body.dataset.playshelf = 'ready';
        ready();
      }
    },
    printErr: (line) => log(line, true),
    setStatus: (status) => {
      if (!status) return;
      // "Downloading data... (1234/5678)": how far the download is.
      const m = /\((\d+)\/(\d+)\)/.exec(status);
      if (m) showProgress(Number(m[1]), Number(m[2]));
      showStatus(status.startsWith('Downloading data') ? t('downloading') : status);
    },
    preRun: [(m) => {
      Object.assign(m.ENV, env);
      if (keep) {
        mountKept(m, config);
      } else {
        m.FS.mkdirTree(DATA);
        m.FS.writeFile(`${DATA}/config.json`, JSON.stringify(config));
      }
      if (CJK.has(language)) preloadCjk(m);
    }],
    onRuntimeInitialized: () => {
      running = true;
      document.body.dataset.playshelf = 'running';
      showStatus(t('starting'));
      showProgress(1, 1);
      requestAnimationFrame(() => requestAnimationFrame(() => { $('overlay').hidden = true; canvas.focus(); }));
      if (!CJK.has(language)) laterCjk();
    },
    onAbort: (what) => showError(`${t('error')} ${what ?? ''}`),
  };
  window.addEventListener('error', (e) => {
    // Not the page's: "Script error." and nothing else (a script the page
    // may not read: iOS' share sheet, an extension), or another site's
    // script. PlayShelf still runs: noted in the log, not shown as its end.
    const elsewhere = e.filename && new URL(e.filename, location.href).origin !== location.origin;
    if (elsewhere || (!e.error && !e.filename && /^Script error\.?$/.test(e.message ?? ''))) {
      log(`page: ${e.message || 'an error'}${e.filename ? ` (${e.filename})` : ''}, not PlayShelf's`, true);
      return;
    }
    if (started) showError(`${t('error')} ${e.message ?? e.error ?? ''}`);
  });
  started = true;
  document.body.append(el('script', { src: `playshelf.js${version ? `?v=${version}` : ''}` }));
}

// ---- Input ------------------------------------------------------------

function press(button, down) {
  window.Module?._aurore_web_button?.(button, down ? 1 : 0);
}

// The keys SDL reads itself, as the buttons they press, for the steps.
const SDL_KEYS = { ArrowUp: BUTTON.UP, ArrowDown: BUTTON.DOWN, ArrowLeft: BUTTON.LEFT, ArrowRight: BUTTON.RIGHT, Enter: BUTTON.A, Escape: BUTTON.B };

function setUpKeys() {
  const held = new Map();
  canvas.addEventListener('keydown', (e) => {
    if (e.ctrlKey || e.metaKey || e.altKey) return;
    if (!e.repeat) noteButton(SDL_KEYS[e.key] ?? KEYS[e.key]);
    const button = KEYS[e.key];
    if (button == null) return;
    e.preventDefault();
    if (e.repeat) return;
    held.set(e.code, button);
    press(button, true);
  });
  canvas.addEventListener('keyup', (e) => {
    const button = held.get(e.code);
    if (button == null) return;
    held.delete(e.code);
    press(button, false);
  });
  canvas.addEventListener('blur', () => { for (const b of held.values()) press(b, false); held.clear(); });
  canvas.addEventListener('pointerdown', (e) => {
    canvas.focus();
    // A touch, in the console's 1280x720.
    const r = canvas.getBoundingClientRect();
    if (!e.isTrusted || !r.width) return;
    noteStep(`t${Math.round((e.clientX - r.left) / r.width * 1280)},${Math.round((e.clientY - r.top) / r.height * 720)}`);
  });
}

// The tools under the screen: in a row where there is room, behind a
// button on a phone (closed again once one is used).
function setUpTools() {
  const tools = $('tools');
  const narrow = matchMedia('(max-width: 600px)');
  const fit = () => { tools.open = !narrow.matches; };
  fit();
  narrow.addEventListener('change', fit);
  tools.querySelector('.tool-list').addEventListener('click', (e) => {
    if (narrow.matches && e.target.closest('button')) tools.open = false;
  });
}

// Full screen, where the browser allows it (Android, a computer; not an
// iPhone's Safari, whose home-screen app plays in landscape instead):
// only the screen and the pad (html.game), turned to landscape on a phone.
function setUpFullscreen() {
  const root = document.documentElement;
  if (!document.fullscreenEnabled || !root.requestFullscreen) return;
  const button = $('fullscreen');
  button.hidden = false;
  button.addEventListener('click', async () => {
    try {
      await root.requestFullscreen({ navigationUI: 'hide' });
      if (matchMedia('(pointer: coarse)').matches) await screen.orientation?.lock?.('landscape');
    } catch { /* refused, or no orientation lock: full screen as it is */ }
  });
  $('fs-exit').addEventListener('click', () => document.exitFullscreen().catch(() => {}));
  document.addEventListener('fullscreenchange', () => {
    const on = document.fullscreenElement === root;
    root.classList.toggle('game', on);
    if (!on) screen.orientation?.unlock?.();
    canvas.focus();
  });
}

function setUpPad() {
  for (const button of $('pad').querySelectorAll('[data-button]')) {
    const code = Number(button.dataset.button);
    const up = (e) => { button.classList.remove('active'); press(code, false); e.preventDefault(); };
    button.addEventListener('pointerdown', (e) => {
      button.setPointerCapture(e.pointerId);
      button.classList.add('active');
      // A finger's press felt, as on a controller (Android; iOS has no vibration).
      if (e.pointerType === 'touch') navigator.vibrate?.(10);
      noteButton(code);
      press(code, true);
      e.preventDefault();
    });
    button.addEventListener('pointerup', up);
    button.addEventListener('pointercancel', up);
    button.addEventListener('contextmenu', (e) => e.preventDefault());
  }
  const toggle = $('pad-toggle');
  const show = (on) => {
    $('pad').hidden = !on;
    toggle.setAttribute('aria-pressed', String(on));
    try { localStorage.setItem('ps-pad', on ? '1' : '0'); } catch { /* storage refused */ }
  };
  let saved = null;
  try { saved = localStorage.getItem('ps-pad'); } catch { /* storage refused */ }
  show(saved ? saved === '1' : matchMedia('(pointer: coarse)').matches);
  toggle.addEventListener('click', () => show($('pad').hidden));
}

// A file name that says the state: playshelf_unlocked_1.png.
const fileName = (ext) => `playshelf${decodeURIComponent(toQuery(current)).replace(/[^a-z0-9.]+/gi, '_')}.${ext}`;

function download(blob, name) {
  const a = el('a', { href: URL.createObjectURL(blob), download: name });
  a.click();
  setTimeout(() => URL.revokeObjectURL(a.href), 1000);
}

function screenshot() {
  canvas.toBlob((blob) => { if (blob) download(blob, fileName('png')); }, 'image/png');
}

// A video of the screen (WebM, 30 frames a second) until Stop.
let recorder = null;
function record() {
  const button = $('record');
  if (recorder) { recorder.stop(); return; }
  const type = ['video/webm;codecs=vp9', 'video/webm'].find((m) => MediaRecorder.isTypeSupported(m));
  const chunks = [];
  recorder = new MediaRecorder(canvas.captureStream(30), type ? { mimeType: type } : {});
  recorder.ondataavailable = (e) => { if (e.data.size) chunks.push(e.data); };
  recorder.onstop = () => {
    recorder = null;
    button.textContent = t('record');
    button.setAttribute('aria-pressed', 'false');
    download(new Blob(chunks, { type: 'video/webm' }), fileName('webm'));
  };
  recorder.start(1000);
  button.textContent = t('stop_recording');
  button.setAttribute('aria-pressed', 'true');
  canvas.focus();
}

// ---- Steps ------------------------------------------------------------
//
// Their format is SimLogic's (sim_logic.js): buttons, waits, touches, live
// settings and the tour's lines, one per dot.

const STEP_BUTTONS = {
  up: BUTTON.UP, down: BUTTON.DOWN, left: BUTTON.LEFT, right: BUTTON.RIGHT, a: BUTTON.A, b: BUTTON.B,
  x: BUTTON.X, y: BUTTON.Y, l: BUTTON.L, r: BUTTON.R, zl: BUTTON.ZL, zr: BUTTON.ZR, plus: BUTTON.PLUS, minus: BUTTON.MINUS,
};
const BUTTON_STEP = Object.fromEntries(Object.entries(STEP_BUTTONS).map(([name, code]) => [code, name]));
// A touch's press, and the pause after a step: Aurore's animations last
// 300 ms.
const PRESS_MS = 60;
const AFTER_MS = 400;
// The tour (tour=1): the Library, the demo server's Catalogue, a game added
// to the downloads and installed, the Downloads tab, the Preferences.
const TOUR = 'w10.say:home.w40.left.down.say:library.w30.down.say:catalogue.w50.right.down*2.say:game.w20.a.w30.say:download.a.w40' +
             '.b.left.down.say:downloads.right.w60.left.down.say:preferences.w45.say:end.w60';

const sleep = (ms) => new Promise((done) => setTimeout(done, ms));
// The next display refresh, which runs one of PlayShelf's frames
// (emscripten_set_main_loop, source/main.cpp).
const frame = () => new Promise((done) => requestAnimationFrame(() => done()));

let steps = [];                 // what was done since the first screen
let lastStep = 0;
let replaying = null;           // the steps being replayed: { stop }

function noteStep(step) {
  if (!lastStep || replaying) return;
  const now = performance.now();
  const wait = Math.round((now - lastStep) / 100);
  if (wait >= 2) steps.push(`w${Math.min(wait, 600)}`);
  steps.push(step);
  lastStep = now;
}
function noteButton(code) {
  if (code != null && BUTTON_STEP[code]) noteStep(BUTTON_STEP[code]);
}

function touch(x, y) {
  const r = canvas.getBoundingClientRect();
  const at = { clientX: r.left + x / 1280 * r.width, clientY: r.top + y / 720 * r.height, bubbles: true, button: 0 };
  canvas.dispatchEvent(new MouseEvent('mousedown', at));
  return sleep(PRESS_MS).then(() => canvas.dispatchEvent(new MouseEvent('mouseup', at)));
}

// Plays `text` (SimLogic.parseSteps()); `onStep(n, total, step)` before
// each. Resolves when done or stopped; rejects on a step it does not know,
// before playing any.
async function runSteps(text, onStep) {
  const list = SimLogic.parseSteps(text);
  const run = { stopped: false };
  replaying = run;
  // Nobody touches the phone while the tour plays: its screen kept on.
  const awake = keepAwake();
  try {
    for (let i = 0; i < list.length && !run.stopped; i++) {
      const step = list[i];
      onStep?.(i + 1, list.length, step.step);
      if (step.button) {
        // A button is tapped: down and up at once, which aurore_web_button()
        // keeps for the next frame as one press. Held for a while instead,
        // its release could come late on a busy page (SwiftShader's frames,
        // a timer run late) and Aurore repeat the press: the smoke test's
        // tabs one off. Two refreshes then, so that a frame has read it
        // before the next tap.
        for (let n = step.times; n > 0 && !run.stopped; n--) {
          press(STEP_BUTTONS[step.button], true);
          press(STEP_BUTTONS[step.button], false);
          await frame();
          await frame();
          await sleep(AFTER_MS);
        }
      } else if (step.wait != null) {
        await sleep(step.wait);
      } else if (step.touch) {
        await touch(...step.touch);
        await sleep(AFTER_MS);
      } else if (step.set) {
        const [name, value] = step.set;
        const values = new Map(current);
        if (value) values.set(name, value); else values.delete(name);
        apply(values);
          } else if (step.say) {
        $('steps-text').textContent = t(`tour_${step.say}`);
      }
    }
  } finally {
    awake.release();
    if (replaying === run) replaying = null;
  }
}

// The screen kept on (Wake Lock: Android, iOS 16.4+) until release(); taken
// again when the page comes back into view, which drops it.
function keepAwake() {
  let lock = null;
  let done = false;
  const take = () => {
    if (done || document.visibilityState !== 'visible' || !navigator.wakeLock) return;
    navigator.wakeLock.request('screen').then((l) => {
      if (done) l.release().catch(() => {});
      else lock = l;
    }, () => { /* refused (battery saver, a page without focus): no matter */ });
  };
  document.addEventListener('visibilitychange', take);
  take();
  return {
    release() {
      done = true;
      document.removeEventListener('visibilitychange', take);
      lock?.release().catch(() => {});
    },
  };
}

// Replays steps with the notice above the screen, and Stop.
async function replay(text, tour) {
  const notice = $('steps-notice');
  notice.hidden = false;
  $('steps-stop').onclick = () => { if (replaying) replaying.stopped = true; };
  log(`steps: ${tour ? 'tour' : 'replaying'} ${text}`);
  try {
    await runSteps(text, (n, total, step) => {
      if (!tour && !step.startsWith('w')) $('steps-text').textContent = t('replaying').replace('{n}', n).replace('{total}', total);
    });
    log('steps: done');
  } catch (e) {
    log(String(e), true);
  } finally {
    notice.hidden = true;
    canvas.focus();
  }
}

// Once the first screen is up: steps are noted from now on, and those of
// the URL (rec=, tour=1) played. They leave the URL: a reload starts plain.
function ready() {
  lastStep = performance.now();
  const rec = current.get('rec');
  const tour = isOn(current.get('tour'));
  if (rec || tour) {
    current.delete('rec');
    current.delete('tour');
    history.replaceState(null, '', linkFor(current));
    setTimeout(() => replay(tour ? TOUR : rec, tour), 1000);
  }
}

// The link: the settings PlayShelf started with, and the steps since.
function stepsLink() {
  const values = new Map(startValues);
  const text = SimLogic.stepsText(steps);
  if (text) values.set('rec', text);
  return linkFor(values);
}

// A link handed on: the system's share sheet on a phone (Messages, mail…),
// else the clipboard. True once copied, false when shared or given up.
async function handOn(link, label) {
  if (navigator.share && matchMedia('(pointer: coarse)').matches) {
    try { await navigator.share({ title: `PlayShelf · ${t('title')}`, url: link }); } catch { /* closed */ }
    return false;
  }
  try { await navigator.clipboard.writeText(link); } catch { prompt(label, link); return false; }
  return true;
}

async function copySteps() {
  if (await handOn(stepsLink(), t('steps_copy'))) toast(t('steps_copied'));
}

// A bug report on GitHub, its form filled in: the steps link, the build, the
// browser and the end of the log. GitHub takes a URL of 8 KB or so.
function report() {
  const repo = buildInfo?.repo || REPO;
  const build = buildInfo ? `commit ${buildInfo.sha.slice(0, 7)}${buildInfo.pr ? `, pull request #${buildInfo.pr}` : ''}` : t('unknown_build');
  const url = (lines) => {
    const tail = logLines.slice(-lines).map(([line]) => line).join('\n');
    const body = `In the web simulator (${build}), these steps:\n${stepsLink()}\n\n` +
                  `Browser: ${navigator.userAgent}\n\nThe end of the log:\n\`\`\`\n${tail}\n\`\`\``;
    const q = new URLSearchParams({ title: '[simulator] ', body });
    return `https://github.com/${repo}/issues/new?${q}`;
  };
  let lines = 40;
  while (lines > 0 && url(lines).length > 8000) lines -= 5;
  // Opened as a link, so the issue page gets the origin alone (el()).
  el('a', { href: url(lines), target: '_blank', rel: 'noopener' }).click();
}

// ---- The page ------------------------------------------------------------

function translatePage() {
  document.documentElement.lang = language;
  document.title = `PlayShelf · ${t('title')}`;
  for (const node of document.querySelectorAll('[data-i18n]')) node.textContent = t(node.dataset.i18n);
  for (const node of document.querySelectorAll('[data-i18n-html]')) node.innerHTML = t(node.dataset.i18nHtml);
  for (const node of document.querySelectorAll('[data-i18n-title]')) node.title = t(node.dataset.i18nTitle);
  for (const node of document.querySelectorAll('[data-i18n-label]')) node.setAttribute('aria-label', t(node.dataset.i18nLabel));
}

// The website's pages in the page's language (tools/build_site.py's
// folders): its home, and the header's links as on the website.
// A link to another of Boréale's sites says where the visit comes from, for
// Umami (workbench's "Mesure d'audience"); the simulator is part of shelf.boreale.dev.
const SITE_TAGS = 'utm_source=shelf&utm_medium=nav';

function homeLink() {
  const home = `${SITE_UP}${language === 'en-US' ? '' : `${language.toLowerCase()}/`}`;
  $('home-link').href = home;
  $('nav-features').href = `${home}#features`;
  $('nav-install').href = `${home}#install`;
  $('nav-docs').href = `${home}docs/getting-started/`;
  // Boréale's other sites, in French where the page is (they all have it).
  const french = language.startsWith('fr');
  for (const link of document.querySelectorAll('#sites [data-site]')) {
    link.href = `${link.dataset.site}${french ? 'fr/' : ''}?${SITE_TAGS}`;
  }
  // The privacy and legal notice, boreale.dev's, in the same folder.
  $('legal-link').href = `https://boreale.dev/${language === 'en-US' ? '' : `${language.toLowerCase()}/`}legal/?${SITE_TAGS}`;
  $('sites-button').setAttribute('aria-label', `PlayShelf · ${t('sites_label')}`);
}

// The header's languages, as on the website (boreale-site's Site.languages()):
// a button with the page's flag and name (its code on a phone), and under it
// their menu (a popover: the browser closes it on Esc or a click elsewhere,
// and gives the focus back to the button; boreale-site's boreale.js opens it
// under the button, the page's language in view), each with its flag, the
// page's marked. Each is a link to this page in it (the URL's settings kept), which
// a click applies (PlayShelf starts again in it); the arrows move through
// them. Without popovers (an old browser), the button does nothing.
function setUpLanguages() {
  const button = $('language-button');
  const menu = $('languages');
  const name = LANGUAGES.find(([code]) => code === language)?.[1] ?? language;
  $('language-flag').textContent = SimLogic.flag(language);
  $('language-name').textContent = name;
  $('language-code').textContent = language.split('-')[0].toUpperCase();
  button.setAttribute('aria-label', `${t('language')} · ${name}`);
  const values = (code) => new Map(current).set('lang', code);
  const links = LANGUAGES.map(([code, label]) => {
    const link = el('a', { href: linkFor(values(code)), 'data-lang': code, lang: code, hreflang: code, 'aria-current': code === language ? 'true' : false },
      el('span', { class: 'flag', 'aria-hidden': 'true', text: SimLogic.flag(code) }), label);
    link.addEventListener('click', (e) => {
      if (e.metaKey || e.ctrlKey || e.shiftKey || e.button) return;
      e.preventDefault();
      menu.hidePopover?.();
      if (code !== language) apply(values(code));
    });
    return link;
  });
  menu.replaceChildren(...links.map((link) => el('li', {}, link)));
  if (!menu.showPopover) return;
  const inUse = () => links.find((a) => a.dataset.lang === language) ?? links[0];
  menu.addEventListener('beforetoggle', (event) => {
    if (event.newState !== 'open') return;
    // The settings as they are now (a live change, the theme): the links follow.
    for (const link of links) link.href = linkFor(values(link.dataset.lang));
  });
  // The arrows: down (or up) on the button opens the menu at the language in
  // use; in the menu, through the two columns, Home and End to either end.
  button.addEventListener('keydown', (e) => {
    if (e.key !== 'ArrowDown' && e.key !== 'ArrowUp') return;
    e.preventDefault();
    menu.showPopover();
    inUse().focus();
  });
  menu.addEventListener('keydown', (e) => {
    const i = links.indexOf(document.activeElement);
    const step = { ArrowRight: 1, ArrowLeft: -1, ArrowDown: 2, ArrowUp: -2 }[e.key];
    let next = null;
    if (step) next = links[i < 0 ? 0 : i + step] ?? null;
    else if (e.key === 'Home') next = links[0];
    else if (e.key === 'End') next = links[links.length - 1];
    else return;
    e.preventDefault();
    next?.focus();
  });
}

function setUpHeader() {
  setUpLanguages();
  const theme = $('theme-select');
  for (const choice of THEMES) theme.append(el('option', { value: choice, text: t(`theme_${choice}`) }));
  theme.value = chosenTheme();
  theme.addEventListener('change', () => chooseTheme(theme.value));
  dark.addEventListener('change', showTheme);
  $('theme-restart').addEventListener('click', () => location.reload());
}

// The build's commit (build.json, written by CI), or '' for a local build.
async function showBuild() {
  const box = $('build');
  try {
    const res = await fetch('build.json', { cache: 'no-cache' });
    if (!res.ok) throw new Error(res.status);
    const b = await res.json();
    const repo = `https://github.com/${b.repo}`;
    const where = b.pr ? el('a', { href: `${repo}/pull/${b.pr}`, text: `PR #${b.pr}` }) : el('span', { text: b.ref });
    const sha = el('a', { href: `${repo}/commit/${b.sha}` }, el('code', { text: b.sha.slice(0, 7) }));
    box.replaceChildren(where, ' · ', sha, b.title ? ` · ${b.title}` : '');
    box.title = b.title ?? '';
    buildInfo = b;
    return b.sha;
  } catch {
    box.textContent = t('unknown_build');
    return '';
  }
}

// The simulator of each release beside main's (releases.json, pages.yml):
// the one chosen opens with the same settings. simulator/ itself says which
// it is in its build.json (a release's: its tag, put there by pages.yml).
async function setUpVersions(build) {
  const field = $('version-field');
  const select = $('version-select');
  const up = VERSION_DIR ? '../' : '';
  let tags = [];
  try {
    const res = await fetch(`${up}releases.json`, { cache: 'no-cache' });
    if (res.ok) tags = (await res.json()).map((r) => r.tag).filter((tag) => /^v\d+\.\d+\.\d+$/.test(tag));
  } catch { /* none: main's alone */ }
  if (!tags.length) return;
  await build;
  const here = VERSION_DIR ?? (tags.includes(buildInfo?.ref) ? buildInfo.ref : 'dev');
  select.append(el('option', { value: 'dev', text: t('version_dev') }), ...tags.map((tag) => el('option', { value: tag, text: tag })));
  select.value = here;
  select.addEventListener('change', () => {
    location.href = `${up}${select.value}/${location.search}`;
  });
  field.hidden = false;
}

function setUpPanel() {
  const layout = document.querySelector('.layout');
  const toggle = $('panel-toggle');
  toggle.addEventListener('click', () => {
    // Narrow, the panel is under the screen: the button goes there.
    if (matchMedia('(max-width: 900px)').matches) { $('panel').scrollIntoView({ behavior: 'smooth' }); return; }
    const open = layout.classList.toggle('no-panel') === false;
    toggle.setAttribute('aria-expanded', String(open));
    if (open) return;
    canvas.focus();
  });
  // The panel's tabs (WAI-ARIA's tabs pattern): the one selected takes the
  // Tab key's stop, ← / → (Home / End) move to the others and show them.
  const tabs = [...document.querySelectorAll('.tab')];
  const select = (tab) => {
    for (const other of tabs) {
      other.setAttribute('aria-selected', String(other === tab));
      other.tabIndex = other === tab ? 0 : -1;
    }
    for (const body of document.querySelectorAll('.tab-body')) body.hidden = body.dataset.body !== tab.dataset.tab;
    if (tab.dataset.tab === 'log') renderLog();
  };
  for (const tab of tabs) {
    tab.addEventListener('click', () => select(tab));
    tab.addEventListener('keydown', (e) => {
      const at = tabs.indexOf(tab);
      const to = { ArrowRight: at + 1, ArrowLeft: at - 1, Home: 0, End: tabs.length - 1 }[e.key];
      if (to === undefined) return;
      e.preventDefault();
      const next = tabs[(to + tabs.length) % tabs.length];
      select(next);
      next.focus();
    });
  }
  $('form').addEventListener('input', markChanges);
  $('form').addEventListener('change', markChanges);
  $('other').addEventListener('input', markChanges);
  $('form').addEventListener('submit', (e) => { e.preventDefault(); apply(readForm()); });
  $('apply').addEventListener('click', () => apply(readForm()));
  $('reset').addEventListener('click', () => apply(keepingHeader(() => false)));
  const keeping = isOn(current.get('keep'));
  $('forget').hidden = !keeping;
  $('forget').addEventListener('click', forget);
  $('copy').addEventListener('click', async () => {
    if (!await handOn(linkFor(readForm()), t('copy'))) return;
    const button = $('copy');
    button.textContent = t('copied');
    setTimeout(() => { button.textContent = t('copy'); }, 1500);
  });
  $('other-add').addEventListener('click', () => {
    const name = $('other-name').value.trim().toLowerCase().replace(/^playshelf_sim_/, '');
    if (!NAME.test(name)) { $('other-name').focus(); return; }
    const values = readForm();
    values.set(name, $('other-value').value.trim() || '1');
    $('other-name').value = $('other-value').value = '';
    buildOther(new Map([...values].filter(([n]) => !known.has(n))));
    markChanges();
  });
  $('shot').addEventListener('click', screenshot);
  $('record').hidden = typeof MediaRecorder === 'undefined' || !canvas.captureStream;
  $('record').addEventListener('click', record);
  $('steps-copy').addEventListener('click', copySteps);
  $('tour').addEventListener('click', () => { if (running && !replaying) replay(TOUR, true); });
  $('report').addEventListener('click', report);
}

async function main() {
  current = readUrl();
  startValues = new Map([...current].filter(([n]) => n !== 'rec' && n !== 'tour'));
  language = SimLogic.consoleLanguage(current.get('lang')) ?? browserLanguage();
  showTheme();
  try {
    const res = await fetch('params.json', { cache: 'no-cache' });
    manifest = await res.json();
  } catch (e) {
    log(`params.json: ${e}`, true);
  }
  known = new Map(manifest.params.map((p) => [p.name, p]));
  await loadTexts(language);
  const build = showBuild();
  translatePage();
  homeLink();
  setUpHeader();
  setUpPanel();
  setUpVersions(build);
  setUpKeys();
  setUpPad();
  setUpFullscreen();
  setUpTools();
  buildPresets();
  buildForm(current);
  version = await build;
  if (await dataSaverWaits()) waitForTap();
  else start();
  showTheme();
}

// Data saver on (navigator.connection.saveData, where the browser says so)
// and PlayShelf not kept yet by the service worker: a few megabytes to
// download, so on a touch, not by itself.
async function dataSaverWaits() {
  if (!navigator.connection?.saveData) return false;
  try {
    const kept = await (await caches.open('playshelf-simulator')).keys();
    return !kept.some((r) => r.url.includes('playshelf.wasm'));
  } catch {
    return true;
  }
}

function waitForTap() {
  const overlay = $('overlay');
  overlay.classList.add('waiting');
  showStatus(t('save_data'));
  const button = el('button', { type: 'button', class: 'btn primary', text: t('start') });
  button.addEventListener('click', () => {
    button.remove();
    overlay.classList.remove('waiting');
    start();
  });
  overlay.append(button);
}

document.body.dataset.playshelf = 'loading';
main();
