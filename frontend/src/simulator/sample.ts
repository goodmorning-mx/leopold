import { SduiDocument } from './contract';
import sharedFirmwareUi from '../shared/firmware-ui.json';

export const sampleDocument = {
  ...sharedFirmwareUi,
  revision: sharedFirmwareUi.revision,
  poll_after_ms: 5000,
} as SduiDocument;

export const sampleFirmwareVersion = sharedFirmwareUi.firmware_release.replace(/^firmware-v/, '');
