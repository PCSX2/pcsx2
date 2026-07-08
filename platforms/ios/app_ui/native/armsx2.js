import { NativeEventEmitter, NativeModules, Platform } from 'react-native';

const MODULE_NAME = 'Armsx2Bridge';
const Native = NativeModules[MODULE_NAME];
const emitter = Native ? new NativeEventEmitter(Native) : null;

let warnedMissing = false;
const warnMissing = () => {
  if (!__DEV__ || warnedMissing) return;
  warnedMissing = true;
  console.warn(
    `${MODULE_NAME} native module is not available. ` +
      'Make sure React Native is enabled for this build and the native module is registered.'
  );
};

async function safeCall(fn, fallback = null) {
  if (!Native) {
    warnMissing();
    return fallback;
  }
  try {
    return await fn();
  } catch (error) {
    console.warn(`${MODULE_NAME} call failed`, error);
    return fallback;
  }
}

export const subscribeToRetroAchievements = (listener) =>
  emitter?.addListener('armsx2.retroAchievements', listener);

export const subscribeToRetroAchievementsLogin = (listener) =>
  emitter?.addListener('armsx2.retroAchievementsLogin', listener);

export const subscribeToDiscord = (listener) => emitter?.addListener('armsx2.discord', listener);

export function isAvailable() {
  return Boolean(Native);
}

export async function getSetting(section, key, type = 'string') {
  return safeCall(() => Native.getSetting(section, key, type), null);
}

export async function setSetting(section, key, type, value) {
  return safeCall(() => Native.setSetting(section, key, type, value), false);
}

export async function refreshBIOS() {
  return safeCall(() => Native.refreshBIOS(), null);
}

export async function hasValidVm() {
  return safeCall(() => Native.hasValidVm(), false);
}

export async function getDataRoot() {
  return safeCall(() => Native.getDataRoot(), '');
}

export async function setDataRootOverride(path) {
  return safeCall(() => Native.setDataRootOverride(path), '');
}

export async function getRetroAchievementsState() {
  return safeCall(() => Native.getRetroAchievementsState(), null);
}

export async function refreshRetroAchievementsState() {
  return safeCall(() => Native.refreshRetroAchievementsState(), null);
}

export async function loginRetroAchievements(username, password) {
  return safeCall(() => Native.loginRetroAchievements(username, password), { success: false });
}

export async function logoutRetroAchievements() {
  return safeCall(() => Native.logoutRetroAchievements(), null);
}

export async function setRetroAchievementsEnabled(enabled) {
  return safeCall(() => Native.setRetroAchievementsEnabled(enabled), null);
}

export async function setRetroAchievementsHardcore(enabled) {
  return safeCall(() => Native.setRetroAchievementsHardcore(enabled), null);
}

export async function getDiscordProfile() {
  return safeCall(() => Native.getDiscordProfile(), null);
}

export async function beginDiscordLogin() {
  return safeCall(() => Native.beginDiscordLogin(), null);
}

export async function logoutDiscord() {
  return safeCall(() => Native.logoutDiscord(), null);
}

export async function setPadVibration(enabled) {
  return safeCall(() => Native.setPadVibration(enabled), null);
}

export async function convertIsoToChd(path) {
  return safeCall(() => Native.convertIsoToChd(path), -1);
}

export default {
  isAvailable,
  getSetting,
  setSetting,
  refreshBIOS,
  hasValidVm,
  getDataRoot,
  setDataRootOverride,
  getRetroAchievementsState,
  refreshRetroAchievementsState,
  loginRetroAchievements,
  logoutRetroAchievements,
  setRetroAchievementsEnabled,
  setRetroAchievementsHardcore,
  getDiscordProfile,
  beginDiscordLogin,
  logoutDiscord,
  setPadVibration,
  convertIsoToChd,
  subscribeToRetroAchievements,
  subscribeToRetroAchievementsLogin,
  subscribeToDiscord,
};
