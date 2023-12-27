// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "common/WindowInfo.h"

#include <QtCore/QByteArray>
#include <QtCore/QMetaType>
#include <QtCore/QString>
#include <QtCore/QAbstractItemModel>
#include <functional>
#include <initializer_list>
#include <string_view>
#include <optional>

class ByteStream;

class QAction;
class QComboBox;
class QFrame;
class QKeyEvent;
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

	/// Returns a key id for a key event, including any modifiers that we need (e.g. Keypad).
	/// NOTE: Defined in QtKeyCodes.cpp, not QtUtils.cpp.
	u32 KeyEventToCode(const QKeyEvent* ev);

	/// Opens a URL with the default handler.
	void OpenURL(QWidget* parent, const QUrl& qurl);

	/// Opens a URL string with the default handler.
	void OpenURL(QWidget* parent, const char* url);

	/// Opens a URL string with the default handler.
	void OpenURL(QWidget* parent, const QString& url);

	/// Converts a std::string_view to a QString safely.
	QString StringViewToQString(const std::string_view& str);

	/// Sets a widget to italics if the setting value is inherited.
	void SetWidgetFontForInheritedSetting(QWidget* widget, bool inherited);

	/// Changes whether a window is resizable.
	void SetWindowResizeable(QWidget* widget, bool resizeable);

	/// Adjusts the fixed size for a window if it's not resizeable.
	void ResizePotentiallyFixedSizeWindow(QWidget* widget, int width, int height);

	/// Returns the pixel ratio/scaling factor for a widget.
	qreal GetDevicePixelRatioForWidget(const QWidget* widget);

	/// Returns the common window info structure for a Qt widget.
	std::optional<WindowInfo> GetWindowInfoForWidget(QWidget* widget);

	/// Converts a value to a QString of said value with a proper fixed width
	template <typename T>
	QString FilledQStringFromValue(T val, u32 base)
	{
		return QString("%1").arg(QString::number(val, base), sizeof(val) * 2, '0').toUpper();
	};

	/// Converts an abstract item model to a CSV string.
	QString AbstractItemModelToCSV(QAbstractItemModel* model, int role = Qt::DisplayRole, bool useQuotes = false);
} // namespace QtUtils
