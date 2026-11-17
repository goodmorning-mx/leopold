export type FirmwareModule = {
  HEAPU8: Uint8Array;
  _lvgl_wasm_init: () => void;
  _lvgl_wasm_begin: (id: number, background: number) => void;
  _lvgl_wasm_set_asset: (index: number, pointer: number, size: number, width: number, height: number) => void;
  _lvgl_wasm_clear_asset: (index: number) => void;
  _lvgl_wasm_set_backdrop: (pointer: number, size: number) => void;
  _lvgl_wasm_set_pressed: (id: number) => void;
  _lvgl_wasm_set_state: (...values: number[]) => void;
  _lvgl_wasm_render: () => void;
  _lvgl_wasm_framebuffer: () => number;
  _lvgl_wasm_framebuffer_size: () => number;
  _malloc: (size: number) => number;
  _free: (pointer: number) => void;
  ccall: (name: string, result: string, args: string[], values: unknown[]) => unknown;
};

let modulePromise: Promise<FirmwareModule> | undefined;
let readyModule: FirmwareModule | undefined;
export function readyFirmwareModule() { return readyModule; }
export function loadFirmwareModule() {
  if (!modulePromise) {
    modulePromise = fetch(`/wasm/lvgl.js?${Date.now()}`).then((response) => response.text()).then(async (source) => {
      const url = URL.createObjectURL(new Blob([source], { type: 'text/javascript' }));
      const loaded = await import(/* @vite-ignore */ url) as { default: (options: { locateFile: (path: string) => string }) => Promise<FirmwareModule> };
      URL.revokeObjectURL(url);
      const module = await loaded.default({ locateFile: (path) => `/wasm/${path}?${Date.now()}` });
      module._lvgl_wasm_init();
      readyModule = module;
      return module;
    });
  }
  return modulePromise;
}
