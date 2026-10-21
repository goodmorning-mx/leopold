# Leopold Phase 1 Settings reference pack

Product-owner decisions applied:

- Settings rows are neutral at rest.
- Pressed rows use a consistent cyan outline/glow, cyan icon, cyan chevron, and white label.
- Brightness is a live control and does not require separate full-screen 5/50/100 references.
- The left banner icon represents Bluetooth connected/disconnected state.
- Simulator-connected and ESP32-connected use the same visual connection treatment.

The five Settings files are lossless 466 × 466 PNG references derived from one generated neutral master. The two status files are lossless 660 × 206 PNG references normalized from the supplied banner artwork.

These files are reference inputs, not yet registered runtime assets. Their source identity, checksums, state mappings, masks, synchronized derivatives, and comparison profiles must be registered through the Leopold parity workflow before implementation.

## Display sub-menu references

`Display Sub Menu Grid.png` is the preserved 1240 × 1240 source sheet (SHA-256 `0a7446ad04cada3e46738f819c3c9a6320fcbfcea926563ec0845dc6f8e88bfb`). The individually usable references are in `Sub Menu/`. Each is a 320 × 320 lossless PNG crop at the source pixel scale; no resizing or redrawing was applied. Crop rectangles are listed as `(left, top, width, height)` in source-image pixels. The two empty N/A placeholders were intentionally omitted.

| Reference | Target state | Source crop | SHA-256 |
| --- | --- | --- | --- |
| `Sub Menu/Display_Default.png` | Display menu, nothing selected | `(118, 73, 320, 320)` | `4edb07a49f96e67b852b5c19774672c9d185abe3551e94e3118c850b4ab354a5` |
| `Sub Menu/Display_Background_Expanded.png` | Background expanded with image choice selected | `(460, 73, 320, 320)` | `9122ed908fe7512076a6cb949819e8d035e370dfc5242f15accaa398c49c5eae` |
| `Sub Menu/Display_Appearance_Expanded.png` | Appearance expanded | `(802, 73, 320, 320)` | `2065ea8dd654b2d22b3fbcd6e9eb78f00a90465a53d36cb2eb62cc409fb6c531` |
| `Sub Menu/Display_Weather_Location_Expanded.png` | Weather location expanded with postal-code field | `(118, 458, 320, 320)` | `27c1931b3ac50d67856ce4ecf22d26dd2ff3eae03751ab61f243e18e7d6a0953` |
| `Sub Menu/Display_Clock_Submenu.png` | Clock submenu | `(460, 458, 320, 320)` | `f885c7c121bbcbc1e89e93193c2e60607de2f486cb19bb2e3952b064bb780ed4` |
| `Sub Menu/Display_Units_Expanded.png` | Units expanded | `(802, 458, 320, 320)` | `a578a3e70aa05079936f5ba11ede96f2e55d74f81878e0fcfff39404d9688caf` |
| `Sub Menu/Display_Language_Expanded.png` | Language expanded | `(460, 840, 320, 320)` | `d0dbd8c284e9ab7fbd1a1f5f41d264958eb33f9286208513962a02731697049d` |

These crops are comparison references, not registered runtime assets. The clock and status-bar time are baked into the supplied artwork; declare dynamic masks if those areas are excluded during a future comparison. The source shows a round display presentation inside each square image.
