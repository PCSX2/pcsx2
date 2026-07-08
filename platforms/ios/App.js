import React from 'react';
import Router from './app_ui/Router.jsx';
import { ThemeProvider } from './app_ui/theme.jsx';

export default function App() {
  return (
    <ThemeProvider>
      <Router />
    </ThemeProvider>
  );
}
