import React, { useCallback, useEffect, useMemo, useRef, useState } from 'react';
import { BackHandler, FlatList, SafeAreaView, StyleSheet, View } from 'react-native';
import { Appbar, Button, Card, FAB, Portal, Searchbar, Text } from 'react-native-paper';
import { BottomSheetModal, BottomSheetModalProvider } from '@gorhom/bottom-sheet';
import { GestureHandlerRootView } from 'react-native-gesture-handler';
import ReactNativeHapticFeedback from 'react-native-haptic-feedback';
import SideDrawer from '../components/SideDrawer.jsx';
import ThemedButton from '../components/ThemedButton.jsx';
import { useTheme } from '../theme.jsx';

const hapticOptions = {
  enableVibrateFallback: true,
  ignoreAndroidSystemSettings: false,
};

function GameItem({ item, colors, onPress }) {
  const cardStyle = useMemo(
    () => ({
      backgroundColor: colors.surface,
      borderRadius: 16,
    }),
    [colors]
  );

  return (
    <Card
      mode="elevated"
      style={cardStyle}
      onPress={() => {
        ReactNativeHapticFeedback.trigger('impactLight', hapticOptions);
        onPress?.(item);
      }}
    >
      <Card.Content style={styles.cardContent}>
        <View style={[styles.cover, { backgroundColor: colors.surfaceContainerHigh }]} />
        <View style={styles.cardText}>
          <Text variant="titleMedium" style={[styles.itemTitle, { color: colors.onSurface }]}>
            {item.title}
          </Text>
          {item.subtitle ? (
            <Text variant="bodySmall" style={{ color: colors.onSurfaceVariant }}>
              {item.subtitle}
            </Text>
          ) : null}
        </View>
      </Card.Content>
    </Card>
  );
}

export default function GameSelector() {
  const { colors } = useTheme();
  const [drawerOpen, setDrawerOpen] = useState(false);
  const [games] = useState([]);
  const [searchVisible, setSearchVisible] = useState(false);
  const [searchText, setSearchText] = useState('');
  const [layout, setLayout] = useState('grid');
  const [fabOpen, setFabOpen] = useState(false);
  const [coverUrl, setCoverUrl] = useState('');

  const bottomSheetRef = useRef(null);
  const snapPoints = useMemo(() => ['40%'], []);

  const drawerItems = useMemo(
    () => [
      { type: 'section', label: 'Emulation' },
      { label: 'Boot BIOS', onPress: () => {} },
      { label: 'BIOS', onPress: () => {} },
      { type: 'section', label: 'Library' },
      { label: 'Choose games folder', onPress: () => {} },
      { label: 'Refresh games', onPress: () => {} },
      { label: 'Covers', onPress: () => {} },
      { label: 'Remove cover URL', onPress: () => {} },
      { type: 'section', label: 'Background' },
      { label: 'Choose landscape background', onPress: () => {} },
      { label: 'Choose portrait background', onPress: () => {} },
      { label: 'Clear background', onPress: () => {} },
    ],
    []
  );

  const filteredGames = useMemo(() => {
    const query = searchText.trim().toLowerCase();
    if (!query) return games;
    return games.filter((g) => (g.title ?? '').toLowerCase().includes(query));
  }, [games, searchText]);

  const handleToggleLayout = useCallback(() => {
    ReactNativeHapticFeedback.trigger('impactLight', hapticOptions);
    setLayout((prev) => (prev === 'grid' ? 'list' : 'grid'));
  }, []);

  const handleOpenCoverSheet = useCallback(() => {
    ReactNativeHapticFeedback.trigger('impactMedium', hapticOptions);
    bottomSheetRef.current?.present();
    setFabOpen(false);
  }, []);

  const handleSaveCoverUrl = useCallback(() => {
    ReactNativeHapticFeedback.trigger('notificationSuccess', hapticOptions);
    // Wire up to native module in next step.
    bottomSheetRef.current?.dismiss();
  }, []);

  return (
    <GestureHandlerRootView style={{ flex: 1 }}>
      <BottomSheetModalProvider>
        <SafeAreaView style={[styles.container, { backgroundColor: colors.background }]}>
          <AndroidBackCloser open={drawerOpen} onClose={() => setDrawerOpen(false)} />

          <Appbar.Header mode="small" elevated style={{ backgroundColor: colors.surface }}>
            <Appbar.Action
              icon="menu"
              onPress={() => {
                ReactNativeHapticFeedback.trigger('impactLight', hapticOptions);
                setDrawerOpen(true);
              }}
            />
            <Appbar.Content title="Games" subtitle={layout === 'grid' ? 'Grid view' : 'List view'} />
            <Appbar.Action
              icon={layout === 'grid' ? 'view-list' : 'view-grid-outline'}
              onPress={handleToggleLayout}
            />
            <Appbar.Action
              icon={searchVisible ? 'close' : 'magnify'}
              onPress={() => {
                ReactNativeHapticFeedback.trigger('impactLight', hapticOptions);
                setSearchVisible((v) => !v);
                if (searchVisible) setSearchText('');
              }}
            />
          </Appbar.Header>

          {searchVisible && (
            <View style={styles.searchWrap}>
              <Searchbar
                placeholder="Search games"
                value={searchText}
                onChangeText={setSearchText}
                onIconPress={() => setSearchText('')}
                style={{ backgroundColor: colors.surfaceContainer }}
                inputStyle={{ color: colors.onSurface }}
                iconColor={colors.onSurfaceVariant}
                placeholderTextColor={colors.onSurfaceVariant}
              />
            </View>
          )}

          {filteredGames.length === 0 ? (
            <View style={styles.emptyContainer}>
              <Text variant="titleMedium" style={[styles.emptyText, { color: colors.onSurface }]}>
                No games yet
              </Text>
              <Text variant="bodyMedium" style={{ color: colors.onSurfaceVariant, marginBottom: 16 }}>
                Point ARMSX2 to your library to see everything here.
              </Text>
              <ThemedButton title="Choose games folder" onPress={() => {}} colors={colors} variant="outlined" />
            </View>
          ) : (
            <FlatList
              contentContainerStyle={{ padding: 16 }}
              data={filteredGames}
              keyExtractor={(g, i) => g.id ?? String(i)}
              renderItem={({ item }) => <GameItem item={item} colors={colors} onPress={() => {}} />}
              numColumns={layout === 'grid' ? 2 : 1}
              columnWrapperStyle={layout === 'grid' ? styles.gridWrapper : undefined}
              ItemSeparatorComponent={() => <View style={{ height: layout === 'list' ? 12 : 0 }} />}
            />
          )}

          <Portal>
            <FAB.Group
              open={fabOpen}
              visible
              icon={fabOpen ? 'close' : 'plus'}
              onStateChange={({ open }) => setFabOpen(open)}
              fabStyle={{ backgroundColor: colors.primary }}
              color={colors.onPrimary}
              actions={[
                { icon: 'folder', label: 'Choose folder', onPress: () => {}, small: false },
                { icon: 'refresh', label: 'Refresh games', onPress: () => {}, small: false },
                { icon: 'image-plus', label: 'Add cover URL', onPress: handleOpenCoverSheet, small: false },
              ]}
            />
          </Portal>

          <SideDrawer open={drawerOpen} onClose={() => setDrawerOpen(false)} colors={colors} items={drawerItems} />

          <BottomSheetModal
            ref={bottomSheetRef}
            snapPoints={snapPoints}
            backgroundStyle={{ backgroundColor: colors.surface }}
            handleIndicatorStyle={{ backgroundColor: colors.onSurfaceVariant }}
          >
            <View style={styles.sheetContent}>
              <Text variant="titleMedium" style={{ color: colors.onSurface, marginBottom: 12 }}>
                Add cover image URL
              </Text>
              <Text variant="bodySmall" style={{ color: colors.onSurfaceVariant, marginBottom: 12 }}>
                Paste a cover URL and we will apply it to the selected game. Native wiring comes next.
              </Text>
              <Button
                mode="outlined"
                style={{ marginBottom: 12 }}
                textColor={colors.onSurface}
                onPress={() => {
                  ReactNativeHapticFeedback.trigger('impactLight', hapticOptions);
                }}
              >
                Paste from clipboard
              </Button>
              <Searchbar
                placeholder="https://example.com/cover.jpg"
                value={coverUrl}
                onChangeText={setCoverUrl}
                style={{ backgroundColor: colors.surfaceContainer }}
                inputStyle={{ color: colors.onSurface }}
                iconColor={colors.onSurfaceVariant}
                placeholderTextColor={colors.onSurfaceVariant}
              />
              <Button
                mode="contained"
                style={{ marginTop: 16 }}
                onPress={handleSaveCoverUrl}
                buttonColor={colors.primary}
                textColor={colors.onPrimary}
              >
                Save cover URL
              </Button>
            </View>
          </BottomSheetModal>
        </SafeAreaView>
      </BottomSheetModalProvider>
    </GestureHandlerRootView>
  );
}

function AndroidBackCloser({ open, onClose }) {
  useEffect(() => {
    if (!open) return;
    const sub = BackHandler.addEventListener('hardwareBackPress', () => {
      onClose();
      return true;
    });
    return () => sub.remove();
  }, [open, onClose]);
  return null;
}

const styles = StyleSheet.create({
  container: { flex: 1 },
  searchWrap: { paddingHorizontal: 16, paddingTop: 12, paddingBottom: 4 },
  emptyContainer: { flex: 1, alignItems: 'center', justifyContent: 'center', paddingHorizontal: 24 },
  emptyText: { marginBottom: 4 },
  cardContent: { flexDirection: 'row', alignItems: 'center', gap: 12, paddingVertical: 8 },
  cover: { width: 72, aspectRatio: 2 / 3, borderRadius: 10, marginRight: 4 },
  cardText: { flex: 1 },
  itemTitle: { marginBottom: 2 },
  gridWrapper: { gap: 12 },
  sheetContent: { paddingHorizontal: 20, paddingVertical: 12, gap: 8 },
});
