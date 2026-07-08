import React, { useMemo, useState } from 'react';
import { SafeAreaView, View, Text, StyleSheet, ScrollView, Switch, Pressable } from 'react-native';
import { Picker } from '@react-native-picker/picker';
import ReactNativeHapticFeedback from 'react-native-haptic-feedback';
import SwipeableSlider from '../components/SwipeableSlider.jsx';
import { useTheme } from '../theme.jsx';

// Haptic feedback options
const hapticOptions = {
  enableVibrateFallback: true,
  ignoreAndroidSystemSettings: false,
};

function SettingsScreen({ navigation }) {
  const { colors } = useTheme();

  // General
  const [fsui, setFsui] = useState(false);
  const [frameLimiter, setFrameLimiter] = useState(true);
  const [fpsLimit, setFpsLimit] = useState(60);
  const [aspect, setAspect] = useState('16:9');
  const [fastBoot, setFastBoot] = useState(false);
  const [brightness, setBrightness] = useState(100);
  const [oscTimeout, setOscTimeout] = useState(3);
  const [oscNever, setOscNever] = useState(false);

  // Graphics
  const [renderer, setRenderer] = useState('Vulkan');
  const [upscale, setUpscale] = useState(1);
  const [filtering, setFiltering] = useState('Bilinear');
  const [interlace, setInterlace] = useState('Auto');
  const [fxaa, setFxaa] = useState(false);
  const [casMode, setCasMode] = useState('Off');
  const [casSharpness, setCasSharpness] = useState(50);
  const [hwMipmap, setHwMipmap] = useState(false);
  const [vsync, setVsync] = useState(false);
  const [autoFlushSw, setAutoFlushSw] = useState(false);
  const [autoFlushHw, setAutoFlushHw] = useState('Off');

  // Controller
  const [vibration, setVibration] = useState(true);

  // Performance 
  const [cpuCore, setCpuCore] = useState('Dynarec');

  // Enhanced switch handler with haptic feedback
  const handleSwitchChange = (setter, value) => {
    ReactNativeHapticFeedback.trigger('impactMedium', hapticOptions);
    setter(value);
  };

  // Enhanced slider handler with haptic feedback
  const handleSliderChange = (setter, value) => {
    setter(value);
  };

  const handleSliderComplete = () => {
    ReactNativeHapticFeedback.trigger('impactLight', hapticOptions);
  };

  const cardStyle = useMemo(() => ({ 
    backgroundColor: colors.surfaceContainer, 
    borderColor: colors.outline 
  }), [colors]);

  return (
    <SafeAreaView style={[styles.screen, { backgroundColor: colors.background }]}> 
      {/* Floating back button - no header bar to match XML */}
      <Pressable 
        onPress={() => {
          ReactNativeHapticFeedback.trigger('impactLight', hapticOptions);
          navigation?.goBack();
        }} 
        style={[styles.floatingBackButton, { backgroundColor: colors.surfaceContainerHigh }]}
        android_ripple={{ color: colors.primary, borderless: true }}
      >
        <Text style={[styles.backIcon, { color: colors.primary }]}>‹</Text>
      </Pressable>

      <ScrollView contentContainerStyle={styles.scrollContent}>
        <Text style={[styles.screenTitle, { color: colors.onSurface }]}>Settings</Text>

        {/* General Card */}
        <View style={[styles.card, cardStyle]}>
          <Text style={[styles.cardHeader, { color: colors.primary }]}>General</Text>
          
          <View style={styles.row}>
            <Text style={[styles.label, { color: colors.onSurface }]}>FSUI</Text>
            <Switch
              value={fsui}
              onValueChange={(value) => handleSwitchChange(setFsui, value)}
              trackColor={{ false: colors.outline, true: colors.primary }}
              thumbColor={fsui ? colors.onPrimary : colors.onSurfaceVariant}
            />
          </View>

          <View style={styles.row}>
            <Text style={[styles.label, { color: colors.onSurface }]}>Frame limiter</Text>
            <Switch
              value={frameLimiter}
              onValueChange={(value) => handleSwitchChange(setFrameLimiter, value)}
              trackColor={{ false: colors.outline, true: colors.primary }}
              thumbColor={frameLimiter ? colors.onPrimary : colors.onSurfaceVariant}
            />
          </View>

          <View style={styles.column}>
            <Text style={[styles.label, { color: colors.onSurface }]}>FPS limit</Text>
            <SwipeableSlider
              value={fpsLimit}
              minimumValue={30}
              maximumValue={120}
              step={5}
              onValueChange={(value) => handleSliderChange(setFpsLimit, value)}
              onSlidingComplete={handleSliderComplete}
              colors={colors}
              style={styles.slider}
            />
            <Text style={[styles.sliderValue, { color: colors.onSurfaceVariant }]}>{fpsLimit} fps</Text>
          </View>

          <View style={styles.column}>
            <Text style={[styles.label, { color: colors.onSurface }]}>Aspect ratio</Text>
            <View style={[styles.pickerContainer, { backgroundColor: colors.surface, borderColor: colors.outline }]}>
              <Picker
                selectedValue={aspect}
                onValueChange={setAspect}
                dropdownIconColor={colors.onSurface}
                style={[styles.picker, { color: colors.onSurface }]}
              >
                <Picker.Item label="16:9" value="16:9" color={colors.onSurface} />
                <Picker.Item label="4:3" value="4:3" color={colors.onSurface} />
                <Picker.Item label="Stretch" value="stretch" color={colors.onSurface} />
              </Picker>
            </View>
          </View>

          <View style={styles.row}>
            <Text style={[styles.label, { color: colors.onSurface }]}>Fast boot</Text>
            <Switch
              value={fastBoot}
              onValueChange={(value) => handleSwitchChange(setFastBoot, value)}
              trackColor={{ false: colors.outline, true: colors.primary }}
              thumbColor={fastBoot ? colors.onPrimary : colors.onSurfaceVariant}
            />
          </View>

          <View style={styles.column}>
            <Text style={[styles.label, { color: colors.onSurface }]}>Brightness</Text>
            <SwipeableSlider
              value={brightness}
              minimumValue={25}
              maximumValue={125}
              step={5}
              onValueChange={(value) => handleSliderChange(setBrightness, value)}
              onSlidingComplete={handleSliderComplete}
              colors={colors}
              style={styles.slider}
            />
            <Text style={[styles.sliderValue, { color: colors.onSurfaceVariant }]}>{brightness}%</Text>
          </View>
        </View>

        {/* Graphics Card */}
        <View style={[styles.card, cardStyle]}>
          <Text style={[styles.cardHeader, { color: colors.primary }]}>Graphics</Text>
          
          <View style={styles.column}>
            <Text style={[styles.label, { color: colors.onSurface }]}>Renderer</Text>
            <View style={[styles.pickerContainer, { backgroundColor: colors.surface, borderColor: colors.outline }]}>
              <Picker
                selectedValue={renderer}
                onValueChange={setRenderer}
                dropdownIconColor={colors.onSurface}
                style={[styles.picker, { color: colors.onSurface }]}
              >
                <Picker.Item label="Vulkan" value="Vulkan" color={colors.onSurface} />
                <Picker.Item label="OpenGL" value="OpenGL" color={colors.onSurface} />
                <Picker.Item label="Software" value="Software" color={colors.onSurface} />
              </Picker>
            </View>
          </View>

          <View style={styles.column}>
            <Text style={[styles.label, { color: colors.onSurface }]}>Upscaling</Text>
            <SwipeableSlider
              value={upscale}
              minimumValue={1}
              maximumValue={6}
              step={1}
              onValueChange={(value) => handleSliderChange(setUpscale, value)}
              onSlidingComplete={handleSliderComplete}
              colors={colors}
              style={styles.slider}
            />
            <Text style={[styles.sliderValue, { color: colors.onSurfaceVariant }]}>{upscale}x</Text>
          </View>

          <View style={styles.row}>
            <Text style={[styles.label, { color: colors.onSurface }]}>FXAA</Text>
            <Switch
              value={fxaa}
              onValueChange={(value) => handleSwitchChange(setFxaa, value)}
              trackColor={{ false: colors.outline, true: colors.primary }}
              thumbColor={fxaa ? colors.onPrimary : colors.onSurfaceVariant}
            />
          </View>

          <View style={styles.row}>
            <Text style={[styles.label, { color: colors.onSurface }]}>VSync</Text>
            <Switch
              value={vsync}
              onValueChange={(value) => handleSwitchChange(setVsync, value)}
              trackColor={{ false: colors.outline, true: colors.primary }}
              thumbColor={vsync ? colors.onPrimary : colors.onSurfaceVariant}
            />
          </View>
        </View>

        {/* Controller Card */}
        <View style={[styles.card, cardStyle]}>
          <Text style={[styles.cardHeader, { color: colors.primary }]}>Controller</Text>
          
          <View style={styles.row}>
            <Text style={[styles.label, { color: colors.onSurface }]}>Vibration</Text>
            <Switch
              value={vibration}
              onValueChange={(value) => handleSwitchChange(setVibration, value)}
              trackColor={{ false: colors.outline, true: colors.primary }}
              thumbColor={vibration ? colors.onPrimary : colors.onSurfaceVariant}
            />
          </View>
        </View>

        {/* Performance Card */}
        <View style={[styles.card, cardStyle]}>
          <Text style={[styles.cardHeader, { color: colors.primary }]}>Performance</Text>
          
          <View style={styles.column}>
            <Text style={[styles.label, { color: colors.onSurface }]}>CPU core</Text>
            <View style={[styles.pickerContainer, { backgroundColor: colors.surface, borderColor: colors.outline }]}>
              <Picker
                selectedValue={cpuCore}
                onValueChange={setCpuCore}
                dropdownIconColor={colors.onSurface}
                style={[styles.picker, { color: colors.onSurface }]}
              >
                <Picker.Item label="Dynarec" value="Dynarec" color={colors.onSurface} />
                <Picker.Item label="Interpreter" value="Interpreter" color={colors.onSurface} />
                <Picker.Item label="Cached interpreter" value="CachedInterpreter" color={colors.onSurface} />
              </Picker>
            </View>
          </View>
        </View>
      </ScrollView>
    </SafeAreaView>
  );
}

const styles = StyleSheet.create({
  screen: {
    flex: 1,
  },
  floatingBackButton: {
    position: 'absolute',
    top: 16,
    left: 16,
    width: 40,
    height: 40,
    borderRadius: 20,
    justifyContent: 'center',
    alignItems: 'center',
    elevation: 3,
    zIndex: 100,
    shadowColor: '#000',
    shadowOpacity: 0.3,
    shadowRadius: 3,
    shadowOffset: { width: 0, height: 2 },
  },
  backIcon: { 
    fontSize: 28, 
    fontWeight: '300' 
  },
  scrollContent: {
    padding: 16,
    paddingTop: 70, // Space for floating back button
  },
  screenTitle: {
    fontSize: 28,
    fontWeight: '700',
    marginBottom: 24,
    paddingLeft: 4,
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
    marginBottom: 16,
  },
  row: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    paddingVertical: 12,
  },
  column: {
    paddingVertical: 12,
  },
  label: {
    fontSize: 16,
    flex: 1,
  },
  slider: {
    marginVertical: 8,
  },
  sliderValue: {
    fontSize: 14,
    textAlign: 'center',
    marginTop: 4,
  },
  pickerContainer: {
    borderRadius: 8,
    borderWidth: 1,
    marginTop: 8,
  },
  picker: { 
    backgroundColor: 'transparent' 
  },
});

export default SettingsScreen;
