// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "DisplayWidget.h"
#include "MainWindow.h"
#include "QtHost.h"
#include "QtUtils.h"

#include "pcsx2/ImGui/FullscreenUI.h"
#include "pcsx2/ImGui/ImGuiManager.h"

#include "common/Assertions.h"
#include "common/Console.h"

#include <QtCore/QDebug>
#include <QtCore/QTimer>
#include <QtGui/QGuiApplication>
#include <QtGui/QKeyEvent>
#include <QtGui/QPlatformSurfaceEvent>
#include <QtGui/QResizeEvent>
#include <QtGui/QScreen>
#include <QtGui/QWindow>
#include <QtGui/QWindowStateChangeEvent>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>

#if defined(_WIN32)
#include "common/RedtapeWindows.h"
#include "common/StringUtil.h"
#include "common/Timer.h"

#include "fmt/format.h"
#endif

DisplaySurface::DisplaySurface()
	: QWindow()
{
	m_resize_debounce_timer = new QTimer(this);
	m_resize_debounce_timer->setSingleShot(true);
	m_resize_debounce_timer->setTimerType(Qt::PreciseTimer);
	connect(m_resize_debounce_timer, &QTimer::timeout, this, &DisplaySurface::onResizeDebounceTimer);
}

DisplaySurface::~DisplaySurface()
{
#ifdef _WIN32
	QtRawMouse::ClearDisplaySurface(this);
	if (m_clip_mouse_enabled)
		ClipCursor(nullptr);
#endif
}

QWidget* DisplaySurface::createWindowContainer(QWidget* parent)
{
	m_container = QWidget::createWindowContainer(this, parent);
	m_container->installEventFilter(this);
	m_container->setFocusPolicy(Qt::StrongFocus);
	return m_container;
}

std::optional<WindowInfo> DisplaySurface::getWindowInfo()
{
	std::optional<WindowInfo> ret(QtUtils::GetWindowInfoForWindow(this));
#ifdef _WIN32
	// The native window exists by now, so raw mouse positions can be mapped into it.
	QtRawMouse::UpdateFromSettings();
	QtRawMouse::SetDisplaySurface(this);
#endif
	if (ret.has_value())
	{
		m_last_window_width = ret->surface_width;
		m_last_window_height = ret->surface_height;
		m_last_window_scale = ret->surface_scale;
	}
	return ret;
}

void DisplaySurface::updateRelativeMode(bool enabled)
{
#ifdef _WIN32
	// prefer ClipCursor() over warping movement when we're using raw input
	bool clip_cursor = enabled && false /*InputManager::IsUsingRawInput()*/;
	if (m_relative_mouse_enabled == enabled && m_clip_mouse_enabled == clip_cursor)
		return;

	DevCon.WriteLn("updateRelativeMode(): relative=%s, clip=%s", enabled ? "yes" : "no", clip_cursor ? "yes" : "no");

	if (!clip_cursor && m_clip_mouse_enabled)
	{
		m_clip_mouse_enabled = false;
		ClipCursor(nullptr);
	}
#else
	if (m_relative_mouse_enabled == enabled)
		return;

	DevCon.WriteLn("updateRelativeMode(): relative=%s", enabled ? "yes" : "no");
#endif

	if (enabled)
	{
#ifdef _WIN32
		m_relative_mouse_enabled = !clip_cursor;
		m_clip_mouse_enabled = clip_cursor;
#else
		m_relative_mouse_enabled = true;
#endif
		m_relative_mouse_start_pos = QCursor::pos();
		updateCenterPos();
		setMouseGrabEnabled(true);
	}
	else if (m_relative_mouse_enabled)
	{
		m_relative_mouse_enabled = false;
		QCursor::setPos(m_relative_mouse_start_pos);
		setMouseGrabEnabled(false);
	}
}

void DisplaySurface::updateCursor(bool hidden)
{
	if (m_cursor_hidden == hidden)
		return;

	m_cursor_hidden = hidden;
	if (hidden)
	{
		DevCon.WriteLn("updateCursor(): Cursor is now hidden");
		setCursor(Qt::BlankCursor);
	}
	else
	{
		DevCon.WriteLn("updateCursor(): Cursor is now shown");
		unsetCursor();
	}
}

void DisplaySurface::handleCloseEvent(QCloseEvent* event)
{
	// Closing the separate widget will either cancel the close, or trigger shutdown.
	// In the latter case, it's going to destroy us, so don't let Qt do it first.
	// Treat a close event while fullscreen as an exit, that way ALT+F4 closes PCSX2,
	// rather than just the game.
	if (QtHost::IsVMValid() && !isFullScreen())
	{
		QMetaObject::invokeMethod(g_main_window, "requestShutdown", Q_ARG(bool, true),
			Q_ARG(bool, true), Q_ARG(bool, false));
	}
	else
	{
		QMetaObject::invokeMethod(g_main_window, "requestExit", Q_ARG(bool, true));
	}

	// Cancel the event from closing the window.
	event->ignore();
}

bool DisplaySurface::isFullScreen() const
{
	// DisplaySurface may be in a container
	return (parent() ? parent()->windowState() : windowState()) & Qt::WindowFullScreen;
}

void DisplaySurface::setFocus()
{
	if (m_container)
		m_container->setFocus();
	else
		requestActivate();
}

QByteArray DisplaySurface::saveGeometry() const
{
	if (m_container)
		return m_container->saveGeometry();
	else
	{
		// QWindow lacks saveGeometry, so create a dummy widget and copy geometry across.
		QWidget dummy = QWidget();
		dummy.setGeometry(geometry());
		return dummy.saveGeometry();
	}
}

void DisplaySurface::restoreGeometry(const QByteArray& geometry)
{
	if (m_container)
		m_container->restoreGeometry(geometry);
	else
	{
		// QWindow lacks restoreGeometry, so create a dummy widget and copy geometry across.
		QWidget dummy = QWidget();
		dummy.restoreGeometry(geometry);
		setGeometry(dummy.geometry());
	}
}

void DisplaySurface::updateCenterPos()
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

// Keyboard focus and child windows are inconsistant across platforms;
// Windows: Can programmatically focus the child window, NVidia overlay can defocus it.
// X11: Can programmatically focus the child window.
// Wayland: Child window cannot be focused at all on most(?) DE.
// Mac: Can programmatically focus the child window.
// Thus for KB inputs we need to sometimes use the event filter.
// Mouse events are always delivered to the child window, so that seems consistant.
void DisplaySurface::handleKeyInputEvent(QEvent* event)
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
				// Don't forward backspace characters. We send the backspace as a normal key event,
				// so if we send the character too, it double-deletes.
				QString text(key_event->text());
				text.remove(QChar('\b'));
				if (!text.isEmpty())
					ImGuiManager::AddTextInput(text.toStdString());
			}

			if (key_event->isAutoRepeat())
				return;

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
					return;
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

			return;
		}

		default:
			pxAssert(false);
			return;
	}
}

void DisplaySurface::onResizeDebounceTimer()
{
	emit windowResizedEvent(m_last_window_width, m_last_window_height, m_last_window_scale);
}

bool DisplaySurface::event(QEvent* event)
{
	switch (event->type())
	{
		case QEvent::KeyPress:
		case QEvent::KeyRelease:
		{
			handleKeyInputEvent(event);
			return true;
		}

		case QEvent::MouseMove:
		{
			const QMouseEvent* mouse_event = static_cast<QMouseEvent*>(event);

#ifdef _WIN32
			// With multi-mouse raw input, Qt only sees the merged system cursor. That's no good for the
			// gun pointers, but it is exactly what the menus should follow, so give it to the UI only.
			if (QtRawMouse::IsActive())
			{
				const qreal dpr = devicePixelRatio();
				const QPoint mouse_pos = mouse_event->pos();
				ImGuiManager::UpdateMousePosition(static_cast<float>(static_cast<qreal>(mouse_pos.x()) * dpr),
					static_cast<float>(static_cast<qreal>(mouse_pos.y()) * dpr));
				return true;
			}
#endif

			if (!m_relative_mouse_enabled)
			{
				const qreal dpr = devicePixelRatio();
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
#ifdef _WIN32
			const bool qt_buttons_to_pointer = !QtRawMouse::IsActive();
#else
			const bool qt_buttons_to_pointer = true;
#endif
			if (const u32 button_mask = qt_buttons_to_pointer ? static_cast<u32>(static_cast<const QMouseEvent*>(event)->button()) : 0u)
			{
				Host::RunOnCPUThread([button_index = std::countr_zero(button_mask),
										 pressed = (event->type() != QEvent::MouseButtonRelease)]() {
					InputManager::InvokeEvents(
						InputManager::MakePointerButtonKey(0, button_index), static_cast<float>(pressed));
				});
			}

			// don't toggle fullscreen when we're bound.. that wouldn't end well.
			if (event->type() == QEvent::MouseButtonDblClick &&
				static_cast<const QMouseEvent*>(event)->button() == Qt::LeftButton &&
				QtHost::IsVMValid() && !FullscreenUI::HasActiveWindow() &&
				((!QtHost::IsVMPaused() && !InputManager::HasAnyBindingsForKey(InputManager::MakePointerButtonKey(0, 0))) ||
					(QtHost::IsVMPaused() && !ImGuiManager::WantsMouseInput())) &&
				Host::GetBoolSettingValue("UI", "DoubleClickTogglesFullscreen", true))
			{
				g_emu_thread->toggleFullscreen();
			}

			return true;
		}

		case QEvent::Wheel:
		{
#ifdef _WIN32
			if (QtRawMouse::IsActive())
				return true;
#endif
			const QPoint delta_angle(static_cast<QWheelEvent*>(event)->angleDelta());
			const float dx = std::clamp(static_cast<float>(delta_angle.x()) / QtUtils::MOUSE_WHEEL_DELTA, -1.0f, 1.0f);
			if (dx != 0.0f)
				InputManager::UpdatePointerRelativeDelta(0, InputPointerAxis::WheelX, dx);

			const float dy = std::clamp(static_cast<float>(delta_angle.y()) / QtUtils::MOUSE_WHEEL_DELTA, -1.0f, 1.0f);
			if (dy != 0.0f)
				InputManager::UpdatePointerRelativeDelta(0, InputPointerAxis::WheelY, dy);

			return true;
		}

		case QEvent::DevicePixelRatioChange:
		case QEvent::Resize:
		{
			QWindow::event(event);

			const float dpr = devicePixelRatio();
			const u32 scaled_width = static_cast<u32>(std::max(static_cast<int>(std::round(static_cast<qreal>(width()) * dpr)), 1));
			const u32 scaled_height = static_cast<u32>(std::max(static_cast<int>(std::round(static_cast<qreal>(height()) * dpr)), 1));

			// avoid spamming resize events for paint events (sent on move on windows)
			if (m_last_window_width != scaled_width || m_last_window_height != scaled_height || m_last_window_scale != dpr)
			{
				m_last_window_width = scaled_width;
				m_last_window_height = scaled_height;
				m_last_window_scale = dpr;
				// qt spams resize events, sometimes several time per ms.
				// since a vulkan resize swap chain event takes between 15 to 25ms this is,
				// need less to say, unwanted.
				m_resize_debounce_timer->start(100);
			}

			updateCenterPos();
			return true;
		}

		case QEvent::DragEnter:
			QWindow::event(event);
			emit dragEnterEvent(static_cast<QDragEnterEvent*>(event));
			return event->isAccepted();

		case QEvent::Drop:
			QWindow::event(event);
			emit dropEvent(static_cast<QDropEvent*>(event));
			return event->isAccepted();

		case QEvent::Move:
			updateCenterPos();
			return true;

#ifdef _WIN32
		case QEvent::PlatformSurface:
		{
			// The native window can be destroyed and recreated; don't map positions into a dead HWND.
			const auto surface_event_type = static_cast<QPlatformSurfaceEvent*>(event)->surfaceEventType();
			if (surface_event_type == QPlatformSurfaceEvent::SurfaceAboutToBeDestroyed)
				QtRawMouse::ClearDisplaySurface(this);
			const bool ret = QWindow::event(event);
			if (surface_event_type == QPlatformSurfaceEvent::SurfaceCreated && QtRawMouse::IsActive())
				QtRawMouse::SetDisplaySurface(this);
			return ret;
		}
#endif

		// These events only work on the top level control.
		// Which is this container when render to seperate or fullscreen is active (Windows).
		case QEvent::Close:
			handleCloseEvent(static_cast<QCloseEvent*>(event));
			return true;
		case QEvent::WindowStateChange:
			if (static_cast<QWindowStateChangeEvent*>(event)->oldState() & Qt::WindowMinimized)
				emit windowRestoredEvent();
			return false;

		default:
			return QWindow::event(event);
	}
}

bool DisplaySurface::eventFilter(QObject* object, QEvent* event)
{
	switch (event->type())
	{
		case QEvent::KeyPress:
		case QEvent::KeyRelease:
#ifdef _WIN32
			// Nvidia overlay causes the child window to lose focus, but not its parent.
			// Refocus the child window.
			requestActivate();
#endif
			handleKeyInputEvent(event);
			return true;

		// These events only work on the top level control.
		// Which is this container when render to seperate or fullscreen is active (Non-Windows).
		case QEvent::Close:
			handleCloseEvent(static_cast<QCloseEvent*>(event));
			return true;
		case QEvent::WindowStateChange:
			if (static_cast<QWindowStateChangeEvent*>(event)->oldState() & Qt::WindowMinimized)
				emit windowRestoredEvent();
			return false;

		case QEvent::ChildWindowRemoved:
			if (static_cast<QChildWindowEvent*>(event)->child() == this)
			{
				object->removeEventFilter(this);
				m_container = nullptr;
			}
			return false;

		case QEvent::FocusIn:
			// macOS: When we (the display window) get focus from another window with a toolbar we update to the MainWindow toolbar.
			// This is because we are a different native window from our MainWindow. So, whenever we get focus, focus our MainWindow.
			// That way macOS will show the MainWindow toolbar when you click from the debugger / log window to the game.

			// Don't try to steal focus when we're showing a modal dialog
			// We end up ping ponging focus in a feedback loop
			if (QApplication::activeModalWidget() != nullptr)
				return false;

			if (const auto* w = qobject_cast<QWidget*>(object))
				w->window()->activateWindow();

			return false;
		default:
			return false;
	}
}

#ifdef _WIN32

// ------------------------------------------------------------------------
// Multi-mouse raw input (Windows only)
// A hidden message-only window receives WM_INPUT for every mouse, so it keeps working while the
// settings window is open (for binding) as well as in game. Each device gets a stable Pointer-N.
// Needs proper testing: written for absolute-mode lightguns (e.g. Sinden), relative mice are best-effort.
// ------------------------------------------------------------------------

namespace QtRawMouse
{
	struct Device
	{
		HANDLE handle;
		std::string name;
		float x;
		float y;

		// Diagnostics only: set while an absolute device reports (0,0), i.e. a lightgun that can't see the screen.
		bool lost = false;
	};

	static constexpr USHORT USAGE_PAGE_GENERIC = 0x01;
	static constexpr USHORT USAGE_GENERIC_MOUSE = 0x02;
	static constexpr const wchar_t* WINDOW_CLASS_NAME = L"PCSX2RawMouseWindow";

	static void Start();
	static void Stop();
	static void RefreshDevices();
	static void HandleInput(HRAWINPUT handle);
	static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

	static HWND s_hwnd = nullptr;
	static bool s_class_registered = false;
	static std::string s_last_config;

	// GunCon 2 "Lightgun device" per USB port. A device chosen for USB port N is always given Pointer-(N-1),
	// so guns don't swap numbers when Windows enumerates them differently.
	static constexpr u32 NUM_PINNED_PORTS = 2;
	static std::string GetPinnedDevice(u32 port)
	{
		return Host::GetStringSettingValue(fmt::format("USB{}", port + 1).c_str(), "guncon2_lightgun_device", "");
	}
	static std::string GetConfigKey()
	{
		std::string key = Host::GetStringSettingValue("InputSources", "RawInputMouseFilter", "");
		for (u32 port = 0; port < NUM_PINNED_PORTS; port++)
			key.append("|").append(GetPinnedDevice(port));
		return key;
	}

	// Screen/window geometry, cached so each input report doesn't make several Windows calls.
	// Refreshed every few hundred ms, which is plenty for window moves and resolution changes.
	struct CachedGeometry
	{
		HWND hwnd = nullptr;
		POINT origin = {};
		RECT client = {};
		int primary_w = 0, primary_h = 0;
		int virt_x = 0, virt_y = 0, virt_w = 0, virt_h = 0;
		Common::Timer age;
	};
	static CachedGeometry s_geom;
	static void UpdateCachedGeometry(HWND hwnd);

	// Diagnostics, logged at most every 5 seconds.
	static u64 s_stat_packets = 0;
	static std::array<u64, InputManager::MAX_POINTER_DEVICES> s_stat_packets_per_pointer = {};
	static std::array<u64, InputManager::MAX_POINTER_DEVICES> s_stat_dropouts_per_pointer = {};

	// Last raw report per pointer, for diagnosing aim problems (logged with the stats).
	struct LastReport
	{
		bool valid;
		USHORT flags;
		LONG raw_x, raw_y;
		float win_x, win_y;
	};
	static std::array<LastReport, InputManager::MAX_POINTER_DEVICES> s_last_report = {};
	static u64 s_stat_device_changes = 0;
	static double s_stat_handle_ms = 0.0;
	static Common::Timer s_stat_timer;
	static void MaybeLogStats();
	static std::vector<Device> s_devices;
	static DisplaySurface* s_display = nullptr;
} // namespace QtRawMouse

bool QtRawMouse::IsActive()
{
	return (s_hwnd != nullptr);
}

void QtRawMouse::SetDisplaySurface(DisplaySurface* surface)
{
	s_display = surface;
}

void QtRawMouse::ClearDisplaySurface(DisplaySurface* surface)
{
	if (s_display == surface)
		s_display = nullptr;
}

void QtRawMouse::UpdateFromSettings()
{
	const bool wanted = Host::GetBoolSettingValue("InputSources", "RawInputMouse", false);
	if (!wanted)
		Stop();
	else if (!s_hwnd)
		Start();
	else if (GetConfigKey() != s_last_config)
		RefreshDevices(); // only re-scan when the filter/pins changed; device plug/unplug is handled by WM_INPUT_DEVICE_CHANGE
}

void QtRawMouse::Start()
{
	const HINSTANCE instance = GetModuleHandleW(nullptr);
	if (!s_class_registered)
	{
		WNDCLASSEXW wc = {};
		wc.cbSize = sizeof(wc);
		wc.lpfnWndProc = WndProc;
		wc.hInstance = instance;
		wc.lpszClassName = WINDOW_CLASS_NAME;
		if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
		{
			Console.Error("(RawInputMouse) RegisterClassExW() failed: %u", static_cast<unsigned>(GetLastError()));
			return;
		}
		s_class_registered = true;
	}

	// Created on the UI thread, so Qt's event loop dispatches its messages for us.
	s_hwnd = CreateWindowExW(0, WINDOW_CLASS_NAME, L"PCSX2 Raw Mouse", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, instance, nullptr);
	if (!s_hwnd)
	{
		Console.Error("(RawInputMouse) CreateWindowExW() failed: %u", static_cast<unsigned>(GetLastError()));
		return;
	}

	// INPUTSINK so input arrives even when PCSX2 isn't focused (and while the settings window is);
	// DEVNOTIFY so we hear about plug/unplug. No RIDEV_NOLEGACY: the normal cursor is still needed for the UI.
	RAWINPUTDEVICE rid = {};
	rid.usUsagePage = USAGE_PAGE_GENERIC;
	rid.usUsage = USAGE_GENERIC_MOUSE;
	rid.dwFlags = RIDEV_INPUTSINK | RIDEV_DEVNOTIFY;
	rid.hwndTarget = s_hwnd;
	if (!RegisterRawInputDevices(&rid, 1, sizeof(rid)))
	{
		Console.Error("(RawInputMouse) RegisterRawInputDevices() failed: %u", static_cast<unsigned>(GetLastError()));
		DestroyWindow(s_hwnd);
		s_hwnd = nullptr;
		return;
	}

	InputManager::SetUICursorFromHost(true);
	Console.WriteLn("(RawInputMouse) Multi-mouse raw input started.");
	RefreshDevices();
}

void QtRawMouse::Stop()
{
	if (!s_hwnd)
		return;

	RAWINPUTDEVICE rid = {};
	rid.usUsagePage = USAGE_PAGE_GENERIC;
	rid.usUsage = USAGE_GENERIC_MOUSE;
	rid.dwFlags = RIDEV_REMOVE;
	rid.hwndTarget = nullptr;
	RegisterRawInputDevices(&rid, 1, sizeof(rid));

	DestroyWindow(s_hwnd);
	s_hwnd = nullptr;
	s_devices.clear();
	InputManager::SetHostPointerDeviceNames({});
	InputManager::SetUnassignedHostPointerDeviceNames({});
	InputManager::SetUICursorFromHost(false);
	Console.WriteLn("(RawInputMouse) Multi-mouse raw input stopped.");
}

void QtRawMouse::UpdateCachedGeometry(HWND hwnd)
{
	if (s_geom.hwnd == hwnd && s_geom.age.GetTimeMilliseconds() < 250.0)
		return;

	s_geom.hwnd = hwnd;
	s_geom.origin = {0, 0};
	ClientToScreen(hwnd, &s_geom.origin);
	GetClientRect(hwnd, &s_geom.client);
	s_geom.primary_w = GetSystemMetrics(SM_CXSCREEN);
	s_geom.primary_h = GetSystemMetrics(SM_CYSCREEN);
	s_geom.virt_x = GetSystemMetrics(SM_XVIRTUALSCREEN);
	s_geom.virt_y = GetSystemMetrics(SM_YVIRTUALSCREEN);
	s_geom.virt_w = GetSystemMetrics(SM_CXVIRTUALSCREEN);
	s_geom.virt_h = GetSystemMetrics(SM_CYVIRTUALSCREEN);
	s_geom.age.Reset();
}

void QtRawMouse::MaybeLogStats()
{
	const double elapsed_ms = s_stat_timer.GetTimeMilliseconds();
	if (elapsed_ms < 2000.0)
		return;

	std::string per_pointer;
	for (u32 i = 0; i < InputManager::MAX_POINTER_DEVICES; i++)
	{
		per_pointer += fmt::format(" P{}={:.0f}", i, static_cast<double>(s_stat_packets_per_pointer[i]) * 1000.0 / elapsed_ms);
		if (s_stat_dropouts_per_pointer[i] > 0)
			per_pointer += fmt::format("(lost {})", s_stat_dropouts_per_pointer[i]);
	}

	Console.WriteLn("(RawInputMouse) %.0f reports/sec (%s ), %.3f ms/sec handling them, %llu device changes",
		static_cast<double>(s_stat_packets) * 1000.0 / elapsed_ms, per_pointer.c_str(), s_stat_handle_ms * 1000.0 / elapsed_ms,
		static_cast<unsigned long long>(s_stat_device_changes));
	for (u32 i = 0; i < InputManager::MAX_POINTER_DEVICES; i++)
	{
		const LastReport& r = s_last_report[i];
		if (!r.valid || s_stat_packets_per_pointer[i] == 0)
			continue;

		const RECT& rc = s_geom.client;
		const bool outside = (r.win_x < 0.0f || r.win_y < 0.0f || r.win_x >= static_cast<float>(rc.right) || r.win_y >= static_cast<float>(rc.bottom));
		Console.WriteLn("(RawInputMouse)   Pointer-%u: flags=0x%02X raw=(%ld,%ld) window=(%.0f,%.0f) of %ldx%ld%s", i, r.flags,
			r.raw_x, r.raw_y, r.win_x, r.win_y, rc.right, rc.bottom, outside ? "  <-- OUTSIDE WINDOW" : "");
	}

	s_stat_packets = 0;
	s_stat_packets_per_pointer = {};
	s_stat_dropouts_per_pointer = {};
	s_stat_device_changes = 0;
	s_stat_handle_ms = 0.0;
	s_stat_timer.Reset();
}

LRESULT CALLBACK QtRawMouse::WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	if (msg == WM_INPUT)
	{
		Common::Timer handle_timer;
		HandleInput(reinterpret_cast<HRAWINPUT>(lparam));
		s_stat_handle_ms += handle_timer.GetTimeMilliseconds();
		s_stat_packets++;
		MaybeLogStats();
		// Fall through to DefWindowProc, which cleans up the raw input buffer.
	}
	else if (msg == WM_INPUT_DEVICE_CHANGE)
	{
		s_stat_device_changes++;
		RefreshDevices();
		return 0;
	}

	return DefWindowProcW(hwnd, msg, wparam, lparam);
}

void QtRawMouse::RefreshDevices()
{
	UINT count = 0;
	if (GetRawInputDeviceList(nullptr, &count, sizeof(RAWINPUTDEVICELIST)) != 0)
		return;

	std::vector<RAWINPUTDEVICELIST> list(count);
	const UINT got = (count > 0) ? GetRawInputDeviceList(list.data(), &count, sizeof(RAWINPUTDEVICELIST)) : 0;
	if (got == static_cast<UINT>(-1))
		return;
	list.resize(got);

	// Optional comma-separated filter on the device path, e.g. "VID_16C0" to only use the lightguns,
	// so a desk mouse or trackball doesn't take a pointer slot.
	s_last_config = GetConfigKey();
	const std::string filter_str = StringUtil::toUpper(Host::GetStringSettingValue("InputSources", "RawInputMouseFilter", ""));
	const std::vector<std::string_view> filters = StringUtil::SplitString(filter_str, ',');

	std::vector<Device> found;
	for (const RAWINPUTDEVICELIST& dev : list)
	{
		if (dev.dwType != RIM_TYPEMOUSE)
			continue;

		UINT name_len = 0;
		GetRawInputDeviceInfoW(dev.hDevice, RIDI_DEVICENAME, nullptr, &name_len);
		if (name_len == 0)
			continue;

		std::wstring wname(name_len, L'\0');
		if (GetRawInputDeviceInfoW(dev.hDevice, RIDI_DEVICENAME, wname.data(), &name_len) == static_cast<UINT>(-1))
			continue;
		wname.resize(std::wcslen(wname.c_str()));

		std::string name = StringUtil::WideStringToUTF8String(wname);
		const std::string upper_name = StringUtil::toUpper(name);

		// Skip virtual mice such as the Remote Desktop one.
		if (upper_name.find("ROOT#") != std::string::npos)
			continue;

		if (!filters.empty() &&
			std::none_of(filters.begin(), filters.end(), [&upper_name](const std::string_view f) {
				const std::string_view trimmed = StringUtil::StripWhitespace(f);
				return !trimmed.empty() && upper_name.find(trimmed) != std::string::npos;
			}))
		{
			continue;
		}

		// Keep the last known position if this device was already tracked.
		float x = 0.0f, y = 0.0f;
		for (const Device& existing : s_devices)
		{
			if (existing.handle == dev.hDevice)
			{
				x = existing.x;
				y = existing.y;
				break;
			}
		}

		found.push_back(Device{dev.hDevice, std::move(name), x, y});
	}

	// Sort by device path so the Pointer-N order is the same every time, not plug-in order.
	std::sort(found.begin(), found.end(), [](const Device& a, const Device& b) { return a.name < b.name; });

	// Slot = pointer index. Pinned guns go to their port's slot first, everything else fills the gaps.
	// Needs proper testing with guns swapped between USB ports.
	std::vector<std::optional<Device>> pointer_slots(InputManager::MAX_POINTER_DEVICES);
	for (u32 port = 0; port < NUM_PINNED_PORTS && port < InputManager::MAX_POINTER_DEVICES; port++)
	{
		const std::string pin = GetPinnedDevice(port);
		if (pin.empty())
			continue;

		const auto it = std::find_if(found.begin(), found.end(),
			[&pin](const Device& d) { return StringUtil::compareNoCase(d.name, pin); });
		if (it == found.end())
		{
			Console.Warning("(RawInputMouse) Lightgun device for USB port %u isn't connected: %s", port + 1, pin.c_str());
			continue;
		}

		pointer_slots[port] = std::move(*it);
		found.erase(it);
	}

	std::vector<std::string> unassigned;
	for (Device& d : found)
	{
		const auto free_slot = std::find_if(pointer_slots.begin(), pointer_slots.end(), [](const std::optional<Device>& s) { return !s.has_value(); });
		if (free_slot == pointer_slots.end())
		{
			Console.Warning("(RawInputMouse) Ignored (no free pointer): %s", d.name.c_str());
			unassigned.push_back(d.name);
			continue;
		}
		*free_slot = std::move(d);
	}

	// Keep empty slots as placeholder entries so the vector index stays equal to the pointer index.
	s_devices.clear();
	std::vector<std::string> names;
	for (size_t i = 0; i < pointer_slots.size(); i++)
	{
		s_devices.push_back(pointer_slots[i].has_value() ? std::move(*pointer_slots[i]) : Device{nullptr, std::string(), 0.0f, 0.0f});
		names.push_back(s_devices.back().name);
		if (!s_devices.back().name.empty())
			Console.WriteLn("(RawInputMouse) Pointer-%zu = %s", i, s_devices.back().name.c_str());
	}

	InputManager::SetHostPointerDeviceNames(std::move(names));
	InputManager::SetUnassignedHostPointerDeviceNames(std::move(unassigned));
}

void QtRawMouse::HandleInput(HRAWINPUT handle)
{
	RAWINPUT ri;
	UINT size = sizeof(ri);
	if (GetRawInputData(handle, RID_INPUT, &ri, &size, sizeof(RAWINPUTHEADER)) == static_cast<UINT>(-1) ||
		ri.header.dwType != RIM_TYPEMOUSE)
	{
		return;
	}

	const auto it = std::find_if(s_devices.begin(), s_devices.end(),
		[&ri](const Device& d) { return d.handle != nullptr && d.handle == ri.header.hDevice; });
	if (it == s_devices.end())
		return;

	const u32 index = static_cast<u32>(it - s_devices.begin());
	s_stat_packets_per_pointer[index]++;
	Device& dev = *it;
	const RAWMOUSE& m = ri.data.mouse;

	// Positions only mean something relative to the game display. Buttons are always forwarded,
	// so binding in the settings window works without a game running.
	const HWND display_hwnd = s_display ? reinterpret_cast<HWND>(s_display->winId()) : nullptr;

	if (m.usFlags & MOUSE_MOVE_ABSOLUTE)
	{
		if (display_hwnd)
		{
			// Absolute devices (lightguns) report 0..65535 across the primary monitor, or across the
			// whole virtual desktop when MOUSE_VIRTUAL_DESKTOP is set. Convert to screen pixels, then
			// to window client pixels, which is what the Qt mouse path passes to the pointer code.
			UpdateCachedGeometry(display_hwnd);
			const bool virtual_desktop = (m.usFlags & MOUSE_VIRTUAL_DESKTOP) != 0;
			const int left = virtual_desktop ? s_geom.virt_x : 0;
			const int top = virtual_desktop ? s_geom.virt_y : 0;
			const int width = virtual_desktop ? s_geom.virt_w : s_geom.primary_w;
			const int height = virtual_desktop ? s_geom.virt_h : s_geom.primary_h;

			const float new_x = static_cast<float>(left) + (static_cast<float>(m.lLastX) * static_cast<float>(width) / 65536.0f) -
			                    static_cast<float>(s_geom.origin.x);
			const float new_y = static_cast<float>(top) + (static_cast<float>(m.lLastY) * static_cast<float>(height) / 65536.0f) -
			                    static_cast<float>(s_geom.origin.y);

			s_last_report[index] = LastReport{true, m.usFlags, m.lLastX, m.lLastY, new_x, new_y};

			// Diagnostics only: count each time a lightgun starts reporting (0,0) (lost sight of the screen).
			const bool lost_now = (m.lLastX == 0 && m.lLastY == 0);
			if (lost_now && !dev.lost)
				s_stat_dropouts_per_pointer[index]++;
			dev.lost = lost_now;

			// Guns repeat the same position constantly; only pass on real movement.
			if (new_x != dev.x || new_y != dev.y)
			{
				dev.x = new_x;
				dev.y = new_y;
				InputManager::UpdatePointerAbsolutePosition(index, dev.x, dev.y);
			}
		}
	}
	else if (display_hwnd && (m.lLastX != 0 || m.lLastY != 0))
	{
		const float dx = static_cast<float>(m.lLastX);
		const float dy = static_cast<float>(m.lLastY);
		if (s_display->isRelativeMouseMode())
		{
			if (dx != 0.0f)
				InputManager::UpdatePointerRelativeDelta(index, InputPointerAxis::X, dx);
			if (dy != 0.0f)
				InputManager::UpdatePointerRelativeDelta(index, InputPointerAxis::Y, dy);
		}
		else
		{
			// Normal mice: keep a per-device virtual cursor inside the window.
			UpdateCachedGeometry(display_hwnd);
			const RECT& rc = s_geom.client;
			dev.x = std::clamp(dev.x + dx, 0.0f, static_cast<float>(std::max<LONG>(rc.right - 1, 0)));
			dev.y = std::clamp(dev.y + dy, 0.0f, static_cast<float>(std::max<LONG>(rc.bottom - 1, 0)));
			InputManager::UpdatePointerAbsolutePosition(index, dev.x, dev.y);
		}
	}

	const USHORT flags = m.usButtonFlags;
	static constexpr std::pair<USHORT, USHORT> button_flags[] = {
		{RI_MOUSE_BUTTON_1_DOWN, RI_MOUSE_BUTTON_1_UP},
		{RI_MOUSE_BUTTON_2_DOWN, RI_MOUSE_BUTTON_2_UP},
		{RI_MOUSE_BUTTON_3_DOWN, RI_MOUSE_BUTTON_3_UP},
		{RI_MOUSE_BUTTON_4_DOWN, RI_MOUSE_BUTTON_4_UP},
		{RI_MOUSE_BUTTON_5_DOWN, RI_MOUSE_BUTTON_5_UP},
	};
	for (u32 button = 0; button < static_cast<u32>(std::size(button_flags)); button++)
	{
		const bool down = (flags & button_flags[button].first) != 0;
		const bool up = (flags & button_flags[button].second) != 0;
		if (!down && !up)
			continue;

		// Same index scheme as the Qt path: 0 = left, 1 = right, 2 = middle, 3/4 = side buttons.
		// A single report can carry both a press and a release, so send them in that order.
		Host::RunOnCPUThread([index, button, down, up]() {
			const InputBindingKey key = InputManager::MakePointerButtonKey(index, button);
			if (down)
				InputManager::InvokeEvents(key, 1.0f);
			if (up)
				InputManager::InvokeEvents(key, 0.0f);
		});
	}

	if (display_hwnd && (flags & RI_MOUSE_WHEEL))
	{
		const float d = std::clamp(static_cast<float>(static_cast<SHORT>(m.usButtonData)) / WHEEL_DELTA, -1.0f, 1.0f);
		if (d != 0.0f)
			InputManager::UpdatePointerRelativeDelta(index, InputPointerAxis::WheelY, d);
	}
	if (display_hwnd && (flags & RI_MOUSE_HWHEEL))
	{
		const float d = std::clamp(static_cast<float>(static_cast<SHORT>(m.usButtonData)) / WHEEL_DELTA, -1.0f, 1.0f);
		if (d != 0.0f)
			InputManager::UpdatePointerRelativeDelta(index, InputPointerAxis::WheelX, d);
	}
}

#endif // _WIN32

#include "moc_DisplayWidget.cpp"
