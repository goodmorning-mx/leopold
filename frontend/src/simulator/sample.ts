import { SduiDocument } from './contract';
import sharedFirmwareUi from '../shared/firmware-ui.json';

export const sampleDocument = {
  ...sharedFirmwareUi,
  revision: 12,
  poll_after_ms: 5000,
} as SduiDocument;
