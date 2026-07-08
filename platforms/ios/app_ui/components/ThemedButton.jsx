import React from 'react';
import { Pressable, Text, StyleSheet, View } from 'react-native';
import ReactNativeHapticFeedback from 'react-native-haptic-feedback';

const hapticOptions = {
  enableVibrateFallback: true,
  ignoreAndroidSystemSettings: false,
};

export default function ThemedButton({ title, onPress, colors, variant = 'filled', style, textStyle }) {
  const styles = getStyles(colors, variant);
  const rippleColor = variant === 'filled' ? colors.onPrimary : (colors.onSurfaceVariant ?? colors.onSurface);
  
  const handlePress = () => {
    ReactNativeHapticFeedback.trigger('impactLight', hapticOptions);
    onPress?.();
  };
  
  return (
    <Pressable 
      onPress={handlePress} 
      android_ripple={{ color: rippleColor }} 
      style={({ pressed }) => [styles.button, pressed && styles.pressed, style] }
    >
      <Text style={[styles.text, textStyle]}>{title}</Text>
    </Pressable>
  );
}

function getStyles(colors, variant) {
  const isFilled = variant === 'filled';
  const isTonal = variant === 'tonal';
  const baseBg = isFilled ? colors.primary : isTonal ? colors.surfaceContainerHigh ?? colors.surfaceContainer : 'transparent';
  const baseFg = isFilled ? colors.onPrimary : colors.onSurface;
  const borderColor = variant === 'outlined' ? (colors.outline ?? colors.onSurfaceVariant) : 'transparent';
  
  return StyleSheet.create({
    button: {
      backgroundColor: baseBg,
      paddingHorizontal: 16,
      paddingVertical: 12,
      borderRadius: 8,
      borderWidth: variant === 'outlined' ? 1 : 0,
      borderColor,
      alignItems: 'center',
      justifyContent: 'center',
      minHeight: 40,
    },
    text: {
      color: baseFg,
      fontWeight: '600',
      fontSize: 14,
    },
    pressed: {
      opacity: 0.85,
    },
  });
}
