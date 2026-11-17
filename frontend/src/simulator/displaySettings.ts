import { SduiScreen } from './contract';
import { ACCENT_NAMES, AccentName } from './theme';
import { readyFirmwareModule } from './firmwareModule';

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


export type DisplayModel = {
  screen: SduiScreen;
  maxScroll: number;
  effect: number;
  state: Omit<DisplayPreferences, 'weatherLocation' | 'backgroundGalleryOffset'>;
};

function model(prefs: DisplayPreferences, clock: boolean, action = ''): DisplayModel {
  const module = readyFirmwareModule();
  if (!module) throw new Error('Shared C++ Display model is not ready');
  const values = [prefs.expanded ?? '', prefs.scrollOffset, prefs.backgroundArt, prefs.backgroundGalleryOffset,
    prefs.appearanceDark ? 1 : 0, Math.max(0, ACCENT_NAMES.indexOf(prefs.accentColor)),
    prefs.measurementSystem === 'metric' ? 1 : 0, prefs.spanishLanguage ? 1 : 0,
    prefs.country === 'US' ? 1 : prefs.country === 'MX' ? 2 : 0, prefs.keypadOpen ? 1 : 0,
    prefs.weatherLocation, prefs.postalDraft, prefs.postalStatus, prefs.clock24 ? 1 : 0,
    prefs.clockFace, prefs.clockBackground, prefs.clockShowWeather ? 1 : 0, clock ? 1 : 0, action];
  const raw = module.ccall('lvgl_wasm_display_model', 'string',
    values.map((v) => typeof v === 'string' ? 'string' : 'number'), values);
  if (typeof raw !== 'string' || !raw) throw new Error('Shared Display model serialization failed');
  return JSON.parse(raw) as DisplayModel;
}

export function displayMenuMaximumScroll(expanded: string | null, prefs: DisplayPreferences) {
  const module = readyFirmwareModule();
  return module ? Number(module.ccall('lvgl_wasm_display_max_scroll', 'number', ['string', 'number', 'number'],
    [expanded ?? '', prefs.country === 'US' ? 1 : prefs.country === 'MX' ? 2 : 0, prefs.keypadOpen ? 1 : 0])) : 0;
}
export function displayScreen(base: SduiScreen, prefs: DisplayPreferences): SduiScreen {
  return { ...base, ...model(prefs, false).screen };
}
export function clockSettingsScreen(base: SduiScreen, prefs: DisplayPreferences): SduiScreen {
  return { ...base, ...model(prefs, true).screen };
}
export function displayMenuAction(prefs: DisplayPreferences, id: string, clock: boolean) {
  return model(prefs, clock, id);
}
