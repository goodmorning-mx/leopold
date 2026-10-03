import { PointerEvent, useEffect, useMemo, useRef, useState } from 'react';
import { SduiButton, SduiDocument, SduiSlider, validateSdui } from './contract';
import FirmwareCanvas, { FirmwareImages } from './FirmwareCanvas';
import { classifyGesture, containsPoint, sliderValue } from './interaction';
import { sampleDocument } from './sample';
import interaction from '../shared/interaction.json';
import persistence from '../shared/persistence.json';
import assetManifest from '../shared/asset-manifest.json';

type EventLog = { at: string; kind: string; detail: string; payload?: string };
const firmwareReference = 'firmware-v0.8.2';

function stored<T extends number | boolean>(key: string, fallback: T): T {
  const value = window.localStorage.getItem(`display:${key}`);
  if (value === null) return fallback;
  if (typeof fallback === 'boolean') return (value === 'true') as T;
  const numeric = Number(value);
  return (Number.isFinite(numeric) ? numeric : fallback) as T;
}

async function decodeFirmwareAsset(path: string, width: number, height: number): Promise<string> {
  const bytes = new Uint8Array(await (await fetch(path)).arrayBuffer());
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
  const [document, setDocument] = useState<SduiDocument>(sampleDocument);
  const [screenIndex, setScreenIndex] = useState(0);
  const [clock24, setClock24] = useState(() => stored('clock_24', Boolean(persistence.settings.clock_24_hour.default)));
  const [weatherFahrenheit, setWeatherFahrenheit] = useState(() => stored('weather_f', Boolean(persistence.settings.weather_fahrenheit.default)));
  const [brightness, setBrightness] = useState(() => stored('brightness', Number(persistence.settings.brightness.default)));
  const [playing, setPlaying] = useState(false);
  const [localPanel, setLocalPanel] = useState('none');
  const [pressed, setPressed] = useState<string | null>(null);
  const [backgroundMode, setBackgroundMode] = useState(() => stored('background_mode', Number(persistence.settings.background_mode.default)));
  const [spanishLanguage, setSpanishLanguage] = useState(() => stored('spanish', Boolean(persistence.settings.spanish_language.default)));
  const [wifiConnected, setWifiConnected] = useState(true);
  const [cloudConnected] = useState(false);
  const [bluetoothEnabled, setBluetoothEnabled] = useState(true);
  const [batteryPercent, setBatteryPercent] = useState(82);
  const [now, setNow] = useState(() => new Date());
  const [etag, setEtag] = useState('"mock-ui-12"');
  const [pollCount, setPollCount] = useState(0);
  const [events, setEvents] = useState<EventLog[]>([{ at: new Date().toLocaleTimeString(), kind: 'boot', detail: 'Mock firmware booted; UI revision 12 loaded.', payload: '{"revision":12}' }]);
  const [json, setJson] = useState(() => JSON.stringify(sampleDocument, null, 2));
  const [jsonMessage, setJsonMessage] = useState('');
  const [images, setImages] = useState<FirmwareImages>({});
  const pointer = useRef<{ x: number; y: number; at: number } | null>(null);
  const lastSliderValue = useRef<number | null>(null);
  const screen = document.screens[screenIndex] ?? document.screens[0];
  const screenId = screen?.id ?? '';
  useEffect(() => { window.localStorage.setItem(`${persistence.namespace}:brightness`, String(brightness)); }, [brightness]);
  useEffect(() => { window.localStorage.setItem(`${persistence.namespace}:clock_24`, String(clock24)); }, [clock24]);
  useEffect(() => { window.localStorage.setItem(`${persistence.namespace}:weather_f`, String(weatherFahrenheit)); }, [weatherFahrenheit]);
  useEffect(() => { window.localStorage.setItem(`${persistence.namespace}:background_mode`, String(backgroundMode)); }, [backgroundMode]);
  useEffect(() => { window.localStorage.setItem(`${persistence.namespace}:spanish`, String(spanishLanguage)); }, [spanishLanguage]);
  useEffect(() => {
    let active = true;
    void Promise.all([
      ...Object.entries(assetManifest.assets).map(async ([key, asset]) => [key, await decodeFirmwareAsset(`/firmware-assets/${asset.file}`, asset.width, asset.height)] as const),
    ]).then((entries) => { if (active) setImages(Object.fromEntries(entries)); }).catch(() => addEvent('asset', 'Firmware asset loading failed; using text fallback.'));
    return () => { active = false; };
  }, []);
  useEffect(() => { const timer = window.setInterval(() => setNow(new Date()), 1000); return () => window.clearInterval(timer); }, []);
  const timeText = now.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit', hour12: !clock24 });
  const addEvent = (kind: string, detail: string, payload: Record<string, unknown> = {}) => setEvents((current) => [{ at: new Date().toLocaleTimeString(), kind, detail, payload: JSON.stringify(payload) }, ...current].slice(0, 30));
  const emitTap = (componentId: string, payload: Record<string, unknown> = {}) => {
    const occurredAt = new Date().toISOString();
    const eventId = `mock-leopold-${Date.now().toString(16)}-${Math.floor(Math.random() * 0xffff).toString(16).padStart(4, '0')}`;
    const event = { event_id: eventId, revision: document.revision, type: 'tap', component_id: componentId, payload, occurred_at: occurredAt };
    addEvent('event', `${componentId} emitted`, event);
  };
  const pointFor = (event: PointerEvent<HTMLDivElement>) => { const rect = event.currentTarget.getBoundingClientRect(); return { x: Math.round(((event.clientX - rect.left) / rect.width) * 466), y: Math.round(((event.clientY - rect.top) / rect.height) * 466) }; };

  function buttonAction(component: SduiButton) {
    emitTap(component.id);
    addEvent('tap', `${component.id} on ${screenId}`, { revision: document.revision, component_id: component.id, type: 'tap' });
    if (component.id === 'weather-units') setWeatherFahrenheit((value) => !value);
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
    const value = sliderValue(slider, point);
    if (lastSliderValue.current === value) return;
    lastSliderValue.current = value;
    if (slider.id === 'display-brightness') setBrightness(value);
    setDocument((current) => ({ ...current, screens: current.screens.map((item) => ({ ...item, components: item.components.map((component) => component.id === slider.id && (component.type === 'slider' || component.type === 'progress') ? { ...component, value } : component) })) }));
    addEvent('slider', `${slider.id}: ${value}%`, { revision: document.revision, component_id: slider.id, type: 'tap', value });
  }

  function handlePointerDown(event: PointerEvent<HTMLDivElement>) {
    event.currentTarget.setPointerCapture(event.pointerId);
    const point = pointFor(event); pointer.current = { ...point, at: Date.now() }; lastSliderValue.current = null;
    const button = screen.components.find((item): item is SduiButton => item.type === 'button' && containsPoint(item, point));
    setPressed(button?.id ?? null);
    updateSlider(point);
  }
  function handlePointerMove(event: PointerEvent<HTMLDivElement>) {
    if (!pointer.current) return;
    const point = pointFor(event);
    const button = screen.components.find((item): item is SduiButton => item.type === 'button' && containsPoint(item, point));
    setPressed(button?.id ?? null);
    updateSlider(point);
  }
  function handlePointerUp(event: PointerEvent<HTMLDivElement>) {
    const start = pointer.current; pointer.current = null; if (!start) return;
    setPressed(null);
    const point = pointFor(event); const gesture = classifyGesture(start, { ...point, at: Date.now() });
    addEvent('gesture', `${gesture} (${point.x}, ${point.y})`);
    if (gesture === 'swipe-left') { setLocalPanel('none'); setScreenIndex((index) => (index + 1) % document.screens.length); }
    else if (gesture === 'swipe-right') { setLocalPanel('none'); setScreenIndex((index) => (index + document.screens.length - 1) % document.screens.length); }
    else if (gesture === 'swipe-up') { const home = document.screens.findIndex((item) => item.id === interaction.behaviour.swipe_up_screen); setLocalPanel('none'); setScreenIndex(home >= 0 ? home : 0); }
    else if (gesture === 'long-press' && screenId === 'sky-clock') { setClock24((value) => !value); addEvent('state', `Clock format: ${clock24 ? '12-hour' : '24-hour'}`); }
    else if (gesture === 'tap' && localPanel !== 'none') {
      if (point.x <= 116 && point.y <= 108) setLocalPanel(localPanel === 'wifi-networks' || localPanel === 'bluetooth-devices' ? (localPanel === 'wifi-networks' ? 'wifi' : 'bluetooth') : 'none');
      else if ((localPanel === 'wifi' || localPanel === 'cloud' || localPanel === 'bluetooth') && point.y >= 356) setLocalPanel(localPanel === 'wifi' ? 'wifi-networks' : localPanel === 'bluetooth' ? 'bluetooth-devices' : 'cloud');
    } else if (gesture === 'tap') {
      if (screenId === 'device-control-center' && point.x >= 72 && point.x <= 170 && point.y >= 120 && point.y <= 218) { setLocalPanel('wifi'); return; }
      if (screenId === 'device-control-center' && point.x >= 180 && point.x <= 278 && point.y >= 120 && point.y <= 218) { setLocalPanel('bluetooth'); return; }
      if (screenId === 'device-control-center' && point.x >= 124 && point.x <= 222 && point.y >= 224 && point.y <= 322) { setLocalPanel('cloud'); return; }
      const padding = interaction.touch.slider_touch_padding;
      const slider = screen.components.find((item): item is SduiSlider => item.type === 'slider' && point.x >= item.x - padding && point.x <= item.x + item.width + padding && point.y >= item.y - padding && point.y <= item.y + item.height + padding);
      if (slider) { const value = sliderValue(slider, point); updateSlider(point); addEvent('slider', `${slider.id}: ${value}%`, { revision: document.revision, component_id: slider.id, type: 'tap', value }); }
      if (slider?.id === 'display-brightness') setBrightness(sliderValue(slider, point));
      const button = screen.components.find((item): item is SduiButton => item.type === 'button' && point.x >= item.x && point.x < item.x + item.width && point.y >= item.y && point.y < item.y + item.height);
      if (button) buttonAction(button);
    }
  }

  function applyJson() { try { const parsed: unknown = JSON.parse(json); const result = validateSdui(parsed); if (!result.document) { setJsonMessage(result.errors.join(' ')); return; } setDocument(result.document); setEtag(`"mock-ui-${result.document.revision}"`); setScreenIndex(0); setLocalPanel('none'); setJsonMessage(`Loaded revision ${result.document.revision}; mock Cloud ETag updated.`); addEvent('cloud', `Applied SDUI revision ${result.document.revision}`, { revision: result.document.revision, etag: `mock-ui-${result.document.revision}` }); } catch { setJsonMessage('JSON is not valid.'); } }
  const status = useMemo(() => `${screenIndex + 1} / ${document.screens.length}`, [document.screens.length, screenIndex]);

  return <main className="simulator-shell">
    <header className="simulator-header"><div><p className="kicker">Leopold · firmware {firmwareReference}</p><h1>Device lab</h1><p className="muted">Browser rendering of the firmware source at commit {firmwareReference}. Cloud is unavailable in the browser; time, weather, Spotify, and USB responses are mocked.</p></div><a className="button secondary compact" href="/">Exit simulator</a></header>
    <div className="simulator-grid">
      <section className="device-column"><div className="device-frame"><div className="device-surface" onPointerDown={handlePointerDown} onPointerMove={handlePointerMove} onPointerUp={handlePointerUp} onPointerCancel={() => { pointer.current = null; setPressed(null); }}>
        <FirmwareCanvas screen={screen} images={images} clock24={clock24} weatherFahrenheit={weatherFahrenheit} brightness={brightness} playing={playing} localPanel={localPanel} pressed={pressed} darkBackground={backgroundMode === 2} spanishLanguage={spanishLanguage} wifiConnected={wifiConnected} cloudConnected={cloudConnected} bluetoothEnabled={bluetoothEnabled} batteryPercent={batteryPercent} timeText={timeText} />
      </div></div><div className="device-meta"><span>{screen.id}{localPanel !== 'none' ? ` · ${localPanel}` : ''}</span><span>466 × 466 · firmware renderer</span><span>{status}</span></div><p className="gesture-help">Swipe left/right to navigate · swipe up for home · hold the clock for 700 ms · drag sliders</p></section>
        <aside className="simulator-panel"><section className="status-card"><h2>Simulator state</h2><div className="state-grid"><span>Cloud</span><strong>Unavailable (browser)</strong><span>Wi-Fi</span><strong className={wifiConnected ? 'online' : ''}>{wifiConnected ? 'Connected (mock)' : 'Offline (mock)'}</strong><span>Bluetooth</span><strong>{bluetoothEnabled ? 'Enabled (mock)' : 'Disabled (mock)'}</strong><span>Battery</span><strong>{batteryPercent}%</strong><span>Time</span><strong>{clock24 ? '24-hour' : '12-hour'} · {timeText}</strong><span>ETag</span><strong>{etag}</strong><span>Polls</span><strong>{pollCount}</strong><span>USB</span><strong>Companion mocked</strong></div><div className="simulator-controls"><button className="text-button" onClick={() => setWifiConnected((value) => !value)}>Wi-Fi</button><button className="text-button" onClick={() => setBluetoothEnabled((value) => !value)}>Bluetooth</button><button className="text-button" onClick={() => setBatteryPercent((value) => value <= 15 ? 82 : value - 5)}>Battery</button><button className="text-button" onClick={() => { setEvents((current) => [{ at: new Date().toLocaleTimeString(), kind: 'heartbeat', detail: 'Mock heartbeat accepted (202)', payload: JSON.stringify({ firmware_version: '0.8.2', battery_percent: batteryPercent, uptime_seconds: Math.floor(performance.now() / 1000) }) }, ...current].slice(0, 30)); }}>Heartbeat</button></div></section>
        <section className="debug-card"><div className="panel-heading"><div><p className="kicker">Debug panel</p><h2>SDUI JSON</h2></div><button className="button primary compact" onClick={applyJson}>Apply JSON</button></div><textarea value={json} onChange={(event) => setJson(event.target.value)} spellCheck={false} aria-label="SDUI JSON editor" />{jsonMessage && <p className="notice" role="status">{jsonMessage}</p>}<p className="muted small-copy">The editor accepts the same schema as the Cloud. Publish through the admin endpoint only after validating the document.</p></section>
        <section className="events-card"><div className="panel-heading"><div><p className="kicker">Device events</p><h2>Event log</h2></div><button className="text-button" onClick={() => setEvents([])}>Clear</button></div><div className="event-list">{events.length === 0 ? <span className="muted">No events yet.</span> : events.map((event, index) => <div className="event-row" key={`${event.at}-${index}`}><time>{event.at}</time><strong>{event.kind}</strong><span>{event.detail}</span></div>)}</div></section>
      </aside>
    </div>
  </main>;
}
