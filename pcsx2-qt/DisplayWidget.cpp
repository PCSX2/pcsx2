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

#include "PrecompiledHeader.h"

#include "common/Assertions.h"

#include "pcsx2/Frontend/ImGuiManager.h"

#include "DisplayWidget.h"
#include "EmuThread.h"
#include "MainWindow.h"
#include "QtHost.h"
#include "QtUtils.h"

#include "pcsx2/GS/GSIntrin.h" // _BitScanForward

#include <QtCore/QDebug>
#include <QtGui/QGuiApplication>
#include <QtGui/QKeyEvent>
#include <QtGui/QResizeEvent>
#include <QtGui/QScreen>
#include <QtGui/QWindow>
#include <QtGui/QWindowStateChangeEvent>
#include <cmath>

#if defined(_WIN32)
#include "common/RedtapeWindows.h"
#elif !defined(APPLE)
#include <qpa/qplatformnativeinterface.h>
#endif

DisplayWidget::DisplayWidget(QWidget* parent)
	: QWidget(parent)
{
	// We want a native window for both D3D and OpenGL.
	setAutoFillBackground(false);
	setAttribute(Qt::WA_NativeWindow, true);
	setAttribute(Qt::WA_NoSystemBackground, true);
	setAttribute(Qt::WA_PaintOnScreen, true);
	setAttribute(Qt::WA_KeyCompression, false);
	setFocusPolicy(Qt::StrongFocus);
	setMouseTracking(true);
}

DisplayWidget::~DisplayWidget()
{
#ifdef _WIN32
	if (m_clip_mouse_enabled)
		ClipCursor(nullptr);
#endif
}

qreal DisplayWidget::devicePixelRatioFromScreen() const
{
	const QScreen* screen_for_ratio = screen();
	if (!screen_for_ratio)
		screen_for_ratio = QGuiApplication::primaryScreen();

	return screen_for_ratio ? screen_for_ratio->devicePixelRatio() : static_cast<qreal>(1);
}

int DisplayWidget::scaledWindowWidth() const
{
	return std::max(static_cast<int>(std::ceil(static_cast<qreal>(width()) * devicePixelRatioFromScreen())), 1);
}

int DisplayWidget::scaledWindowHeight() const
{
	return std::max(static_cast<int>(std::ceil(static_cast<qreal>(height()) * devicePixelRatioFromScreen())), 1);
}

std::optional<WindowInfo> DisplayWidget::getWindowInfo()
{
	WindowInfo wi;

	// Windows and Apple are easy here since there's no display connection.
#if defined(_WIN32)
	wi.type = WindowInfo::Type::Win32;
	wi.window_handle = reinterpret_cast<void*>(winId());
#elif defined(__APPLE__)
	wi.type = WindowInfo::Type::MacOS;
	wi.window_handle = reinterpret_cast<void*>(winId());
#else
	QPlatformNativeInterface* pni = QGuiApplication::platformNativeInterface();
	const QString platform_name = QGuiApplication::platformName();
	if (platform_name == QStringLiteral("xcb"))
	{
		wi.type = WindowInfo::Type::X11;
		wi.display_connection = pni->nativeResourceForWindow("display", windowHandle());
		wi.window_handle = reinterpret_cast<void*>(winId());
	}
	else if (platform_name == QStringLiteral("wayland"))
	{
		wi.type = WindowInfo::Type::Wayland;
		wi.display_connection = pni->nativeResourceForWindow("display", windowHandle());
		wi.window_handle = pni->nativeResourceForWindow("surface", windowHandle());
	}
	else
	{
		qCritical() << "Unknown PNI platform " << platform_name;
		return std::nullopt;
	}
#endif

	m_last_window_width = wi.surface_width = static_cast<u32>(scaledWindowWidth());
	m_last_window_height = wi.surface_height = static_cast<u32>(scaledWindowHeight());
	m_last_window_scale = wi.surface_scale = static_cast<float>(devicePixelRatioFromScreen());
	return wi;
}

void DisplayWidget::updateRelativeMode(bool master_enable)
{
	bool relative_mode = master_enable && InputManager::HasPointerAxisBinds();

#ifdef _WIN32
	// prefer ClipCursor() over warping movement when we're using raw input
	bool clip_cursor = relative_mode && false /*InputManager::IsUsingRawInput()*/;
	if (m_relative_mouse_enabled == relative_mode && m_clip_mouse_enabled == clip_cursor)
		return;

	DevCon.WriteLn("updateRelativeMode(): relative=%s, clip=%s", relative_mode ? "yes" : "no", clip_cursor ? "yes" : "no");

	if (!clip_cursor && m_clip_mouse_enabled)
	{
		m_clip_mouse_enabled = false;
		ClipCursor(nullptr);
	}
#else
	if (m_relative_mouse_enabled == relative_mode)
		return;

	DevCon.WriteLn("updateRelativeMode(): relative=%s", relative_mode ? "yes" : "no");
#endif

	if (relative_mode)
	{
#ifdef _WIN32
		m_relative_mouse_enabled = !clip_cursor;
		m_clip_mouse_enabled = clip_cursor;
#else
		m_relative_mouse_enabled = true;
#endif
		m_relative_mouse_start_pos = QCursor::pos();
		updateCenterPos();
		grabMouse();
	}
	else if (m_relative_mouse_enabled)
	{
		m_relative_mouse_enabled = false;
		QCursor::setPos(m_relative_mouse_start_pos);
		releaseMouse();
	}

}

void DisplayWidget::updateCursor(bool master_enable)
{
#ifdef _WIN32
	const bool hide = master_enable && (m_should_hide_cursor || m_relative_mouse_enabled || m_clip_mouse_enabled);
#else
	const bool hide = master_enable && (m_should_hide_cursor || m_relative_mouse_enabled);
#endif
	if (m_cursor_hidden == hide)
		return;

	m_cursor_hidden = hide;
	if (hide)
		setCursor(Qt::BlankCursor);
	else
		unsetCursor();
}

void DisplayWidget::handleCloseEvent(QCloseEvent* event)
{
	// Closing the separate widget will either cancel the close, or trigger shutdown.
	// In the latter case, it's going to destroy us, so don't let Qt do it first.
	if (QtHost::IsVMValid())
	{
		QMetaObject::invokeMethod(g_main_window, "requestShutdown", Q_ARG(bool, true),
			Q_ARG(bool, true), Q_ARG(bool, false));
	}
	else
	{
		QMetaObject::invokeMethod(g_main_window, &MainWindow::requestExit);
	}

	// Cancel the event from closing the window.
	event->ignore();
}

void DisplayWidget::updateCenterPos()
{
#ifdef _WIN32
	if (m_clip_mouse_enabled)
	{
		RECT rc;
		if (GetWindowRect(reinterpret_cast<HWND>(winId()), &rc))
			ClipCursor(&rc);
	}
	else if (m_relative_mouse_enabled)
	{
		RECT rc;
		if (GetWindowRect(reinterpret_cast<HWND>(winId()), &rc))
		{
			m_relative_mouse_center_pos.setX(((rc.right - rc.left) / 2) + rc.left);
			m_relative_mouse_center_pos.setY(((rc.bottom - rc.top) / 2) + rc.top);
			SetCursorPos(m_relative_mouse_center_pos.x(), m_relative_mouse_center_pos.y());
		}
	}
#else
	if (m_relative_mouse_enabled)
	{
		// we do a round trip here because these coordinates are dpi-unscaled
		m_relative_mouse_center_pos = mapToGlobal(QPoint((width() + 1) / 2, (height() + 1) / 2));
		QCursor::setPos(m_relative_mouse_center_pos);
		m_relative_mouse_center_pos = QCursor::pos();
	}
#endif
}

QPaintEngine* DisplayWidget::paintEngine() const
{
	return nullptr;
}

bool DisplayWidget::event(QEvent* event)
{
	switch (event->type())
	{
		case QEvent::KeyPress:
		case QEvent::KeyRelease:
		{
			const QKeyEvent* key_event = static_cast<QKeyEvent*>(event);
			
			// Forward text input to imgui.
			if (ImGuiManager::WantsTextInput() && key_event->type() == QEvent::KeyPress)
			{
				const QString text(key_event->text());
				if (!text.isEmpty())
					ImGuiManager::AddTextInput(text.toStdString());
			}

			if (key_event->isAutoRepeat())
				return true;

			// For some reason, Windows sends "fake" key events.
			// Scenario: Press shift, press F1, release shift, release F1.
			// Events: Shift=Pressed, F1=Pressed, Shift=Released, **F1=Pressed**, F1=Released.
			// To work around this, we keep track of keys pressed with modifiers in a list, and
			// discard the press event when it's been previously activated. It's pretty gross,
			// but I can't think of a better way of handling it, and there doesn't appear to be
			// any window flag which changes this behavior that I can see.

			const u32 key = QtUtils::KeyEventToCode(key_event);
			const Qt::KeyboardModifiers modifiers = key_event->modifiers();
			const bool pressed = (key_event->type() == QEvent::KeyPress);
			const auto it = std::find(m_keys_pressed_with_modifiers.begin(), m_keys_pressed_with_modifiers.end(), key);
			if (it != m_keys_pressed_with_modifiers.end())
			{
				if (pressed)
					return true;
				else
					m_keys_pressed_with_modifiers.erase(it);
			}
			else if (modifiers != Qt::NoModifier && modifiers != Qt::KeypadModifier && pressed)
			{
				m_keys_pressed_with_modifiers.push_back(key);
			}

			Host::RunOnCPUThread([key, pressed]() {
				InputManager::InvokeEvents(InputManager::MakeHostKeyboardKey(key), static_cast<float>(pressed));
			});

			return true;
		}

		case QEvent::MouseMove:
		{
			const QMouseEvent* mouse_event = static_cast<QMouseEvent*>(event);

			if (!m_relative_mouse_enabled)
			{
				const qreal dpr = devicePixelRatioFromScreen();
				const QPoint mouse_pos = mouse_event->pos();

				const float scaled_x = static_cast<float>(static_cast<qreal>(mouse_pos.x()) * dpr);
				const float scaled_y = static_cast<float>(static_cast<qreal>(mouse_pos.y()) * dpr);
				InputManager::UpdatePointerAbsolutePosition(0, scaled_x, scaled_y);
			}
			else
			{
				// On windows, we use winapi here. The reason being that the coordinates in QCursor
				// are un-dpi-scaled, so we lose precision at higher desktop scalings.
				float dx = 0.0f, dy = 0.0f;

#ifndef _WIN32
				const QPoint mouse_pos = QCursor::pos();
				if (mouse_pos != m_relative_mouse_center_pos)
				{
					dx = static_cast<float>(mouse_pos.x() - m_relative_mouse_center_pos.x());
					dy = static_cast<float>(mouse_pos.y() - m_relative_mouse_center_pos.y());
					QCursor::setPos(m_relative_mouse_center_pos);
				}
#else
				POINT mouse_pos;
				if (GetCursorPos(&mouse_pos))
				{
					dx = static_cast<float>(mouse_pos.x - m_relative_mouse_center_pos.x());
					dy = static_cast<float>(mouse_pos.y - m_relative_mouse_center_pos.y());
					SetCursorPos(m_relative_mouse_center_pos.x(), m_relative_mouse_center_pos.y());
				}
#endif

				if (dx != 0.0f)
					InputManager::UpdatePointerRelativeDelta(0, InputPointerAxis::X, dx);
				if (dy != 0.0f)
					InputManager::UpdatePointerRelativeDelta(0, InputPointerAxis::Y, dy);
			}

			return true;
		}

		case QEvent::MouseButtonPress:
		case QEvent::MouseButtonDblClick:
		case QEvent::MouseButtonRelease:
		{
			unsigned long button_index;
			if (_BitScanForward(&button_index, static_cast<u32>(static_cast<const QMouseEvent*>(event)->button())))
			{
				Host::RunOnCPUThread([button_index, pressed = (event->type() != QEvent::MouseButtonRelease)]() {
					InputManager::InvokeEvents(InputManager::MakePointerButtonKey(0, button_index), static_cast<float>(pressed));
				});
			}

			// don't toggle fullscreen when we're bound.. that wouldn't end well.
			if (event->type() == QEvent::MouseButtonDblClick &&
				static_cast<const QMouseEvent*>(event)->button() == Qt::LeftButton &&
				QtHost::IsVMValid() && !QtHost::IsVMPaused() &&
				!InputManager::HasAnyBindingsForKey(InputManager::MakePointerButtonKey(0, 0)) &&
				Host::GetBoolSettingValue("UI", "DoubleClickTogglesFullscreen", true))
			{
				g_emu_thread->toggleFullscreen();
			}

			return true;
		}

		case QEvent::Wheel:
		{
			const QPoint delta_angle(static_cast<QWheelEvent*>(event)->angleDelta());
			const float dx = std::clamp(static_cast<float>(delta_angle.x()) / QtUtils::MOUSE_WHEEL_DELTA, -1.0f, 1.0f);
			if (dx != 0.0f)
				InputManager::UpdatePointerRelativeDelta(0, InputPointerAxis::WheelX, dx);

			const float dy = std::clamp(static_cast<float>(delta_angle.y()) / QtUtils::MOUSE_WHEEL_DELTA, -1.0f, 1.0f);
			if (dy != 0.0f)
				InputManager::UpdatePointerRelativeDelta(0, InputPointerAxis::WheelY, dy);

			return true;
		}

		// According to https://bugreports.qt.io/browse/QTBUG-95925 the recommended practice for handling DPI change is responding to paint events
		case QEvent::Paint:
		case QEvent::Resize:
		{
			QWidget::event(event);

			const float dpr = devicePixelRatioFromScreen();
			const u32 scaled_width = static_cast<u32>(std::max(static_cast<int>(std::ceil(static_cast<qreal>(width()) * dpr)), 1));
			const u32 scaled_height = static_cast<u32>(std::max(static_cast<int>(std::ceil(static_cast<qreal>(height()) * dpr)), 1));

			// avoid spamming resize events for paint events (sent on move on windows)
			if (m_last_window_width != scaled_width || m_last_window_height != scaled_height || m_last_window_scale != dpr)
			{
				m_last_window_width = scaled_width;
				m_last_window_height = scaled_height;
				m_last_window_scale = dpr;
				emit windowResizedEvent(scaled_width, scaled_height, dpr);
			}

			updateCenterPos();
			return true;
		}

		case QEvent::Move:
		{
			updateCenterPos();
			return true;
		}

		case QEvent::Close:
		{
			handleCloseEvent(static_cast<QCloseEvent*>(event));
			return true;
		}

		case QEvent::WindowStateChange:
		{
			QWidget::event(event);

			if (static_cast<QWindowStateChangeEvent*>(event)->oldState() & Qt::WindowMinimized)
				emit windowRestoredEvent();

			return true;
		}

		default:
			return QWidget::event(event);
	}
}

DisplayContainer::DisplayContainer()
	: QStackedWidget(nullptr)
{
}

DisplayContainer::~DisplayContainer() = default;

bool DisplayContainer::isNeeded(bool fullscreen, bool render_to_main)
{
#if defined(_WIN32) || defined(__APPLE__)
	return false;
#else
	if (!isRunningOnWayland())
		return false;

	// We only need this on Wayland because of client-side decorations...
	return (fullscreen || !render_to_main);
#endif
}

bool DisplayContainer::isRunningOnWayland()
{
#if defined(_WIN32) || defined(__APPLE__)
	return false;
#else
	const QString platform_name = QGuiApplication::platformName();
	return (platform_name == QStringLiteral("wayland"));
#endif
}

void DisplayContainer::setDisplayWidget(DisplayWidget* widget)
{
	pxAssert(!m_display_widget);
	m_display_widget = widget;
	addWidget(widget);
}

DisplayWidget* DisplayContainer::removeDisplayWidget()
{
	DisplayWidget* widget = m_display_widget;
	pxAssert(widget);
	m_display_widget = nullptr;
	removeWidget(widget);
	return widget;
}

bool DisplayContainer::event(QEvent* event)
{
	if (event->type() == QEvent::Close && m_display_widget)
	{
		m_display_widget->handleCloseEvent(static_cast<QCloseEvent*>(event));
		return true;
	}

	const bool res = QStackedWidget::event(event);
	if (!m_display_widget)
		return res;

	switch (event->type())
	{
		case QEvent::WindowStateChange:
		{
			if (static_cast<QWindowStateChangeEvent*>(event)->oldState() & Qt::WindowMinimized)
				emit m_display_widget->windowRestoredEvent();
		}
		break;

		default:
			break;
	}

	return res;
}
