// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once
#include "common/WindowInfo.h"
#include <QtGui/QWindow>
#include <optional>
#include <vector>

class QCloseEvent;

class DisplaySurface final : public QWindow
{
	Q_OBJECT

public:
	explicit DisplaySurface();
	~DisplaySurface();

	// while QWindow can be used directly as a window, Popups requre a QWidget parent.
	// Additionally, we use saveGeometry/restoreGeometry for render to seperate window mode
	// but those functions only exist on QWidget.
	// Thus, we always need a container widget.
	QWidget* createWindowContainer(QWidget* parent = nullptr);

	int scaledWindowWidth() const;
	int scaledWindowHeight() const;

	std::optional<WindowInfo> getWindowInfo();

	void updateRelativeMode(bool enabled);
	void updateCursor(bool hidden);

Q_SIGNALS:
	void windowResizedEvent(int width, int height, float scale);
	void windowRestoredEvent();

protected:
	void handleCloseEvent(QCloseEvent* event);
	void handleKeyInputEvent(QEvent* event);
	bool event(QEvent* event) override;
	bool eventFilter(QObject* object, QEvent* event) override;

private:
	bool isActuallyFullscreen() const;
	void updateCenterPos();

	QPoint m_relative_mouse_start_pos{};
	QPoint m_relative_mouse_center_pos{};
	bool m_relative_mouse_enabled = false;
#ifdef _WIN32
	bool m_clip_mouse_enabled = false;
#endif
	bool m_cursor_hidden = false;

	std::vector<int> m_keys_pressed_with_modifiers;

	u32 m_last_window_width = 0;
	u32 m_last_window_height = 0;
	float m_last_window_scale = 1.0f;

	QWidget* m_container = nullptr;
};
