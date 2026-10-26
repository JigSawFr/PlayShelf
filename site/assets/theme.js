// The website's theme before the first paint (site.js keeps it up to date):
// the one chosen on an earlier visit, here or in the simulator, else the
// system's. A file of its own, not inline: the pages' Content-Security-Policy
// runs no inline script.
(function () {
  var c = null;
  try { c = localStorage.getItem('ps-theme'); } catch (e) { /* storage refused */ }
  // The choice (the header's icon on a phone), then the theme it gives.
  document.documentElement.dataset.themeChoice = c === 'light' || c === 'dark' ? c : 'auto';
  if (c !== 'light' && c !== 'dark') c = matchMedia('(prefers-color-scheme: dark)').matches ? 'dark' : 'light';
  document.documentElement.dataset.theme = c;
  // The browser's bar the colour of the header, in the theme chosen.
  var bar = c === 'dark' ? '#111d31' : '#ffffff';
  var metas = document.querySelectorAll('meta[name="theme-color"]');
  for (var i = 0; i < metas.length; i++) metas[i].content = bar;
})();
