import React, { useMemo } from 'react';
import { useColorScheme } from 'react-native';
import { MD3DarkTheme, MD3LightTheme, PaperProvider, useTheme as usePaperTheme } from 'react-native-paper';

const extendWithSurface = (baseTheme) => {
  const level2 = baseTheme.colors.elevation?.level2 ?? baseTheme.colors.surface;
  const level3 = baseTheme.colors.elevation?.level3 ?? level2;

  return {
    ...baseTheme,
    colors: {
      ...baseTheme.colors,
      surfaceContainer: level2,
      surfaceContainerHigh: level3,
      card: baseTheme.colors.surfaceVariant ?? level2,
      textPrimary: baseTheme.colors.onSurface,
      textSecondary: baseTheme.colors.onSurfaceVariant,
      border: baseTheme.colors.outline,
      tint: baseTheme.colors.primary,
    },
  };
};

export function ThemeProvider({ children }) {
  const scheme = useColorScheme() ?? 'light';

  const paperTheme = useMemo(() => {
    const base = scheme === 'dark' ? MD3DarkTheme : MD3LightTheme;
    return extendWithSurface(base);
  }, [scheme]);

  return (
    <PaperProvider theme={paperTheme}>
      {children}
    </PaperProvider>
  );
}

/**
 * @returns {{ scheme: 'light'|'dark', colors: Record<string, string|undefined> }}
 */
export function useTheme() {
  const paperTheme = usePaperTheme();
  const scheme = paperTheme.dark ? 'dark' : 'light';
  return { scheme, colors: paperTheme.colors };
}
