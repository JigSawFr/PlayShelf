// The documentation read on a light ground, when chosen before (site.js's
// "Light reading" button), set before the first paint so the page does not
// flash dark first. A file of its own: the pages' Content-Security-Policy
// runs no inline script.
(function () {
  var light = null;
  try { light = localStorage.getItem('ps-reading'); } catch (e) { /* storage refused */ }
  if (light === 'light') document.documentElement.dataset.reading = 'light';
})();

// The language a visitor chose before boreale-site's lang.js, which keeps it
// as "boreale-lang", the menu's data-lang (the folder, "en-US" for English):
// the old "pg-site-lang" (a code: "fr-CA", "pt-BR", "en-US") moved there
// once, then removed. Here rather than in site.js: this file runs in <head>
// before lang.js, whose redirects read the choice.
// To delete after 2027-01-31 (workbench plans/playshelf/suivi.md).
(function () {
  try {
    var old = localStorage.getItem('pg-site-lang');
    if (old === null) return;
    var code = old.toLowerCase();
    var known = ['en-us', 'fr', 'fr-ca', 'de', 'es', 'es-419', 'it', 'nl', 'pt', 'pt-br', 'ru', 'ja', 'ko',
      'zh-hans', 'zh-hant'];
    var folder = code === 'en' || code === 'en-us' ? 'en-US' : known.indexOf(code) >= 0 ? code : null;
    if (folder && localStorage.getItem('boreale-lang') === null) localStorage.setItem('boreale-lang', folder);
    localStorage.removeItem('pg-site-lang');
  } catch (e) { /* storage refused: nothing kept */ }
})();
