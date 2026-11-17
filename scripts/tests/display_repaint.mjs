// Exercise actual LVGL pixels, including alpha-bearing wallpaper assets.
import assert from 'node:assert/strict';
import fs from 'node:fs';
import { pathToFileURL } from 'node:url';

const repo = process.argv[2];
const wasm = repo + '/frontend/public/wasm/lvgl';
const factory = (await import(pathToFileURL(wasm + '.js'))).default;
const module = await factory({
  instantiateWasm(imports, receive) {
    WebAssembly.instantiate(fs.readFileSync(wasm + '.wasm'), imports)
      .then(result => receive(result.instance));
    return {};
  },
});
module._lvgl_wasm_init();

const keys = JSON.parse(fs.readFileSync(repo + '/shared/wasm-asset-index.json')).keys;
const assets = JSON.parse(fs.readFileSync(repo + '/shared/asset-manifest.json')).assets;
const loaded = new Set();
keys.forEach((key, index) => {
  const asset = assets[key];
  if (!asset) return;
  const path = repo + '/shared/assets/' + asset.file;
  if (!fs.existsSync(path)) return;
  const bytes = fs.readFileSync(path);
  const pointer = module._malloc(bytes.length);
  module.HEAPU8.set(bytes, pointer);
  module._lvgl_wasm_set_asset(index, pointer, bytes.length, asset.width, asset.height);
  loaded.add(key);
});
for (const key of ['background-flower', 'background-chevron',
  'background-mechanical-flower', 'background-black-white-circle']) {
  assert(loaded.has(key), 'Missing wallpaper: ' + key);
}

function call(name, values) {
  return module.ccall(name, 'number',
    values.map(value => typeof value === 'string' ? 'string' : 'number'), values);
}

function rgb565(color) {
  if (typeof color === 'number') return color;
  return ((parseInt(color.slice(1, 3), 16) >> 3) << 11)
    | ((parseInt(color.slice(3, 5), 16) >> 2) << 5)
    | (parseInt(color.slice(5, 7), 16) >> 3);
}

function render(background, expanded = 'background') {
  const values = [expanded, 0, background, 0, 1, 4, 1, 0, 0, 0,
    '', '', '', 1, 0, 0, 0, 0, ''];
  const model = JSON.parse(module.ccall('lvgl_wasm_display_model', 'string',
    values.map(value => typeof value === 'string' ? 'string' : 'number'), values));
  const screen = model.screen;
  assert.equal(screen.id, 'device-additional-options');
  call('lvgl_wasm_begin', [screen.id, rgb565(screen.background)]);
  for (const component of screen.components) {
    if (component.type === 'text') {
      call('lvgl_wasm_add_text', [component.id, component.text, component.x,
        component.y, component.size, rgb565(component.color),
        component.align === 'center' ? 1 : component.align === 'right' ? 2 : 0]);
    } else if (component.type === 'button') {
      call('lvgl_wasm_add_button', [component.id, component.label, component.icon ?? '',
        component.x, component.y, component.width, component.height,
        rgb565(component.color ?? '#FFFFFF')]);
    }
  }
  call('lvgl_wasm_set_state', ['10:00', 'NOV 7', 80, 0, 1, 0, 1, 0,
    75, 1, background, 1, 0, 0, 0, 4]);
  module._lvgl_wasm_render();
  const pointer = module._lvgl_wasm_framebuffer();
  return Buffer.from(module.HEAPU8.slice(pointer,
    pointer + module._lvgl_wasm_framebuffer_size()));
}

const results = [];
for (let background = 0; background < 4; background++) {
  render(3, '');
  const first = render(background);
  const second = render(background);
  assert(first.some(byte => byte !== 0), 'Rendered frame must not be blank');
  let changedPixels = 0;
  for (let offset = 0; offset < first.length; offset += 2) {
    if (first.readUInt16LE(offset) !== second.readUInt16LE(offset)) changedPixels++;
  }
  results.push({ background, changed_pixels_on_identical_redraw: changedPixels });
}
console.log(JSON.stringify(results));
