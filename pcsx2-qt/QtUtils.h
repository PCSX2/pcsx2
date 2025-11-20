// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "common/WindowInfo.h"

#include <QtCore/QByteArray>
#include <QtCore/QMetaType>
#include <QtCore/QString>
#include <QtCore/QAbstractItemModel>
#include <QtGui/QGuiApplication>
#include <QtGui/QWindow>
#if !defined(_WIN32) and !defined(__APPLE__)
#include <qpa/qplatformnativeinterface.h>
#endif
#include <QtGui/QScreen>
#include <functional>
#include <initializer_list>
#include <string_view>
#include <type_traits>
#include <optional>

#include "common/Console.h"

class ByteStream;

class QAction;
class QComboBox;
class QFileInfo;
class QFrame;
class QIcon;
class QLabel;
class QKeyEvent;
class QSlider;
class QTableView;
class QTreeView;
class QVariant;
class QWidget;
class QUrl;

namespace QtUtils
{
	/// Wheel delta is 120 as in winapi.
	static constexpr float MOUSE_WHEEL_DELTA = 120.0f;

	/// Marks an action as the "default" - i.e. makes the text bold.
	void MarkActionAsDefault(QAction* action);

	/// Creates a horizontal line widget.
	QFrame* CreateHorizontalLine(QWidget* parent);

	/// Returns the greatest parent of a widget, i.e. its dialog/window.
	QWidget* GetRootWidget(QWidget* widget, bool stop_at_window_or_dialog = true);

	/// Resizes columns of the table view to at the specified widths. A negative width will stretch the column to use the
	/// remaining space.
	void ResizeColumnsForTableView(QTableView* view, const std::initializer_list<int>& widths);
	void ResizeColumnsForTreeView(QTreeView* view, const std::initializer_list<int>& widths);

	enum struct ScalingMode
	{
		Fit,
		Fill,
		Stretch,
		Center,
		Tile,

		MaxCount
	};

	/// Resize and scale a given Pixmap (and optionally adjust opacity)
	void resizeAndScalePixmap(QPixmap* pm, const int expected_width, const int expected_height, const qreal dpr, const ScalingMode scaling_mode, const float opacity);

	/// Returns a key id for a key event, including any modifiers that we need (e.g. Keypad).
	/// NOTE: Defined in QtKeyCodes.cpp, not QtUtils.cpp.
	u32 KeyEventToCode(const QKeyEvent* ev);

	/// Shows a file, or the containing folder if unsupported, with the system file explorer
	void ShowInFileExplorer(QWidget* parent, const QFileInfo& file);

	/// Get the context menu name for the action performed by ShowInFileExplorer
	QString GetShowInFileExplorerMessage();

	/// Opens a URL with the default handler.
	void OpenURL(QWidget* parent, const QUrl& qurl);

	/// Opens a URL string with the default handler.
	void OpenURL(QWidget* parent, const char* url);

	/// Opens a URL string with the default handler.
	void OpenURL(QWidget* parent, const QString& url);

	/// Converts a std::string_view to a QString safely.
	QString StringViewToQString(const std::string_view str);

	/// Sets a widget to italics if the setting value is inherited.
	void SetWidgetFontForInheritedSetting(QWidget* widget, bool inherited);

	/// Binds a label to a slider's value.
	void BindLabelToSlider(QSlider* slider, QLabel* label, float range = 1.0f);

	/// Changes whether a window is resizable.
	void SetWindowResizeable(QWidget* widget, bool resizeable);

	/// Adjusts the fixed size for a window if it's not resizeable.
	void ResizePotentiallyFixedSizeWindow(QWidget* widget, int width, int height);

	/// Returns the common window info structure for a Qt Window/Widget.
	template <class T>
		requires std::is_base_of_v<QWidget, T> || std::is_base_of_v<QWindow, T>
	std::optional<WindowInfo> GetWindowInfoForWindow(T* window)
	{
		WindowInfo wi;

		// Windows and Apple are easy here since there's no display connection.
#if defined(_WIN32)
		wi.type = WindowInfo::Type::Win32;
		wi.window_handle = reinterpret_cast<void*>(window->winId());
#elif defined(__APPLE__)
		wi.type = WindowInfo::Type::MacOS;
		wi.window_handle = reinterpret_cast<void*>(window->winId());
#else
		QPlatformNativeInterface* pni = QGuiApplication::platformNativeInterface();
		const QString platform_name = QGuiApplication::platformName();

		QWindow* windowHandle;
		if constexpr (std::is_base_of_v<QWidget, T>)
			windowHandle = window->windowHandle();
		else
			windowHandle = window;

		if (platform_name == QStringLiteral("xcb"))
		{
			// Can't get a handle for an unmapped window in X, it doesn't like it.
			if (!window->isVisible())
			{
				Console.WriteLn("Returning null window info for widget because it is not visible.");
				return std::nullopt;
			}

			wi.type = WindowInfo::Type::X11;
			wi.display_connection = pni->nativeResourceForWindow("display", windowHandle);
			wi.window_handle = reinterpret_cast<void*>(window->winId());
		}
		else if (platform_name == QStringLiteral("wayland"))
		{
			wi.type = WindowInfo::Type::Wayland;
			wi.display_connection = pni->nativeResourceForWindow("display", windowHandle);
			wi.window_handle = pni->nativeResourceForWindow("surface", windowHandle);
		}
		else
		{
			Console.WriteLn("Unknown PNI platform '%s'.", platform_name.toUtf8().constData());
			return std::nullopt;
		}
#endif

		qreal dpr;
		if constexpr (std::is_base_of_v<QWidget, T>)
			dpr = window->devicePixelRatioF();
		else
			dpr = window->devicePixelRatio();

		wi.surface_width = static_cast<u32>(std::max(static_cast<int>(std::round(static_cast<qreal>(window->width()) * dpr)), 1));
		wi.surface_height = static_cast<u32>(std::max(static_cast<int>(std::round(static_cast<qreal>(window->height()) * dpr)), 1));
		wi.surface_scale = static_cast<float>(dpr);

		// Query refresh rate, we need it for sync.
		std::optional<float> surface_refresh_rate = WindowInfo::QueryRefreshRateForWindow(wi);
		if (!surface_refresh_rate.has_value())
		{
			// Fallback to using the screen, getting the rate for Wayland is an utter mess otherwise.
			const QScreen* widget_screen = window->screen();
			if (!widget_screen)
				widget_screen = QGuiApplication::primaryScreen();
			surface_refresh_rate = widget_screen ? static_cast<float>(widget_screen->refreshRate()) : 0.0f;
		}

		wi.surface_refresh_rate = surface_refresh_rate.value();
		INFO_LOG("Surface refresh rate: {} hz", wi.surface_refresh_rate);

		return wi;
	}

	/// Converts a value to a QString of said value with a proper fixed width
	template <typename T>
	QString FilledQStringFromValue(T val, u32 base)
	{
		return QString("%1").arg(QString::number(val, base), sizeof(val) * 2, '0').toUpper();
	};

	/// Converts an abstract item model to a CSV string.
	QString AbstractItemModelToCSV(QAbstractItemModel* model, int role = Qt::DisplayRole, bool useQuotes = false);

	/// Checks if we can use transparency effects e.g. for dock drop indicators.
	bool IsCompositorManagerRunning();

	/// Sets the scalable icon to a given label (svg icons, or icons with multiple size pixmaps)
	/// The icon will then be reloaded on DPR changes using an event filter
	void SetScalableIcon(QLabel* lbl, const QIcon& icon, const QSize& size);

	/// Gets the system language code, matching it against available languages
	/// Returns the best matching language code, or "en-US" if no match is found
	QString GetSystemLanguageCode();

	/// Gets a flag icon for a given language code
	/// Returns an empty QIcon if no flag is available for the language
	QIcon GetFlagIconForLanguage(const QString& language_code);
} // namespace QtUtils
