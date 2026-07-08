import React, { useState, useEffect } from 'react';
import { SafeAreaView, View, Text, StyleSheet, ScrollView, Pressable, Platform, Dimensions } from 'react-native';
import ReactNativeHapticFeedback from 'react-native-haptic-feedback';
import { useTheme } from '../theme.jsx';

const hapticOptions = {
  enableVibrateFallback: true,
  ignoreAndroidSystemSettings: false,
};

function AboutScreen({ navigation }) {
  const { colors } = useTheme();
  const [deviceInfo, setDeviceInfo] = useState({});
  const [appInfo, setAppInfo] = useState({});
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    const getDeviceInfo = async () => {
      try {
        const [
          brand,
          model,
          deviceName,
          deviceType,
          systemVersion,
          apiLevel,
          buildNumber,
          bootloader,
          codename,
          fingerprint,
          hardware,
          manufacturer,
          product,
          serialNumber,
          androidId,
          baseOs,
          securityPatch,
          systemManufacturer,
          deviceId,
          carrier,
          totalMemory,
          totalDisk,
          freeDisk,
          batteryLevel,
          powerState,
          firstInstall,
          lastUpdate,
          installReferrer,
          isEmulator,
          hasNotch,
          hasVulkan,
          maxMemory,
          usedMemory,
          cpuArchitectures
        ] = await Promise.all([
          DeviceInfo.getBrand(),
          DeviceInfo.getModel(),
          DeviceInfo.getDeviceName(),
          DeviceInfo.getDeviceType(),
          DeviceInfo.getSystemVersion(),
          DeviceInfo.getApiLevel(),
          DeviceInfo.getBuildNumber(),
          DeviceInfo.getBootloader(),
          DeviceInfo.getCodename(),
          DeviceInfo.getFingerprint(),
          DeviceInfo.getHardware(),
          DeviceInfo.getManufacturer(),
          DeviceInfo.getProduct(),
          DeviceInfo.getSerialNumber().catch(() => 'N/A'),
          DeviceInfo.getAndroidId(),
          DeviceInfo.getBaseOs(),
          DeviceInfo.getSecurityPatch(),
          DeviceInfo.getSystemManufacturer(),
          DeviceInfo.getDeviceId(),
          DeviceInfo.getCarrier(),
          DeviceInfo.getTotalMemory(),
          DeviceInfo.getTotalDiskCapacity(),
          DeviceInfo.getFreeDiskStorage(),
          DeviceInfo.getBatteryLevel(),
          DeviceInfo.getPowerState(),
          DeviceInfo.getFirstInstallTime(),
          DeviceInfo.getLastUpdateTime(),
          DeviceInfo.getInstallReferrer().catch(() => 'N/A'),
          DeviceInfo.isEmulator(),
          DeviceInfo.hasNotch(),
          DeviceInfo.hasSystemFeature('android.hardware.vulkan.version').catch(() => false),
          DeviceInfo.getMaxMemory(),
          DeviceInfo.getUsedMemory(),
          DeviceInfo.supportedCpuArchitectures()
        ]);

        const screenData = Dimensions.get('window');
        const screenPhysical = Dimensions.get('screen');

        setDeviceInfo({
          brand,
          model,
          deviceName,
          deviceType,
          systemVersion,
          apiLevel,
          buildNumber,
          bootloader,
          codename,
          fingerprint: fingerprint?.substring(0, 50) + '...' || 'N/A',
          hardware,
          manufacturer,
          product,
          serialNumber,
          androidId: androidId?.substring(0, 16) + '...' || 'N/A',
          baseOs,
          securityPatch,
          systemManufacturer,
          deviceId,
          carrier,
          totalMemory,
          totalDisk,
          freeDisk,
          batteryLevel,
          powerState,
          firstInstall,
          lastUpdate,
          installReferrer,
          isEmulator,
          hasNotch,
          hasVulkan,
          maxMemory,
          usedMemory,
          cpuArchitectures,
          screenWidth: Math.round(screenData.width),
          screenHeight: Math.round(screenData.height),
          screenScale: screenData.scale,
          screenPhysicalWidth: Math.round(screenPhysical.width),
          screenPhysicalHeight: Math.round(screenPhysical.height),
        });

        const [
          appName,
          bundleId,
          version,
          buildVersion,
          readableVersion
        ] = await Promise.all([
          DeviceInfo.getApplicationName(),
          DeviceInfo.getBundleId(),
          DeviceInfo.getVersion(),
          DeviceInfo.getBuildNumber(),
          DeviceInfo.getReadableVersion()
        ]);

        setAppInfo({
          appName,
          bundleId,
          version,
          buildVersion,
          readableVersion
        });

        setLoading(false);
      } catch (error) {
        console.log('Error getting device info:', error);
        setLoading(false);
      }
    };

    getDeviceInfo();
  }, []);

  const screenData = Dimensions.get('window');
  const systemInfo = {
    platform: Platform.OS,
    version: Platform.Version,
    architecture: Platform.select({
      android: 'ARM64',
      default: 'Unknown'
    }),
    screenWidth: Math.round(screenData.width),
    screenHeight: Math.round(screenData.height),
    scale: screenData.scale
  };

  if (!colors || !colors.surface) {
    return (
      <View style={{ flex: 1, backgroundColor: '#0F1419' }}>
        <View style={{ flex: 1, justifyContent: 'center', alignItems: 'center' }}>
          <Text style={{ color: '#C4C7C5', fontSize: 16 }}>Loading...</Text>
        </View>
      </View>
    );
  }

  const formatBytes = (bytes) => {
    if (!bytes) return 'N/A';
    const gb = bytes / (1024**3);
    const mb = bytes / (1024**2);
    return gb >= 1 ? `${gb.toFixed(1)} GB` : `${mb.toFixed(0)} MB`;
  };

  const formatDate = (timestamp) => {
    if (!timestamp) return 'N/A';
    return new Date(timestamp).toLocaleDateString();
  };

  return (
    <SafeAreaView style={[styles.screen, { backgroundColor: colors.background }]}>
      <ScrollView contentContainerStyle={styles.scrollContent}>
        {/* App Info Card */}
        <View style={[styles.card, { backgroundColor: colors.surfaceContainer, borderColor: colors.outline }]}>
          <Text style={[styles.cardHeader, { color: colors.primary }]}>ARMSX2</Text>
          <Text style={[styles.appVersion, { color: colors.onSurface }]}>Version 1.0.0</Text>
          <Text style={[styles.subtitle, { color: colors.primary }]}>by MoonPower</Text>
        </View>

        {/* Credits Card */}
        <View style={[styles.card, { backgroundColor: colors.surfaceContainer, borderColor: colors.outline }]}>
          <Text style={[styles.cardHeader, { color: colors.primary }]}>Thanks to:</Text>
          <Text style={[styles.creditItem, { color: colors.onSurface }]}>• pontos2024 (emulator base)</Text>
          <Text style={[styles.creditItem, { color: colors.onSurface }]}>• PCSX2 v2.3.430 (core emulator)</Text>
          <Text style={[styles.creditItem, { color: colors.onSurface }]}>• SDL (SDL3)</Text>
          <Text style={[styles.creditItem, { color: colors.onSurface }]}>• Fffathur (Icon)</Text>
        </View>

        {/* System Info Card */}
        <View style={[styles.card, { backgroundColor: colors.surfaceContainer, borderColor: colors.outline }]}>
          <Text style={[styles.cardHeader, { color: colors.primary }]}>Device Information</Text>
          <Text style={[styles.systemInfo, { color: colors.onSurfaceVariant }]}>
            Platform: {systemInfo.platform} {systemInfo.version}{'\n'}
            Architecture: {systemInfo.architecture}{'\n'}
            Screen: {systemInfo.screenWidth}x{systemInfo.screenHeight} (@{systemInfo.scale}x){'\n'}
            Framework: React Native 0.79
          </Text>
        </View>

        {/* Action Buttons - XML Style */}
        <View style={styles.buttonContainer}>
          <Pressable 
            onPress={() => {
              ReactNativeHapticFeedback.trigger('impactLight', hapticOptions);
              navigation?.goBack();
            }} 
            style={[styles.actionButton, { backgroundColor: colors.surfaceContainerHigh }]}
            android_ripple={{ color: colors.primary + '33' }}
          >
            <View style={[styles.backIconLarge, { borderColor: colors.onSurface }]} />
            <Text style={[styles.buttonText, { color: colors.onSurface }]}>Back</Text>
          </Pressable>
        </View>
      </ScrollView>
    </SafeAreaView>
  );
}

const styles = StyleSheet.create({
  screen: {
    flex: 1,
  },
  scrollContent: {
    padding: 16,
    paddingBottom: 32,
  },
  card: {
    borderRadius: 12,
    borderWidth: 1,
    padding: 16,
    marginBottom: 16,
  },
  cardHeader: {
    fontSize: 18,
    fontWeight: '700',
    marginBottom: 12,
  },
  appVersion: {
    fontSize: 16,
    fontWeight: '600',
    marginBottom: 8,
  },
  subtitle: {
    fontSize: 14,
    fontWeight: '500',
  },
  creditItem: {
    fontSize: 14,
    lineHeight: 20,
    marginBottom: 4,
  },
  systemInfo: {
    fontSize: 14,
    lineHeight: 20,
  },
  buttonContainer: {
    marginTop: 8,
    alignItems: 'center',
  },
  actionButton: {
    flexDirection: 'row',
    alignItems: 'center',
    paddingHorizontal: 24,
    paddingVertical: 12,
    borderRadius: 8,
    minWidth: 120,
    justifyContent: 'center',
  },
  backIconLarge: {
    width: 16,
    height: 16,
    borderLeftWidth: 2,
    borderBottomWidth: 2,
    transform: [{ rotate: '45deg' }],
    marginRight: 8,
  },
  buttonText: {
    fontSize: 16,
    fontWeight: '500',
  },
});

export default AboutScreen;
