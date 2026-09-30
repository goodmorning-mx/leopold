export type SduiText = {
  type: 'text'; id: string; x: number; y: number; text: string;
  align?: 'left' | 'center' | 'right'; size: number; color: string;
};
export type SduiIndicator = {
  type: 'indicator'; id: string; x: number; y: number; label: string; state: string;
};
export type SduiButton = {
  type: 'button'; id: string; x: number; y: number; label: string;
  width: number; height: number; icon?: string; color?: string;
};
export type SduiProgress = {
  type: 'progress'; id: string; x: number; y: number; value: number;
  width: number; color?: string; max?: number; height?: number;
};
export type SduiSlider = {
  type: 'slider'; id: string; x: number; y: number; value: number;
  width: number; height: number; color: string;
};
export type SduiIcon = {
  type: 'icon'; id: string; x: number; y: number; name: 'spotify' | 'bluetooth';
  size: number; color: string;
};
export type SduiComponent = SduiText | SduiIndicator | SduiButton | SduiProgress | SduiSlider | SduiIcon;
export type SduiScreen = { id: string; background: string; components: SduiComponent[] };
export type SduiDocument = { schema_version: 1; revision: number; poll_after_ms: number; screens: SduiScreen[] };

const types = new Set(['text', 'indicator', 'button', 'progress', 'slider', 'icon']);
const color = (value: unknown) => typeof value === 'string' && /^#[0-9a-f]{6}$/i.test(value);
const id = (value: unknown) => typeof value === 'string' && /^[A-Za-z0-9_-]{1,64}$/.test(value);
const integer = (value: unknown) => typeof value === 'number' && Number.isInteger(value);
const coordinate = (value: unknown) => integer(value) && (value as number) >= 0 && (value as number) <= 465;

export function validateSdui(input: unknown): { document?: SduiDocument; errors: string[] } {
  const errors: string[] = [];
  if (!input || typeof input !== 'object') return { errors: ['Document must be an object.'] };
  const raw = input as Record<string, unknown>;
  if (raw.schema_version !== 1) errors.push('schema_version must be 1.');
  if (!integer(raw.revision) || (raw.revision as number) < 1) errors.push('revision must be a positive integer.');
  if (!integer(raw.poll_after_ms) || (raw.poll_after_ms as number) < 1000 || (raw.poll_after_ms as number) > 300000) {
    errors.push('poll_after_ms must be between 1000 and 300000.');
  }
  const screens = Array.isArray(raw.screens) ? raw.screens : raw.screen ? [raw.screen] : null;
  if (!screens || screens.length < 1 || screens.length > 12) errors.push('screens must contain 1–12 screens.');
  if (errors.length) return { errors };
  screens!.forEach((screen, screenIndex) => {
    if (!screen || typeof screen !== 'object') { errors.push(`screens[${screenIndex}] must be an object.`); return; }
    const value = screen as Record<string, unknown>;
    if (!id(value.id)) errors.push(`screens[${screenIndex}].id is invalid.`);
    if (!color(value.background)) errors.push(`screens[${screenIndex}].background is invalid.`);
    if (!Array.isArray(value.components) || value.components.length > 32) { errors.push(`screens[${screenIndex}].components is invalid.`); return; }
    value.components.forEach((component, componentIndex) => {
      if (!component || typeof component !== 'object') { errors.push(`components[${componentIndex}] is invalid.`); return; }
      const item = component as Record<string, unknown>;
      if (!types.has(String(item.type))) { return; } // firmware ignores unknown component types
      if (!id(item.id) || !coordinate(item.x) || !coordinate(item.y)) errors.push(`components[${componentIndex}] has invalid id or coordinates.`);
      if (item.type === 'text' && (typeof item.text !== 'string' || !color(item.color))) errors.push(`components[${componentIndex}] text is invalid.`);
      if (item.type === 'indicator' && (typeof item.label !== 'string' || typeof item.state !== 'string')) errors.push(`components[${componentIndex}] indicator is invalid.`);
      if (item.type === 'button' && (typeof item.label !== 'string' || !integer(item.width) || !integer(item.height))) errors.push(`components[${componentIndex}] button is invalid.`);
      if ((item.type === 'progress' || item.type === 'slider') && (!integer(item.value) || (item.value as number) < 0 || (item.value as number) > 100)) errors.push(`components[${componentIndex}] ${item.type} value is invalid.`);
      if (item.type === 'icon' && !['spotify', 'bluetooth'].includes(String(item.name))) errors.push(`components[${componentIndex}] icon is invalid.`);
    });
  });
  if (errors.length) return { errors };
  return { document: { schema_version: 1, revision: raw.revision as number, poll_after_ms: raw.poll_after_ms as number, screens: screens as SduiScreen[] }, errors: [] };
}
