// The web simulator's page and the rule "liens sortants anonymes": the page
// sends no referrer, a link to a third party has rel="noreferrer", one to
// boreale.dev, *.boreale.dev or a repository of github.com/JigSawFr/ keeps the
// origin alone (referrerpolicy="strict-origin"). `node --test tests/web_page/*.test.js`.
'use strict';

const test = require('node:test');
const assert = require('node:assert/strict');
const path = require('node:path');
const fs = require('node:fs');

const own = (href) => {
  const u = new URL(href);
  const host = u.hostname.toLowerCase();
  return u.protocol === 'https:' && (host === 'boreale.dev' || host.endsWith('.boreale.dev') ||
    (host === 'github.com' && u.pathname.toLowerCase().startsWith('/jigsawfr/')));
};

test('own sites, and look-alikes that are not', () => {
  for (const href of ['https://boreale.dev/', 'https://guard.boreale.dev/fr/', 'https://github.com/JigSawFr/PlayShelf']) {
    assert.ok(own(href), href);
  }
  for (const href of ['https://github.com/JigSawFr', 'https://github.com/jigsawfrx/x', 'https://gist.github.com/JigSawFr/1',
    'https://boreale.dev.example.com/', 'https://notboreale.dev/', 'http://boreale.dev/']) {
    assert.ok(!own(href), href);
  }
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
