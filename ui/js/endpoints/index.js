// Registry of browser endpoint kinds (ARCHITECTURE §6 names). Order = order shown in the UI.
import websocket from './websocket.js';
import webrtc from './webrtc.js';
import serial from './serial.js';
import ble from './ble.js';
import audio from './audio.js';
import loopback from './loopback.js';

export const ENDPOINTS = [websocket, webrtc, serial, ble, audio, loopback];

export function endpointModule(kind, list = ENDPOINTS) {
  return list.find((m) => m.kind === kind) || null;
}

export { HostRouter } from './router.js';
