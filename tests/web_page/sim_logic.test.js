// Tests for web/sim_logic.js, the web simulator's page logic that touches
// neither the page nor PlayShelf: `node --test tests/web_page/*.test.js`
// (CI's host tests). The page itself runs in CI's headless Chromium
// (tools/web_smoke.py).
'use strict';

const test = require('node:test');
const assert = require('node:assert/strict');
const path = require('node:path');
const fs = require('node:fs');

const L = require('../../web/sim_logic.js');

const manifest = JSON.parse(fs.readFileSync(path.join(__dirname, '../../web/params.json'), 'utf8'));
const known = new Map(manifest.params.map((p) => [p.name, p]));

test('every language has its flag, as on the website', () => {
  assert.deepEqual(Object.keys(L.FLAGS).sort(), L.LANGUAGES.map(([code]) => code).sort());
  assert.equal(L.flag('fr'), '\u{1F1EB}\u{1F1F7}');
  assert.equal(L.flag('es-419'), '\u{1F1F2}\u{1F1FD}');
  assert.equal(L.flag('zh-Hant'), '\u{1F1F9}\u{1F1FC}');
  assert.equal(L.flag('xx'), '');
  // The website's (tools/build_site.py's FLAGS), the same.
  const site = fs.readFileSync(path.join(__dirname, '../../tools/build_site.py'), 'utf8');
  const pairs = /^FLAGS = \{([^}]*)\}/m.exec(site)[1].matchAll(/"([^"]+)": "([A-Z]{2})"/g);
  assert.deepEqual(Object.fromEntries([...pairs].map(([, c, f]) => [c, f])), L.FLAGS);
});

test('a browser language to one of the console\'s', () => {
  assert.equal(L.consoleLanguage('fr-CA'), 'fr-CA');
  assert.equal(L.consoleLanguage('fr-BE'), 'fr');
  assert.equal(L.consoleLanguage('en-GB'), 'en-US');
  assert.equal(L.consoleLanguage('es'), 'es');
  assert.equal(L.consoleLanguage('es-ES'), 'es');
  assert.equal(L.consoleLanguage('es-MX'), 'es-419');
  assert.equal(L.consoleLanguage('pt-BR'), 'pt-BR');
  assert.equal(L.consoleLanguage('pt-PT'), 'pt');
  assert.equal(L.consoleLanguage('zh-TW'), 'zh-Hant');
  assert.equal(L.consoleLanguage('zh-Hant-HK'), 'zh-Hant');
  assert.equal(L.consoleLanguage('zh-CN'), 'zh-Hans');
  assert.equal(L.consoleLanguage('ZH-HANS'), 'zh-Hans');
  assert.equal(L.consoleLanguage('ja-JP'), 'ja');
  assert.equal(L.consoleLanguage('pl'), null);
  assert.equal(L.consoleLanguage(undefined), null);
  assert.equal(L.firstConsoleLanguage(['pl', 'cs', 'de-AT', 'fr']), 'de');
  assert.equal(L.firstConsoleLanguage(['pl']), 'en-US');
  // Every language of the console is one the page offers.
  assert.deepEqual(L.LANGUAGES.map(([code]) => code).sort(), [...known.get('lang').options].sort());
});

test('a flag is off as core/sim_env.h says', () => {
  for (const off of [undefined, null, '', '0', 'false', 'OFF', 'No']) assert.equal(L.isOn(off), false, String(off));
  for (const on of ['1', 'yes', 'true', 'on', 'anything']) assert.equal(L.isOn(on), true, on);
});

test('the URL: only what is set, in the panel\'s order, flags as 1', () => {
  const values = new Map([['zzz', 'x'], ['applet', 'yes'], ['fw', '20.5.0'], ['emummc', '0'], ['now', '']]);
  assert.equal(L.toQuery(values, known), '?fw=20.5.0&applet=1&zzz=x');
  assert.equal(L.toQuery(new Map(), known), '');
  // Read back: names in lower case, the others left out.
  const read = L.readQuery('?APPLET=1&fw=20.5.0&bad-name=1&x%20y=2');
  assert.deepEqual([...read], [['applet', '1'], ['fw', '20.5.0']]);
  // What changed between two sets of settings, as their URLs say.
  const a = new Map([['applet', '1'], ['speed', '300']]);
  const b = new Map([['applet', 'true'], ['speed', '600'], ['battery', '1']]);
  assert.deepEqual(L.changedNames(a, b, known).sort(), ['battery', 'speed']);
});

test('the variables and config.json PlayShelf starts with', () => {
  const values = new Map([
    ['lang', 'de'], ['theme', 'dark'], ['tab', 'catalogue'], ['rec', 'down'], ['applet', 'yes'],
    ['emummc', '0'], ['fw', '20.5.0'], ['no_demo', '1'], ['keep', '1'], ['unknown_one', 'v'],
  ]);
  const { env, config } = L.startEnvironment(values, known, 'fr', 'light');
  // The page's language and theme, not the URL's: the page already chose.
  assert.deepEqual(config, { language: 'fr', theme: 'light', start_tab: 'catalogue' });
  assert.equal(env.PLAYSHELF_SIM_APPLET, '1');
  assert.equal(env.PLAYSHELF_SIM_EMUMMC, undefined);
  assert.equal(env.PLAYSHELF_SIM_FW, '20.5.0');
  assert.equal(env.PLAYSHELF_SIM_UNKNOWN_ONE, 'v');   // a newer build's variable still reaches it
  // The page's own: no variable.
  for (const own of ['REC', 'NO_DEMO', 'KEEP']) assert.equal(env[`PLAYSHELF_SIM_${own}`], undefined, own);
  assert.equal(L.envValue(known.get('applet'), 'off'), '');
  assert.equal(L.envName('dev_builds'), 'PLAYSHELF_SIM_DEV_BUILDS');
});

test('steps: read before any is played', () => {
  assert.deepEqual(L.parseSteps('down*3.w15.t640,360.s:skip=3600.say:end.a').map((s) => {
    const { step, ...rest } = s;
    return rest;
  }), [
    { button: 'down', times: 3 }, { wait: 1500 }, { touch: [640, 360] }, { set: ['skip', '3600'] }, { say: 'end' },
    { button: 'a', times: 1 },
  ]);
  assert.deepEqual(L.parseSteps(''), []);
  assert.deepEqual(L.parseSteps('..a..'), [{ button: 'a', times: 1, step: 'a' }]);
  for (const bad of ['jump', 'w', 't1', 's:Bad=1', 'say:', 's:fw=%E0%A4%A']) assert.throws(() => L.parseSteps(`a.${bad}`), /no such step/, bad);
  // A live value with dots and spaces goes through a step and back.
  const step = `s:${L.stepValue('fw', '20.5.0 x')}`;
  assert.ok(!step.slice(2).includes('.'));
  assert.deepEqual(L.parseSteps(step)[0].set, ['fw', '20.5.0 x']);
  assert.deepEqual(L.parseSteps(`s:${L.stepValue('skip', '')}`)[0].set, ['skip', '']);
  // Noted steps written back: a button pressed in a row, once with *N.
  assert.equal(L.stepsText(['down', 'down', 'down', 'w5', 'a', 'a', 's:skip=60', 'say:end']), 'down*3.w5.a*2.s:skip=60.say:end');
  // Every button a step names is one the page presses.
  assert.equal(L.STEP_BUTTONS.length, 14);
});
