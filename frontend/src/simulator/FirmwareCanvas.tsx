import { useEffect, useRef } from 'react';
import { SduiComponent, SduiScreen, SduiText } from './contract';
import typography from '../shared/typography.json';
import rendering from '../shared/rendering.json';
import referenceManifest from '../shared/reference-manifest.json';

export type FirmwareImages = Record<string, string>;

type Props = {
  screen: SduiScreen;
  images: FirmwareImages;
  clock24: boolean;
  weatherFahrenheit: boolean;
  brightness: number;
  playing: boolean;
  localPanel: string;
  pressed: string | null;
  darkBackground: boolean;
  spanishLanguage: boolean;
  wifiConnected: boolean;
  cloudConnected: boolean;
  bluetoothEnabled: boolean;
  batteryPercent: number;
  timeText: string;
  settingsTimeText: string;
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
  if (component.id === 'weather-temp') return weatherFahrenheit ? '73 F' : '23 C';
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
    // The reference home bitmap contains a static time glyph. Paint only the
    // live time slot back to the pill color before drawing the live value so
    // the fixed bitmap time cannot show through as a duplicate.
    ctx.fillStyle = '#555C6A'; ctx.fillRect(211, 208, 68, 31);
    text(ctx, match ? match[1] : live, status.time.x, 214, fonts[status.time.size], status.text_color);
    if (match) text(ctx, match[2], status.time.x + 29, 234, 8, status.text_color);
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

function drawHome(ctx: CanvasRenderingContext2D, screen: SduiScreen, images: FirmwareImages, darkBackground: boolean, pressed: string | null) {
  drawAsset(ctx, images, 'reference-home', 0, 0, W, H);
}

function drawExactClock(ctx: CanvasRenderingContext2D, stateName: 'home' | 'settings', value: string) {
  const state = referenceManifest.states.find((candidate) => candidate.name === stateName);
  const mask = state?.dynamic_masks[0];
  if (!mask) return;
  ctx.save();
  ctx.beginPath();
  ctx.rect(mask.x, mask.y, mask.width, mask.height);
  ctx.clip();
  ctx.fillStyle = stateName === 'home' ? '#555C6A' : '#28313E';
  ctx.fillRect(mask.x, mask.y, mask.width, mask.height);
  ctx.fillStyle = '#F4F7FC';
  ctx.font = `${stateName === 'home' ? 15 : 13}px Montserrat, Arial, sans-serif`;
  ctx.textAlign = 'center';
  ctx.textBaseline = 'middle';
  ctx.fillText(value, mask.x + mask.width / 2, mask.y + mask.height / 2);
  ctx.restore();
}

function drawGeneric(ctx: CanvasRenderingContext2D, screen: SduiScreen, images: FirmwareImages, clock24: boolean, weatherFahrenheit: boolean, darkBackground: boolean, spanishLanguage: boolean, pressed: string | null) {
  const flower = ['sky-clock', 'sky-voltage', 'sky-weather'].includes(screen.id);
  if (flower) {
    if (!darkBackground) { drawAsset(ctx, images, 'flower', 0, 0, W, H); ctx.fillStyle = 'rgba(0,0,0,.20)'; ctx.fillRect(0, 0, W, H); }
    if (screen.id === 'sky-voltage') drawAsset(ctx, images, 'open-voltage', 193, 45, 80, 80);
    if (screen.id === 'sky-weather') drawAsset(ctx, images, 'open-weather', 196, 45, 75, 75);
  } else { ctx.fillStyle = screen.background || '#000'; ctx.fillRect(0, 0, W, H); }
  screen.components.forEach((component) => {
    if (flower && ['voltage-symbol', 'weather-symbol'].includes(component.id)) return;
    if (component.type === 'text') {
      const size = fonts[component.size] || 20;
      const align = component.align === 'center' ? 'center' : 'left';
      text(ctx, componentValue(component, clock24, weatherFahrenheit, darkBackground, spanishLanguage), component.x, component.y, size, color(component.color), align);
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

function drawSettingsIcon(ctx: CanvasRenderingContext2D, kind: string, x: number, y: number, active: boolean) {
  const icon = rendering.settings.icon_center;
  const iconColor = active ? icon.active_color : icon.color;
  ctx.save(); ctx.translate(x * (1 - icon.scale), y * (1 - icon.scale)); ctx.scale(icon.scale, icon.scale); glow(ctx, '#DCE9FF', 4); ctx.strokeStyle = iconColor; ctx.fillStyle = iconColor; ctx.lineWidth = icon.stroke; ctx.lineCap = 'round'; ctx.lineJoin = 'round';
  if (kind === 'wifi') { [11, 7].forEach((r) => { ctx.beginPath(); ctx.arc(x, y + 1, r, Math.PI * 1.22, Math.PI * 1.78); ctx.stroke(); }); ctx.beginPath(); ctx.arc(x, y + 5, 2, 0, Math.PI * 2); ctx.fill(); }
  if (kind === 'bluetooth') { ctx.beginPath(); ctx.moveTo(x, y - 11); ctx.lineTo(x, y + 11); ctx.moveTo(x, y - 11); ctx.lineTo(x + 7, y - 5); ctx.lineTo(x - 6, y + 5); ctx.moveTo(x - 6, y - 5); ctx.lineTo(x + 7, y + 5); ctx.lineTo(x, y + 11); ctx.stroke(); }
  if (kind === 'display') { ctx.strokeRect(x - 10, y - 8, 20, 14); ctx.beginPath(); ctx.moveTo(x, y + 6); ctx.lineTo(x, y + 11); ctx.moveTo(x - 7, y + 11); ctx.lineTo(x + 7, y + 11); ctx.stroke(); }
  if (kind === 'sound') { ctx.beginPath(); ctx.moveTo(x - 12, y - 4); ctx.lineTo(x - 6, y - 4); ctx.lineTo(x + 2, y - 11); ctx.lineTo(x + 2, y + 11); ctx.lineTo(x - 6, y + 4); ctx.lineTo(x - 12, y + 4); ctx.closePath(); ctx.stroke(); ctx.beginPath(); ctx.arc(x, y, 11, -0.9, 0.9); ctx.stroke(); }
  ctx.restore();
}

function drawGear(ctx: CanvasRenderingContext2D, x: number, y: number) {
  ctx.save();
  ctx.translate(x, y);
  ctx.fillStyle = '#65758B';
  ctx.strokeStyle = '#65758B';
  ctx.lineWidth = 4;
  ctx.lineJoin = 'round';
  for (let index = 0; index < 8; index += 1) {
    ctx.save();
    ctx.rotate(index * Math.PI / 4);
    ctx.fillRect(-2.5, -13, 5, 7);
    ctx.restore();
  }
  ctx.beginPath(); ctx.arc(0, 0, 10, 0, Math.PI * 2); ctx.fill();
  ctx.fillStyle = '#EEF5FF'; ctx.beginPath(); ctx.arc(0, 0, 4, 0, Math.PI * 2); ctx.fill();
  ctx.restore();
}

function drawSettings(ctx: CanvasRenderingContext2D, images: FirmwareImages, brightness: number, wifiConnected: boolean, bluetoothEnabled: boolean, pressed: string | null, timeText: string) {
  drawAsset(ctx, images, 'reference-settings', 0, 0, W, H);
  drawExactClock(ctx, 'settings', timeText);
  return;
  /* Reconstructed-UI fallback retained for future editable-native mode. */
  ctx.fillStyle = '#000'; ctx.fillRect(0, 0, W, H);
  const s = rendering.settings;
  const bubble = s.icon_bubble;
  const iconCenter = s.icon_center;
  const chevron = s.chevron;
  ctx.save(); glow(ctx, '#A8C9F2', 15); rounded(ctx, s.status_pill.x, s.status_pill.y, s.status_pill.width, s.status_pill.height, s.status_pill.radius, '#293A50', '#9CB3CD', 1); ctx.restore();
  drawBluetooth(ctx, 157, 55, bluetoothEnabled); drawWifi(ctx, 190, 54, wifiConnected); text(ctx, timeText, 216, 47, 20, '#F4F7FC');
  ctx.save(); glow(ctx, '#A9C9F4', s.status_cloud.glow_blur); drawCloud(ctx, s.status_cloud.x, s.status_cloud.y, true); ctx.restore(); text(ctx, '82%', 301, 48, 11, '#DFE8F4');
  ctx.save(); glow(ctx, '#EAF2FF', 18); ctx.fillStyle = '#EEF5FF'; ctx.beginPath(); ctx.arc(s.title.gear_x, s.title.gear_y, s.title.gear_diameter / 2, 0, Math.PI * 2); ctx.fill(); ctx.restore();
  drawGear(ctx, s.title.gear_x, s.title.gear_y); text(ctx, 'SETTINGS', s.title.text_x, s.title.text_y, s.title.font_size, '#F6F7FB', 'left', 500);
  s.rows.forEach((row: {id:string;x:number;y:number;width:number;height:number;active:boolean;label:string;icon:string}) => {
    const selected = row.id === 'settings-wifi' && wifiConnected;
    const rowGradient = ctx.createLinearGradient(0, row.y, 0, row.y + row.height);
    if (selected) { rowGradient.addColorStop(0, '#12B7FF'); rowGradient.addColorStop(0.48, '#087EBD'); rowGradient.addColorStop(1, '#06446F'); }
    else { rowGradient.addColorStop(0, '#3B4858'); rowGradient.addColorStop(0.45, '#202B39'); rowGradient.addColorStop(1, '#101823'); }
    ctx.save(); glow(ctx, selected ? '#079BFF' : '#BFD5F5', selected ? 18 : 11); rounded(ctx, row.x, row.y, row.width, row.height, 24, rowGradient, selected ? '#08A8FF' : '#6C7C91', selected ? 2 : 1.5); ctx.restore();
    const bubbleX = row.x + bubble.x_offset; const bubbleY = row.y + bubble.y_offset;
    const iconX = row.x + iconCenter.x_offset; const iconY = row.y + iconCenter.y_offset;
    ctx.save(); glow(ctx, bubble.glow, bubble.glow_blur); const iconGradient = ctx.createRadialGradient(bubbleX + 17, bubbleY + 14, 2, bubbleX + bubble.radius, bubbleY + bubble.radius, bubble.radius); iconGradient.addColorStop(0, '#FFFFFF'); iconGradient.addColorStop(0.55, selected ? bubble.active_gradient_inner : bubble.gradient_inner); iconGradient.addColorStop(1, selected ? bubble.active_gradient_outer : bubble.gradient_outer); ctx.fillStyle = iconGradient; ctx.beginPath(); ctx.arc(bubbleX + bubble.radius, bubbleY + bubble.radius, bubble.radius, 0, Math.PI * 2); ctx.fill(); ctx.restore();
    drawSettingsIcon(ctx, row.icon, iconX, iconY, selected);
    text(ctx, row.label, row.x + 63, row.y + 13, 20, '#F4F6FA');
    ctx.save(); glow(ctx, chevron.glow, chevron.glow_blur); ctx.strokeStyle = chevron.color; ctx.lineWidth = chevron.stroke; ctx.lineCap = 'round'; ctx.lineJoin = 'round'; ctx.beginPath(); ctx.moveTo(row.x + chevron.x_offset + chevron.points[0][0], row.y + chevron.y_offset + chevron.points[0][1]); ctx.lineTo(row.x + chevron.x_offset + chevron.points[1][0], row.y + chevron.y_offset + chevron.points[1][1]); ctx.lineTo(row.x + chevron.x_offset + chevron.points[2][0], row.y + chevron.y_offset + chevron.points[2][1]); ctx.stroke(); ctx.restore();
  });
  const slider = s.slider; const ratio = Math.max(0, Math.min(1, brightness / 100)); const knobX = slider.x + slider.width * ratio;
  ctx.save(); glow(ctx, '#29AFFF', 12); rounded(ctx, slider.x, slider.y, slider.width, slider.height, slider.radius, slider.track, '#198FD3', 1.5); const fillGradient = ctx.createLinearGradient(slider.x, 0, Math.max(slider.x + 1, knobX), 0); fillGradient.addColorStop(0, slider.fill_start); fillGradient.addColorStop(1, slider.fill_end); rounded(ctx, slider.x + 2, slider.y + 2, Math.max(0, knobX - slider.x - 2), slider.height - 4, slider.radius - 2, fillGradient); ctx.restore();
  ctx.save(); glow(ctx, '#1AA9FF', 16); ctx.fillStyle = slider.accent; ctx.beginPath(); ctx.arc(knobX, slider.y + slider.height / 2, slider.knob_radius, 0, Math.PI * 2); ctx.fill(); ctx.restore();
  centered(ctx, 'Brightness', s.brightness_label.x, s.brightness_label.y, s.brightness_label.font_size, '#F2F3F7');
}

function drawBack(ctx: CanvasRenderingContext2D, x = 28, y = 43, w = 74, h = 48) {
  rounded(ctx, x, y, w, h, h / 2, '#1C1C1E'); centered(ctx, '‹', x + w / 2, y + 8, 32);
}

function drawConnectivity(ctx: CanvasRenderingContext2D, localPanel: string) {
  const wifi = localPanel === 'wifi';
  const bluetooth = localPanel === 'bluetooth';
  const title = wifi ? 'Wi-Fi' : bluetooth ? 'Bluetooth' : 'GoodMorning Cloud';
  drawBack(ctx);
  centered(ctx, title, 233, 51, 24);
  const accent = (wifi || bluetooth) ? '#30D158' : '#30D158';
  rounded(ctx, 149, wifi || bluetooth ? 112 : 119, 168, 168, 84, '#1C1C1E', accent, 5);
  centered(ctx, wifi ? '⌁' : bluetooth ? 'ᛒ' : '↥', 233, wifi || bluetooth ? 158 : 165, 36, accent);
  centered(ctx, wifi ? 'Conectado' : bluetooth ? 'Listo para buscar' : 'Conectado', 233, wifi || bluetooth ? 207 : 214, 20);
  centered(ctx, wifi ? 'Leopold Wi-Fi  |  -42 dBm' : bluetooth ? 'Accesorios Bluetooth Low Energy' : 'Leopold esta conectado al servidor', 233, 307, 14, '#D1D1D6');
  centered(ctx, wifi ? 'La conexion se recupera automaticamente' : bluetooth ? '' : 'Heartbeats y UI remota activos', 233, 333, 12, '#8E8E93');
  rounded(ctx, wifi ? 47 : 82, 372, wifi ? 174 : 302, 62, 31, '#0A84FF');
  centered(ctx, wifi ? 'Reintentar' : bluetooth ? 'Buscar dispositivos' : 'Reintentar', wifi ? 134 : 233, 393, 16);
  if (wifi) { rounded(ctx, 245, 372, 174, 62, 31, '#2C2C2E'); centered(ctx, 'Cambiar red', 332, 393, 16); }
}

function drawNetworkList(ctx: CanvasRenderingContext2D, bluetooth: boolean) {
  ctx.fillStyle = '#000'; ctx.fillRect(0, 0, W, H); drawBack(ctx, 28, 35, 62, 46);
  centered(ctx, bluetooth ? 'Bluetooth' : 'Elegir Wi-Fi', 233, 43, 24);
  centered(ctx, bluetooth ? 'Toca uno para conectar' : 'Redes disponibles', 233, 80, 12, '#8E8E93');
  const names = bluetooth ? ['Leopold Companion', 'MacBook Pro'] : ['Leopold Wi-Fi', 'GoodMorning Guest', 'Casa'];
  names.forEach((name, index) => { const y = 111 + index * 57; rounded(ctx, 34, y, 398, 49, 17, '#1C1C1E'); centered(ctx, bluetooth ? 'ᛒ' : '⌁', 66, y + 14, 20, '#0A84FF'); text(ctx, name, 88, y + 14, 16); text(ctx, bluetooth ? '-42' : index === 0 ? '••••' : 'Abierta', 382, y + 17, 12, '#8E8E93', 'right'); });
  rounded(ctx, 111, 410, 244, 45, 23, '#2C2C2E'); centered(ctx, bluetooth ? 'Buscar otra vez' : 'Buscar otra vez', 233, 424, 14);
}

function drawSpotify(ctx: CanvasRenderingContext2D, images: FirmwareImages, screen: SduiScreen, playing: boolean) {
  ctx.fillStyle = '#000'; ctx.fillRect(0, 0, W, H);
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

export default function FirmwareCanvas({ screen, images, clock24, weatherFahrenheit, brightness, playing, localPanel, pressed, darkBackground, spanishLanguage, wifiConnected, cloudConnected, bluetoothEnabled, batteryPercent, timeText, settingsTimeText }: Props) {
  const ref = useRef<HTMLCanvasElement>(null);
  useEffect(() => {
    const canvas = ref.current; if (!canvas) return;
    const ctx = canvas.getContext('2d'); if (!ctx) return;
    const render = () => {
      ctx.clearRect(0, 0, W, H); ctx.imageSmoothingEnabled = true;
      if (localPanel === 'wifi' || localPanel === 'cloud' || localPanel === 'bluetooth') drawConnectivity(ctx, localPanel);
      else if (localPanel === 'wifi-networks') drawNetworkList(ctx, false);
      else if (localPanel === 'bluetooth-devices') drawNetworkList(ctx, true);
      else if (screen.id === 'sky-home') { drawHome(ctx, screen, images, darkBackground, pressed); drawExactClock(ctx, 'home', timeText); }
      else if (screen.id === 'device-control-center') drawSettings(ctx, images, brightness, wifiConnected, bluetoothEnabled, pressed, settingsTimeText);
      else if (screen.id === 'spotify-player') drawSpotify(ctx, images, screen, playing);
      else if (screen.id === 'youtube-feed') drawYoutube(ctx, screen);
      else drawGeneric(ctx, screen, images, clock24, weatherFahrenheit, darkBackground, spanishLanguage, pressed);
      if (localPanel === 'none' && screen.id !== 'device-control-center' && screen.id !== 'sky-home' && rendering.status_bar.visible_screens.includes(screen.id)) {
        drawStatus(ctx, screen.id === 'sky-home', clock24, wifiConnected, cloudConnected, bluetoothEnabled, batteryPercent, timeText);
      }
      if (new URLSearchParams(window.location.search).get('capture') === '1') {
        canvas.dataset.framebufferPng = canvas.toDataURL('image/png');
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
  }, [screen, images, clock24, weatherFahrenheit, brightness, playing, localPanel, pressed, darkBackground, spanishLanguage, wifiConnected, cloudConnected, bluetoothEnabled, batteryPercent, timeText, settingsTimeText]);
  return <canvas ref={ref} width={W} height={H} className="firmware-canvas" aria-label={`Firmware screen ${screen.id}`} />;
}
