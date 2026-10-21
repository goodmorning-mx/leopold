import { useEffect, useRef } from 'react';
import { SduiScreen } from './contract';
import assetManifest from '../shared/asset-manifest.json';

type Props = { screen: SduiScreen; pressed: string | null; timeText: string; weatherFahrenheit: boolean; batteryPercent: number; wifiConnected: boolean; cloudConnected: boolean; bluetoothEnabled: boolean; brightness: number; backgroundMode: number; playing: boolean; backgroundArt?: number; appearanceDark?: boolean; clockBackground?: number; clockFace?: number; clockShowWeather?: boolean; onFramebufferCapture?: (dataUrl: string) => void };
type Module = {
  HEAPU8: Uint8Array;
  _lvgl_wasm_init: () => void;
  _lvgl_wasm_begin: (id: number, background: number) => void;
  _lvgl_wasm_set_asset: (index: number, pointer: number, size: number, width: number, height: number) => void;
  _lvgl_wasm_set_pressed: (id: number) => void;
  _lvgl_wasm_set_state: (...values: number[]) => void;
  _lvgl_wasm_render: () => void;
  _lvgl_wasm_framebuffer: () => number;
  _lvgl_wasm_framebuffer_size: () => number;
  _malloc: (size: number) => number;
  ccall: (name: string, result: string, args: string[], values: unknown[]) => unknown;
};

let modulePromise: Promise<Module> | undefined;
let assetsLoaded: Promise<void> | undefined;

function loadModule() {
  if (!modulePromise) {
    modulePromise = fetch(`/wasm/lvgl.js?${Date.now()}`).then((response) => response.text()).then(async (source) => {
      const url = URL.createObjectURL(new Blob([source], { type: 'text/javascript' }));
      const loaded = await import(/* @vite-ignore */ url) as { default: (options: { locateFile: (path: string) => string }) => Promise<Module> };
      URL.revokeObjectURL(url);
      const module = await loaded.default({ locateFile: (path) => `/wasm/${path}` });
      module._lvgl_wasm_init();
      return module;
    });
  }
  return modulePromise;
}

function rgb565(value: string) {
  const rgb = Number.parseInt(value.replace('#', ''), 16) || 0;
  return (((rgb >> 19) & 0x1f) << 11) | (((rgb >> 10) & 0x3f) << 5) | ((rgb >> 3) & 0x1f);
}

function callString(module: Module, name: string, values: unknown[]) {
  module.ccall(name, 'void', values.map((value) => typeof value === 'string' ? 'string' : 'number'), values);
}

async function loadAssets(module: Module) {
  if (!assetsLoaded) {
    assetsLoaded = Promise.all(Object.entries(assetManifest.assets).map(async ([key, asset], index) => {
      const assetIndex = [
        'home', 'flower', 'chevron', 'open-clock', 'open-weather', 'open-navigation',
        'open-whatsapp', 'open-spotify', 'spotify-large', 'settings-neutral', 'settings-pressed-border-mask',
        'settings-wifi-pressed-icon-mask', 'settings-wifi-pressed-chevron-mask',
        'settings-bluetooth-pressed-icon-mask', 'settings-bluetooth-pressed-chevron-mask',
        'settings-display-pressed-icon-mask', 'settings-display-pressed-chevron-mask',
        'settings-sound-pressed-icon-mask', 'settings-sound-pressed-chevron-mask',
        'settings-status-bluetooth-active',
        'settings-status-wifi-active', 'settings-status-cloud-active', 'settings-gears-base', 'settings-gears-wallpaper',
        'background-flower', 'background-chevron', 'background-mechanical-flower', 'home-controls-transparent',
        'display-icon-background', 'display-icon-appearance', 'display-icon-clock', 'display-icon-units',
        'display-icon-language', 'display-icon-weather-location', 'display-icon-display', 'settings-status-banner',
        'background-preview-flower', 'background-preview-chevron', 'background-preview-mechanical',
        'clock-background-preview-flower', 'clock-background-preview-chevron', 'clock-background-preview-mechanical',
        'clock-face-preview-0', 'clock-face-preview-1', 'clock-face-preview-2',
      ].indexOf(key);
      if (assetIndex < 0) return;
      const bytes = new Uint8Array(await (await fetch(`/firmware-assets/${asset.file}`)).arrayBuffer());
      const pointer = module._malloc(bytes.byteLength);
      module.HEAPU8.set(bytes, pointer);
      module._lvgl_wasm_set_asset(assetIndex, pointer, bytes.byteLength, asset.width, asset.height);
    })).then(() => undefined);
  }
  return assetsLoaded;
}

function paint(canvas: HTMLCanvasElement, module: Module) {
  const pointer = module._lvgl_wasm_framebuffer();
  const bytes = module.HEAPU8.subarray(pointer, pointer + module._lvgl_wasm_framebuffer_size());
  const image = new ImageData(466, 466);
  for (let index = 0; index < 466 * 466; index += 1) {
    const pixel = bytes[index * 2] | bytes[index * 2 + 1] << 8;
    image.data[index * 4] = Math.round(((pixel >> 11) & 0x1f) * 255 / 31);
    image.data[index * 4 + 1] = Math.round(((pixel >> 5) & 0x3f) * 255 / 63);
    image.data[index * 4 + 2] = Math.round((pixel & 0x1f) * 255 / 31);
    image.data[index * 4 + 3] = 255;
  }
  canvas.getContext('2d')?.putImageData(image, 0, 0);
}

export default function FirmwareLvglCanvas({ screen, pressed, timeText, weatherFahrenheit, batteryPercent, wifiConnected, cloudConnected, bluetoothEnabled, brightness, backgroundMode, playing, backgroundArt = 255, appearanceDark = true, clockBackground = 255, clockFace = 0, clockShowWeather = false, onFramebufferCapture }: Props) {
  const canvas = useRef<HTMLCanvasElement>(null);
  useEffect(() => {
    let active = true;
    void loadModule().then(async (module) => {
      await loadAssets(module);
      if (!active || !canvas.current) return;
      const backgroundLabel = backgroundMode === 0 ? 'BACKGROUND: DARK' : backgroundMode === 2 ? 'BACKGROUND: CHEVRON' : 'BACKGROUND: ORIGINAL';
      const effectiveScreen = {
        ...screen,
        components: screen.components.map((component) => {
          if (component.type === 'button' && component.id === 'background-toggle') return { ...component, label: backgroundLabel };
          if (component.type !== 'text') return component;
          if (component.id === 'sky-time') return { ...component, text: timeText };
          if (component.id === 'weather-temp') return { ...component, text: weatherFahrenheit ? '72 F' : '22 C' };
          return component;
        }),
      };
      callString(module, 'lvgl_wasm_begin', [effectiveScreen.id, rgb565(effectiveScreen.background)]);
      effectiveScreen.components.forEach((component) => {
        if (component.type === 'text') callString(module, 'lvgl_wasm_add_text', [component.text, component.x, component.y, component.size, rgb565(component.color), component.align === 'center' ? 1 : component.align === 'right' ? 2 : 0]);
        if (component.type === 'button') callString(module, 'lvgl_wasm_add_button', [component.id, component.label, component.icon ?? '', component.x, component.y, component.width, component.height, rgb565(component.color ?? '#FFFFFF')]);
        if (component.type === 'slider') callString(module, 'lvgl_wasm_add_slider', [component.id, component.x, component.y, component.width, component.height, component.value, 100, rgb565(component.color)]);
      });
      callString(module, 'lvgl_wasm_set_pressed', [pressed ?? '']);
      callString(module, 'lvgl_wasm_set_state', [timeText, batteryPercent, wifiConnected ? 1 : 0, cloudConnected ? 1 : 0, bluetoothEnabled ? 1 : 0, bluetoothEnabled ? 1 : 0, brightness, backgroundMode, backgroundArt, appearanceDark ? 1 : 0, clockBackground, clockFace, clockShowWeather ? 1 : 0]);
      module._lvgl_wasm_render();
      paint(canvas.current, module);
      const dataUrl = canvas.current.toDataURL('image/png');
      canvas.current.dataset.framebufferPng = dataUrl;
      onFramebufferCapture?.(dataUrl);
    });
    return () => { active = false; };
  }, [screen, pressed, timeText, weatherFahrenheit, batteryPercent, wifiConnected, cloudConnected, bluetoothEnabled, brightness, backgroundMode, playing, backgroundArt, appearanceDark, clockBackground, clockFace, clockShowWeather, onFramebufferCapture]);
  return <canvas ref={canvas} width={466} height={466} className="firmware-canvas" aria-label="LVGL firmware framebuffer" />;
}
