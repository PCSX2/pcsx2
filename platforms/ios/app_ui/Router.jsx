import React, { useEffect, useMemo, useState } from 'react';
import { BackHandler, SafeAreaView, StatusBar, StyleSheet } from 'react-native';
import { BottomNavigation } from 'react-native-paper';
import GameSelector from './screens/GameSelector.jsx';
import SettingsScreen from './screens/SettingsScreen.jsx';
import { useTheme } from './theme.jsx';

export default function Router() {
  const { scheme, colors } = useTheme();
  const [index, setIndex] = useState(0);

  const routes = useMemo(
    () => [
      { key: 'games', title: 'Games', focusedIcon: 'controller-classic', unfocusedIcon: 'controller-classic-outline' },
      { key: 'settings', title: 'Settings', focusedIcon: 'cog', unfocusedIcon: 'cog-outline' },
    ],
    []
  );

  useEffect(() => {
    const backHandler = BackHandler.addEventListener('hardwareBackPress', () => {
      if (index !== 0) {
        setIndex(0);
        return true;
      }
      return false;
    });
    return () => backHandler.remove();
  }, [index]);

  const renderScene = ({ route }) => {
    switch (route.key) {
      case 'settings':
        return <SettingsScreen />;
      case 'games':
      default:
        return <GameSelector />;
    }
  };

  return (
    <SafeAreaView style={[styles.container, { backgroundColor: colors.background }]}>
      <StatusBar
        barStyle={scheme === 'dark' ? 'light-content' : 'dark-content'}
        backgroundColor={colors.background}
      />
      <BottomNavigation
        navigationState={{ index, routes }}
        onIndexChange={setIndex}
        renderScene={renderScene}
        sceneAnimationEnabled
        barStyle={{ backgroundColor: colors.surface }}
        activeColor={colors.primary}
        inactiveColor={colors.onSurfaceVariant}
      />
    </SafeAreaView>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1 },
});
