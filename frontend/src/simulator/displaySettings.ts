import { SduiButton, SduiScreen, SduiText } from './contract';
import rendering from '../shared/rendering.json';

export type DisplayPreferences = {
  expanded: string | null;
  scrollOffset: number;
  backgroundArt: number;
  appearanceDark: boolean;
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

function button(id: string, label: string, x: number, y: number, width: number, height: number, color = '#D5E3FA', icon = ''): SduiButton {
  return { type: 'button', id, label, x, y, width, height, color, ...(icon ? { icon } : {}) };
}

function title(text: string): SduiText {
  return { type: 'text', id: 'submenu-title', text, x: 196, y: 91, size: 2, color: '#FFFFFF', align: 'left' };
}

function expansionHeight(row: string | null, prefs: DisplayPreferences) {
  if (!row) return 0;
  if (row === 'background') return 145;
  if (row === 'appearance' || row === 'units' || row === 'language') return 50;
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
    title(spanish ? 'PANTALLA' : 'DISPLAY'),
    button('menu-back', '‹', MENU_BACK.x, MENU_BACK.y, MENU_BACK.width, MENU_BACK.height, '#FFFFFF'),
  ];
  const maxScroll = displayMenuMaximumScroll(prefs.expanded, prefs);
  const scroll = Math.max(0, Math.min(maxScroll, prefs.scrollOffset));
  let y = MENU_VIEWPORT.y - scroll;
  for (const [id, english, spanishLabel] of ROWS) {
    const label = spanish ? spanishLabel : english;
    components.push(button(`display-row-${id}`, label, MENU_VIEWPORT.x, y, MENU_VIEWPORT.width, ROW_HEIGHT, id === prefs.expanded ? '#00D2FF' : '#D5E3FA', id === prefs.expanded ? 'expanded' : 'row'));
    y += ROW_HEIGHT;
    if (id === prefs.expanded) {
      const controlY = y + 3;
      if (id === 'background') {
        for (let index = 0; index < 3; index += 1) {
          components.push(button(`display-background-${index}`, ['Flower', 'Chevron', 'Mechanical'][index], 68 + index * 105, controlY + 3, 100, 135, prefs.backgroundArt === index ? '#00D2FF' : '#69778A', `bgprev-${index}`));
        }
      } else if (id === 'appearance') {
        components.push(button('display-appearance-light', spanish ? 'Claro' : 'Light', 102, controlY + 4, 116, 38, prefs.appearanceDark ? '#69778A' : '#00D2FF'));
        components.push(button('display-appearance-dark', spanish ? 'Oscuro' : 'Dark', 246, controlY + 4, 116, 38, prefs.appearanceDark ? '#00D2FF' : '#69778A'));
      } else if (id === 'units') {
        components.push(button('display-units-imperial', 'Imperial', 102, controlY + 4, 116, 38, prefs.measurementSystem === 'imperial' ? '#00D2FF' : '#69778A'));
        components.push(button('display-units-metric', 'Metric', 246, controlY + 4, 116, 38, prefs.measurementSystem === 'metric' ? '#00D2FF' : '#69778A'));
      } else if (id === 'language') {
        components.push(button('display-language-en', 'English', 102, controlY + 4, 116, 38, !spanish ? '#00D2FF' : '#69778A'));
        components.push(button('display-language-es', 'Español', 246, controlY + 4, 116, 38, spanish ? '#00D2FF' : '#69778A'));
      } else if (id === 'weather-location') {
        if (!prefs.country) {
          components.push(button('display-country-US', 'United States', 87, controlY + 4, 140, 38, '#00D2FF'));
          components.push(button('display-country-MX', 'Mexico', 239, controlY + 4, 140, 38, '#00D2FF'));
        } else {
          const countryLabel = prefs.country === 'US' ? 'United States' : 'Mexico';
          components.push(button('display-country', `${countryLabel} (${prefs.country})`, 90, controlY, 286, 34, '#69778A'));
          components.push(button('display-postal-field', prefs.postalDraft || 'Postal code', 90, controlY + 37, 286, 34, '#00D2FF'));
          components.push({ type: 'text', id: 'display-postal-status', text: prefs.postalStatus || (prefs.weatherLocation ? `Saved: ${prefs.weatherLocation}` : 'Enter a postal code'), x: 92, y: controlY + 72, size: 1, color: '#A0ACB9', align: 'left' });
          if (prefs.keypadOpen) {
            const keys = ['1','2','3','4','5','6','7','8','9','DEL','0','C'];
            keys.forEach((key, index) => {
              const row = Math.floor(index / 3);
              const col = index % 3;
              components.push(button(`postal-key-${key === 'DEL' ? 'delete' : key === 'C' ? 'clear' : key}`, key, 107 + col * 90, controlY + 94 + row * 26, 78, 22, '#69778A'));
            });
            components.push(button('display-postal-cancel', 'Cancel', 96, controlY + 201, 118, 28, '#69778A'));
            components.push(button('display-postal-apply', 'Apply', 238, controlY + 201, 118, 28, '#00D2FF'));
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
    title(spanish ? 'RELOJ' : 'CLOCK'),
    button('menu-back', '‹', MENU_BACK.x, MENU_BACK.y, MENU_BACK.width, MENU_BACK.height, '#FFFFFF'),
    { type: 'text', id: 'clock-format-label', text: spanish ? 'Formato horario' : 'Time format', x: 73, y: 143, size: 1, color: '#A0ACB9' },
    button('clock-format-12', '12 hour', 74, 165, 137, 45, prefs.clock24 ? '#69778A' : '#00D2FF'),
    button('clock-format-24', '24 hour', 231, 165, 137, 45, prefs.clock24 ? '#00D2FF' : '#69778A'),
    { type: 'text', id: 'clock-face-label', text: spanish ? 'Caratula' : 'Face Selection', x: 73, y: 215, size: 1, color: '#A0ACB9' },
    button('clock-face-0', 'Classic', 74, 232, 96, 72, prefs.clockFace === 0 ? '#00D2FF' : '#69778A', 'clkface-0'),
    button('clock-face-1', 'Minimal', 174, 232, 96, 72, prefs.clockFace === 1 ? '#00D2FF' : '#69778A', 'clkface-1'),
    button('clock-face-2', 'Digital', 274, 232, 96, 72, prefs.clockFace === 2 ? '#00D2FF' : '#69778A', 'clkface-2'),
    { type: 'text', id: 'clock-background-label', text: spanish ? 'Fondo del reloj' : 'Clock background', x: 73, y: 310, size: 1, color: '#A0ACB9' },
    button('clock-background-0', 'Flower', 74, 328, 96, 52, prefs.clockBackground === 0 ? '#00D2FF' : '#69778A', 'clkbg-0'),
    button('clock-background-1', 'Chevron', 174, 328, 96, 52, prefs.clockBackground === 1 ? '#00D2FF' : '#69778A', 'clkbg-1'),
    button('clock-background-2', 'Mechanical', 274, 328, 96, 52, prefs.clockBackground === 2 ? '#00D2FF' : '#69778A', 'clkbg-2'),
    { type: 'text', id: 'clock-weather-label', text: spanish ? 'Mostrar clima' : 'Show weather', x: 73, y: 385, size: 1, color: '#A0ACB9' },
    button('clock-weather-yes', spanish ? 'Si' : 'Yes', 231, 379, 68, 35, prefs.clockShowWeather ? '#00D2FF' : '#69778A'),
    button('clock-weather-no', spanish ? 'No' : 'No', 307, 379, 68, 35, prefs.clockShowWeather ? '#69778A' : '#00D2FF'),
  ];
  return { ...base, id: 'device-clock-options', components };
}
