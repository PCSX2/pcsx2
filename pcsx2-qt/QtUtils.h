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

	/// Returns a string identifier for a Qt key ID.
	QString GetKeyIdentifier(int key);

	/// Returns the integer Qt key ID for an identifier.
	std::optional<int> GetKeyIdForIdentifier(const QString& key_identifier);

	/// Stringizes a key event.
	QString KeyEventToString(int key, Qt::KeyboardModifiers mods);

	/// Returns an integer id for a stringized key event. Modifiers are in the upper bits.
	std::optional<int> ParseKeyString(const QString& key_str);

	/// Returns a key id for a key event, including any modifiers.
	int KeyEventToInt(int key, Qt::KeyboardModifiers mods);

	/// Opens a URL with the default handler.
	void OpenURL(QWidget* parent, const QUrl& qurl);

	/// Opens a URL string with the default handler.
	void OpenURL(QWidget* parent, const char* url);

	/// Opens a URL string with the default handler.
	void OpenURL(QWidget* parent, const QString& url);

	/// Converts a std::string_view to a QString safely.
	QString StringViewToQString(const std::string_view& str);
} // namespace QtUtils