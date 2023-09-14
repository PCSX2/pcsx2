/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2022  PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once
#include "common/WindowInfo.h"
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QWidget>
#include <optional>
#include <vector>

class QCloseEvent;

class DisplayWidget final : public QWidget
{
	Q_OBJECT

public:
	explicit DisplayWidget(QWidget* parent);
	~DisplayWidget();

	QPaintEngine* paintEngine() const override;

	int scaledWindowWidth() const;
	int scaledWindowHeight() const;

	std::optional<WindowInfo> getWindowInfo();

	void updateRelativeMode(bool enabled);
	void updateCursor(bool hidden);

	void handleCloseEvent(QCloseEvent* event);
	void destroy();

Q_SIGNALS:
	void windowResizedEvent(int width, int height, float scale);
	void windowRestoredEvent();

protected:
	bool event(QEvent* event) override;

private:
	void updateCenterPos();

	QPoint m_relative_mouse_start_pos{};
	QPoint m_relative_mouse_center_pos{};
	bool m_relative_mouse_enabled = false;
#ifdef _WIN32
	bool m_clip_mouse_enabled = false;
#endif
	bool m_cursor_hidden = false;
	bool m_destroying = false;

	std::vector<int> m_keys_pressed_with_modifiers;

	u32 m_last_window_width = 0;
	u32 m_last_window_height = 0;
	float m_last_window_scale = 1.0f;
};

class DisplayContainer final : public QStackedWidget
{
	Q_OBJECT

public:
	DisplayContainer();
	~DisplayContainer();

	// Wayland is broken in lots of ways, so we need to check for it.
	static bool isRunningOnWayland();

	static bool isNeeded(bool fullscreen, bool render_to_main);

	void setDisplayWidget(DisplayWidget* widget);
	DisplayWidget* removeDisplayWidget();

protected:
	bool event(QEvent* event) override;

private:
	DisplayWidget* m_display_widget = nullptr;
};
