export type SimulatorGesture = 'tap' | 'swipe-left' | 'swipe-right' | 'swipe-up' | 'long-press' | 'release';

export function classifyGesture(start: { x: number; y: number; at: number }, end: { x: number; y: number; at: number }): SimulatorGesture {
  const dx = end.x - start.x;
  const dy = end.y - start.y;
  const horizontal = Math.abs(dx);
  const vertical = Math.abs(dy);
  const duration = end.at - start.at;
  if (vertical >= 104 && dy < 0 && vertical * 10 >= horizontal * 17 && duration <= 600) return 'swipe-up';
  if (horizontal >= 96 && horizontal * 10 >= vertical * 17 && duration <= 550) return dx < 0 ? 'swipe-left' : 'swipe-right';
  if (duration >= 700 && horizontal < 40 && vertical < 40) return 'long-press';
  if (duration <= 650 && horizontal < 24 && vertical < 24) return 'tap';
  return 'release';
}

export function sliderValue(component: { x: number; y: number; width: number; height: number }, point: { x: number; y: number }): number {
  if (component.height > component.width) return Math.max(0, Math.min(100, Math.round(((component.y + component.height - point.y) / component.height) * 100)));
  return Math.max(0, Math.min(100, Math.round(((point.x - component.x) / component.width) * 100)));
}

export function containsPoint(component: { x: number; y: number; width: number; height: number }, point: { x: number; y: number }): boolean {
  return point.x >= component.x && point.x < component.x + component.width && point.y >= component.y && point.y < component.y + component.height;
}
