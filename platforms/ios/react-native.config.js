/**
 * React Native CLI configuration for brownfield Android project at repo root.
 * Points autolinking to the Gradle project containing settings.gradle and app module.
 */
module.exports = {
  reactNativePath: './node_modules/react-native',
  project: {
    android: {
      sourceDir: '.',
      // appName: 'app',
    },
  },
};
