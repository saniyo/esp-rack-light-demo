import { AxiosPromise } from "axios";

import { AXIOS, WEB_SOCKET_ROOT } from "../api/endpoints";
import { LightMqttSettings, LightState } from "./types";

export const LIGHT_SETTINGS_WEBSOCKET_URL = WEB_SOCKET_ROOT + "lightState";

export function readState(): AxiosPromise<Record<string, any>> {
  return AXIOS.get('/lightState');
}

export function updateState(data: Record<string, any>): AxiosPromise<Record<string, any>> {
  return AXIOS.post('/lightState', data);
}

export function readLightState(): AxiosPromise<LightState> {
  return AXIOS.get('/lightState');
}

export function updateLightState(lightState: LightState): AxiosPromise<LightState> {
  return AXIOS.post('/lightState', lightState);
}

export function readBrokerSettings(): AxiosPromise<LightMqttSettings> {
  return AXIOS.get('/brokerSettings');
}

export function updateBrokerSettings(lightMqttSettings: LightMqttSettings): AxiosPromise<LightMqttSettings> {
  return AXIOS.post('/brokerSettings', lightMqttSettings);
}
