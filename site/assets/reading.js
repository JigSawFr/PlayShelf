// The documentation read on a light ground, when chosen before (site.js's
// "Light reading" button), set before the first paint so the page does not
// flash dark first. A file of its own: the pages' Content-Security-Policy
// runs no inline script.
(function () {
  var light = null;
  try { light = localStorage.getItem('ps-reading'); } catch (e) { /* storage refused */ }
  if (light === 'light') document.documentElement.dataset.reading = 'light';
})();
