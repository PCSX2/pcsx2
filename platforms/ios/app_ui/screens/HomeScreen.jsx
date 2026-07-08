import React from 'react';
import { SafeAreaView, View, Text, StyleSheet } from 'react-native';
import ThemedButton from '../components/ThemedButton.jsx';

export default function HomeScreen({ navigation, colors }) {
  return (
    <SafeAreaView style={[styles.container, { backgroundColor: colors.background }] }>
      <View style={[styles.card, { backgroundColor: colors.surfaceContainer }] }>
        <Text style={[styles.title, { color: colors.onSurface }]}>ARMSX2</Text>
        <Text style={[styles.subtitle, { color: colors.onSurfaceVariant }]}>React Native powered UI</Text>
        <Text style={[styles.body, { color: colors.onSurfaceVariant }]}>Home screen</Text>
  <View style={{ height: 12 }} />
        <View style={{ height: 8, borderRadius: 4, backgroundColor: colors.primary }} />
        <View style={{ height: 16 }} />
  <ThemedButton title="Go to Settings" onPress={() => navigation.navigate('Settings')} colors={colors} />
      </View>
    </SafeAreaView>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, justifyContent: 'center', alignItems: 'center' },
  card: { padding: 24, borderRadius: 12, width: '86%' },
  title: { fontSize: 22, fontWeight: '700', marginBottom: 8 },
  subtitle: { marginBottom: 12 },
  body: {},
});
