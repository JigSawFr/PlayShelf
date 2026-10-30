// The web simulator's page and the rule "liens sortants anonymes": the page
// sends no referrer, a link to a third party has rel="noreferrer", one to
// boreale.dev, *.boreale.dev or a repository of github.com/JigSawFr/ keeps the
// origin alone (referrerpolicy="strict-origin"). `node --test tests/web_page/*.test.js`.
'use strict';

const test = require('node:test');
const assert = require('node:assert/strict');
const path = require('node:path');
const fs = require('node:fs');

const L = require('../../web/sim_logic.js');
const own = L.ownSite;

test('own sites, and look-alikes that are not', () => {
  for (const href of ['https://boreale.dev/', 'https://guard.boreale.dev/fr/', 'https://github.com/JigSawFr/PlayShelf']) {
    assert.ok(own(href), href);
  }
  for (const href of ['https://github.com/JigSawFr', 'https://github.com/jigsawfrx/x', 'https://gist.github.com/JigSawFr/1',
    'https://boreale.dev.example.com/', 'https://notboreale.dev/', 'http://boreale.dev/']) {
    assert.ok(!own(href), href);
  }
});

test('the attributes a link built by the page takes', () => {
  assert.deepEqual(L.linkOutAttributes('https://github.com/JigSawFr/X/pull/1'), { referrerpolicy: 'strict-origin' });
  assert.deepEqual(L.linkOutAttributes('https://github.com/someone/X/pull/1'), { rel: 'noreferrer' });
  assert.deepEqual(L.linkOutAttributes('../docs/'), {});
  assert.equal(own('not a url'), false);
});

test('the page sends no referrer, but the origin to our own sites', () => {
  const page = fs.readFileSync(path.join(__dirname, '../../web/index.html'), 'utf8');
  assert.match(page, /<meta name="referrer" content="no-referrer">/);
  const links = [...page.matchAll(/<a\b[^>]*\bhref="(https?:\/\/[^"]*)"[^>]*>/g)];
  assert.ok(links.length > 0);
  for (const [tag, href] of links) {
    const rel = (/\brel="([^"]*)"/.exec(tag) || [, ''])[1].split(/\s+/);
    if (own(href)) {
      assert.ok(!rel.includes('noreferrer') && /\breferrerpolicy="strict-origin"/.test(tag), tag);
    } else {
      assert.ok(rel.includes('noreferrer'), tag);
    }
  }
});

test('a link to another of our sites says it comes from shelf.boreale.dev, no other has UTM tags', () => {
  const page = fs.readFileSync(path.join(__dirname, '../../web/index.html'), 'utf8');
  for (const [, raw] of page.matchAll(/<a\b[^>]*\bhref="(https?:\/\/[^"]*)"/g)) {
    const u = new URL(raw.replace(/&amp;/g, '&'));
    const ours = u.hostname === 'boreale.dev' || u.hostname.endsWith('.boreale.dev');
    if (ours && u.hostname !== 'shelf.boreale.dev') {
      assert.equal(u.searchParams.get('utm_source'), 'shelf', raw);
      assert.equal(u.searchParams.get('utm_medium'), 'nav', raw);
    } else {
      assert.ok(![...u.searchParams.keys()].some((k) => k.startsWith('utm_')), raw);
    }
  }
});
