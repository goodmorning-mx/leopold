import rendering from '../shared/rendering.json';

export const ACCENT_NAMES = Object.keys(rendering.theme.accents) as (keyof typeof rendering.theme.accents)[];
export type AccentName = typeof ACCENT_NAMES[number];
export const DEFAULT_ACCENT = rendering.theme.default_accent as AccentName;
export const accentPalette = (name: AccentName) => rendering.theme.accents[name];
export type NeutralRole = keyof typeof rendering.theme.schemes.dark;
export const neutralRole = (role: NeutralRole, dark: boolean) =>
  rendering.theme.schemes[dark ? 'dark' : 'light'][role];
export const accentForAppearance = (name: AccentName, dark: boolean) => {
  const palette = accentPalette(name);
  return dark ? palette.accentBright : palette.accent;
};

export function isAccentName(value: string): value is AccentName {
  return ACCENT_NAMES.includes(value as AccentName);
}
