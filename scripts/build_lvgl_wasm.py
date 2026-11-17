"""Build the pinned LVGL software renderer for the browser framebuffer."""
from __future__ import annotations
import os
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LVGL = ROOT / "third_party" / "lvgl"
EMCC = Path(os.environ.get("EMCC", ROOT / ".tools" / "emsdk" / "upstream" / "emscripten" / "emcc"))
OUT = ROOT / "frontend" / "public" / "wasm" / "lvgl.js"

def main() -> None:
    if not EMCC.exists() or not (LVGL / "src" / "lvgl.h").exists(): raise SystemExit("LVGL or Emscripten is not available")
    roots = ["core", "display", "draw", "font", "indev", "layouts", "misc", "osal", "stdlib/builtin", "themes", "tick", "widgets/arc", "widgets/bar", "widgets/button", "widgets/image", "widgets/label", "widgets/line", "widgets/slider", "libs/bin_decoder"]
    excluded = {"dma2d", "nema_gfx", "nxp", "opengles", "renesas", "sdl", "vg_lite", "arm2d", "mono", "simple", "default"}
    sources = []
    for root in roots:
        for path in (LVGL / "src" / root).rglob("*.c"):
            parts = path.relative_to(LVGL / "src").parts
            if any(part in excluded for part in parts): continue
            if path.name in {"lv_cmsis_rtos2.c", "lv_freertos.c", "lv_linux.c", "lv_mqx.c", "lv_pthread.c", "lv_rtthread.c", "lv_sdl2.c", "lv_windows.c"}: continue
            sources.append(str(path))
    sources += [str(LVGL / "src" / "lv_init.c"), str(LVGL / "src" / "stdlib" / "lv_mem.c")]
    OUT.parent.mkdir(parents=True, exist_ok=True)
    command = [str(EMCC), str(ROOT / "frontend/wasm/lvgl_wasm.cpp"), str(ROOT / "shared/lvgl_renderer.cpp"), *sorted(set(sources)), "-I", str(LVGL), "-I", str(ROOT / "frontend/wasm"), "-I", str(ROOT / "shared"), "-I", str(ROOT / "device/include"), "-DLV_CONF_INCLUDE_SIMPLE", "-DLEOPOLD_WASM", "-O2", "-s", "MODULARIZE=1", "-s", "EXPORT_ES6=1", "-s", "ENVIRONMENT=web", "-s", "ALLOW_MEMORY_GROWTH=1", "-s", "INITIAL_MEMORY=67108864", "-s", "EXPORTED_FUNCTIONS=['_lvgl_wasm_init','_lvgl_wasm_display_model','_lvgl_wasm_display_max_scroll','_lvgl_wasm_set_asset','_lvgl_wasm_clear_asset','_lvgl_wasm_set_backdrop','_lvgl_wasm_begin','_lvgl_wasm_add_text','_lvgl_wasm_add_button','_lvgl_wasm_add_slider','_lvgl_wasm_set_pressed','_lvgl_wasm_set_state','_lvgl_wasm_render','_lvgl_wasm_render_lifecycle','_lvgl_wasm_framebuffer','_lvgl_wasm_framebuffer_size','_malloc','_free']", "-s", "EXPORTED_RUNTIME_METHODS=['HEAPU8','ccall']", "-o", str(OUT)]
    subprocess.run(command, check=True, cwd=ROOT)

if __name__ == "__main__": main()
