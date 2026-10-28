// Renders branding/*.svg to the raster files the build and the README use, as
// PlayGuard's tools/render_branding.mjs does (Aurore's sky, Sora):
//   icon.jpg (256x256, the NRO's icon: full-bleed, the HOME menu rounds it),
//   resources/img/wordmark.png and wordmark-light.png (720x176, transparent:
//   the loading screen's, dark and light), .github/readme/banner.png (1280x320)
//   and .github/readme/social-preview.png (1280x640, the repository's social
//   preview, uploaded by hand in Settings › General), images/apple-touch-icon.png
//   (180x180: an iPhone's home screen, the web simulator's) and
//   images/icon-192.png and images/icon-512.png (Android's home screen and
//   splash, from the simulator's manifest; maskable as they are: the logo is
//   inside the middle 80 % Android may crop to).
// <image href="icon.svg" …> in an SVG becomes the icon itself, its corners rounded.
// Needs Node and Playwright (Chromium); loads Sora from Google Fonts.
//   node tools/render_branding.mjs [out-prefix…]   (only the files whose path starts so)
import { chromium } from 'playwright';
import { readFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const root = join(dirname(fileURLToPath(import.meta.url)), '..');
const jobs = [
  { svg: 'branding/icon.svg', out: 'icon.jpg', w: 256, h: 256, type: 'jpeg', scale: 1 },
  { svg: 'branding/wordmark.svg', out: 'resources/img/wordmark.png', w: 720, h: 176, type: 'png', scale: 1, transparent: true },
  { svg: 'branding/wordmark-light.svg', out: 'resources/img/wordmark-light.png', w: 720, h: 176, type: 'png', scale: 1, transparent: true },
  { svg: 'branding/banner.svg', out: '.github/readme/banner.png', w: 1280, h: 320, type: 'png', scale: 2 },
  { svg: 'branding/social-preview.svg', out: '.github/readme/social-preview.png', w: 1280, h: 640, type: 'png', scale: 1 },
  { svg: 'branding/icon.svg', out: 'images/apple-touch-icon.png', w: 180, h: 180, type: 'png', scale: 1 },
  { svg: 'branding/icon.svg', out: 'images/icon-192.png', w: 192, h: 192, type: 'png', scale: 1 },
  { svg: 'branding/icon.svg', out: 'images/icon-512.png', w: 512, h: 512, type: 'png', scale: 1 },
];

const iconTile = readFileSync(join(root, 'branding/icon.svg'), 'utf8').replace(/<\/?svg[^>]*>/g, '');
const inline = (svg) => svg.replace(/<image href="icon\.svg" x="([\d.]+)" y="([\d.]+)" width="([\d.]+)" height="[\d.]+"\/>/g,
  (_, x, y, w) => `<svg x="${x}" y="${y}" width="${w}" height="${w}" viewBox="0 0 256 256"><clipPath id="icon-round"><rect width="256" height="256" rx="56"/></clipPath><g clip-path="url(#icon-round)">${iconTile}</g></svg>`);

const browser = await chromium.launch(process.env.CHROMIUM_PATH ? { executablePath: process.env.CHROMIUM_PATH } : {});
const only = process.argv.slice(2);
for (const job of jobs.filter((j) => !only.length || only.some((p) => j.out.startsWith(p)))) {
  const page = await browser.newPage({ viewport: { width: job.w, height: job.h }, deviceScaleFactor: job.scale });
  const svg = inline(readFileSync(join(root, job.svg), 'utf8'));
  await page.setContent(`<!doctype html><html><head>
<link href="https://fonts.googleapis.com/css2?family=Sora:wght@300;400;700&display=block" rel="stylesheet">
<style>html,body{margin:0;overflow:hidden;background:transparent}svg{display:block;width:100vw;height:100vh}</style></head><body>${svg}</body></html>`,
    { waitUntil: 'networkidle' });
  await page.evaluate(async () => {
    await document.fonts.load('700 30px Sora');
    await document.fonts.load('400 20px Sora');
    await document.fonts.ready;
  });
  if (!(await page.evaluate(() => document.fonts.check('700 30px Sora')))) throw new Error('Sora font did not load');
  await page.screenshot({ path: join(root, job.out), type: job.type, quality: job.type === 'jpeg' ? 95 : undefined,
                          omitBackground: !!job.transparent });
  await page.close();
  console.log(`${job.svg} -> ${job.out}`);
}
await browser.close();
