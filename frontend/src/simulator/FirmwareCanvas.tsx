import { useEffect, useRef } from 'react';
import { SduiButton, SduiComponent, SduiScreen, SduiText } from './contract';
import typography from '../shared/typography.json';
import interaction from '../shared/interaction.json';
import rendering from '../shared/rendering.json';
import calibrationContract from '../shared/calibration.json';
import referenceManifest from '../shared/reference-manifest.json';

export type FirmwareImages = Record<string, string>;
export const SIMULATOR_WIFI_NETWORKS = [
  { name: 'Mock network A', requiresPassword: true },
  { name: 'Mock network B', requiresPassword: false },
  { name: 'Mock network C', requiresPassword: true },
] as const;

type Props = {
  demoWifiName: string;
  demoPendingWifi: string;
  demoSavedWifi: string[];
  screen: SduiScreen;
  images: FirmwareImages;
  clock24: boolean;
  weatherFahrenheit: boolean;
  brightness: number;
  playing: boolean;
  localPanel: string;
  pressed: string | null;
  darkBackground: boolean;
  backgroundArt: number;
  clockBackground: number;
  clockFace: number;
  clockShowWeather: boolean;
  spanishLanguage: boolean;
  wifiConnected: boolean;
  wifiEnabled: boolean;
  cloudConnected: boolean;
  bluetoothEnabled: boolean;
  batteryPercent: number;
  timeText: string;
  settingsTimeText: string;
  calibration: boolean;
  onFramebufferCapture?: (dataUrl: string) => void;
};

const W = 466;
const H = 466;
const fonts: Record<number, number> = Object.fromEntries(
  Object.entries(typography.sizes).map(([key, value]) => [Number(key), value.browser]),
);
const color = (value: string | undefined, fallback = '#ffffff') => value || fallback;

function rounded(ctx: CanvasRenderingContext2D, x: number, y: number, w: number, h: number, r: number, fill: string | CanvasGradient, stroke?: string, line = 0) {
  ctx.beginPath();
  ctx.roundRect(x, y, w, h, r);
  ctx.fillStyle = fill;
  ctx.fill();
  if (stroke && line) { ctx.lineWidth = line; ctx.strokeStyle = stroke; ctx.stroke(); }
}

function text(ctx: CanvasRenderingContext2D, value: string, x: number, y: number, size: number, fill = '#fff', align: CanvasTextAlign = 'left', weight = 400) {
  ctx.font = `${weight} ${size}px Montserrat, Arial, sans-serif`;
  ctx.fillStyle = fill;
  ctx.textAlign = align;
  ctx.textBaseline = 'top';
  ctx.fillText(value, x, y);
}

function centered(ctx: CanvasRenderingContext2D, value: string, x: number, y: number, size: number, fill = '#fff', weight = 400) {
  text(ctx, value, x, y, size, fill, 'center', weight);
}

const imageCache = new Map<string, HTMLImageElement>();

function drawAsset(ctx: CanvasRenderingContext2D, images: FirmwareImages, key: string, x: number, y: number, w?: number, h?: number) {
  const source = images[key];
  if (!source) return;
  let image = imageCache.get(source);
  if (!image) {
    image = new Image();
    image.src = source;
    imageCache.set(source, image);
  }
  if (image.complete && image.naturalWidth > 0) ctx.drawImage(image, x, y, w || image.width, h || image.height);
  else image.onload = () => ctx.drawImage(image, x, y, w || image.width, h || image.height);
}

function drawWhiteAsset(ctx: CanvasRenderingContext2D, images: FirmwareImages, key: string, x: number, y: number, w: number, h: number) {
  const source = images[key]; if (!source) return;
  const image = new Image(); image.onload = () => { const offscreen = document.createElement('canvas'); offscreen.width = w; offscreen.height = h; const off = offscreen.getContext('2d'); if (!off) return; off.drawImage(image, 0, 0, w, h); off.globalCompositeOperation = 'source-in'; off.fillStyle = '#fff'; off.fillRect(0, 0, w, h); ctx.drawImage(offscreen, x, y); }; image.src = source;
}

function componentValue(component: SduiText, clock24: boolean, weatherFahrenheit: boolean, darkBackground: boolean, spanishLanguage: boolean) {
  if (component.id === 'sky-time') return clock24 ? '20:42' : '8:42 PM';
  if (component.id === 'weather-temp') return weatherFahrenheit ? '72 F' : '22 C';
  if (component.id === 'options-title') return spanishLanguage ? 'OPCIONES' : 'ADDITIONAL OPTIONS';
  if (component.id === 'background-toggle') return spanishLanguage ? (darkBackground ? 'FONDO OSCURO' : 'FONDO COLOR') : (darkBackground ? 'DARK BACKGROUND' : 'COLOR BACKGROUND');
  if (component.id === 'language-toggle') return spanishLanguage ? 'IDIOMA: ESPANOL' : 'LANGUAGE: ENGLISH';
  if (component.id === 'open-settings' && component.x === 126) return spanishLanguage ? 'ATRAS' : 'BACK';
  return component.text;
}

function drawWifi(ctx: CanvasRenderingContext2D, x: number, y: number, connected: boolean) {
  ctx.strokeStyle = connected ? '#F3F6FA' : '#D2D8E2'; ctx.lineWidth = 2;
  [11, 7].forEach((radius) => { ctx.beginPath(); ctx.arc(x, y + 4, radius, Math.PI * 1.22, Math.PI * 1.78); ctx.stroke(); });
  ctx.fillStyle = connected ? '#F3F6FA' : '#D2D8E2'; ctx.beginPath(); ctx.arc(x, y + 7, 2, 0, Math.PI * 2); ctx.fill();
}

function drawBluetooth(ctx: CanvasRenderingContext2D, x: number, y: number, enabled: boolean) {
  ctx.strokeStyle = enabled ? '#F3F6FA' : '#D2D8E2'; ctx.lineWidth = 2;
  ctx.beginPath(); ctx.moveTo(x, y - 9); ctx.lineTo(x, y + 9); ctx.moveTo(x, y - 9); ctx.lineTo(x + 6, y - 4); ctx.lineTo(x - 5, y + 5); ctx.moveTo(x - 5, y - 5); ctx.lineTo(x + 6, y + 4); ctx.lineTo(x, y + 9); ctx.stroke();
}

function drawCloud(ctx: CanvasRenderingContext2D, x: number, y: number, connected: boolean) {
  const edge = connected ? '#F3F6FA' : '#D2D8E2';
  const fill = connected ? '#C8DCFF' : '#AAB8CC';
  const disc = (cx: number, cy: number, radius: number) => {
    const gradient = ctx.createRadialGradient(cx - radius * 0.35, cy - radius * 0.35, 1, cx, cy, radius);
    gradient.addColorStop(0, '#FFFFFF'); gradient.addColorStop(0.5, fill); gradient.addColorStop(1, '#7FA6E0');
    ctx.fillStyle = gradient; ctx.strokeStyle = edge; ctx.lineWidth = 1;
    ctx.beginPath(); ctx.arc(cx, cy, radius, 0, Math.PI * 2); ctx.fill(); ctx.stroke();
  };
  disc(x + 7, y + 9, 7); disc(x + 14, y + 11, 6);
  ctx.fillStyle = fill; ctx.beginPath(); ctx.arc(x + 14, y + 2, 2, 0, Math.PI * 2); ctx.fill();
}

function drawStatus(ctx: CanvasRenderingContext2D, isHome: boolean, clock24: boolean, wifiConnected: boolean, cloudConnected: boolean, bluetoothEnabled: boolean, batteryPercent: number, timeText: string) {
  const status = rendering.status_bar;
  if (isHome) {
    // The reference home bitmap already contains the exact status icons and battery.
    // Clear and redraw only the live time so those bitmap elements are not doubled.
    const live = timeText || (clock24 ? '20:42' : '8:42');
    const match = live.match(/^(.*?)(?:\s(AM|PM))$/);
    // The shared Home asset clears only its declared live-clock mask while
    // preserving the pill pixels, so the live value needs no backing shape.
    text(ctx, match ? match[1] : live, status.time.x, status.time.y, fonts[status.time.size], status.text_color);
    if (match) text(ctx, match[2], status.time.x + 29, status.time.period_y, 8, status.text_color);
    return;
  }
  ctx.fillStyle = '#000'; ctx.fillRect(0, 0, W, status.height);
  drawBluetooth(ctx, status.bluetooth.x, status.bluetooth.y, bluetoothEnabled);
  drawWifi(ctx, status.wifi.x, status.wifi.y, wifiConnected);
  drawCloud(ctx, status.cloud.x, status.cloud.y, cloudConnected);
  if (!isHome) text(ctx, `${batteryPercent}%`, status.battery.x, status.battery.y, fonts[status.battery.size], status.text_color);
  text(ctx, timeText || (clock24 ? '20:42' : '8:42'), status.time.x, status.time.y, fonts[status.time.size], status.text_color);
}

function drawWhatsapp(ctx: CanvasRenderingContext2D, x: number, y: number) {
  rounded(ctx, x + 17, y + 17, 38, 34, 14, '#30D158');
  rounded(ctx, x + 20, y + 43, 10, 8, 3, '#30D158');
  rounded(ctx, x + 26, y + 24, 20, 14, 7, '#141416');
}

function drawUsb(ctx: CanvasRenderingContext2D, x: number, y: number, stroke: string) {
  ctx.strokeStyle = stroke;
  ctx.lineWidth = rendering.home.usb_icon.stroke;
  ctx.lineCap = 'round';
  ctx.lineJoin = 'round';
  rendering.home.usb_icon.segments.forEach(([x1, y1, x2, y2]) => {
    ctx.beginPath();
    ctx.moveTo(x + x1, y + y1);
    ctx.lineTo(x + x2, y + y2);
    ctx.stroke();
  });
}

function drawHome(ctx: CanvasRenderingContext2D, screen: SduiScreen, images: FirmwareImages, darkBackground: boolean, backgroundArt: number, pressed: string | null, clock24: boolean, wifiConnected: boolean, cloudConnected: boolean, bluetoothEnabled: boolean, batteryPercent: number, timeText: string) {
  if (backgroundArt >= 0 && backgroundArt <= 3) {
    ctx.fillStyle = darkBackground ? '#000000' : '#FFFFFF'; ctx.fillRect(0, 0, W, H);
    drawAsset(ctx, images, ['background-flower', 'background-chevron', 'background-mechanical-flower', 'background-black-white-circle'][backgroundArt], 0, 0, W, H);
  } else drawAsset(ctx, images, 'reference-home', 0, 0, W, H);
  if (backgroundArt >= 0 && backgroundArt <= 3) {
    screen.components.filter((item): item is SduiButton => item.type === 'button' && item.id.startsWith('open-')).forEach((button) => {
      const isPressed = pressed === button.id;
      const cx = button.x + button.width / 2;
      const cy = button.y + button.height / 2;
      const radius = 28;
      const style = rendering.home.glass_button;
      const fillOpacity = style.fill_opa / 100;
      const borderOpacity = style.border_opa / 100;
      const glass = ctx.createRadialGradient(cx - 11, cy - 14, 2, cx, cy, radius + 2);
      glass.addColorStop(0, `rgba(225,238,255,${Math.min(1, fillOpacity + (isPressed ? .18 : 0))})`);
      glass.addColorStop(.7, `rgba(142,169,204,${Math.min(1, fillOpacity * .66 + (isPressed ? .12 : 0))})`);
      glass.addColorStop(1, `rgba(47,69,99,${fillOpacity * .59})`);
      ctx.beginPath(); ctx.arc(cx, cy, radius, 0, Math.PI * 2); ctx.fillStyle = glass; ctx.fill();
      ctx.lineWidth = isPressed ? style.border_width + 1 : style.border_width; ctx.strokeStyle = isPressed ? '#FFFFFF' : `rgba(243,246,250,${borderOpacity})`; ctx.stroke();
      ctx.beginPath(); ctx.arc(cx, cy, radius - 4, Math.PI * 1.08, Math.PI * 1.88);
      ctx.lineWidth = 2; ctx.strokeStyle = `rgba(255,255,255,${style.inner_highlight_opa / 100 + (isPressed ? .4 : 0)})`; ctx.stroke();
      if (button.id === 'open-usb') {
        drawUsb(ctx, button.x, button.y, '#F7FBFF');
      } else if (button.id === 'open-settings') {
        ctx.save(); ctx.translate(cx, cy); ctx.fillStyle = '#FFFFFF';
        for (let tooth = 0; tooth < 8; tooth += 1) { ctx.rotate(Math.PI / 4); ctx.fillRect(-3, -18, 6, 8); }
        ctx.beginPath(); ctx.arc(0, 0, 12, 0, Math.PI * 2); ctx.fill();
        ctx.fillStyle = '#6F839E'; ctx.beginPath(); ctx.arc(0, 0, 4.5, 0, Math.PI * 2); ctx.fill(); ctx.restore();
      } else {
        const size = button.id === 'open-spotify' ? 48 : 54;
        drawAsset(ctx, images, button.id, cx - size / 2, cy - size / 2, size, size);
      }
    });
    drawHomeStatus(ctx, clock24, wifiConnected, cloudConnected, bluetoothEnabled, batteryPercent, timeText);
  }
}

function drawHomeStatus(ctx: CanvasRenderingContext2D, clock24: boolean, wifiConnected: boolean, cloudConnected: boolean, bluetoothEnabled: boolean, batteryPercent: number, timeText: string) {
  const status = rendering.status_bar;
  const pill = status.home_pill;
  rounded(ctx, pill.x, pill.y, pill.width, pill.height, pill.height / 2, 'rgba(85,92,106,.76)', 'rgba(224,232,243,.72)', 1);
  drawBluetooth(ctx, status.bluetooth.x - 4, status.bluetooth.y - 7, bluetoothEnabled);
  drawWifi(ctx, status.wifi.x - 4, status.wifi.y - 7, wifiConnected);
  drawCloud(ctx, status.cloud.x - 7, status.cloud.y - 7, cloudConnected);
  const live = timeText || (clock24 ? '20:42' : '8:42 PM');
  const match = live.match(/^(.*?)(?:\s(AM|PM))$/);
  text(ctx, match ? match[1] : live, status.time.x, status.time.y, fonts[status.time.size], status.text_color);
  if (match) text(ctx, match[2], status.time.x + 29, status.time.period_y, 8, status.text_color);
  text(ctx, `${batteryPercent}%`, status.battery.x, status.battery.y, fonts[status.battery.size], status.text_color);
}

function drawExactClock(ctx: CanvasRenderingContext2D, stateName: 'home' | 'settings', value: string) {
  const state = referenceManifest.states.find((candidate) => candidate.name === stateName);
  const mask = state?.dynamic_masks[0];
  if (!mask) return;
  ctx.save();
  ctx.beginPath();
  ctx.rect(mask.x, mask.y, mask.width, mask.height);
  ctx.clip();
  if (stateName === 'home') {
    const sampled = ctx.getImageData(mask.x - 1, mask.y, mask.width + 2, mask.height);
    const background = ctx.createImageData(mask.width, mask.height);
    for (let y = 0; y < mask.height; y += 1) {
      const left = (y * (mask.width + 2)) * 4;
      const right = (y * (mask.width + 2) + mask.width + 1) * 4;
      for (let x = 0; x < mask.width; x += 1) {
        const ratio = (x + 1) / (mask.width + 1);
        const target = (y * mask.width + x) * 4;
        for (let channel = 0; channel < 3; channel += 1) {
          background.data[target + channel] = Math.round(sampled.data[left + channel] * (1 - ratio) + sampled.data[right + channel] * ratio);
        }
        background.data[target + 3] = 255;
      }
    }
    ctx.putImageData(background, mask.x, mask.y);
  } else {
    ctx.fillStyle = '#28313E';
    ctx.fillRect(mask.x, mask.y, mask.width, mask.height);
  }
  ctx.fillStyle = '#F4F7FC';
  ctx.font = `${stateName === 'home' ? 17 : 13}px Montserrat, Arial, sans-serif`;
  ctx.textAlign = 'center';
  ctx.textBaseline = 'middle';
  ctx.fillText(value, mask.x + mask.width / 2, mask.y + mask.height / 2);
  ctx.restore();
}

function drawGeneric(ctx: CanvasRenderingContext2D, screen: SduiScreen, images: FirmwareImages, clock24: boolean, weatherFahrenheit: boolean, darkBackground: boolean, spanishLanguage: boolean, pressed: string | null, backgroundArt: number, clockBackground: number, clockFace: number, clockShowWeather: boolean, playing: boolean, timeText: string, settingsTimeText: string) {
  const flower = ['sky-clock', 'sky-voltage', 'sky-weather'].includes(screen.id);
  const isClock = screen.id === 'sky-clock';
  const isGlobalArtScreen = ['sky-weather', 'computer-controls', 'sky-navigation'].includes(screen.id) || (screen.id === 'spotify-player' && !playing);
  const artIndex = isClock ? clockBackground : isGlobalArtScreen ? backgroundArt : -1;
  if (artIndex >= 0 && artIndex <= (isClock ? 2 : 3)) {
    ctx.fillStyle = darkBackground ? '#000000' : '#FFFFFF'; ctx.fillRect(0, 0, W, H);
    drawAsset(ctx, images, ['background-flower', 'background-chevron', 'background-mechanical-flower', 'background-black-white-circle'][artIndex], 0, 0, W, H);
  } else if (flower) {
    if (!darkBackground) { drawAsset(ctx, images, 'flower', 0, 0, W, H); ctx.fillStyle = 'rgba(0,0,0,.20)'; ctx.fillRect(0, 0, W, H); }
    if (screen.id === 'sky-voltage') drawAsset(ctx, images, 'open-voltage', 193, 45, 80, 80);
  } else if (screen.id === 'device-additional-options') {
    drawAsset(ctx, images, 'settings-gears-wallpaper', 0, 0, W, H);
  } else { ctx.fillStyle = screen.background || '#000'; ctx.fillRect(0, 0, W, H); }
  if (screen.id === 'sky-weather') drawAsset(ctx, images, 'open-weather', 196, 83, 75, 75);
  screen.components.forEach((component) => {
    if (flower && ['voltage-symbol', 'weather-symbol'].includes(component.id)) return;
    if (screen.id === 'sky-weather' && ['weather-title', 'weather-location', 'weather-units'].includes(component.id)) return;
    if (component.type === 'text') {
      const size = fonts[component.size] || 20;
      const align = component.align === 'center' ? 'center' : 'left';
      const visibleText = component.id === 'sky-time' ? (clock24 ? settingsTimeText : timeText) : componentValue(component, clock24, weatherFahrenheit, darkBackground, spanishLanguage);
      text(ctx, visibleText, component.x, component.y, component.id === 'sky-time' && clockFace === 1 ? Math.max(24, size - 8) : size, color(component.color), align);
    } else if (component.type === 'button') {
      rounded(ctx, component.x, component.y, component.width, component.height, component.height / 2, pressed === component.id ? '#2C2C30' : '#141416', color(component.color), 2);
      if (component.icon === 'whatsapp') drawWhatsapp(ctx, component.x, component.y);
      else if (component.icon === 'usb') drawUsb(ctx, component.x, component.y, color(component.color));
      else if (component.label) centered(ctx, component.label, component.x + component.width / 2, component.y + component.height / 2 - 9, 18, color(component.color));
    } else if (component.type === 'progress') {
      rounded(ctx, component.x, component.y, component.width, component.height || 8, 4, '#3A3A3C');
      rounded(ctx, component.x, component.y, component.width * Math.min(1, component.value / Math.max(1, component.max || 100)), component.height || 8, 4, color(component.color));
    }
  });
  if (isClock && clockShowWeather) centered(ctx, 'WEATHER  --°', 233, 280, 14, '#A7A7A7');
}

function drawControlCenter(ctx: CanvasRenderingContext2D, brightness: number, wifiConnected: boolean, cloudConnected: boolean, bluetoothEnabled: boolean, pressed: string | null, spanishLanguage: boolean) {
  ctx.fillStyle = '#000'; ctx.fillRect(0, 0, W, H);
  centered(ctx, 'Centro de control', 233, 49, 24, '#fff');
  rounded(ctx, 54, 108, 238, 240, 42, '#1C1C1E');
  const items = [[82, 130, '⌁', 'Wi-Fi'], [186, 130, 'ᛒ', 'Bluetooth'], [134, 234, '↥', 'Nube']];
  const states = [wifiConnected, bluetoothEnabled, cloudConnected];
  items.forEach(([x, y, glyph, label], index) => { const id = index === 0 ? 'wifi' : index === 1 ? 'bluetooth' : 'cloud'; rounded(ctx, Number(x), Number(y), 78, 78, 39, pressed === id ? '#2C2C30' : states[index] ? (index === 2 ? '#30D158' : '#0A84FF') : '#3A3A3C'); centered(ctx, String(glyph), Number(x) + 39, Number(y) + 24, 28); centered(ctx, spanishLanguage && label === 'Nube' ? 'NUBE' : String(label), Number(x) + 39, Number(y) + 86, 14, states[index] ? '#fff' : '#8E8E93'); });
  rounded(ctx, 326, 116, 78, 232, 39, '#2C2C2E');
  const fillHeight = 232 * Math.max(0, Math.min(1, brightness / 100));
  rounded(ctx, 326, 348 - fillHeight, 78, fillHeight, 39, '#fff');
  centered(ctx, '⚙', 365, 300, 22, '#000');
  centered(ctx, `${brightness}%`, 365, 364, 20); centered(ctx, 'BRILLO', 365, 394, 12, '#8E8E93');
  rounded(ctx, 112, 369, 210, 55, 27, '#3D7FFF'); centered(ctx, 'Additional options', 217, 388, 14);
}

function glow(ctx: CanvasRenderingContext2D, color: string, blur: number) {
  ctx.shadowColor = color; ctx.shadowBlur = blur;
}

function drawCalibration(ctx: CanvasRenderingContext2D) {
  const calibration = calibrationContract;
  ctx.fillStyle = calibration.background;
  ctx.fillRect(0, 0, calibration.width, calibration.height);
  ctx.strokeStyle = calibration.border.color;
  ctx.lineWidth = calibration.border.width;
  ctx.strokeRect(0.5, 0.5, W - 1, H - 1);
  ctx.fillStyle = calibration.corner_markers.color;
  const marker = calibration.corner_markers.size;
  [[0, 0], [W - marker, 0], [0, H - marker], [W - marker, H - marker]].forEach(([x, y]) => ctx.fillRect(x, y, marker, marker));
  ctx.strokeStyle = calibration.grid.color;
  ctx.lineWidth = calibration.grid.width;
  for (let coordinate = calibration.grid.spacing; coordinate < W; coordinate += calibration.grid.spacing) {
    ctx.beginPath(); ctx.moveTo(coordinate + 0.5, 0); ctx.lineTo(coordinate + 0.5, H); ctx.stroke();
    ctx.beginPath(); ctx.moveTo(0, coordinate + 0.5); ctx.lineTo(W, coordinate + 0.5); ctx.stroke();
  }
  ctx.strokeStyle = calibration.crosshair.color;
  ctx.lineWidth = calibration.crosshair.width;
  ctx.beginPath(); ctx.moveTo(W / 2, 0); ctx.lineTo(W / 2, H); ctx.moveTo(0, H / 2); ctx.lineTo(W, H / 2); ctx.stroke();
  ctx.strokeStyle = calibration.circles.color;
  ctx.lineWidth = calibration.circles.width;
  calibration.circles.radii.forEach((radius) => { ctx.beginPath(); ctx.arc(W / 2, H / 2, radius, 0, Math.PI * 2); ctx.stroke(); });
}

function drawBack(ctx: CanvasRenderingContext2D, x = interaction.local_panels.back.x, y = interaction.local_panels.back.y, w = interaction.local_panels.back.width, h = interaction.local_panels.back.height) {
  rounded(ctx, x, y, w, h, h / 2, '#1C1C1E'); centered(ctx, '‹', x + w / 2, y + 8, 32);
}

function drawConnectivity(ctx: CanvasRenderingContext2D, images: FirmwareImages, localPanel: string, connected: boolean, networkName: string, wifiEnabled: boolean) {
  const wifi = localPanel === 'wifi';
  const bluetooth = localPanel === 'bluetooth';
  const title = wifi ? 'Wi-Fi' : bluetooth ? 'Bluetooth' : 'GoodMorning Cloud';
  if (wifi) drawAsset(ctx, images, 'settings-gears-wallpaper', 0, 0, W, H);
  else { ctx.fillStyle = '#000000'; ctx.fillRect(0, 0, W, H); }
  drawBack(ctx);
  centered(ctx, title, 233, wifi ? 78 : 84, 24);
  const accent = connected ? '#30D158' : '#0A84FF';
  if (wifi) {
    rounded(ctx, 60, 112, 346, 64, 24, '#0D1520', '#287AC8', 1.5); rounded(ctx, 73, 122, 44, 44, 22, '#15263A', '#69B9FF', 1.5); drawWifi(ctx, 95, 142, connected);
    text(ctx, 'Wi-Fi', 132, 127, 18); text(ctx, wifiEnabled ? 'On' : 'Off', 312, 130, 15, '#BBD7F5', 'right');
    rounded(ctx, 333, 124, 58, 34, 17, wifiEnabled ? '#087BFF' : '#333944'); rounded(ctx, 336 + (wifiEnabled ? 25 : 0), 127, 28, 28, 14, '#F6FAFF');
    if (connected) {
      rounded(ctx, 60, 181, 346, 58, 21, '#0D1520', '#287AC8', 1.5); rounded(ctx, 73, 189, 42, 42, 21, '#15263A', '#69B9FF', 1.5); drawWifi(ctx, 94, 209, true);
      text(ctx, networkName, 128, 194, 15); text(ctx, 'Connected (mock)', 128, 216, 11, '#A9C9F4'); centered(ctx, '✓', 372, 199, 23, '#00D2FF');
    }
    centered(ctx, 'Available Networks', 233, connected ? 259 : 246, 17, '#BBD7F5');
    centered(ctx, 'Use Change network to scan demo results', 233, connected ? 285 : 272, 12, '#8E9BAE');
    rounded(ctx, 48, 352, 174, 52, 26, '#087BFF'); centered(ctx, 'Scan networks', 135, 370, 14);
    rounded(ctx, 244, 352, 174, 52, 26, '#172332', '#338DFF', 1); centered(ctx, 'Change network', 331, 370, 14);
    centered(ctx, 'Simulator only · network state is mocked', 233, 420, 10, '#D5E3FA');
  } else {
    rounded(ctx, 149, 112, 168, 168, 84, '#1C1C1E', accent, 5);
    centered(ctx, bluetooth ? 'ᛒ' : '↥', 233, 158, 36, accent);
    centered(ctx, bluetooth ? 'Listo para buscar' : 'Conectado', 233, 207, 20);
    centered(ctx, bluetooth ? 'Accesorios Bluetooth Low Energy' : 'Leopold esta conectado al servidor', 233, 307, 14, '#D1D1D6');
    centered(ctx, bluetooth ? '' : 'Heartbeats y UI remota activos', 233, 333, 12, '#8E8E93');
    rounded(ctx, 82, 372, 302, 62, 31, '#0A84FF'); centered(ctx, bluetooth ? 'Buscar dispositivos' : 'Reintentar', 233, 393, 16);
  }
}

function drawNetworkList(ctx: CanvasRenderingContext2D, images: FirmwareImages, bluetooth: boolean, saved: string[]) {
  drawAsset(ctx, images, 'settings-gears-wallpaper', 0, 0, W, H); drawBack(ctx);
  centered(ctx, bluetooth ? 'Bluetooth' : 'Elegir Wi-Fi', 233, bluetooth ? 84 : 88, 24);
  centered(ctx, bluetooth ? 'Toca uno para conectar' : 'Demo networks — no real scan', 233, bluetooth ? 110 : 119, 12, '#D5E3FA');
  const names = bluetooth ? ['Nearby device A', 'Nearby device B'] : SIMULATOR_WIFI_NETWORKS.map((network) => network.name);
  names.forEach((name, index) => { const y = (bluetooth ? 131 : 146) + index * 57; rounded(ctx, 34, y, 398, 49, 17, '#101923', '#27507C', 1.5); rounded(ctx, 43, y + 7, 34, 34, 17, '#172B40', '#65B7FF', 1); centered(ctx, bluetooth ? 'ᛒ' : '⌁', 60, y + 12, 18, '#BBD7F5'); text(ctx, name, 88, y + 14, 16); text(ctx, bluetooth ? 'Nearby' : saved.includes(name) ? 'Saved demo' : SIMULATOR_WIFI_NETWORKS[index]?.requiresPassword ? 'Password' : 'Open demo', 382, y + 17, 12, '#BBD7F5', 'right'); });
  rounded(ctx, 111, 410, 244, 45, 23, '#2C2C2E'); centered(ctx, 'Buscar otra vez', 233, 424, 14);
}

function drawSpotify(ctx: CanvasRenderingContext2D, images: FirmwareImages, screen: SduiScreen, playing: boolean, backgroundArt: number, darkBackground: boolean) {
  if (!playing && backgroundArt >= 0 && backgroundArt <= 3) {
    ctx.fillStyle = darkBackground ? '#000000' : '#FFFFFF'; ctx.fillRect(0, 0, W, H);
    drawAsset(ctx, images, ['background-flower', 'background-chevron', 'background-mechanical-flower', 'background-black-white-circle'][backgroundArt], 0, 0, W, H);
  } else { ctx.fillStyle = '#000'; ctx.fillRect(0, 0, W, H); }
  const arc = rendering.spotify.arc;
  const center = arc.x + arc.size / 2;
  const radius = arc.size / 2;
  const startAngle = Math.PI * (arc.rotation + arc.background_start) / 180;
  const endAngle = Math.PI * (arc.rotation + arc.background_end) / 180;
  const volumeComponent = screen.components.find((c) => c.id === 'spotify-volume' && (c.type === 'slider' || c.type === 'progress'));
  const volume = volumeComponent && (volumeComponent.type === 'slider' || volumeComponent.type === 'progress') ? volumeComponent.value : 0;
  ctx.beginPath(); ctx.arc(center, center, radius, startAngle, endAngle); ctx.strokeStyle = arc.background; ctx.lineWidth = arc.width; ctx.lineCap = 'round'; ctx.stroke();
  const valueAngle = startAngle + (endAngle - startAngle) * volume / 100;
  ctx.beginPath(); ctx.arc(center, center, radius, startAngle, valueAngle); ctx.strokeStyle = arc.active; ctx.stroke();
  ctx.fillStyle = '#fff'; ctx.beginPath(); ctx.arc(center + radius * Math.cos(valueAngle), center + radius * Math.sin(valueAngle), 7, 0, Math.PI * 2); ctx.fill();
  // These controls follow the LVGL Spotify renderer's fixed positions.
  centered(ctx, '☷', 132, 94, 24); centered(ctx, '⌘', 233, 94, 24); centered(ctx, '✓', 334, 94, 24);
  drawWhiteAsset(ctx, images, 'spotify-large', rendering.spotify.cover.x, rendering.spotify.cover.y, rendering.spotify.cover.width, rendering.spotify.cover.height);
  const track = (screen.components.find((c): c is SduiText => c.id === 'track' && c.type === 'text')?.text) || 'Sin reproducción';
  const artist = (screen.components.find((c): c is SduiText => c.id === 'artist' && c.type === 'text')?.text) || 'Abre Spotify';
  centered(ctx, track, 233, 252, 22); centered(ctx, artist, 233, 286, 14, '#D1D1D6');
  const controls = rendering.spotify.controls;
  centered(ctx, '◀', controls[0].x + controls[0].size / 2, controls[0].y, controls[0].font_size);
  centered(ctx, playing ? 'Ⅱ' : '▶', controls[1].x + controls[1].size / 2, controls[1].y, controls[1].font_size);
  centered(ctx, '▶', controls[2].x + controls[2].size / 2, controls[2].y, controls[2].font_size);
  const progressComponent = screen.components.find((c) => c.id === 'spotify-seek' && (c.type === 'slider' || c.type === 'progress'));
  const progress = progressComponent && (progressComponent.type === 'slider' || progressComponent.type === 'progress') ? progressComponent.value : 0;
  const seek = rendering.spotify.seek;
  rounded(ctx, seek.x, seek.y, seek.width, seek.height, seek.radius, seek.background);
  rounded(ctx, seek.x, seek.y, seek.width * progress / 100, seek.height, seek.radius, seek.active);
}

function drawYoutube(ctx: CanvasRenderingContext2D, screen: SduiScreen) {
  ctx.fillStyle = '#000'; ctx.fillRect(0, 0, W, H);
  rounded(ctx, 172, 42, 122, 54, 18, '#FF453A'); centered(ctx, '▶ YouTube', 233, 61, 16);
  const account = screen.components.find((c): c is SduiText => c.id === 'youtube-account' && c.type === 'text')?.text || 'Tu feed'; centered(ctx, account, 233, 105, 14, '#A7A7A7');
  for (let index = 0; index < 3; index += 1) {
    const title = screen.components.find((c): c is SduiText => c.id === `youtube-title-${index}` && c.type === 'text'); if (!title) continue;
    const channel = screen.components.find((c): c is SduiText => c.id === `youtube-channel-${index}` && c.type === 'text'); const y = 139 + index * 76;
    rounded(ctx, 48, y, 370, 66, 23, index === 0 ? '#211617' : '#1C1C1E'); rounded(ctx, 64, y + 18, 30, 30, 15, '#FF453A'); centered(ctx, '▶', 79, y + 24, 12); text(ctx, title.text, 106, y + 11, 14); text(ctx, channel?.text || 'YouTube', 106, y + 36, 12, '#8E8E93');
  }
  rounded(ctx, 139, 381, 188, 54, 27, '#FF453A'); centered(ctx, 'Actualizar', 233, 397, 16);
}

export default function FirmwareCanvas({ demoWifiName, demoPendingWifi, demoSavedWifi, screen, images, clock24, weatherFahrenheit, brightness, playing, localPanel, pressed, darkBackground, backgroundArt, clockBackground, clockFace, clockShowWeather, spanishLanguage, wifiConnected, wifiEnabled, cloudConnected, bluetoothEnabled, batteryPercent, timeText, settingsTimeText, calibration, onFramebufferCapture }: Props) {
  const ref = useRef<HTMLCanvasElement>(null);
  useEffect(() => {
    const canvas = ref.current; if (!canvas) return;
    const ctx = canvas.getContext('2d'); if (!ctx) return;
    const render = () => {
      ctx.clearRect(0, 0, W, H); ctx.imageSmoothingEnabled = true;
      if (calibration) drawCalibration(ctx);
      else if (localPanel === 'wifi' || localPanel === 'cloud' || localPanel === 'bluetooth') drawConnectivity(ctx, images, localPanel, localPanel === 'wifi' ? wifiConnected : localPanel === 'cloud' ? cloudConnected : bluetoothEnabled, demoWifiName, wifiEnabled);
      else if (localPanel === 'wifi-password') {
        drawAsset(ctx, images, 'settings-gears-wallpaper', 0, 0, W, H); drawBack(ctx);
        centered(ctx, 'Demo Wi-Fi', 233, 90, 24);
        centered(ctx, demoPendingWifi, 233, 130, 20);
        centered(ctx, 'No real password is needed here.', 233, 200, 16);
        centered(ctx, 'Enter real passwords only on Leopold.', 233, 235, 14, '#A0ACB9');
        rounded(ctx, 111, 360, 244, 60, 26, '#0A84FF');
        centered(ctx, 'Connect demo network', 233, 380, 16);
      }
      else if (localPanel === 'wifi-connecting') {
        drawAsset(ctx, images, 'settings-gears-wallpaper', 0, 0, W, H); drawBack(ctx);
        rounded(ctx, 153, 91, 160, 160, 80, '#1C1C1E', '#0A84FF', 5);
        drawWifi(ctx, 233, 169, false);
        centered(ctx, 'Conectando', 233, 282, 24);
        centered(ctx, demoPendingWifi || demoWifiName, 233, 321, 18, '#D1D1D6');
        centered(ctx, 'Validando contrasena...', 233, 361, 14, '#8E8E93');
      }
      else if (localPanel === 'wifi-networks') drawNetworkList(ctx, images, false, demoSavedWifi);
      else if (localPanel === 'bluetooth-devices') drawNetworkList(ctx, images, true, []);
      else if (screen.id === 'sky-home') { drawHome(ctx, screen, images, darkBackground, backgroundArt, pressed, clock24, wifiConnected, cloudConnected, bluetoothEnabled, batteryPercent, timeText || settingsTimeText); drawExactClock(ctx, 'home', settingsTimeText); }
      else if (screen.id === 'spotify-player') drawSpotify(ctx, images, screen, playing, backgroundArt, darkBackground);
      else if (screen.id === 'youtube-feed') drawYoutube(ctx, screen);
      else drawGeneric(ctx, screen, images, clock24, weatherFahrenheit, darkBackground, spanishLanguage, pressed, backgroundArt, clockBackground, clockFace, clockShowWeather, playing, timeText, settingsTimeText);
      if (localPanel !== 'none' || new URLSearchParams(window.location.search).get('capture') === '1') {
        const dataUrl = canvas.toDataURL('image/png');
        canvas.dataset.framebufferPng = dataUrl;
        onFramebufferCapture?.(dataUrl);
      }
    };
    let cancelled = false;
    let redrawTimers: number[] = [];
    void Promise.all(Object.values(images).map((source) => new Promise<void>((resolve) => {
      const image = new Image();
      image.onload = () => resolve();
      image.onerror = () => resolve();
      image.src = source;
      if (image.complete) resolve();
    }))).then(() => {
      if (cancelled) return;
      render();
      redrawTimers = [100, 300, 700].map((delay) => window.setTimeout(render, delay));
    });
    return () => { cancelled = true; redrawTimers.forEach((timer) => window.clearTimeout(timer)); };
  }, [demoWifiName, demoPendingWifi, demoSavedWifi, screen, images, clock24, weatherFahrenheit, brightness, playing, localPanel, pressed, darkBackground, backgroundArt, clockBackground, clockFace, clockShowWeather, spanishLanguage, wifiConnected, wifiEnabled, cloudConnected, bluetoothEnabled, batteryPercent, timeText, settingsTimeText, calibration, onFramebufferCapture]);
  return <canvas ref={ref} width={W} height={H} className="firmware-canvas" aria-label={`Firmware screen ${screen.id}`} />;
}
