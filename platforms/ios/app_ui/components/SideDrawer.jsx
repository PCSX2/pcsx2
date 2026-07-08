import React, { useEffect, useRef } from 'react';
import { View, Text, StyleSheet, Pressable, Animated, Easing, Image, ScrollView } from 'react-native';
import ReactNativeHapticFeedback from 'react-native-haptic-feedback';

const hapticOptions = {
  enableVibrateFallback: true,
  ignoreAndroidSystemSettings: false,
};

export default function SideDrawer({ open, onClose, colors, headerImage, items }) {
  const translate = useRef(new Animated.Value(-300)).current;
  const overlay = useRef(new Animated.Value(0)).current;

  useEffect(() => {
    if (open) {
      Animated.parallel([
        Animated.timing(translate, { 
          toValue: 0, 
          duration: 300, 
          easing: Easing.bezier(0.25, 0.46, 0.45, 0.94), 
          useNativeDriver: true 
        }),
        Animated.timing(overlay, { 
          toValue: 1, 
          duration: 300, 
          easing: Easing.quad, 
          useNativeDriver: true 
        }),
      ]).start();
    } else {
      Animated.parallel([
        Animated.timing(translate, { 
          toValue: -300, 
          duration: 250, 
          easing: Easing.bezier(0.55, 0.06, 0.68, 0.19), 
          useNativeDriver: true 
        }),
        Animated.timing(overlay, { 
          toValue: 0, 
          duration: 250, 
          easing: Easing.quad, 
          useNativeDriver: true 
        }),
      ]).start();
    }
  }, [open, translate, overlay]);

  return (
    <>
      <Pressable onPress={onClose} pointerEvents={open ? 'auto' : 'none'} style={StyleSheet.absoluteFill}>
        <Animated.View style={[styles.scrim, { opacity: overlay }]} />
      </Pressable>

      <Animated.View style={[styles.drawer, { backgroundColor: colors.surface, transform: [{ translateX: translate }] }]}> 
        <View style={[styles.header, { backgroundColor: colors.surfaceContainerHigh }]}> 
          <Image 
            source={require('../../app_icons/icon.png')} 
            style={styles.backgroundIcon} 
            resizeMode="cover"
          />
          {headerImage ? (
            <Image source={headerImage} resizeMode="cover" style={styles.headerImage} />
          ) : null}
          <View style={styles.headerOverlay} />
          <View style={styles.headerTextWrap}>
            <Text style={[styles.headerTitle, { color: colors.onSurface }]}>ARMSX2</Text>
            <Text style={[styles.headerSub, { color: colors.onSurfaceVariant }]}>Game Selector</Text>
          </View>
        </View>
        <ScrollView contentContainerStyle={{ paddingBottom: 24 }}>
          {items.map((it, idx) => (
            it.type === 'section' ? (
              <Text key={idx} style={[styles.section, { color: colors.onSurfaceVariant }]}>{it.label}</Text>
            ) : (
              <Pressable
                key={idx}
                onPress={() => { 
                  ReactNativeHapticFeedback.trigger('impactLight', hapticOptions);
                  it.onPress?.(); 
                  onClose(); 
                }}
                android_ripple={{ color: colors.surfaceContainerHigh }}
                style={({ pressed }) => [styles.item, pressed && { opacity: 0.95 }]}
              >
                <View style={[styles.iconCircle, { backgroundColor: colors.surfaceContainerHigh }]} />
                <Text 
                  style={[styles.itemLabel, { color: colors.onSurface }]} 
                  numberOfLines={1}
                  ellipsizeMode="tail"
                >
                  {it.label}
                </Text>
              </Pressable>
            )
          ))}
        </ScrollView>
      </Animated.View>
    </>
  );
}

const styles = StyleSheet.create({
  scrim: { flex: 1, backgroundColor: '#0008', width: '100%', height: '100%' },
  drawer: { 
    position: 'absolute', 
    top: 0, 
    bottom: 0, 
    left: 0, 
    width: 300, 
    elevation: 8, 
    shadowColor: '#000', 
    shadowOpacity: 0.3, 
    shadowRadius: 12, 
    shadowOffset: { width: 0, height: 4 } 
  },
  header: { height: 180, overflow: 'hidden', position: 'relative' },
  backgroundIcon: { 
    ...StyleSheet.absoluteFillObject,
    width: undefined,
    height: undefined,
    opacity: 1.0 
  },
  headerImage: { ...StyleSheet.absoluteFillObject, width: undefined, height: undefined },
  headerOverlay: { ...StyleSheet.absoluteFillObject, backgroundColor: '#00000020' },
  headerTextWrap: { position: 'absolute', left: 16, right: 16, bottom: 16 },
  headerTitle: { fontSize: 20, fontWeight: '700' },
  headerSub: { marginTop: 2, fontSize: 14 },
  section: { 
    marginTop: 16, 
    marginBottom: 6, 
    paddingHorizontal: 16, 
    fontSize: 12, 
    textTransform: 'uppercase',
    fontWeight: '600',
    letterSpacing: 0.5,
  },
  item: { 
    flexDirection: 'row', 
    alignItems: 'center', 
    paddingHorizontal: 16, 
    paddingVertical: 14,
    minHeight: 48,
  },
  iconCircle: { 
    width: 24, 
    height: 24, 
    borderRadius: 12, 
    marginRight: 16,
    flexShrink: 0,
  },
  itemLabel: { 
    fontSize: 16,
    flex: 1,
    fontWeight: '400',
    lineHeight: 20,
  },
});
