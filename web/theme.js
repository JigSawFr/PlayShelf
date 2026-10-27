// The simulator's theme before the first paint (simulator.js keeps it up to
// date): the URL's, else the one chosen on an earlier visit, else dark, as
// PlayShelf starts on a console (Auto: the system's). A file of its own, not inline: the page's Content-Security-Policy
// runs no inline script.
(function () {
  var known = function (c) { return c === 'auto' || c === 'light' || c === 'dark'; };
  var choice = new URLSearchParams(location.search).get('theme');
  if (!known(choice)) {
    try { choice = localStorage.getItem('ps-theme'); } catch (e) { choice = null; }
  }
  if (!known(choice)) choice = 'dark';
  // The choice (the header's icon on a phone), then the theme it gives.
  document.documentElement.dataset.themeChoice = choice;
  if (choice === 'auto') choice = matchMedia('(prefers-color-scheme: dark)').matches ? 'dark' : 'light';
  document.documentElement.dataset.theme = choice;
  // The browser's bar in the same colour as the header (simulator.js'
  // barColour()).
  var bar = document.querySelector('meta[name="theme-color"]');
  if (bar) bar.setAttribute('content', choice === 'dark' ? '#111d31' : '#ffffff');
})();
