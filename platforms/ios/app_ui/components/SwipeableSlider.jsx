import React, { useState, useRef } from 'react';
import { View, Text, Animated, StyleSheet, PanResponder } from 'react-native';
import ReactNativeHapticFeedback from 'react-native-haptic-feedback';

const hapticOptions = {
  enableVibrateFallback: true,
  ignoreAndroidSystemSettings: false,
};

export default function SwipeableSlider({ 
  value, 
  minimumValue = 0, 
  maximumValue = 100, 
  step = 1, 
  onValueChange, 
  onSlidingComplete,
  colors,
  style 
}) {
  const [sliderWidth, setSliderWidth] = useState(200);
  const lastHapticValue = useRef(value);
  
  const normalizedValue = Math.max(minimumValue, Math.min(maximumValue, value));
  const percentage = (normalizedValue - minimumValue) / (maximumValue - minimumValue);
  
  const panResponder = PanResponder.create({
    onStartShouldSetPanResponder: () => true,
    onMoveShouldSetPanResponder: () => true,
    
    onPanResponderMove: (event, gestureState) => {
      const { dx, x0, moveX } = gestureState;
      const relativePosition = (moveX - x0) / sliderWidth;
      const newPercentage = Math.max(0, Math.min(1, relativePosition));
      const newValue = Math.round((newPercentage * (maximumValue - minimumValue) + minimumValue) / step) * step;
      
      if (newValue !== value) {
        onValueChange?.(newValue);
        
        // Haptic feedback every few values
        if (Math.abs(newValue - lastHapticValue.current) >= step * 3) {
          ReactNativeHapticFeedback.trigger('impactLight', hapticOptions);
          lastHapticValue.current = newValue;
        }
      }
    },
    
    onPanResponderRelease: () => {
      onSlidingComplete?.();
      ReactNativeHapticFeedback.trigger('impactLight', hapticOptions);
    },
  });

  return (
    <View 
      style={[styles.container, style]}
      onLayout={(e) => setSliderWidth(e.nativeEvent.layout.width)}
    >
      <View style={[styles.track, { backgroundColor: colors.outline }]}>
        <View 
          style={[
            styles.activeTrack, 
            { 
              backgroundColor: colors.primary,
              width: `${percentage * 100}%`
            }
          ]} 
        />
        <View 
          {...panResponder.panHandlers}
          style={[
            styles.thumb, 
            { 
              backgroundColor: colors.primary,
              left: `${percentage * 100}%`,
            }
          ]}
        />
      </View>
    </View>
  );
}

const styles = StyleSheet.create({
  container: {
    height: 40,
    justifyContent: 'center',
    marginVertical: 8,
  },
  track: {
    height: 4,
    borderRadius: 2,
    position: 'relative',
  },
  activeTrack: {
    height: 4,
    borderRadius: 2,
  },
  thumb: {
    width: 20,
    height: 20,
    borderRadius: 10,
    position: 'absolute',
    top: -8,
    marginLeft: -10,
    elevation: 2,
    shadowColor: '#000',
    shadowOpacity: 0.2,
    shadowRadius: 2,
    shadowOffset: { width: 0, height: 1 },
  },
});
