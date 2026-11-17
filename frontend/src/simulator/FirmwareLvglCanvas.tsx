import { useEffect, useRef } from 'react';
import { loadFirmwareModule as loadModule, FirmwareModule as Module } from './firmwareModule';
import { SduiScreen } from './contract';
import assetManifest from '../shared/asset-manifest.json';
import assetDelivery from '../shared/asset-delivery.json';
import wasmAssetIndex from '../shared/wasm-asset-index.json';
import { ACCENT_NAMES, AccentName } from './theme';

type LifecyclePreview = { kind: 0 | 1 | 2 | 3; step: number };
type Props = { screen: SduiScreen; pressed: string | null; timeText: string; dateText: string; clockSecond: number; weatherFahrenheit: boolean; batteryPercent: number; wifiConnected: boolean; cloudConnected: boolean; bluetoothEnabled: boolean; bluetoothConnected: boolean; brightness: number; backgroundMode: number; playing: boolean; backgroundArt?: number; appearanceDark?: boolean; accentColor?: AccentName; clockBackground?: number; clockFace?: number; clockShowWeather?: boolean; lifecyclePreview?: LifecyclePreview; onFramebufferCapture?: (dataUrl: string) => void; onAssetSelectionResult?: (kind: 'global' | 'clock', index: number, success: boolean) => void };
const assetPromises = new Map<string, Promise<void>>();
const assetPointers = new Map<string, number>();
const WASM_ASSET_KEYS: readonly string[] = wasmAssetIndex.keys;


function rgb565(value: string) {
  const rgb = Number.parseInt(value.replace('#', ''), 16) || 0;
  return (((rgb >> 19) & 0x1f) << 11) | (((rgb >> 10) & 0x3f) << 5) | ((rgb >> 3) & 0x1f);
}

function callString(module: Module, name: string, values: unknown[]) {
  module.ccall(name, 'void', values.map((value) => typeof value === 'string' ? 'string' : 'number'), values);
}

function requiredAssets(screen: SduiScreen, backgroundArt: number, clockBackground: number, wifiConnected: boolean, cloudConnected: boolean, bluetoothConnected: boolean) {
  const keys = new Set<string>();
  const dependencies = (assetDelivery.dependencies as Record<string, string[]>)[screen.id] ?? [];
  dependencies.forEach((key) => { if (WASM_ASSET_KEYS.includes(key)) keys.add(key); });
  if (screen.id !== 'sky-clock') ['status-cloud-icon', 'status-bluetooth-icon', 'status-wifi-icon'].forEach((key) => keys.add(key));

  const artKeys = ['background-flower', 'background-chevron', 'background-mechanical-flower', 'background-black-white-circle'];
  const clockArtKeys = ['background-flower', 'background-chevron', 'background-mechanical-flower'];
  const includeArt = (index: number, clock = false) => {
    // The embedded-device fallback is Flower; keep its canonical shared bytes
    // available as the renderer's deterministic offline fallback as well.
    keys.add('background-flower');
    const selectedKeys = clock ? clockArtKeys : artKeys;
    if (index >= 0 && index < selectedKeys.length) keys.add(selectedKeys[index]);
    else if (screen.id === 'sky-home') keys.add('chevron');
    else if (screen.id === 'sky-clock') keys.add('flower');
  };

  if (screen.id === 'sky-home') {
    includeArt(backgroundArt);
    ['open-clock', 'open-weather', 'open-navigation', 'open-whatsapp', 'open-spotify', 'home-settings-gear', 'home-usb-icon', 'home-glass-rim'].forEach((key) => keys.add(key));
  } else if (screen.id === 'sky-clock') {
    if (clockBackground >= 0 && clockBackground < 3) includeArt(clockBackground, true);
    else includeArt(backgroundArt);
  } else if (screen.id === 'device-control-center') {
    includeArt(backgroundArt);
    ['settings-pressed-border-mask',
      'settings-wifi-pressed-icon-mask', 'settings-wifi-pressed-chevron-mask',
      'settings-bluetooth-pressed-icon-mask', 'settings-bluetooth-pressed-chevron-mask',
      'settings-display-pressed-icon-mask', 'settings-display-pressed-chevron-mask',
      'settings-sound-pressed-icon-mask', 'settings-sound-pressed-chevron-mask',
      'settings-title-gear', 'settings-row-wifi', 'settings-row-bluetooth', 'settings-row-display', 'settings-row-sound'].forEach((key) => keys.add(key));
  } else if (['device-additional-options', 'device-clock-options', 'device-appearance-options'].includes(screen.id)) {
    includeArt(backgroundArt);
    ['display-icon-background', 'display-icon-appearance', 'display-icon-clock',
      'display-icon-units', 'display-icon-language', 'display-icon-weather-location', 'display-icon-display',
      'background-preview-flower', 'background-preview-chevron', 'background-preview-mechanical',
      'background-preview-black-white-circle',
      'clock-background-preview-flower', 'clock-background-preview-chevron', 'clock-background-preview-mechanical',
      'clock-face-preview-0', 'clock-face-preview-1', 'clock-face-preview-2'].forEach((key) => keys.add(key));

  } else if (screen.id === 'spotify-player') {
    keys.add('spotify-large');
    includeArt(backgroundArt);
  } else {
    includeArt(backgroundArt);
    if (screen.id === 'sky-weather') keys.add('open-weather');
  }

  screen.components.forEach((component) => {
    if (component.type === 'button' && WASM_ASSET_KEYS.includes(component.id)) keys.add(component.id);
  });
  return keys;
}

async function loadAssets(module: Module, keys: Set<string>) {
  const indexByKey = new Map(WASM_ASSET_KEYS.map((key, index) => [key, index]));
  const results = await Promise.all([...keys].map(async (key) => {
    const existing = assetPromises.get(key);
    if (existing) return existing.then(() => null).catch(() => key);
    const asset = assetManifest.assets[key as keyof typeof assetManifest.assets];
    const assetIndex = indexByKey.get(key);
    if (!asset || assetIndex === undefined) return null;
    const loading = (async () => {
      let bytes: Uint8Array | null = null;
      let lastError: unknown;
      for (let attempt = 0; attempt < 3 && bytes === null; attempt += 1) {
        const controller = new AbortController();
        const timer = window.setTimeout(() => controller.abort(), 8000);
        try {
          const response = await fetch(`/firmware-assets/${asset.file}`, { signal: controller.signal, cache: 'no-store' });
          if (!response.ok) throw new Error(`Asset ${asset.file} returned ${response.status}`);
          const payload = new Uint8Array(await response.arrayBuffer());
          if (payload.byteLength !== asset.bytes) throw new Error(`Asset ${asset.file} length mismatch`);
          const digest = await crypto.subtle.digest('SHA-256', payload);
          const actual = [...new Uint8Array(digest)].map((byte) => byte.toString(16).padStart(2, '0')).join('');
          if (actual !== asset.sha256) throw new Error(`Asset ${asset.file} SHA-256 mismatch`);
          bytes = payload;
        } catch (error) {
          lastError = error;
          if (attempt < 2) await new Promise((resolve) => window.setTimeout(resolve, 250 * (attempt + 1)));
        } finally { window.clearTimeout(timer); }
      }
      if (!bytes) throw lastError ?? new Error(`Asset unavailable: ${asset.file}`);
      const pointer = module._malloc(bytes.byteLength);
      if (!pointer) throw new Error('WebAssembly asset memory exhausted');
      module.HEAPU8.set(bytes, pointer);
      module._lvgl_wasm_set_asset(assetIndex, pointer, bytes.byteLength, asset.width, asset.height);
      assetPointers.set(key, pointer);
    })();
    const settled = loading.catch((error: unknown) => { assetPromises.delete(key); throw error; });
    assetPromises.set(key, settled);
    return settled.then(() => null).catch(() => key);
  }));
  return results.filter((key): key is string => key !== null);
}

function evictUnusedAssets(module: Module, retainedKeys: Set<string>) {
  const indexByKey = new Map(WASM_ASSET_KEYS.map((key, index) => [key, index]));
  assetPointers.forEach((pointer, key) => {
    if (retainedKeys.has(key)) return;
    const index = indexByKey.get(key);
    if (index !== undefined) module._lvgl_wasm_clear_asset(index);
    module._free(pointer);
    assetPointers.delete(key);
    assetPromises.delete(key);
  });
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

export async function loadSharedStatusAssets(module: Module) {
  await loadAssets(module, new Set(['status-cloud-icon', 'status-bluetooth-icon', 'status-wifi-icon']));
}

export default function FirmwareLvglCanvas({ screen, pressed, timeText, dateText, clockSecond, weatherFahrenheit, batteryPercent, wifiConnected, cloudConnected, bluetoothEnabled, bluetoothConnected, brightness, backgroundMode, playing, backgroundArt = 255, appearanceDark = true, accentColor = 'blue', clockBackground = 255, clockFace = 0, clockShowWeather = false, lifecyclePreview, onFramebufferCapture, onAssetSelectionResult }: Props) {
  const canvas = useRef<HTMLCanvasElement>(null);
  const lastGoodArt = useRef(0);
  const lastGoodClock = useRef(0);
  useEffect(() => {
    let active = true;
    void loadModule().then(async (module) => {
      if (canvas.current) canvas.current.dataset.assetStatus = 'loading';
      if (lifecyclePreview) {
        if (!active || !canvas.current) return;
        module.ccall('lvgl_wasm_render_lifecycle', 'void', ['number', 'number'], [lifecyclePreview.kind, lifecyclePreview.step]);
        paint(canvas.current, module);
        canvas.current.dataset.assetStatus = 'ready';
        const dataUrl = canvas.current.toDataURL('image/png');
        canvas.current.dataset.framebufferPng = dataUrl;
        onFramebufferCapture?.(dataUrl);
        return;
      }
      const keys = requiredAssets(screen, backgroundArt, clockBackground, wifiConnected, cloudConnected, bluetoothConnected);
      const previousKeys = requiredAssets(screen, lastGoodArt.current, lastGoodClock.current, wifiConnected, cloudConnected, bluetoothConnected);
      previousKeys.forEach((key) => keys.add(key));
      const failedAssets = await loadAssets(module, keys);
      if (!active || !canvas.current) return;
      let activeArt = backgroundArt;
      let activeClock = clockBackground;
      if (screen.id !== 'sky-clock') {
        const key = ['background-flower', 'background-chevron', 'background-mechanical-flower', 'background-black-white-circle'][backgroundArt];
        if (key && failedAssets.includes(key)) { activeArt = lastGoodArt.current; onAssetSelectionResult?.('global', backgroundArt, false); }
        else if (key) { lastGoodArt.current = backgroundArt; onAssetSelectionResult?.('global', backgroundArt, true); }
      }
      if (screen.id === 'sky-clock') {
        const key = ['background-flower', 'background-chevron', 'background-mechanical-flower'][clockBackground];
        if (key && failedAssets.includes(key)) { activeClock = lastGoodClock.current; onAssetSelectionResult?.('clock', clockBackground, false); }
        else if (key) { lastGoodClock.current = clockBackground; onAssetSelectionResult?.('clock', clockBackground, true); }
      }
      evictUnusedAssets(module, requiredAssets(screen, activeArt, activeClock, wifiConnected, cloudConnected, bluetoothConnected));
      canvas.current.dataset.assetStatus = failedAssets.length ? 'fallback' : 'ready';
      canvas.current.setAttribute('aria-label', failedAssets.length ? 'LVGL firmware framebuffer; verified offline fallback active' : 'LVGL firmware framebuffer');
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
        if (component.type === 'text') callString(module, 'lvgl_wasm_add_text', [component.id, component.text, component.x, component.y, component.size, rgb565(component.color), component.align === 'center' ? 1 : component.align === 'right' ? 2 : 0]);
        if (component.type === 'button') callString(module, 'lvgl_wasm_add_button', [component.id, component.label, component.icon ?? '', component.x, component.y, component.width, component.height, rgb565(component.color ?? '#FFFFFF')]);
        if (component.type === 'slider') callString(module, 'lvgl_wasm_add_slider', [component.id, component.x, component.y, component.width, component.height, component.value, 100, rgb565(component.color)]);
      });
      callString(module, 'lvgl_wasm_set_pressed', [pressed ?? '']);
      callString(module, 'lvgl_wasm_set_state', [timeText, dateText, batteryPercent, clockSecond, wifiConnected ? 1 : 0, cloudConnected ? 1 : 0, bluetoothEnabled ? 1 : 0, bluetoothConnected ? 1 : 0, brightness, backgroundMode, activeArt, appearanceDark ? 1 : 0, activeClock, clockFace, clockShowWeather ? 1 : 0, Math.max(0, ACCENT_NAMES.indexOf(accentColor))]);
      module._lvgl_wasm_render();
      paint(canvas.current, module);
      const dataUrl = canvas.current.toDataURL('image/png');
      canvas.current.dataset.framebufferPng = dataUrl;
      onFramebufferCapture?.(dataUrl);
    });
    return () => { active = false; };
  }, [screen, pressed, timeText, dateText, clockSecond, weatherFahrenheit, batteryPercent, wifiConnected, cloudConnected, bluetoothEnabled, bluetoothConnected, brightness, backgroundMode, playing, backgroundArt, appearanceDark, accentColor, clockBackground, clockFace, clockShowWeather, lifecyclePreview, onFramebufferCapture, onAssetSelectionResult]);
  return <canvas ref={canvas} width={466} height={466} className="firmware-canvas" aria-label="LVGL firmware framebuffer" />;
}
