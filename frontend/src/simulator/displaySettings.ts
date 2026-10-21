import { SduiButton, SduiScreen, SduiText } from './contract';
import rendering from '../shared/rendering.json';
import { ACCENT_NAMES, AccentName, accentForAppearance, accentPalette, neutralRole } from './theme';

export type DisplayPreferences = {
  expanded: string | null;
  scrollOffset: number;
  backgroundArt: number;
  appearanceDark: boolean;
  accentColor: AccentName;
  measurementSystem: 'imperial' | 'metric';
  spanishLanguage: boolean;
  country: '' | 'US' | 'MX';
  weatherLocation: string;
  keypadOpen: boolean;
  postalDraft: string;
  postalStatus: string;
  clock24: boolean;
  clockFace: number;
  clockBackground: number;
  clockShowWeather: boolean;
  backgroundGalleryOffset: number;
};

export const MENU_VIEWPORT = { x: 58, y: 139, width: 350, height: 327 };
export const MENU_BACK = { x: 48, y: 87, width: 48, height: 46 };
const ROW_HEIGHT = rendering.display_submenu.row_height;
const ROW_GAP = rendering.display_submenu.row_gap;
const ROWS = [
  ['background', 'Background', 'Fondo'],
  ['appearance', 'Appearance', 'Apariencia'],
  ['clock', 'Clock', 'Reloj'],
  ['units', 'Units of measurement', 'Unidades de medida'],
  ['language', 'Language', 'Idioma'],
  ['weather-location', 'Weather Location', 'Ubicacion del clima'],
] as const;

function button(id: string, label: string, x: number, y: number, width: number, height: number, color: string, icon = ''): SduiButton {
  return { type: 'button', id, label, x, y, width, height, color, ...(icon ? { icon } : {}) };
}

function title(text: string, dark: boolean): SduiText {
  return { type: 'text', id: 'submenu-title', text, x: 196, y: 91, size: 2, color: neutralRole('text', dark), align: 'left' };
}

function expansionHeight(row: string | null, prefs: DisplayPreferences) {
  if (!row) return 0;
  if (row === 'background') return 145;
  if (row === 'appearance') return 120;
  if (row === 'units' || row === 'language') return 50;
  if (row === 'weather-location') return prefs.country && prefs.keypadOpen ? 234 : 86;
  return 0;
}

export function displayMenuMaximumScroll(expanded: string | null, prefs: DisplayPreferences) {
  const rowsHeight = ROWS.reduce((sum, [id]) => sum + ROW_HEIGHT + ROW_GAP + (expanded === id ? expansionHeight(id, prefs) : 0), 0);
  return Math.max(0, rowsHeight - MENU_VIEWPORT.height);
}

export function displayScreen(base: SduiScreen, prefs: DisplayPreferences): SduiScreen {
  const spanish = prefs.spanishLanguage;
  const components: SduiScreen['components'] = [
    title(spanish ? 'PANTALLA' : 'DISPLAY', prefs.appearanceDark),
    button('menu-back', '‹', MENU_BACK.x, MENU_BACK.y, MENU_BACK.width, MENU_BACK.height, neutralRole('text', prefs.appearanceDark)),
  ];
  const maxScroll = displayMenuMaximumScroll(prefs.expanded, prefs);
  const scroll = Math.max(0, Math.min(maxScroll, prefs.scrollOffset));
  let y = MENU_VIEWPORT.y - scroll;
  for (const [id, english, spanishLabel] of ROWS) {
    const label = spanish ? spanishLabel : english;
    components.push(button(`display-row-${id}`, label, MENU_VIEWPORT.x, y, MENU_VIEWPORT.width, ROW_HEIGHT, id === prefs.expanded ? accentForAppearance(prefs.accentColor, prefs.appearanceDark) : neutralRole('text', prefs.appearanceDark), id === prefs.expanded ? 'expanded' : 'row'));
    y += ROW_HEIGHT;
    if (id === prefs.expanded) {
      const controlY = y + 3;
      if (id === 'background') {
        for (let index = 0; index < 3; index += 1) {
          components.push(button(`display-background-${index}`, ['Flower', 'Chevron', 'Mechanical'][index], 68 + index * 105, controlY + 3, 100, 135, prefs.backgroundArt === index ? accentForAppearance(prefs.accentColor, prefs.appearanceDark) : neutralRole('disabled', prefs.appearanceDark), `bgprev-${index}`));
        }
      } else if (id === 'appearance') {
        components.push(button('display-appearance-light', spanish ? 'Claro' : 'Light', 102, controlY + 4, 116, 38, prefs.appearanceDark ? neutralRole('disabled', prefs.appearanceDark) : accentForAppearance(prefs.accentColor, prefs.appearanceDark)));
        components.push(button('display-appearance-dark', spanish ? 'Oscuro' : 'Dark', 246, controlY + 4, 116, 38, prefs.appearanceDark ? accentForAppearance(prefs.accentColor, prefs.appearanceDark) : neutralRole('disabled', prefs.appearanceDark)));
        ACCENT_NAMES.forEach((name, index) => {
          const palette = accentPalette(name);
          const row = index < 4 ? 0 : 1;
          const column = row === 0 ? index : index - 4;
          const width = row === 0 ? 78 : 104;
          const x = row === 0 ? 64 + column * 82 : 64 + column * 108;
          const label = `${prefs.accentColor === name ? '[x] ' : ''}${palette.label}`;
          components.push(button(`display-accent-${name}`, label, x, controlY + 47 + row * 34, width, 30, palette.accent));
        });
      } else if (id === 'units') {
        components.push(button('display-units-imperial', 'Imperial', 102, controlY + 4, 116, 38, prefs.measurementSystem === 'imperial' ? accentForAppearance(prefs.accentColor, prefs.appearanceDark) : neutralRole('disabled', prefs.appearanceDark)));
        components.push(button('display-units-metric', 'Metric', 246, controlY + 4, 116, 38, prefs.measurementSystem === 'metric' ? accentForAppearance(prefs.accentColor, prefs.appearanceDark) : neutralRole('disabled', prefs.appearanceDark)));
      } else if (id === 'language') {
        components.push(button('display-language-en', 'English', 102, controlY + 4, 116, 38, !spanish ? accentForAppearance(prefs.accentColor, prefs.appearanceDark) : neutralRole('disabled', prefs.appearanceDark)));
        components.push(button('display-language-es', 'Español', 246, controlY + 4, 116, 38, spanish ? accentForAppearance(prefs.accentColor, prefs.appearanceDark) : neutralRole('disabled', prefs.appearanceDark)));
      } else if (id === 'weather-location') {
        if (!prefs.country) {
          components.push(button('display-country-US', 'United States', 87, controlY + 4, 140, 38, accentForAppearance(prefs.accentColor, prefs.appearanceDark)));
          components.push(button('display-country-MX', 'Mexico', 239, controlY + 4, 140, 38, accentForAppearance(prefs.accentColor, prefs.appearanceDark)));
        } else {
          const countryLabel = prefs.country === 'US' ? 'United States' : 'Mexico';
          components.push(button('display-country', `${countryLabel} (${prefs.country})`, 90, controlY, 286, 34, neutralRole('disabled', prefs.appearanceDark)));
          components.push(button('display-postal-field', prefs.postalDraft || 'Postal code', 90, controlY + 37, 286, 34, accentForAppearance(prefs.accentColor, prefs.appearanceDark)));
          components.push({ type: 'text', id: 'display-postal-status', text: prefs.postalStatus || (prefs.weatherLocation ? `Saved: ${prefs.weatherLocation} · GeoNames` : 'Enter a postal code · GeoNames'), x: 92, y: controlY + 72, size: 1, color: neutralRole('mutedText', prefs.appearanceDark), align: 'left' });
          if (prefs.keypadOpen) {
            const keys = ['1','2','3','4','5','6','7','8','9','DEL','0','C'];
            keys.forEach((key, index) => {
              const row = Math.floor(index / 3);
              const col = index % 3;
              components.push(button(`postal-key-${key === 'DEL' ? 'delete' : key === 'C' ? 'clear' : key}`, key, 107 + col * 90, controlY + 94 + row * 26, 78, 22, neutralRole('disabled', prefs.appearanceDark)));
            });
            components.push(button('display-postal-cancel', 'Cancel', 96, controlY + 201, 118, 28, neutralRole('disabled', prefs.appearanceDark)));
            components.push(button('display-postal-apply', 'Apply', 238, controlY + 201, 118, 28, accentForAppearance(prefs.accentColor, prefs.appearanceDark)));
          }
        }
      }
      y += expansionHeight(id, prefs);
    }
    y += ROW_GAP;
  }
  return { ...base, id: 'device-additional-options', components };
}

export function clockSettingsScreen(base: SduiScreen, prefs: DisplayPreferences): SduiScreen {
  const spanish = prefs.spanishLanguage;
  const components: SduiScreen['components'] = [
    title(spanish ? 'RELOJ' : 'CLOCK', prefs.appearanceDark),
    button('menu-back', '‹', MENU_BACK.x, MENU_BACK.y, MENU_BACK.width, MENU_BACK.height, neutralRole('text', prefs.appearanceDark)),
    { type: 'text', id: 'clock-format-label', text: spanish ? 'Formato horario' : 'Time format', x: 73, y: 143, size: 1, color: neutralRole('text', prefs.appearanceDark) },
    button('clock-format-12', '12 hour', 74, 165, 137, 45, prefs.clock24 ? neutralRole('disabled', prefs.appearanceDark) : accentForAppearance(prefs.accentColor, prefs.appearanceDark)),
    button('clock-format-24', '24 hour', 231, 165, 137, 45, prefs.clock24 ? accentForAppearance(prefs.accentColor, prefs.appearanceDark) : neutralRole('disabled', prefs.appearanceDark)),
    { type: 'text', id: 'clock-face-label', text: spanish ? 'Caratula' : 'Face Selection', x: 73, y: 215, size: 1, color: neutralRole('text', prefs.appearanceDark) },
    ...(['Minimal', 'Classic', 'Gold', 'Digital', 'Segment'] as const).map((name, index) => {
      const column = index < 3 ? index : index - 3;
      const row = index < 3 ? 0 : 1;
      const x = 74 + column * 100;
      const y = 230 + row * 51;
      return button(`clock-face-${index}`, name, x, y, 96, 45, prefs.clockFace === index ? accentForAppearance(prefs.accentColor, prefs.appearanceDark) : neutralRole('disabled', prefs.appearanceDark), `clkface-${index}`);
    }),
    { type: 'text', id: 'clock-background-label', text: spanish ? 'Fondo del reloj' : 'Clock background', x: 73, y: 336, size: 1, color: neutralRole('text', prefs.appearanceDark) },
    button('clock-background-0', 'Flower', 74, 353, 96, 42, prefs.clockBackground === 0 ? accentForAppearance(prefs.accentColor, prefs.appearanceDark) : neutralRole('disabled', prefs.appearanceDark), 'clkbg-0'),
    button('clock-background-1', 'Chevron', 174, 353, 96, 42, prefs.clockBackground === 1 ? accentForAppearance(prefs.accentColor, prefs.appearanceDark) : neutralRole('disabled', prefs.appearanceDark), 'clkbg-1'),
    button('clock-background-2', 'Mechanical', 274, 353, 96, 42, prefs.clockBackground === 2 ? accentForAppearance(prefs.accentColor, prefs.appearanceDark) : neutralRole('disabled', prefs.appearanceDark), 'clkbg-2'),
    { type: 'text', id: 'clock-weather-label', text: spanish ? 'Mostrar clima en reloj' : 'Show weather on clock', x: 73, y: 404, size: 1, color: neutralRole('text', prefs.appearanceDark) },
    button('clock-weather-yes', spanish ? 'Si' : 'Yes', 231, 398, 68, 32, prefs.clockShowWeather ? accentForAppearance(prefs.accentColor, prefs.appearanceDark) : neutralRole('disabled', prefs.appearanceDark)),
    button('clock-weather-no', spanish ? 'No' : 'No', 307, 398, 68, 32, prefs.clockShowWeather ? neutralRole('disabled', prefs.appearanceDark) : accentForAppearance(prefs.accentColor, prefs.appearanceDark)),
  ];
  return { ...base, id: 'device-clock-options', components };
}
