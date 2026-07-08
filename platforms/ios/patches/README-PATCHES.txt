ReanimatedPackage.java patched to avoid hard dependency on ReactApplication.
Fallback: uses reflection to retrieve reactInstanceManager from the current Activity.
Keep in sync with node_modules/react-native-reanimated version pinned in package.json.

this only applies for react native builds of ARMSX2.
