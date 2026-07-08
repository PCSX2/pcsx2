import React, { useMemo, useState, useEffect, useRef } from 'react';
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

export default function SettingsScreen({ colors, navigation }) {
  const slideAnim = useRef(new Animated.Value(300)).current;
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

export default function SettingsScreen({ navigation }) {
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
    ReactNativeHapticFeedback.trigger('impactLight', hapticOptions);
    setter(value);
  };

  // Enhanced slider handler with haptic feedback
  const handleSliderChange = (setter, value) => {
    setter(value);
  };

  const handleSliderComplete = () => {
    ReactNativeHapticFeedback.trigger('impactLight', hapticOptions);
  };

  const cardStyle = useMemo(() => ({ backgroundColor: colors.surfaceContainer, borderColor: colors.outline }), [colors]);
  const headerText = { color: colors.primary, fontWeight: '700', fontSize: 18, marginBottom: 16 };
  const labelText = { color: colors.onSurface, fontSize: 16 };
  const subText = { color: colors.onSurfaceVariant };

  return (
    <SafeAreaView style={[styles.screen, { backgroundColor: colors.background }]}> 
      <Animated.View style={[styles.container, { transform: [{ translateX: slideAnim }] }]}>
        <View style={[styles.headerRow, { backgroundColor: colors.surface, borderBottomColor: colors.outline }]}>
          <Pressable 
            onPress={() => {
              ReactNativeHapticFeedback.trigger('impactLight', hapticOptions);
              navigation?.goBack();
            }}
            android_ripple={{ color: colors.surfaceContainerHigh }}
            style={styles.backButton}
          >
            <Text style={[styles.backIcon, { color: colors.primary }]}>‹</Text>
          </Pressable>
          <Text style={[styles.screenTitle, { color: colors.onSurface }]}>Settings</Text>
        </View>
        
        <ScrollView contentContainerStyle={styles.scroll}>

          {/* General */}
          <View style={[styles.card, cardStyle]}>
            <Text style={headerText}>General</Text>
            <Row>
              <Text style={labelText}>Fullscreen UI</Text>
              <Switch 
                value={fsui} 
                onValueChange={(value) => handleSwitchChange(setFsui, value)}
                trackColor={{ false: colors.outline, true: colors.primary + '80' }}
                thumbColor={fsui ? colors.primary : colors.onSurface}
              />
            </Row>
            <Row>
              <Text style={labelText}>Frame Limiter</Text>
              <Switch 
                value={frameLimiter} 
                onValueChange={(value) => handleSwitchChange(setFrameLimiter, value)}
                trackColor={{ false: colors.outline, true: colors.primary + '80' }}
                thumbColor={frameLimiter ? colors.primary : colors.onSurface}
              />
            </Row>
            <View style={styles.block}>
              <Text style={labelText}>Custom FPS Limit: <Text style={{ fontWeight: '700' }}>{fpsLimit}</Text></Text>
              <Slider 
                value={fpsLimit} 
                minimumValue={30} 
                maximumValue={180} 
                step={5} 
                onValueChange={(value) => handleSliderChange(setFpsLimit, Math.round(value))}
                onSlidingComplete={handleSliderComplete}
                minimumTrackTintColor={colors.primary}
                maximumTrackTintColor={colors.outline}
                thumbStyle={{ backgroundColor: colors.primary }}
                trackStyle={{ height: 4, borderRadius: 2 }}
                style={{ marginTop: 8 }}
              />
            </View>
            <View style={styles.block}>
              <Text style={labelText}>Aspect Ratio</Text>
              <Picker selectedValue={aspect} onValueChange={setAspect} style={[styles.picker, { color: colors.onSurface }]}>
                {['16:9','4:3','Stretch'].map(v => <Picker.Item key={v} label={v} value={v} />)}
              </Picker>
            </View>
            <Row>
              <Text style={labelText}>Fast Boot</Text>
              <Switch 
                value={fastBoot} 
                onValueChange={(value) => handleSwitchChange(setFastBoot, value)}
                trackColor={{ false: colors.outline, true: colors.primary + '80' }}
                thumbColor={fastBoot ? colors.primary : colors.onSurface}
              />
            </Row>
            <View style={styles.block}>
              <Text style={labelText}>Brightness: <Text style={{ fontWeight: '700' }}>{(brightness/100).toFixed(2)}</Text></Text>
              <Slider 
                value={brightness} 
                minimumValue={0} 
                maximumValue={200} 
                step={5} 
                onValueChange={(value) => handleSliderChange(setBrightness, Math.round(value))}
                onSlidingComplete={handleSliderComplete}
                minimumTrackTintColor={colors.primary}
                maximumTrackTintColor={colors.outline}
                thumbStyle={{ backgroundColor: colors.primary }}
                trackStyle={{ height: 4, borderRadius: 2 }}
                style={{ marginTop: 8 }}
              />
            </View>
            <View style={styles.block}>
              <Text style={labelText}>On-screen controls timeout: <Text style={{ fontWeight: '700' }}>{oscNever ? 'Never' : `${oscTimeout}s`}</Text></Text>
              <Slider 
                value={oscTimeout} 
                minimumValue={0} 
                maximumValue={60} 
                step={1} 
                onValueChange={(value) => handleSliderChange(setOscTimeout, Math.round(value))}
                onSlidingComplete={handleSliderComplete}
                minimumTrackTintColor={colors.primary}
                maximumTrackTintColor={colors.outline}
                thumbStyle={{ backgroundColor: colors.primary }}
                trackStyle={{ height: 4, borderRadius: 2 }}
                style={{ marginTop: 8, opacity: oscNever ? 0.5 : 1 }}
                disabled={oscNever}
              />
              <Row style={{ marginTop: 8 }}>
                <Text style={labelText}>Never hide</Text>
                <Switch 
                  value={oscNever} 
                  onValueChange={(value) => handleSwitchChange(setOscNever, value)}
                  trackColor={{ false: colors.outline, true: colors.primary + '80' }}
                  thumbColor={oscNever ? colors.primary : colors.onSurface}
                />
              </Row>
            </View>
          </View>

          {/* Graphics */}
          <View style={[styles.card, cardStyle]}>
            <Text style={headerText}>Graphics</Text>
            <Field label="Renderer">
              <Picker selectedValue={renderer} onValueChange={setRenderer} style={[styles.picker, { color: colors.onSurface }]}>
                {['Vulkan','OpenGL','Software'].map(v => <Picker.Item key={v} label={v} value={v} />)}
              </Picker>
            </Field>
            <Field label={`Upscale: ${upscale}x`}>
              <Slider 
                value={upscale} 
                minimumValue={1} 
                maximumValue={8} 
                step={1} 
                onValueChange={(value) => handleSliderChange(setUpscale, Math.round(value))}
                onSlidingComplete={handleSliderComplete}
                minimumTrackTintColor={colors.primary}
                maximumTrackTintColor={colors.outline}
                thumbStyle={{ backgroundColor: colors.primary }}
                trackStyle={{ height: 4, borderRadius: 2 }}
                style={{ marginTop: 8 }}
              />
            </Field>
            <Field label="Texture Filtering">
              <Picker selectedValue={filtering} onValueChange={setFiltering} style={[styles.picker, { color: colors.onSurface }]}>
                {['Bilinear','Trilinear','Anisotropic'].map(v => <Picker.Item key={v} label={v} value={v} />)}
              </Picker>
            </Field>
            <Field label="Interlace Mode">
              <Picker selectedValue={interlace} onValueChange={setInterlace} style={[styles.picker, { color: colors.onSurface }]}>
                {['Auto','Weave','Bob'].map(v => <Picker.Item key={v} label={v} value={v} />)}
              </Picker>
            </Field>
            <Row>
              <Text style={labelText}>FXAA</Text>
              <Switch 
                value={fxaa} 
                onValueChange={(value) => handleSwitchChange(setFxaa, value)}
                trackColor={{ false: colors.outline, true: colors.primary + '80' }}
                thumbColor={fxaa ? colors.primary : colors.onSurface}
              />
            </Row>
            <Field label="CAS Mode">
              <Picker selectedValue={casMode} onValueChange={setCasMode} style={[styles.picker, { color: colors.onSurface }]}>
                {['Off','CAS','FSR2'].map(v => <Picker.Item key={v} label={v} value={v} />)}
              </Picker>
            </Field>
            <Field label={`CAS Sharpness: ${casSharpness}%`}>
              <Slider 
                value={casSharpness} 
                minimumValue={0} 
                maximumValue={100} 
                step={5} 
                onValueChange={(value) => handleSliderChange(setCasSharpness, Math.round(value))}
                onSlidingComplete={handleSliderComplete}
                minimumTrackTintColor={colors.primary}
                maximumTrackTintColor={colors.outline}
                thumbStyle={{ backgroundColor: colors.primary }}
                trackStyle={{ height: 4, borderRadius: 2 }}
                style={{ marginTop: 8 }}
              />
            </Field>
            <Row>
              <Text style={labelText}>Hardware Mipmapping</Text>
              <Switch 
                value={hwMipmap} 
                onValueChange={(value) => handleSwitchChange(setHwMipmap, value)}
                trackColor={{ false: colors.outline, true: colors.primary + '80' }}
                thumbColor={hwMipmap ? colors.primary : colors.onSurface}
              />
            </Row>
            <Row>
              <Text style={labelText}>VSync</Text>
              <Switch 
                value={vsync} 
                onValueChange={(value) => handleSwitchChange(setVsync, value)}
                trackColor={{ false: colors.outline, true: colors.primary + '80' }}
                thumbColor={vsync ? colors.primary : colors.onSurface}
              />
            </Row>
            <Row>
              <Text style={labelText}>Auto Flush (Software)</Text>
              <Switch 
                value={autoFlushSw} 
                onValueChange={(value) => handleSwitchChange(setAutoFlushSw, value)}
                trackColor={{ false: colors.outline, true: colors.primary + '80' }}
                thumbColor={autoFlushSw ? colors.primary : colors.onSurface}
              />
            </Row>
            <Field label="Auto Flush (Hardware)">
              <Picker selectedValue={autoFlushHw} onValueChange={setAutoFlushHw} style={[styles.picker, { color: colors.onSurface }]}>
                {['Off','Partial','Full'].map(v => <Picker.Item key={v} label={v} value={v} />)}
              </Picker>
            </Field>
          </View>

          {/* Controller */}
          <View style={[styles.card, cardStyle]}>
            <Text style={headerText}>Controller</Text>
            <Pressable 
              style={[styles.button, { borderColor: colors.outline }]}
              android_ripple={{ color: colors.surfaceContainerHigh }}
              onPress={() => ReactNativeHapticFeedback.trigger('impactMedium', hapticOptions)}
            > 
              <Text style={{ color: colors.onSurface }}>Calibrate Controller</Text>
            </Pressable>
            <Row>
              <Text style={labelText}>Vibration</Text>
              <Switch 
                value={vibration} 
                onValueChange={(value) => handleSwitchChange(setVibration, value)}
                trackColor={{ false: colors.outline, true: colors.primary + '80' }}
                thumbColor={vibration ? colors.primary : colors.onSurface}
              />
            </Row>
          </View>

          <View style={[styles.card, cardStyle]}>
            <Text style={headerText}>Performance</Text>
            <Row>
              <Text style={labelText}>CPU Core</Text>
              <Picker selectedValue={cpuCore} onValueChange={setCpuCore} style={[styles.pickerCompact, { color: colors.onSurface }]}>
                {['Dynarec','Interpreter'].map(v => <Picker.Item key={v} label={v} value={v} />)}
              </Picker>
            </Row>
          </View>

          <View style={{ height: 32 }} />
        </ScrollView>
      </Animated.View>
    </SafeAreaView>
  );
}

function Row({ children, style }) {
  return <View style={[styles.row, style]}>{children}</View>;
}

function Field({ label, children }) {
  return (
    <View style={styles.block}>
      <Text style={[styles.fieldLabel, { color: '#fff' }]}>{label}</Text>
      {children}
    </View>
  );
}

const styles = StyleSheet.create({
  screen: { flex: 1 },
  container: { flex: 1 },
  headerRow: { 
    flexDirection: 'row', 
    alignItems: 'center', 
    paddingHorizontal: 4, 
    paddingVertical: 8,
    borderBottomWidth: StyleSheet.hairlineWidth,
    minHeight: 56,
  },
  backButton: { 
    width: 48, 
    height: 48, 
    alignItems: 'center', 
    justifyContent: 'center',
    borderRadius: 24,
  },
  backIcon: { 
    fontSize: 32, 
    fontWeight: '300',
    marginLeft: -2, 
  },
  screenTitle: { 
    flex: 1, 
    fontSize: 20, 
    fontWeight: '600', 
    marginLeft: 8,
  },
  scroll: { padding: 16 },
  card: { borderRadius: 12, padding: 16, marginBottom: 16, borderWidth: StyleSheet.hairlineWidth },
  row: { flexDirection: 'row', alignItems: 'center', justifyContent: 'space-between', marginBottom: 16 },
  block: { marginBottom: 16 },
  fieldLabel: { marginBottom: 8, fontWeight: '600' },
  picker: { backgroundColor: 'transparent' },
  pickerCompact: { width: 160 },
  button: { 
    paddingVertical: 12, 
    paddingHorizontal: 16, 
    borderWidth: StyleSheet.hairlineWidth, 
    borderRadius: 8, 
    alignSelf: 'flex-start', 
    marginBottom: 12 
  },
});
