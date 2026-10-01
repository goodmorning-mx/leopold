export type SimulatorGesture = 'tap' | 'swipe-left' | 'swipe-right' | 'swipe-up' | 'long-press' | 'release';

import interaction from '../shared/interaction.json';

const touch = interaction.touch;

export function classifyGesture(start: { x: number; y: number; at: number }, end: { x: number; y: number; at: number }): SimulatorGesture {
  const dx = end.x - start.x;
  const dy = end.y - start.y;
  const horizontal = Math.abs(dx);
  const vertical = Math.abs(dy);
  const duration = end.at - start.at;
  if (vertical >= touch.swipe_up_distance && dy < 0 && vertical * 10 >= horizontal * touch.swipe_direction_ratio * 10 && duration <= touch.swipe_up_max_duration_ms) return 'swipe-up';
  if (horizontal >= touch.swipe_horizontal_distance && horizontal * 10 >= vertical * touch.swipe_direction_ratio * 10 && duration <= touch.swipe_horizontal_max_duration_ms) return dx < 0 ? 'swipe-left' : 'swipe-right';
  if (duration >= touch.long_press_duration_ms && horizontal < touch.long_press_max_movement && vertical < touch.long_press_max_movement) return 'long-press';
  if (duration <= touch.tap_max_duration_ms && horizontal < touch.tap_max_movement && vertical < touch.tap_max_movement) return 'tap';
  return 'release';
}

export function sliderValue(component: { x: number; y: number; width: number; height: number }, point: { x: number; y: number }): number {
  if (component.height > component.width) return Math.max(0, Math.min(100, Math.round(((component.y + component.height - point.y) / component.height) * 100)));
  return Math.max(0, Math.min(100, Math.round(((point.x - component.x) / component.width) * 100)));
}

export function containsPoint(component: { x: number; y: number; width: number; height: number }, point: { x: number; y: number }): boolean {
  return point.x >= component.x && point.x < component.x + component.width && point.y >= component.y && point.y < component.y + component.height;
}
