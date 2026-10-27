// The web simulator's service worker (web/README.md), loaded by the page as a
// plain script too. Two jobs:
// - the COOP / COEP headers GitHub Pages cannot send, which threads need (as
//   coi-serviceworker did, MIT, Guido Zuidhof: https://github.com/gzuidhof/coi-serviceworker);
// - a cache: PlayShelf's files (playshelf.js, .wasm, .data and cjk.otf, asked
//   for with the commit in their URL, ?v=) are taken from it once fetched, so
//   a visit after the first starts at once; the page's own files come from
//   the network, from the cache when there is none (offline).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
'use strict';

if (typeof window === 'undefined') {
  // The site's origin holds several simulators (simulator/, its releases',
  // each pull request's pr-<n>/simulator/): each worker keeps its own cache,
  // named after its scope, and keeps only what is under it, so one
  // deployment's files are never served for another's.
  const SCOPE = new URL(self.registration.scope).pathname;
  const CACHE = `playshelf-simulator:${SCOPE}`;
  // A commit's files never change: one copy per file, the newest commit's.
  const BUILD_FILE = /\/(playshelf\.(js|wasm|data)|cjk\.otf)$/;

  self.addEventListener('install', () => self.skipWaiting());
  // The one cache every deployment shared before: gone.
  self.addEventListener('activate', (event) =>
    event.waitUntil(caches.delete('playshelf-simulator').then(() => self.clients.claim())));

  // The headers a cross-origin isolated page and its threads need.
  const isolated = (response) => {
    if (response.status === 0) return response;
    const headers = new Headers(response.headers);
    headers.set('Cross-Origin-Embedder-Policy', 'require-corp');
    headers.set('Cross-Origin-Resource-Policy', 'cross-origin');
    headers.set('Cross-Origin-Opener-Policy', 'same-origin');
    return new Response(response.body, { status: response.status, statusText: response.statusText, headers });
  };

  // Another commit's copies of the same file go.
  const prune = async (cache, url) => {
    for (const request of await cache.keys()) {
      const u = new URL(request.url);
      if (u.pathname === url.pathname && u.search !== url.search) await cache.delete(request);
    }
  };

  const respond = async (request, url) => {
    // Another deployment's file: from the network, never kept.
    if (!url.pathname.startsWith(SCOPE)) return fetch(request);
    const cache = await caches.open(CACHE);
    if (BUILD_FILE.test(url.pathname) && url.searchParams.has('v')) {
      const hit = await cache.match(request);
      if (hit) return hit;
      const response = await fetch(request);
      if (response.ok) {
        await cache.put(request, response.clone());
        prune(cache, url);
      }
      return response;
    }
    try {
      const response = await fetch(request);
      if (response.ok) cache.put(request, response.clone());
      return response;
    } catch (e) {
      // Offline: the page whatever its settings (?unlocked=1, …), the rest as asked for.
      const hit = await cache.match(request, { ignoreSearch: request.mode === 'navigate' });
      if (hit) return hit;
      throw e;
    }
  };

  self.addEventListener('fetch', (event) => {
    const request = event.request;
    const url = new URL(request.url);
    // The page loads nothing from elsewhere (its Content-Security-Policy).
    if (request.method !== 'GET' || url.origin !== self.location.origin) return;
    if (request.cache === 'only-if-cached' && request.mode !== 'same-origin') return;
    event.respondWith(respond(request, url).then(isolated));
  });
} else {
  // The page: registers the worker, then, when the server did not make the
  // page cross-origin isolated (GitHub Pages), reloads it once the worker
  // controls it, so its headers apply. Once only: if they still do not, the
  // page says so (simulator.js, no_isolation).
  (() => {
    const sw = navigator.serviceWorker;
    if (!sw || !window.isSecureContext) return;
    let reloaded = false;
    try {
      reloaded = sessionStorage.getItem('ps-sw-reloaded') === '1';
      sessionStorage.removeItem('ps-sw-reloaded');
    } catch { /* storage refused */ }
    sw.register(document.currentScript.src).then(() => sw.ready).then(() => {
      if (window.crossOriginIsolated || reloaded) return;
      // Without storage to remember it, no reload: it could loop.
      try { sessionStorage.setItem('ps-sw-reloaded', '1'); } catch { return; }
      location.reload();
    }, (e) => console.error('service worker:', e));
  })();
}
