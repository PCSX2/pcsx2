import React, { useMemo, useState } from 'react';
import { SafeAreaView, ScrollView, StyleSheet, View } from 'react-native';
import { Appbar, Card, Divider, List, SegmentedButtons, Switch, Text } from 'react-native-paper';
import ReactNativeHapticFeedback from 'react-native-haptic-feedback';
import SwipeableSlider from '../components/SwipeableSlider.jsx';
import { useTheme } from '../theme.jsx';

const hapticOptions = {
  enableVibrateFallback: true,
  ignoreAndroidSystemSettings: false,
};

function SectionCard({ title, description, children }) {
  return (
    <Card mode="contained" style={styles.card}>
      <Card.Title title={title} subtitle={description} />
      <Card.Content>{children}</Card.Content>
    </Card>
  );
}

function SettingsScreen() {
  const { colors } = useTheme();

  const [frameLimiter, setFrameLimiter] = useState(true);
  const [fastBoot, setFastBoot] = useState(false);
  const [fpsLimit, setFpsLimit] = useState(60);
  const [brightness, setBrightness] = useState(100);
  const [aspect, setAspect] = useState('16:9');

  const [renderer, setRenderer] = useState('Vulkan');
  const [upscale, setUpscale] = useState(1);
  const [fxaa, setFxaa] = useState(false);
  const [casSharpness, setCasSharpness] = useState(50);
  const [vsync, setVsync] = useState(false);
  const [hwMipmap, setHwMipmap] = useState(false);

  const [vibration, setVibration] = useState(true);
  const [cpuCore, setCpuCore] = useState('Dynarec');

  const handleSwitchChange = (setter, value) => {
    ReactNativeHapticFeedback.trigger('impactMedium', hapticOptions);
    setter(value);
  };

  const handleSliderComplete = () => {
    ReactNativeHapticFeedback.trigger('impactLight', hapticOptions);
  };

  const surfaceStyle = useMemo(
    () => ({
      backgroundColor: colors.surfaceContainer,
      borderColor: colors.outline,
    }),
    [colors]
  );

  return (
    <SafeAreaView style={[styles.container, { backgroundColor: colors.background }]}>
      <Appbar.Header mode="small" elevated style={{ backgroundColor: colors.surface }}>
        <Appbar.Content title="Settings" subtitle="Paper-styled controls" />
      </Appbar.Header>

      <ScrollView contentContainerStyle={styles.content}>
        <SectionCard title="General" description="Boot and overlay">
          <List.Item
            title="Frame limiter"
            description="Keep gameplay smooth"
            right={() => (
              <Switch
                value={frameLimiter}
                onValueChange={(value) => handleSwitchChange(setFrameLimiter, value)}
                color={colors.primary}
              />
            )}
          />
          <Divider />
          <List.Item
            title="Fast boot"
            description="Skip BIOS logo"
            right={() => (
              <Switch
                value={fastBoot}
                onValueChange={(value) => handleSwitchChange(setFastBoot, value)}
                color={colors.primary}
              />
            )}
          />
          <Divider />
          <View style={styles.sliderBlock}>
            <Text variant="titleSmall" style={{ color: colors.onSurface }}>
              FPS limit
            </Text>
            <SwipeableSlider
              value={fpsLimit}
              minimumValue={30}
              maximumValue={120}
              step={5}
              onValueChange={setFpsLimit}
              onSlidingComplete={handleSliderComplete}
              colors={colors}
            />
            <Text variant="bodySmall" style={{ color: colors.onSurfaceVariant }}>
              {fpsLimit} fps
            </Text>
          </View>
          <Divider />
          <View style={styles.segmentRow}>
            <Text variant="titleSmall" style={{ color: colors.onSurface, marginBottom: 8 }}>
              Aspect ratio
            </Text>
            <SegmentedButtons
              value={aspect}
              onValueChange={setAspect}
              buttons={[
                { value: '16:9', label: '16:9' },
                { value: '4:3', label: '4:3' },
                { value: 'stretch', label: 'Stretch' },
              ]}
              style={styles.segmented}
            />
          </View>
          <Divider />
          <View style={styles.sliderBlock}>
            <Text variant="titleSmall" style={{ color: colors.onSurface }}>
              Overlay brightness
            </Text>
            <SwipeableSlider
              value={brightness}
              minimumValue={25}
              maximumValue={125}
              step={5}
              onValueChange={setBrightness}
              onSlidingComplete={handleSliderComplete}
              colors={colors}
            />
            <Text variant="bodySmall" style={{ color: colors.onSurfaceVariant }}>
              {brightness}%
            </Text>
          </View>
        </SectionCard>

        <SectionCard title="Graphics" description="Rendering defaults">
          <View style={styles.segmentRow}>
            <Text variant="titleSmall" style={{ color: colors.onSurface, marginBottom: 8 }}>
              Renderer
            </Text>
            <SegmentedButtons
              value={renderer}
              onValueChange={setRenderer}
              buttons={[
                { value: 'Vulkan', label: 'Vulkan' },
                { value: 'OpenGL', label: 'OpenGL' },
                { value: 'Software', label: 'Software' },
              ]}
              style={styles.segmented}
            />
          </View>
          <Divider />
          <View style={styles.sliderBlock}>
            <Text variant="titleSmall" style={{ color: colors.onSurface }}>
              Upscaling
            </Text>
            <SwipeableSlider
              value={upscale}
              minimumValue={1}
              maximumValue={6}
              step={1}
              onValueChange={setUpscale}
              onSlidingComplete={handleSliderComplete}
              colors={colors}
            />
            <Text variant="bodySmall" style={{ color: colors.onSurfaceVariant }}>
              {upscale}x internal resolution
            </Text>
          </View>
          <Divider />
          <List.Item
            title="FXAA"
            description="Smooth jagged edges"
            right={() => (
              <Switch
                value={fxaa}
                onValueChange={(value) => handleSwitchChange(setFxaa, value)}
                color={colors.primary}
              />
            )}
          />
          <Divider />
          <View style={styles.sliderBlock}>
            <Text variant="titleSmall" style={{ color: colors.onSurface }}>
              CAS sharpening
            </Text>
            <SwipeableSlider
              value={casSharpness}
              minimumValue={0}
              maximumValue={100}
              step={5}
              onValueChange={setCasSharpness}
              onSlidingComplete={handleSliderComplete}
              colors={colors}
            />
            <Text variant="bodySmall" style={{ color: colors.onSurfaceVariant }}>
              {casSharpness}%
            </Text>
          </View>
          <Divider />
          <List.Item
            title="Hardware mipmap"
            description="Reduce shimmering on distant textures"
            right={() => (
              <Switch
                value={hwMipmap}
                onValueChange={(value) => handleSwitchChange(setHwMipmap, value)}
                color={colors.primary}
              />
            )}
          />
          <Divider />
          <List.Item
            title="VSync"
            description="Sync frames to display refresh"
            right={() => (
              <Switch
                value={vsync}
                onValueChange={(value) => handleSwitchChange(setVsync, value)}
                color={colors.primary}
              />
            )}
          />
        </SectionCard>

        <SectionCard title="Controller" description="Input feedback">
          <List.Item
            title="Vibration"
            description="Haptics on supported controllers"
            right={() => (
              <Switch
                value={vibration}
                onValueChange={(value) => handleSwitchChange(setVibration, value)}
                color={colors.primary}
              />
            )}
          />
        </SectionCard>

        <SectionCard title="Performance" description="Runtime profile">
          <View style={styles.segmentRow}>
            <Text variant="titleSmall" style={{ color: colors.onSurface, marginBottom: 8 }}>
              CPU core
            </Text>
            <SegmentedButtons
              value={cpuCore}
              onValueChange={setCpuCore}
              buttons={[
                { value: 'Dynarec', label: 'Dynarec' },
                { value: 'Interpreter', label: 'Interpreter' },
                { value: 'Cached', label: 'Cached' },
              ]}
              style={styles.segmented}
            />
          </View>
          <Divider />
          <List.Item
            title="Diagnostics overlay"
            description="Show perf HUD when needed"
            right={() => <Switch value={false} onValueChange={() => {}} disabled />}
          />
        </SectionCard>

        <View style={[styles.surfaceHint, surfaceStyle]}>
          <Text variant="labelLarge" style={{ color: colors.onSurface }}>
            Heads up
          </Text>
          <Text variant="bodySmall" style={{ color: colors.onSurfaceVariant, marginTop: 6 }}>
            Buttons are intentionally unwired. The native module will own the actual emulator settings and bridge them
            into React Native in the next step.
          </Text>
        </View>
      </ScrollView>
    </SafeAreaView>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1 },
  content: { padding: 16, paddingBottom: 48 },
  card: {
    marginBottom: 16,
    borderRadius: 16,
  },
  sliderBlock: { paddingVertical: 12 },
  segmentRow: { paddingVertical: 12 },
  segmented: { marginTop: 4 },
  surfaceHint: {
    borderWidth: 1,
    borderRadius: 12,
    padding: 16,
    marginBottom: 24,
  },
});

export default SettingsScreen;
