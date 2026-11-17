import { useEffect, useRef } from 'react';
import { loadFirmwareModule, FirmwareModule } from './firmwareModule';
import { loadSharedStatusAssets } from './FirmwareLvglCanvas';
import { ACCENT_NAMES, AccentName } from './theme';

type Props = {
  backdropUrl: string;
  timeText: string;
  batteryPercent: number;
  accentColor: AccentName;
  wifiConnected: boolean;
  cloudConnected: boolean;
  bluetoothEnabled: boolean;
  bluetoothConnected: boolean;
  onFramebufferCapture?: (dataUrl: string) => void;
};

function callString(module: FirmwareModule, name: string, values: unknown[]) {
  module.ccall(name, 'void', values.map((value) => typeof value === 'string' ? 'string' : 'number'), values);
}

function paint(canvas: HTMLCanvasElement, module: FirmwareModule) {
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

export default function FirmwareSharedStatusOverlay({ backdropUrl, timeText, batteryPercent, accentColor, wifiConnected, cloudConnected, bluetoothEnabled, bluetoothConnected, onFramebufferCapture }: Props) {
  const canvas = useRef<HTMLCanvasElement>(null);
  useEffect(() => {
    let active = true;
    void loadFirmwareModule().then(async (module) => {
      await loadSharedStatusAssets(module);
      const backdropImage = new Image();
      await new Promise<void>((resolve, reject) => {
        backdropImage.onload = () => resolve();
        backdropImage.onerror = () => reject(new Error('Unable to read simulator backdrop'));
        backdropImage.src = backdropUrl;
      });
      if (!active || !canvas.current) return;
      const source = document.createElement('canvas');
      source.width = 466; source.height = 466;
      const sourceContext = source.getContext('2d', { willReadFrequently: true });
      if (!sourceContext) return;
      sourceContext.drawImage(backdropImage, 0, 0, 466, 466);
      const rgba = sourceContext.getImageData(0, 0, 466, 466).data;
      const rgb565 = new Uint8Array(466 * 466 * 2);
      for (let index = 0; index < 466 * 466; index += 1) {
        const red = rgba[index * 4] >> 3;
        const green = rgba[index * 4 + 1] >> 2;
        const blue = rgba[index * 4 + 2] >> 3;
        const pixel = (red << 11) | (green << 5) | blue;
        rgb565[index * 2] = pixel & 0xff;
        rgb565[index * 2 + 1] = pixel >> 8;
      }
      const pointer = module._malloc(rgb565.byteLength);
      if (!pointer) throw new Error('WebAssembly backdrop allocation failed');
      try {
        module.HEAPU8.set(rgb565, pointer);
        module._lvgl_wasm_set_backdrop(pointer, rgb565.byteLength);
        callString(module, 'lvgl_wasm_begin', ['shared-status-overlay', 0]);
        callString(module, 'lvgl_wasm_set_state', [timeText, 'NOV 02', batteryPercent, 0, wifiConnected ? 1 : 0, cloudConnected ? 1 : 0, bluetoothEnabled ? 1 : 0, bluetoothConnected ? 1 : 0, 50, 0, 0, 1, 0, 0, 0, Math.max(0, ACCENT_NAMES.indexOf(accentColor))]);
        module._lvgl_wasm_render();
      } finally {
        module._free(pointer);
      }
      if (!active || !canvas.current) return;
      paint(canvas.current, module);
      const dataUrl = canvas.current.toDataURL('image/png');
      canvas.current.dataset.framebufferPng = dataUrl;
      canvas.current.dataset.renderer = 'shared-lvgl-wasm-status-overlay';
      onFramebufferCapture?.(dataUrl);
    }).catch((error) => {
      if (active && canvas.current) canvas.current.dataset.captureError = String(error);
    });
    return () => { active = false; };
  }, [backdropUrl, timeText, batteryPercent, wifiConnected, cloudConnected, bluetoothEnabled, bluetoothConnected, onFramebufferCapture]);
  return <canvas ref={canvas} width={466} height={466} className="firmware-canvas firmware-status-overlay" aria-label="Shared LVGL status banner overlay" />;
}
