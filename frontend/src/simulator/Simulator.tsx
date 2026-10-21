import { PointerEvent, useEffect, useMemo, useRef, useState } from 'react';
import { SduiButton, SduiDocument, SduiSlider, validateSdui } from './contract';
import FirmwareCanvas, { FirmwareImages, SIMULATOR_WIFI_NETWORKS } from './FirmwareCanvas';
import FirmwareLvglCanvas from './FirmwareLvglCanvas';
import { classifyGesture, containsPoint, sliderValue } from './interaction';
import { clockSettingsScreen, displayMenuMaximumScroll, displayScreen } from './displaySettings';
import { sampleDocument } from './sample';
import interaction from '../shared/interaction.json';
import persistence from '../shared/persistence.json';
import assetManifest from '../shared/asset-manifest.json';
import referenceManifest from '../shared/reference-manifest.json';
import { AccentName, DEFAULT_ACCENT, isAccentName } from './theme';

type EventLog = { at: string; kind: string; detail: string; payload?: string };
const firmwareReference = `local preview · UI ${sampleDocument.revision}`;

function stored<T extends number | boolean>(key: string, fallback: T): T {
  const value = window.localStorage.getItem(`display:${key}`);
  if (value === null) return fallback;
  if (typeof fallback === 'boolean') return (value === 'true') as T;
  const numeric = Number(value);
  return (Number.isFinite(numeric) ? numeric : fallback) as T;
}

function storedString(key: string, fallback: string): string {
  return window.localStorage.getItem(`display:${key}`) ?? fallback;
}

function storedMeasurementSystem(): 'imperial' | 'metric' {
  const value = window.localStorage.getItem('display:measurement_system');
  if (value === 'imperial' || value === 'metric') return value;
  const legacyFahrenheit = window.localStorage.getItem('display:weather_f');
  return legacyFahrenheit === 'true' ? 'imperial' : 'metric';
}

async function decodeFirmwareAsset(path: string, width: number, height: number): Promise<string> {
  const bytes = new Uint8Array(await (await fetch(`${path}?asset-revision=home-reference`)).arrayBuffer());
  const pixels = width * height;
  const image = new ImageData(width, height);
  const alphaOffset = pixels * 2;
  for (let index = 0; index < pixels; index += 1) {
    const rgb565 = bytes[index * 2] | (bytes[index * 2 + 1] << 8);
    image.data[index * 4] = Math.round(((rgb565 >> 11) & 0x1f) * 255 / 31);
    image.data[index * 4 + 1] = Math.round(((rgb565 >> 5) & 0x3f) * 255 / 63);
    image.data[index * 4 + 2] = Math.round((rgb565 & 0x1f) * 255 / 31);
    image.data[index * 4 + 3] = bytes[alphaOffset + index] ?? 255;
  }
  const canvas = window.document.createElement('canvas');
  canvas.width = width; canvas.height = height;
  canvas.getContext('2d')?.putImageData(image, 0, 0);
  return canvas.toDataURL('image/png');
}

export default function Simulator() {
  const query = new URLSearchParams(window.location.search);
  const requestedScreen = query.get('screen');
  const referenceState = query.get('reference');
  const framebufferCaptureMode = query.get('capture') === '1';
  const injectedState = query.get('capture-state') ?? referenceState ?? '';
  const countryOverride = query.get('country');
  const diagnosticState = framebufferCaptureMode || Boolean(referenceState) || ['art', 'appearance', 'language', 'units', 'face', 'clock-background', 'clock-weather', 'format', 'country', 'postal', 'keypad', 'gallery', 'expand', 'scroll'].some((key) => query.has(key));
  const referenceSettingsMode = framebufferCaptureMode && referenceState === 'settings';
  // The broad `reference=display` URL is used by the simulator as its default
  // entry point; keep it on Home. Specific capture/preview states still open
  // the Settings or Display view they describe.
  const submenuReference = ['display-background', 'display-appearance', 'display-clock', 'display-units', 'display-language', 'display-location', 'display-keypad', 'display-invalid', 'display-not-found', 'display-offline', 'display-pending', 'clock-settings'].includes(referenceState ?? '');
  const startSettingsMode = referenceSettingsMode || submenuReference || ['display-background', 'display-appearance', 'display-clock', 'display-units', 'display-language', 'display-location', 'display-keypad', 'display-invalid', 'display-not-found', 'display-offline', 'display-pending', 'clock-settings'].includes(injectedState) || ['settings', 'display', 'clock', 'wifi'].includes(query.get('screen') ?? '');
  const captureState = framebufferCaptureMode ? query.get('capture-state') : null;
  const capturePressedByState: Record<string, string> = {
    'wifi-pressed': 'settings-wifi',
    'bluetooth-pressed': 'settings-bluetooth',
    'display-pressed': 'settings-display',
    'sound-pressed': 'settings-sound',
  };
  const capturePressed = captureState ? capturePressedByState[captureState] ?? null : null;
  const captureBrightness = captureState === 'brightness-5' ? 5 : captureState === 'brightness-50' ? 50 : captureState === 'brightness-100' ? 100 : null;
  const captureConnected = captureState === 'connected' ? true : captureState === 'disconnected' ? false : null;
  const [document, setDocument] = useState<SduiDocument>(sampleDocument);
  const [screenIndex, setScreenIndex] = useState(() => {
    const screenRoutes: Record<string, string> = { home: 'sky-home', watch: 'sky-clock', weather: 'sky-weather', navigation: 'sky-navigation', spotify: 'spotify-player', usb: 'computer-controls' };
    const initialScreen = startSettingsMode ? (referenceSettingsMode || ['settings', 'wifi'].includes(query.get('screen') ?? '') ? 'device-control-center' : 'device-additional-options') : screenRoutes[query.get('screen') ?? ''] ?? 'sky-home';
    const initialIndex = sampleDocument.screens.findIndex((item) => item.id === initialScreen);
    return initialIndex >= 0 ? initialIndex : 0;
  });
  const [clock24, setClock24] = useState(() => query.has('format') ? query.get('format') === '24' : stored('clock_24', Boolean(persistence.settings.clock_24_hour.default)));
  const [measurementSystem, setMeasurementSystem] = useState<'imperial' | 'metric'>(() => query.get('units') === 'imperial' || query.get('units') === 'metric' ? query.get('units') as 'imperial' | 'metric' : storedMeasurementSystem());
  const weatherFahrenheit = measurementSystem === 'imperial';
  const [brightness, setBrightness] = useState(() => captureBrightness ?? stored('brightness', Number(persistence.settings.brightness.default)));
  const [playing, setPlaying] = useState(injectedState === 'spotify-playing');
  const [localPanel, setLocalPanel] = useState(() => query.get('screen') === 'wifi' ? 'wifi' : 'none');
  const [pressed, setPressed] = useState<string | null>(capturePressed);
  const [backgroundMode, setBackgroundMode] = useState(() => stored('background_mode', Number(persistence.settings.background_mode.default)));
  const [backgroundArt, setBackgroundArt] = useState(() => {
    const value = query.has('art') ? Number(query.get('art')) : stored('background_art', Number(persistence.settings.background_art.default));
    return value >= 0 && value <= 2 ? value : 0;
  });
  const [appearanceDark, setAppearanceDark] = useState(() => query.has('appearance') ? query.get('appearance') !== 'light' : stored('appearance_dark', Boolean(persistence.settings.appearance_dark.default)));
  const [accentColor, setAccentColor] = useState<AccentName>(() => {
    const saved = storedString('accent_color', DEFAULT_ACCENT);
    return isAccentName(saved) ? saved : DEFAULT_ACCENT;
  });
  const [spanishLanguage, setSpanishLanguage] = useState(() => query.has('language') ? query.get('language') === 'es' : stored('spanish', Boolean(persistence.settings.spanish_language.default)));
  const [clockFace, setClockFace] = useState(() => query.has('face') ? Number(query.get('face')) : stored('clock_face', Number(persistence.settings.clock_face.default)));
  const [clockBackground, setClockBackground] = useState(() => query.has('clock-background') ? Number(query.get('clock-background')) : stored('clock_background', Number(persistence.settings.clock_background.default)));
  const [clockShowWeather, setClockShowWeather] = useState(() => query.has('clock-weather') ? query.get('clock-weather') === 'yes' : stored('clock_show_weather', Boolean(persistence.settings.clock_show_weather.default)));
  const [weatherCountry, setWeatherCountry] = useState<'' | 'US' | 'MX'>(() => {
    if (countryOverride === 'US' || countryOverride === 'MX') return countryOverride;
    if (['display-location', 'display-keypad', 'display-invalid', 'display-not-found', 'display-offline', 'display-pending'].includes(injectedState)) return 'US';
    const saved = storedString('weather_location_country', '');
    return saved === 'US' || saved === 'MX' ? saved : '';
  });
  const [keypadOpen, setKeypadOpen] = useState(() => query.get('keypad') === '1' || ['display-keypad', 'display-invalid', 'display-not-found', 'display-offline', 'display-pending'].includes(injectedState));
  const [confirmedWeatherLocation, setConfirmedWeatherLocation] = useState(() => storedString('weather_location_confirmed', ''));
  const [postalDraft, setPostalDraft] = useState(() => query.get('postal') ?? '');
  const [postalStatus, setPostalStatus] = useState(() => ({ 'display-invalid': 'Postal code is not valid.', 'display-not-found': 'No match found. Previous location kept.', 'display-offline': 'Cloud is offline. Previous location kept.', 'display-pending': 'Looking up postal code…' }[injectedState] ?? ''));
  const [expandedDisplayRow, setExpandedDisplayRow] = useState<string | null>(() => ({
    'display-background': 'background', 'display-appearance': 'appearance', 'display-units': 'units',
    'display-language': 'language', 'display-location': 'weather-location', 'display-keypad': 'weather-location',
    'display-invalid': 'weather-location', 'display-not-found': 'weather-location', 'display-offline': 'weather-location',
  }[injectedState] ?? (['background', 'appearance', 'units', 'language', 'weather-location'].includes(query.get('expand') ?? '') ? query.get('expand') : null)));
  const [displayScrollOffset, setDisplayScrollOffset] = useState(() => Number(query.get('scroll') ?? 0));
  const [backgroundGalleryOffset, setBackgroundGalleryOffset] = useState(() => query.get('gallery') === '1' ? 1 : 0);
  const [optionsView, setOptionsView] = useState<'display' | 'clock'>(() => query.get('screen') === 'clock' || injectedState === 'clock-settings' ? 'clock' : 'display');
  const [postalLookupPending, setPostalLookupPending] = useState(injectedState === 'display-pending');
  const [demoWifiName, setDemoWifiName] = useState('Mock network A');
  const [demoSavedWifi, setDemoSavedWifi] = useState<string[]>([]);
  const [demoPendingWifi, setDemoPendingWifi] = useState('');
  const [wifiConnected, setWifiConnected] = useState(captureConnected ?? false);
  const [wifiEnabled, setWifiEnabled] = useState(true);
  const [cloudConnected, setCloudConnected] = useState(captureConnected ?? false);
  const [bluetoothEnabled, setBluetoothEnabled] = useState(captureConnected ?? false);
  const [batteryPercent, setBatteryPercent] = useState(82);
  useEffect(() => {
    if (framebufferCaptureMode || calibrationMode || startSettingsMode || requestedScreen) return;
    const homeIndex = sampleDocument.screens.findIndex((item) => item.id === 'sky-home');
    if (homeIndex >= 0) setScreenIndex(homeIndex);
  }, [framebufferCaptureMode, startSettingsMode, requestedScreen]);
  const [now, setNow] = useState(() => new Date());
  const [etag, setEtag] = useState(`"mock-ui-${sampleDocument.revision}"`);
  const [pollCount, setPollCount] = useState(0);
  const [events, setEvents] = useState<EventLog[]>([{ at: new Date().toLocaleTimeString(), kind: 'boot', detail: `Mock firmware booted; UI revision ${sampleDocument.revision} loaded.`, payload: JSON.stringify({ revision: sampleDocument.revision }) }]);
  const [json, setJson] = useState(() => JSON.stringify(sampleDocument, null, 2));
  const [jsonMessage, setJsonMessage] = useState('');
  const [captureUrl, setCaptureUrl] = useState('');
  const [captureDownloadUrl, setCaptureDownloadUrl] = useState('');
  const calibrationMode = query.get('calibration') === '1';
  const [images, setImages] = useState<FirmwareImages>({});
  const settingsContact = useRef<{ row: string | null; cancelled: boolean }>({ row: null, cancelled: true });
  const pointer = useRef<{ x: number; y: number; at: number } | null>(null);
  const submenuGesture = useRef<{ owner: 'list' | 'gallery' | 'none'; startX: number; startY: number; lastX: number; lastY: number; moved: boolean } | null>(null);
  const lastSliderValue = useRef<number | null>(null);
  const sliderDrag = useRef(false);
  const baseScreen = document.screens[screenIndex] ?? document.screens[0];
  const displayPreferences = {
    expanded: expandedDisplayRow,
    scrollOffset: displayScrollOffset,
    backgroundArt,
    appearanceDark,
    accentColor,
    measurementSystem,
    spanishLanguage,
    country: weatherCountry as '' | 'US' | 'MX',
    keypadOpen,
    weatherLocation: confirmedWeatherLocation,
    postalDraft,
    postalStatus: postalLookupPending ? (spanishLanguage ? 'Buscando…' : 'Looking up…') : postalStatus,
    clock24,
    clockFace,
    clockBackground,
    clockShowWeather,
    backgroundGalleryOffset,
  };
  const optionsScreen = baseScreen?.id === 'device-additional-options'
    ? optionsView === 'clock' ? clockSettingsScreen(baseScreen, displayPreferences) : displayScreen(baseScreen, displayPreferences)
    : baseScreen;
  const screen = optionsScreen;
  const screenId = screen?.id ?? '';
  useEffect(() => {
    if (!captureUrl) return;
    let active = true;
    let objectUrl = '';
    void fetch(captureUrl).then((response) => response.blob()).then((blob) => {
      if (!active) return;
      objectUrl = URL.createObjectURL(blob);
      setCaptureDownloadUrl(objectUrl);
    });
    return () => { active = false; if (objectUrl) URL.revokeObjectURL(objectUrl); };
  }, [captureUrl]);
  function downloadFramebuffer() {
    const canvas = window.document.querySelector<HTMLCanvasElement>('.firmware-canvas');
    const link = window.document.querySelector<HTMLAnchorElement>('.raw-framebuffer-download');
    if (!link) return;
    if (canvas) {
      canvas.toBlob((blob) => {
        if (!blob) return;
        const objectUrl = URL.createObjectURL(blob);
        link.href = objectUrl;
        link.click();
        window.setTimeout(() => URL.revokeObjectURL(objectUrl), 1000);
      }, 'image/png');
      return;
    }
    if (captureDownloadUrl || captureUrl) {
      link.href = captureDownloadUrl || captureUrl;
      link.click();
    }
  }
  useEffect(() => { if (!diagnosticState) window.localStorage.setItem(`${persistence.namespace}:brightness`, String(brightness)); }, [brightness, diagnosticState]);
  useEffect(() => { if (!diagnosticState) window.localStorage.setItem(`${persistence.namespace}:clock_24`, String(clock24)); }, [clock24, diagnosticState]);
  useEffect(() => {
    if (diagnosticState) return;
    window.localStorage.setItem(`${persistence.namespace}:measurement_system`, measurementSystem);
    window.localStorage.setItem(`${persistence.namespace}:weather_f`, String(weatherFahrenheit));
  }, [measurementSystem, weatherFahrenheit, diagnosticState]);
  useEffect(() => { if (!diagnosticState) window.localStorage.setItem(`${persistence.namespace}:background_mode`, String(backgroundMode)); }, [backgroundMode, diagnosticState]);
  useEffect(() => { if (!diagnosticState) window.localStorage.setItem(`${persistence.namespace}:background_art`, String(backgroundArt)); }, [backgroundArt, diagnosticState]);
  useEffect(() => { if (!diagnosticState) window.localStorage.setItem(`${persistence.namespace}:appearance_dark`, String(appearanceDark)); }, [appearanceDark, diagnosticState]);
  useEffect(() => { if (!diagnosticState) window.localStorage.setItem(`${persistence.namespace}:accent_color`, accentColor); }, [accentColor, diagnosticState]);
  useEffect(() => { if (!diagnosticState) window.localStorage.setItem(`${persistence.namespace}:spanish`, String(spanishLanguage)); }, [spanishLanguage, diagnosticState]);
  useEffect(() => { if (!diagnosticState) window.localStorage.setItem(`${persistence.namespace}:clock_face`, String(clockFace)); }, [clockFace, diagnosticState]);
  useEffect(() => { if (!diagnosticState) window.localStorage.setItem(`${persistence.namespace}:clock_background`, String(clockBackground)); }, [clockBackground, diagnosticState]);
  useEffect(() => { if (!diagnosticState) window.localStorage.setItem(`${persistence.namespace}:clock_show_weather`, String(clockShowWeather)); }, [clockShowWeather, diagnosticState]);
  useEffect(() => { if (!diagnosticState) window.localStorage.setItem(`${persistence.namespace}:weather_location_country`, weatherCountry); }, [weatherCountry, diagnosticState]);
  useEffect(() => { if (!diagnosticState) window.localStorage.setItem(`${persistence.namespace}:weather_location_confirmed`, confirmedWeatherLocation); }, [confirmedWeatherLocation, diagnosticState]);
  useEffect(() => {
    if (['display-location', 'display-keypad', 'display-invalid', 'display-not-found', 'display-offline', 'display-pending'].includes(injectedState)) {
      setDisplayScrollOffset(displayMenuMaximumScroll('weather-location', { ...displayPreferences, country: weatherCountry, keypadOpen, expanded: 'weather-location' }));
    }
  }, []);
  useEffect(() => {
    let active = true;
    if (localPanel === 'none') return () => { active = false; };
    void Promise.all([
      ...Object.entries(assetManifest.assets).map(async ([key, asset]) => [key, await decodeFirmwareAsset(`/firmware-assets/${asset.file}`, asset.width, asset.height)] as const),
      ...referenceManifest.states.map(async (state) => [`reference-${state.name}`, `/firmware-assets/${state.generated_asset.file.split('/').pop()}`] as const),
      ['settings-neutral-exact', '/firmware-assets/settings-neutral-466.png'] as const,
      ['settings-wifi-pressed-exact', '/firmware-assets/settings-wifi-pressed-322x56.png?asset-revision=phase1-pressed-v2'] as const,
      ['settings-bluetooth-pressed-exact', '/firmware-assets/settings-bluetooth-pressed-322x56.png?asset-revision=phase1-pressed-v2'] as const,
      ['settings-display-pressed-exact', '/firmware-assets/settings-display-pressed-322x56.png?asset-revision=phase1-pressed-v2'] as const,
      ['settings-sound-pressed-exact', '/firmware-assets/settings-sound-pressed-322x56.png?asset-revision=phase1-pressed-v2'] as const,
    ]).then((entries) => { if (active) setImages(Object.fromEntries(entries)); }).catch(() => addEvent('asset', 'Firmware asset loading failed; using text fallback.'));
    return () => { active = false; };
  }, [localPanel]);
  useEffect(() => { const timer = window.setInterval(() => setNow(new Date()), 1000); return () => window.clearInterval(timer); }, []);
  const timeText = now.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit', hour12: !clock24 });
  const settingsTimeText = now.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit', hour12: false });
  const dateText = now.toLocaleDateString('en-US', { month: 'short', day: '2-digit' }).toUpperCase();
  const addEvent = (kind: string, detail: string, payload: Record<string, unknown> = {}) => setEvents((current) => [{ at: new Date().toLocaleTimeString(), kind, detail, payload: JSON.stringify(payload) }, ...current].slice(0, 30));
  const emitTap = (componentId: string, payload: Record<string, unknown> = {}) => {
    const occurredAt = new Date().toISOString();
    const eventId = `mock-leopold-${Date.now().toString(16)}-${Math.floor(Math.random() * 0xffff).toString(16).padStart(4, '0')}`;
    const event = { event_id: eventId, revision: document.revision, type: 'tap', component_id: componentId, payload, occurred_at: occurredAt };
    addEvent('event', `${componentId} emitted`, event);
  };
  const pointFor = (event: PointerEvent<HTMLDivElement>) => { const rect = event.currentTarget.getBoundingClientRect(); return { x: Math.round(((event.clientX - rect.left) / rect.width) * 466), y: Math.round(((event.clientY - rect.top) / rect.height) * 466) }; };

  async function lookupPostalCode() {
    const digits = postalDraft.replace(/\D/g, '');
    const normalizedCode = weatherCountry === 'US' && digits.length === 9 ? `${digits.slice(0, 5)}-${digits.slice(5)}` : digits;
    const valid = weatherCountry === 'US' ? /^\d{5}(?:-\d{4})?$/.test(normalizedCode) : weatherCountry === 'MX' ? /^\d{5}$/.test(normalizedCode) : false;
    if (!valid) {
      setPostalStatus(spanishLanguage ? 'Codigo postal no valido.' : 'Postal code is not valid.');
      return;
    }
    setPostalLookupPending(true);
    setPostalStatus('');
    const apiUrl = import.meta.env.VITE_API_URL ?? 'http://localhost:8000';
    try {
      const params = new URLSearchParams({ country: weatherCountry, postal_code: normalizedCode });
      const response = await fetch(`${apiUrl}/v1/weather/geocode?${params.toString()}`);
      const payload = await response.json() as { detail?: string; name?: string; admin1?: string | null; provider?: string };
      if (!response.ok) {
        const message = response.status === 404
          ? (spanishLanguage ? 'No se encontró. Se conserva la ubicación anterior.' : 'No match found. Previous location kept.')
          : (spanishLanguage ? 'La busqueda en la nube no esta disponible.' : 'Cloud lookup is unavailable.');
        setPostalStatus(message);
        addEvent('weather-location', `${message} ${payload.detail ?? ''}`.trim(), { country: weatherCountry });
        return;
      }
      setConfirmedWeatherLocation(`${weatherCountry}:${normalizedCode}`);
      setPostalDraft(digits);
      setKeypadOpen(false);
      const locationLabel = `${payload.name}${payload.admin1 ? `, ${payload.admin1}` : ''}`;
      setPostalStatus(spanishLanguage ? `Encontrado: ${locationLabel}. ${payload.provider ?? 'GeoNames'}.` : `Found: ${locationLabel}. ${payload.provider ?? 'GeoNames'}.`);
      addEvent('weather-location', `Cloud lookup confirmed ${normalizedCode}`, { country: weatherCountry, name: payload.name, provider: payload.provider });
    } catch {
      const message = spanishLanguage ? 'Sin conexion a la nube. Se conserva la ubicacion anterior.' : 'Cloud is offline. Previous location kept.';
      setPostalStatus(message);
      addEvent('weather-location', message, { country: weatherCountry });
    } finally {
      setPostalLookupPending(false);
    }
  }

  function handleSubmenuButton(buttonId: string) {
    if (buttonId === 'menu-back') {
      setPressed(null);
      if (optionsView === 'clock') setOptionsView('display');
      else {
        setLocalPanel('none');
        const settingsIndex = document.screens.findIndex((item) => item.id === 'device-control-center');
        if (settingsIndex >= 0) setScreenIndex(settingsIndex);
      }
      return;
    }
    if (optionsView === 'display') {
      const rowId = buttonId.startsWith('display-row-') ? buttonId.slice('display-row-'.length) : '';
      if (rowId === 'clock') {
        setOptionsView('clock');
        return;
      }
      if (rowId) {
        const next = expandedDisplayRow === rowId ? null : rowId;
        setKeypadOpen(false);
        setExpandedDisplayRow(next);
        const maxScroll = displayMenuMaximumScroll(next, { ...displayPreferences, expanded: next });
        setDisplayScrollOffset((offset) => Math.min(offset, maxScroll));
        return;
      }
      if (/^display-background-[0-2]$/.test(buttonId)) setBackgroundArt(Number(buttonId.slice(-1)));
      else if (buttonId === 'display-appearance-light') setAppearanceDark(false);
      else if (buttonId === 'display-appearance-dark') setAppearanceDark(true);
      else if (buttonId.startsWith('display-accent-')) setAccentColor(buttonId.slice('display-accent-'.length) as AccentName);
      else if (buttonId === 'display-units-imperial') setMeasurementSystem('imperial');
      else if (buttonId === 'display-units-metric') setMeasurementSystem('metric');
      else if (buttonId === 'display-language-en') setSpanishLanguage(false);
      else if (buttonId === 'display-language-es') setSpanishLanguage(true);
      else if (buttonId === 'display-country-US' || buttonId === 'display-country-MX') {
        const nextCountry = buttonId.endsWith('US') ? 'US' : 'MX';
        setWeatherCountry(nextCountry);
        setKeypadOpen(false);
        setPostalDraft(''); setPostalStatus('');
        const maxScroll = displayMenuMaximumScroll('weather-location', { ...displayPreferences, country: nextCountry, keypadOpen: false, expanded: 'weather-location' });
        setDisplayScrollOffset(maxScroll);
      } else if (buttonId === 'display-country') {
        setWeatherCountry(''); setPostalDraft(''); setPostalStatus('');
        setKeypadOpen(false);
      } else if (buttonId === 'display-postal-field') {
        setKeypadOpen(true);
        const maxScroll = displayMenuMaximumScroll('weather-location', { ...displayPreferences, keypadOpen: true, expanded: 'weather-location' });
        setDisplayScrollOffset(maxScroll);
      } else if (buttonId.startsWith('postal-key-')) {
        const key = buttonId.slice('postal-key-'.length);
        if (key === 'delete') setPostalDraft((value) => value.slice(0, -1));
        else if (key === 'clear') setPostalDraft('');
        else setPostalDraft((value) => value.length >= (weatherCountry === 'US' ? 9 : 5) ? value : `${value}${key}`);
      } else if (buttonId === 'display-postal-cancel') {
        setPostalDraft(''); setPostalStatus(''); setKeypadOpen(false);
        setDisplayScrollOffset(displayMenuMaximumScroll('weather-location', { ...displayPreferences, keypadOpen: false, expanded: 'weather-location' }));
      } else if (buttonId === 'display-postal-apply' && !postalLookupPending) {
        void lookupPostalCode();
      }
      return;
    }
    if (buttonId === 'clock-format-12') setClock24(false);
    else if (buttonId === 'clock-format-24') setClock24(true);
    else if (/^clock-face-[0-4]$/.test(buttonId)) setClockFace(Number(buttonId.slice(-1)));
    else if (/^clock-background-[0-2]$/.test(buttonId)) setClockBackground(Number(buttonId.slice(-1)));
    else if (buttonId === 'clock-weather-yes') setClockShowWeather(true);
    else if (buttonId === 'clock-weather-no') setClockShowWeather(false);
  }

  function buttonAction(component: SduiButton) {
    emitTap(component.id);
    addEvent('tap', `${component.id} on ${screenId}`, { revision: document.revision, component_id: component.id, type: 'tap' });
    if (component.id === 'weather-units') setMeasurementSystem((value) => value === 'imperial' ? 'metric' : 'imperial');
    if (component.id === 'spotify-toggle') setPlaying((value) => !value);
    const localTargets: Record<string, string> = {
      'open-home': 'sky-home', 'open-clock': 'sky-clock', 'open-navigation': 'sky-navigation', 'open-voltage': 'sky-voltage',
      'open-weather': 'sky-weather', 'open-usb': 'computer-controls', 'open-whatsapp': 'sky-whatsapp', 'open-spotify': 'spotify-player',
      'open-settings': 'device-control-center', 'open-additional-options': 'device-additional-options',
    };
    const target = localTargets[component.id];
    if (target) { const index = document.screens.findIndex((item) => item.id === target); if (index >= 0) { setLocalPanel('none'); setScreenIndex(index); } }
    if (component.id === 'background-toggle') setBackgroundMode((value) => (value + 1) % 3);
    if (component.id === 'language-toggle') setSpanishLanguage((value) => !value);
    if (component.id === 'background-toggle' || component.id === 'language-toggle') addEvent('state', `${component.label}: firmware preference updated`);
    if (component.id === 'spotify-toggle') addEvent('mock', 'Spotify play/pause result: accepted');
    if (component.id.startsWith('computer-')) addEvent('usb', `${component.label}: mocked USB command result`);
  }

  function updateSlider(point: { x: number; y: number }) {
    if (localPanel !== 'none') return;
    const padding = interaction.touch.slider_touch_padding;
    const slider = screen.components.find((item): item is SduiSlider => item.type === 'slider' && point.x >= item.x - padding && point.x <= item.x + item.width + padding && point.y >= item.y - padding && point.y <= item.y + item.height + padding);
    if (!slider) return;
    const value = slider.id === 'display-brightness' ? Math.max(5, sliderValue(slider, point)) : sliderValue(slider, point);
    if (lastSliderValue.current === value) return;
    lastSliderValue.current = value;
    if (slider.id === 'display-brightness') setBrightness(value);
    setDocument((current) => ({ ...current, screens: current.screens.map((item) => ({ ...item, components: item.components.map((component) => component.id === slider.id && (component.type === 'slider' || component.type === 'progress') ? { ...component, value } : component) })) }));
    addEvent('slider', `${slider.id}: ${value}%`, { revision: document.revision, component_id: slider.id, type: 'tap', value });
  }

  function handlePointerDown(event: PointerEvent<HTMLDivElement>) {
    event.currentTarget.setPointerCapture(event.pointerId);
    const point = pointFor(event); pointer.current = { ...point, at: Date.now() }; lastSliderValue.current = null;
    const submenuScreen = localPanel === 'none' && (screenId === 'device-additional-options' || screenId === 'device-clock-options');
    let owner: 'list' | 'gallery' | 'none' = 'none';
    if (submenuScreen && point.x >= 72 && point.x < 394 && point.y >= 124 && point.y < 414) {
      owner = screen.components.some((item) => item.type === 'button' && item.id.startsWith('display-background-') && containsPoint(item, point)) ? 'gallery' : 'list';
    }
    submenuGesture.current = { owner, startX: point.x, startY: point.y, lastX: point.x, lastY: point.y, moved: false };
    const padding = interaction.touch.slider_touch_padding;
    sliderDrag.current = localPanel === 'none' && screen.components.some((item) => item.type === 'slider' && point.x >= item.x - padding && point.x <= item.x + item.width + padding && point.y >= item.y - padding && point.y <= item.y + item.height + padding);
    const button = screen.components.find((item): item is SduiButton => item.type === 'button' && containsPoint(item, point));
    settingsContact.current = { row: button?.id ?? null, cancelled: !button };
    setPressed(localPanel === 'none' ? button?.id ?? null : null);
    updateSlider(point);
  }
  function handlePointerMove(event: PointerEvent<HTMLDivElement>) {
    if (!pointer.current) return;
    const point = pointFor(event);
    const gestureOwner = submenuGesture.current;
    if (gestureOwner && gestureOwner.owner !== 'none') {
      const deltaX = point.x - gestureOwner.lastX;
      const deltaY = point.y - gestureOwner.lastY;
      const totalX = point.x - gestureOwner.startX;
      const totalY = point.y - gestureOwner.startY;
      const threshold = interaction.settings_submenu.gesture_start_threshold;
      if (!gestureOwner.moved && Math.max(Math.abs(totalX), Math.abs(totalY)) >= threshold) {
        gestureOwner.moved = true;
        settingsContact.current.cancelled = true;
      }
      if (gestureOwner.moved && gestureOwner.owner === 'list' && Math.abs(totalY) >= Math.abs(totalX)) {
        const maxScroll = displayMenuMaximumScroll(expandedDisplayRow, { ...displayPreferences, expanded: expandedDisplayRow });
        setDisplayScrollOffset((offset) => Math.max(0, Math.min(maxScroll, offset - deltaY)));
      }
      gestureOwner.lastX = point.x;
      gestureOwner.lastY = point.y;
      setPressed(gestureOwner.moved ? null : pressed);
      return;
    }
    const button = screen.components.find((item): item is SduiButton => item.type === 'button' && containsPoint(item, point));
    if (screenId === 'device-control-center') {
      if (button?.id !== settingsContact.current.row) settingsContact.current.cancelled = true;
      setPressed(settingsContact.current.cancelled ? null : settingsContact.current.row);
    } else setPressed(localPanel === 'none' ? button?.id ?? null : null);
    updateSlider(point);
  }
  function handlePointerUp(event: PointerEvent<HTMLDivElement>) {
    const start = pointer.current; pointer.current = null; if (!start) return;
    setPressed(null);
    const point = pointFor(event); const gesture = classifyGesture(start, { ...point, at: Date.now() });
    const capturedOwner = submenuGesture.current;
    submenuGesture.current = null;
    if (sliderDrag.current) { sliderDrag.current = false; updateSlider(point); return; }
    if (capturedOwner?.moved) {
      if (gesture === 'swipe-up') {
        const home = document.screens.findIndex((item) => item.id === interaction.behaviour.swipe_up_screen);
        setLocalPanel('none'); setScreenIndex(home >= 0 ? home : 0);
        addEvent('navigation', 'Swipe up returned to Home from settings submenu');
        return;
      }
      if (capturedOwner.owner === 'gallery' && Math.abs(point.x - capturedOwner.startX) >= interaction.settings_submenu.gesture_start_threshold) {
        setBackgroundGalleryOffset(point.x < capturedOwner.startX ? 1 : 0);
      }
      addEvent('gesture', `${capturedOwner.owner} scroll consumed; parent navigation cancelled`);
      return;
    }
    const submenuScreen = localPanel === 'none' && (screenId === 'device-additional-options' || screenId === 'device-clock-options');
    if (submenuScreen && gesture.startsWith('swipe-') && gesture !== 'swipe-up') {
      addEvent('gesture', `${gesture} cancelled by settings submenu owner`);
      return;
    }
    addEvent('gesture', `${gesture} (${point.x}, ${point.y})`);
    if (gesture === 'swipe-left') { setLocalPanel('none'); setScreenIndex((index) => (index + 1) % document.screens.length); }
    else if (gesture === 'swipe-right') { setLocalPanel('none'); setScreenIndex((index) => (index + document.screens.length - 1) % document.screens.length); }
    else if (gesture === 'swipe-up') { const home = document.screens.findIndex((item) => item.id === interaction.behaviour.swipe_up_screen); setLocalPanel('none'); setScreenIndex(home >= 0 ? home : 0); }
    else if (gesture === 'tap' && localPanel === 'none' && screenId === 'sky-home' && point.y >= 205 && point.y <= 255 && point.x >= 167 && point.x <= 300) {
      const panel = point.x < 193 ? 'bluetooth' : point.x < 224 ? 'wifi' : 'cloud';
      setLocalPanel(panel);
      addEvent('navigation', `Home status icon opened ${panel}`);
    }
    else if (gesture === 'long-press' && screenId === 'sky-clock') { setClock24((value) => !value); addEvent('state', `Clock format: ${clock24 ? '12-hour' : '24-hour'}`); }
    else if (gesture === 'tap' && localPanel !== 'none') {
      const back = containsPoint(interaction.local_panels.back, point);
      const connectDemo = (name: string) => {
        setDemoWifiName(name); setWifiEnabled(true); setWifiConnected(true); setCloudConnected(false);
        setDemoSavedWifi((saved) => [name, ...saved.filter((item) => item !== name)].slice(0, 3));
        setDemoPendingWifi(''); setLocalPanel('wifi');
        addEvent('mock-wifi', 'Simulated connection; device network not accessed');
      };
      if (back) {
        setLocalPanel(localPanel === 'wifi-networks' ? 'wifi' : localPanel === 'wifi-password' ? 'wifi-networks' : localPanel === 'bluetooth-devices' ? 'bluetooth' : 'none');
        setDemoPendingWifi('');
      } else if (localPanel === 'wifi' && point.y >= 112 && point.y <= 176) {
        setWifiEnabled((value) => { const next = !value; if (!next) setWifiConnected(false); return next; });
        addEvent('mock-wifi', `Wi-Fi ${wifiEnabled ? 'disabled' : 'enabled'}; device network not accessed`);
      } else if (localPanel === 'wifi' && point.y >= 348 && point.y <= 408) {
        if (point.x >= 245 && point.x <= 419) setLocalPanel('wifi-networks');
        else if (point.x >= 47 && point.x <= 221) {
          setLocalPanel('wifi-networks'); addEvent('mock-wifi', demoSavedWifi.length ? 'Mock scan started; saved demo networks are available' : 'No saved demo network; mock scan started');
        }
      } else if (localPanel === 'wifi-networks' && point.x >= 34 && point.x <= 432 && point.y >= 146 && point.y < 317) {
        const row = Math.floor((point.y - 146) / 57);
        if ((point.y - 146) % 57 < 49) {
          const network = SIMULATOR_WIFI_NETWORKS[row];
          if (!network) return;
          const name = network.name;
          if (!network.requiresPassword || demoSavedWifi.includes(name)) connectDemo(name);
          else { setDemoPendingWifi(name); setLocalPanel('wifi-password'); }
        }
      } else if (localPanel === 'wifi-password' && point.x >= 111 && point.x <= 355 && point.y >= 360 && point.y <= 420) {
        connectDemo(demoPendingWifi);
      } else if (localPanel === 'wifi-networks' && point.y >= 410 && point.y <= 455) {
        addEvent('mock-wifi', 'Demo scan refreshed; no real networks scanned');
      } else if ((localPanel === 'cloud' || localPanel === 'bluetooth') && point.y >= 356) {
        setLocalPanel(localPanel === 'bluetooth' ? 'bluetooth-devices' : 'cloud');
      }
    } else if (gesture === 'tap' && localPanel === 'none' && (screenId === 'device-additional-options' || screenId === 'device-clock-options')) {
      const submenuButton = screen.components.find((item): item is SduiButton => item.type === 'button' && containsPoint(item, point));
      const inMenuViewport = point.x >= 72 && point.x < 394 && point.y >= 124 && point.y < 414;
      if (submenuButton && (submenuButton.id === 'menu-back' || inMenuViewport)) {
        emitTap(submenuButton.id);
        handleSubmenuButton(submenuButton.id);
      }
    } else if (gesture === 'tap' || (screenId === 'device-control-center' && localPanel === 'none' && interaction.behaviour.settings_release.gestures.includes(gesture))) {
      if (screenId === 'device-control-center') {
        const settingsButton = screen.components.find((item): item is SduiButton => item.type === 'button' && containsPoint(item, point));
        if (settingsContact.current.cancelled || settingsButton?.id !== settingsContact.current.row) return;
        const action = settingsButton ? interaction.behaviour.settings_row_actions[settingsButton.id as keyof typeof interaction.behaviour.settings_row_actions] : undefined;
        if (action === 'wifi-panel') { setLocalPanel('wifi'); return; }
        if (action === 'bluetooth-panel') { setLocalPanel('bluetooth'); return; }
        if (action === 'open-additional-options') {
          const index = document.screens.findIndex((item) => item.id === 'device-additional-options');
          if (index >= 0) { setOptionsView('display'); setLocalPanel('none'); setScreenIndex(index); }
          return;
        }
        if (settingsButton && action === 'undefined') {
          addEvent('navigation', `${settingsButton.label}: no destination defined`, { component_id: settingsButton.id });
          return;
        }
      }
      const padding = interaction.touch.slider_touch_padding;
      const slider = screen.components.find((item): item is SduiSlider => item.type === 'slider' && point.x >= item.x - padding && point.x <= item.x + item.width + padding && point.y >= item.y - padding && point.y <= item.y + item.height + padding);
      if (slider) { const value = slider.id === 'display-brightness' ? Math.max(5, sliderValue(slider, point)) : sliderValue(slider, point); updateSlider(point); addEvent('slider', `${slider.id}: ${value}%`, { revision: document.revision, component_id: slider.id, type: 'tap', value }); }
      if (slider?.id === 'display-brightness') setBrightness(Math.max(5, sliderValue(slider, point)));
      const button = screen.components.find((item): item is SduiButton => item.type === 'button' && point.x >= item.x && point.x < item.x + item.width && point.y >= item.y && point.y < item.y + item.height);
      if (button) buttonAction(button);
    }
  }

  function applyJson() { try { const parsed: unknown = JSON.parse(json); const result = validateSdui(parsed); if (!result.document) { setJsonMessage(result.errors.join(' ')); return; } setDocument(result.document); setEtag(`"mock-ui-${result.document.revision}"`); setScreenIndex(0); setLocalPanel('none'); setJsonMessage(`Loaded revision ${result.document.revision}; mock Cloud ETag updated.`); addEvent('cloud', `Applied SDUI revision ${result.document.revision}`, { revision: result.document.revision, etag: `mock-ui-${result.document.revision}` }); } catch { setJsonMessage('JSON is not valid.'); } }
  const status = useMemo(() => `${screenIndex + 1} / ${document.screens.length}`, [document.screens.length, screenIndex]);

  return <main className={`simulator-shell${framebufferCaptureMode ? ' framebuffer-capture' : ''}`}>
    <header className="simulator-header"><div><p className="kicker">Leopold · firmware {firmwareReference}</p><h1>Device lab</h1><p className="muted">Local working-copy preview using the shared firmware renderer. Cloud is unavailable in the browser; time, weather, Spotify, and USB responses are mocked.</p></div><div className="simulator-actions">{framebufferCaptureMode || calibrationMode ? <><a className="raw-framebuffer-download" href={captureDownloadUrl || captureUrl} download={`leopold-${referenceState || (calibrationMode ? 'calibration' : screenId)}-framebuffer.png`} aria-hidden="true" tabIndex={-1}>raw</a><button className="button secondary compact" onClick={downloadFramebuffer}>Download raw PNG</button></> : null}<a className="button secondary compact" href="/">Exit simulator</a></div></header>
    <div className="simulator-grid">
      <section className="device-column"><div className="device-frame"><div className="device-surface" onPointerDown={handlePointerDown} onPointerMove={handlePointerMove} onPointerUp={handlePointerUp} onPointerCancel={() => { pointer.current = null; submenuGesture.current = null; sliderDrag.current = false; settingsContact.current.cancelled = true; setPressed(null); }}>
    {localPanel === 'none' && !calibrationMode ? <FirmwareLvglCanvas screen={screen} pressed={pressed} timeText={screen.id === 'sky-clock' && !clock24 ? timeText : settingsTimeText} dateText={dateText} clockSecond={now.getSeconds()} weatherFahrenheit={weatherFahrenheit} batteryPercent={batteryPercent} wifiConnected={wifiConnected} cloudConnected={cloudConnected} bluetoothEnabled={bluetoothEnabled} brightness={brightness} backgroundMode={backgroundMode} playing={playing} backgroundArt={backgroundArt} appearanceDark={appearanceDark} accentColor={accentColor} clockBackground={clockBackground} clockFace={clockFace} clockShowWeather={clockShowWeather} onFramebufferCapture={setCaptureUrl} /> : <FirmwareCanvas demoWifiName={demoWifiName} demoPendingWifi={demoPendingWifi} demoSavedWifi={demoSavedWifi} screen={screen} images={images} clock24={clock24} weatherFahrenheit={weatherFahrenheit} brightness={brightness} playing={playing} localPanel={localPanel} pressed={pressed} darkBackground={appearanceDark} backgroundArt={backgroundArt} clockBackground={clockBackground} clockFace={clockFace} clockShowWeather={clockShowWeather} spanishLanguage={spanishLanguage} wifiConnected={wifiConnected} wifiEnabled={wifiEnabled} cloudConnected={cloudConnected} bluetoothEnabled={bluetoothEnabled} batteryPercent={batteryPercent} timeText={timeText} settingsTimeText={settingsTimeText} calibration={calibrationMode} onFramebufferCapture={setCaptureUrl} />}
      </div></div><div className="device-meta"><span>{screen.id}{localPanel !== 'none' ? ` · ${localPanel}` : ''}</span><span>466 × 466 · firmware renderer</span><span>{status}</span></div><p className="gesture-help">Swipe left/right to navigate · swipe up for home · hold the clock for 700 ms · drag sliders</p></section>
        <aside className="simulator-panel"><section className="status-card"><h2>Simulator state</h2><div className="state-grid"><span>Cloud</span><strong>{cloudConnected ? 'Connected (mock)' : 'Offline (mock)'}</strong><span>Wi-Fi</span><strong className={wifiConnected ? 'online' : ''}>{wifiConnected ? 'Connected (mock)' : 'Offline (mock)'}</strong><span>Bluetooth</span><strong>{bluetoothEnabled ? 'Connected (mock)' : 'Disconnected (mock)'}</strong><span>Battery</span><strong>{batteryPercent}%</strong><span>Time</span><strong>{clock24 ? '24-hour' : '12-hour'} · {timeText}</strong><span>ETag</span><strong>{etag}</strong><span>Polls</span><strong>{pollCount}</strong><span>USB</span><strong>Companion mocked</strong></div><div className="simulator-controls"><button className="text-button" onClick={() => setWifiConnected((value) => !value)}>Wi-Fi</button><button className="text-button" onClick={() => setBluetoothEnabled((value) => !value)}>Bluetooth</button><button className="text-button" onClick={() => setCloudConnected((value) => !value)}>Cloud</button><button className="text-button" onClick={() => setBatteryPercent((value) => value <= 15 ? 82 : value - 5)}>Battery</button><button className="text-button" onClick={() => { setEvents((current) => [{ at: new Date().toLocaleTimeString(), kind: 'heartbeat', detail: 'Mock heartbeat accepted (202)', payload: JSON.stringify({ firmware_version: '0.8.2', battery_percent: batteryPercent, uptime_seconds: Math.floor(performance.now() / 1000) }) }, ...current].slice(0, 30)); }}>Heartbeat</button></div></section>
        <section className="debug-card"><div className="panel-heading"><div><p className="kicker">Debug panel</p><h2>SDUI JSON</h2></div><button className="button primary compact" onClick={applyJson}>Apply JSON</button></div><textarea value={json} onChange={(event) => setJson(event.target.value)} spellCheck={false} aria-label="SDUI JSON editor" />{jsonMessage && <p className="notice" role="status">{jsonMessage}</p>}<p className="muted small-copy">The editor accepts the same schema as the Cloud. Publish through the admin endpoint only after validating the document.</p></section>
        <section className="events-card"><div className="panel-heading"><div><p className="kicker">Device events</p><h2>Event log</h2></div><button className="text-button" onClick={() => setEvents([])}>Clear</button></div><div className="event-list">{events.length === 0 ? <span className="muted">No events yet.</span> : events.map((event, index) => <div className="event-row" key={`${event.at}-${index}`}><time>{event.at}</time><strong>{event.kind}</strong><span>{event.detail}</span></div>)}</div></section>
      </aside>
    </div>
  </main>;
}
