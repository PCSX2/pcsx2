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

#include <QtCore/QByteArray>
#include <QtCore/QMetaType>
#include <QtCore/QString>
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
} // namespace QtUtils
