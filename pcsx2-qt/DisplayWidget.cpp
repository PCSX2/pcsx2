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

#include "DisplayWidget.h"
#include "EmuThread.h"
#include "MainWindow.h"
#include "QtHost.h"

#include "pcsx2/GS/GSIntrin.h" // _BitScanForward

#include <QtCore/QDebug>
#include <QtGui/QGuiApplication>
#include <QtGui/QKeyEvent>
#include <QtGui/QResizeEvent>
#include <QtGui/QScreen>
#include <QtGui/QWindow>
#include <QtGui/QWindowStateChangeEvent>
#include <cmath>

#if !defined(_WIN32) && !defined(APPLE)
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

DisplayWidget::~DisplayWidget() = default;

qreal DisplayWidget::devicePixelRatioFromScreen() const
{
	const QScreen* screen_for_ratio = screen();
	if (!screen_for_ratio)
		screen_for_ratio = QGuiApplication::primaryScreen();

	return screen_for_ratio ? screen_for_ratio->devicePixelRatio() : static_cast<qreal>(1);
}

int DisplayWidget::scaledWindowWidth() const
{
	return static_cast<int>(std::ceil(static_cast<qreal>(width()) * devicePixelRatioFromScreen()));
}

int DisplayWidget::scaledWindowHeight() const
{
	return static_cast<int>(std::ceil(static_cast<qreal>(height()) * devicePixelRatioFromScreen()));
}

std::optional<WindowInfo> DisplayWidget::getWindowInfo() const
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

	wi.surface_width = scaledWindowWidth();
	wi.surface_height = scaledWindowHeight();
	wi.surface_scale = devicePixelRatioFromScreen();
	return wi;
}

void DisplayWidget::setRelativeMode(bool enabled)
{
	if (m_relative_mouse_enabled == enabled)
		return;

	if (enabled)
	{
		m_relative_mouse_start_position = QCursor::pos();

		const QPoint center_pos = mapToGlobal(QPoint(width() / 2, height() / 2));
		QCursor::setPos(center_pos);
		m_relative_mouse_last_position = center_pos;
		grabMouse();
	}
	else
	{
		QCursor::setPos(m_relative_mouse_start_position);
		releaseMouse();
	}

	m_relative_mouse_enabled = enabled;
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
			if (key_event->isAutoRepeat())
				return true;

			// For some reason, Windows sends "fake" key events.
			// Scenario: Press shift, press F1, release shift, release F1.
			// Events: Shift=Pressed, F1=Pressed, Shift=Released, **F1=Pressed**, F1=Released.
			// To work around this, we keep track of keys pressed with modifiers in a list, and
			// discard the press event when it's been previously activated. It's pretty gross,
			// but I can't think of a better way of handling it, and there doesn't appear to be
			// any window flag which changes this behavior that I can see.

			const int key = key_event->key();
			const bool pressed = (key_event->type() == QEvent::KeyPress);
			const auto it = std::find(m_keys_pressed_with_modifiers.begin(), m_keys_pressed_with_modifiers.end(), key);
			if (it != m_keys_pressed_with_modifiers.end())
			{
				if (pressed)
					return true;
				else
					m_keys_pressed_with_modifiers.erase(it);
			}
			else if (key_event->modifiers() != Qt::NoModifier && pressed)
			{
				m_keys_pressed_with_modifiers.push_back(key);
			}

			emit windowKeyEvent(key, pressed);
			return true;
		}

		case QEvent::MouseMove:
		{
			const QMouseEvent* mouse_event = static_cast<QMouseEvent*>(event);

			if (!m_relative_mouse_enabled)
			{
				const qreal dpr = devicePixelRatioFromScreen();
				const QPoint mouse_pos = mouse_event->pos();
				const int scaled_x = static_cast<int>(static_cast<qreal>(mouse_pos.x()) * dpr);
				const int scaled_y = static_cast<int>(static_cast<qreal>(mouse_pos.y()) * dpr);

				windowMouseMoveEvent(scaled_x, scaled_y);
			}
			else
			{
				const QPoint center_pos = mapToGlobal(QPoint((width() + 1) / 2, (height() + 1) / 2));
				const QPoint mouse_pos = mapToGlobal(mouse_event->pos());

				const int dx = mouse_pos.x() - center_pos.x();
				const int dy = mouse_pos.y() - center_pos.y();
				m_relative_mouse_last_position.setX(m_relative_mouse_last_position.x() + dx);
				m_relative_mouse_last_position.setY(m_relative_mouse_last_position.y() + dy);
				windowMouseMoveEvent(m_relative_mouse_last_position.x(), m_relative_mouse_last_position.y());
				QCursor::setPos(center_pos);
			}

			return true;
		}

		case QEvent::MouseButtonPress:
		case QEvent::MouseButtonDblClick:
		case QEvent::MouseButtonRelease:
		{
			unsigned long button_index;
			if (_BitScanForward(&button_index, static_cast<u32>(static_cast<const QMouseEvent*>(event)->button())))
				emit windowMouseButtonEvent(static_cast<int>(button_index + 1u), event->type() != QEvent::MouseButtonRelease);

			if (event->type() == QEvent::MouseButtonDblClick &&
				static_cast<const QMouseEvent*>(event)->button() == Qt::LeftButton &&
				Host::GetBoolSettingValue("UI", "DoubleClickTogglesFullscreen", true))
			{
				g_emu_thread->toggleFullscreen();
			}

			return true;
		}

		case QEvent::Wheel:
		{
			const QWheelEvent* wheel_event = static_cast<QWheelEvent*>(event);
			emit windowMouseWheelEvent(wheel_event->angleDelta());
			return true;
		}

		case QEvent::Resize:
		{
			QWidget::event(event);

			const qreal dpr = devicePixelRatioFromScreen();
			const QSize size = static_cast<QResizeEvent*>(event)->size();
			const int width = static_cast<int>(std::ceil(static_cast<qreal>(size.width()) * devicePixelRatioFromScreen()));
			const int height = static_cast<int>(std::ceil(static_cast<qreal>(size.height()) * devicePixelRatioFromScreen()));

			emit windowResizedEvent(width, height, dpr);
			return true;
		}

		case QEvent::Close:
		{
			if (!g_main_window->requestShutdown())
			{
				// abort the window close
				event->ignore();
				return true;
			}

			QWidget::event(event);
			return true;
		}

		case QEvent::WindowStateChange:
		{
			QWidget::event(event);

			if (static_cast<QWindowStateChangeEvent*>(event)->oldState() & Qt::WindowMinimized)
				emit windowRestoredEvent();

			return true;
		}

		case QEvent::FocusIn:
		{
			QWidget::event(event);
			emit windowFocusEvent();
			return true;
		}

		case QEvent::ActivationChange:
		{
			QWidget::event(event);
			if (isActiveWindow())
				emit windowFocusEvent();

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

bool DisplayContainer::IsNeeded(bool fullscreen, bool render_to_main)
{
#if defined(_WIN32) || defined(__APPLE__)
	return false;
#else
	if (!fullscreen && render_to_main)
		return false;

	// We only need this on Wayland because of client-side decorations...
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
	if (event->type() == QEvent::Close && !g_main_window->requestShutdown())
	{
		// abort the window close
		event->ignore();
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

		case QEvent::FocusIn:
		{
			emit m_display_widget->windowFocusEvent();
		}
		break;

		case QEvent::ActivationChange:
		{
			if (isActiveWindow())
				emit m_display_widget->windowFocusEvent();
		}
		break;

		default:
			break;
	}

	return res;
}
