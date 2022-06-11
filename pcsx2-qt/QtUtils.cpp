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

#include <QtCore/QCoreApplication>
#include <QtCore/QMetaObject>
#include <QtGui/QDesktopServices>
#include <QtGui/QKeyEvent>

#include <QtGui/QAction>

#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QInputDialog>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QStyle>
#include <QtWidgets/QTableView>
#include <QtWidgets/QTreeView>
#include <algorithm>
#include <array>
#include <map>

#include "QtUtils.h"

namespace QtUtils
{
	void MarkActionAsDefault(QAction* action)
	{
		QFont new_font(action->font());
		new_font.setBold(true);
		action->setFont(new_font);
	}

	QFrame* CreateHorizontalLine(QWidget* parent)
	{
		QFrame* line = new QFrame(parent);
		line->setFrameShape(QFrame::HLine);
		line->setFrameShadow(QFrame::Sunken);
		return line;
	}

	QWidget* GetRootWidget(QWidget* widget, bool stop_at_window_or_dialog)
	{
		QWidget* next_parent = widget->parentWidget();
		while (next_parent)
		{
			if (stop_at_window_or_dialog && (widget->metaObject()->inherits(&QMainWindow::staticMetaObject) ||
												widget->metaObject()->inherits(&QDialog::staticMetaObject)))
			{
				break;
			}

			widget = next_parent;
			next_parent = widget->parentWidget();
		}

		return widget;
	}

	template <typename T>
	static void ResizeColumnsForView(T* view, const std::initializer_list<int>& widths)
	{
		QHeaderView* header;
		if constexpr (std::is_same_v<T, QTableView>)
			header = view->horizontalHeader();
		else
			header = view->header();

		const int min_column_width = header->minimumSectionSize();
		const int scrollbar_width = ((view->verticalScrollBar() && view->verticalScrollBar()->isVisible()) ||
										view->verticalScrollBarPolicy() == Qt::ScrollBarAlwaysOn) ?
                                        view->verticalScrollBar()->width() :
                                        0;
		int num_flex_items = 0;
		int total_width = 0;
		int column_index = 0;
		for (const int spec_width : widths)
		{
			if (!view->isColumnHidden(column_index))
			{
				if (spec_width < 0)
					num_flex_items++;
				else
					total_width += std::max(spec_width, min_column_width);
			}

			column_index++;
		}

		const int flex_width =
			(num_flex_items > 0) ?
                std::max((view->contentsRect().width() - total_width - scrollbar_width) / num_flex_items, 1) :
                0;

		column_index = 0;
		for (const int spec_width : widths)
		{
			if (view->isColumnHidden(column_index))
			{
				column_index++;
				continue;
			}

			const int width = spec_width < 0 ? flex_width : (std::max(spec_width, min_column_width));
			view->setColumnWidth(column_index, width);
			column_index++;
		}
	}

	void ResizeColumnsForTableView(QTableView* view, const std::initializer_list<int>& widths)
	{
		ResizeColumnsForView(view, widths);
	}

	void ResizeColumnsForTreeView(QTreeView* view, const std::initializer_list<int>& widths)
	{
		ResizeColumnsForView(view, widths);
	}

	static const std::map<int, QString> s_qt_key_names = {
		{Qt::Key_Escape, QStringLiteral("Escape")},
		{Qt::Key_Tab, QStringLiteral("Tab")},
		{Qt::Key_Backtab, QStringLiteral("Backtab")},
		{Qt::Key_Backspace, QStringLiteral("Backspace")},
		{Qt::Key_Return, QStringLiteral("Return")},
		{Qt::Key_Enter, QStringLiteral("Enter")},
		{Qt::Key_Insert, QStringLiteral("Insert")},
		{Qt::Key_Delete, QStringLiteral("Delete")},
		{Qt::Key_Pause, QStringLiteral("Pause")},
		{Qt::Key_Print, QStringLiteral("Print")},
		{Qt::Key_SysReq, QStringLiteral("SysReq")},
		{Qt::Key_Clear, QStringLiteral("Clear")},
		{Qt::Key_Home, QStringLiteral("Home")},
		{Qt::Key_End, QStringLiteral("End")},
		{Qt::Key_Left, QStringLiteral("Left")},
		{Qt::Key_Up, QStringLiteral("Up")},
		{Qt::Key_Right, QStringLiteral("Right")},
		{Qt::Key_Down, QStringLiteral("Down")},
		{Qt::Key_PageUp, QStringLiteral("PageUp")},
		{Qt::Key_PageDown, QStringLiteral("PageDown")},
		{Qt::Key_Shift, QStringLiteral("Shift")},
		{Qt::Key_Control, QStringLiteral("Control")},
		{Qt::Key_Meta, QStringLiteral("Meta")},
		{Qt::Key_Alt, QStringLiteral("Alt")},
		{Qt::Key_CapsLock, QStringLiteral("CapsLock")},
		{Qt::Key_NumLock, QStringLiteral("NumLock")},
		{Qt::Key_ScrollLock, QStringLiteral("ScrollLock")},
		{Qt::Key_F1, QStringLiteral("F1")},
		{Qt::Key_F2, QStringLiteral("F2")},
		{Qt::Key_F3, QStringLiteral("F3")},
		{Qt::Key_F4, QStringLiteral("F4")},
		{Qt::Key_F5, QStringLiteral("F5")},
		{Qt::Key_F6, QStringLiteral("F6")},
		{Qt::Key_F7, QStringLiteral("F7")},
		{Qt::Key_F8, QStringLiteral("F8")},
		{Qt::Key_F9, QStringLiteral("F9")},
		{Qt::Key_F10, QStringLiteral("F10")},
		{Qt::Key_F11, QStringLiteral("F11")},
		{Qt::Key_F12, QStringLiteral("F12")},
		{Qt::Key_F13, QStringLiteral("F13")},
		{Qt::Key_F14, QStringLiteral("F14")},
		{Qt::Key_F15, QStringLiteral("F15")},
		{Qt::Key_F16, QStringLiteral("F16")},
		{Qt::Key_F17, QStringLiteral("F17")},
		{Qt::Key_F18, QStringLiteral("F18")},
		{Qt::Key_F19, QStringLiteral("F19")},
		{Qt::Key_F20, QStringLiteral("F20")},
		{Qt::Key_F21, QStringLiteral("F21")},
		{Qt::Key_F22, QStringLiteral("F22")},
		{Qt::Key_F23, QStringLiteral("F23")},
		{Qt::Key_F24, QStringLiteral("F24")},
		{Qt::Key_F25, QStringLiteral("F25")},
		{Qt::Key_F26, QStringLiteral("F26")},
		{Qt::Key_F27, QStringLiteral("F27")},
		{Qt::Key_F28, QStringLiteral("F28")},
		{Qt::Key_F29, QStringLiteral("F29")},
		{Qt::Key_F30, QStringLiteral("F30")},
		{Qt::Key_F31, QStringLiteral("F31")},
		{Qt::Key_F32, QStringLiteral("F32")},
		{Qt::Key_F33, QStringLiteral("F33")},
		{Qt::Key_F34, QStringLiteral("F34")},
		{Qt::Key_F35, QStringLiteral("F35")},
		{Qt::Key_Super_L, QStringLiteral("Super_L")},
		{Qt::Key_Super_R, QStringLiteral("Super_R")},
		{Qt::Key_Menu, QStringLiteral("Menu")},
		{Qt::Key_Hyper_L, QStringLiteral("Hyper_L")},
		{Qt::Key_Hyper_R, QStringLiteral("Hyper_R")},
		{Qt::Key_Help, QStringLiteral("Help")},
		{Qt::Key_Direction_L, QStringLiteral("Direction_L")},
		{Qt::Key_Direction_R, QStringLiteral("Direction_R")},
		{Qt::Key_Space, QStringLiteral("Space")},
		{Qt::Key_Any, QStringLiteral("Any")},
		{Qt::Key_Exclam, QStringLiteral("Exclam")},
		{Qt::Key_QuoteDbl, QStringLiteral("QuoteDbl")},
		{Qt::Key_NumberSign, QStringLiteral("NumberSign")},
		{Qt::Key_Dollar, QStringLiteral("Dollar")},
		{Qt::Key_Percent, QStringLiteral("Percent")},
		{Qt::Key_Ampersand, QStringLiteral("Ampersand")},
		{Qt::Key_Apostrophe, QStringLiteral("Apostrophe")},
		{Qt::Key_ParenLeft, QStringLiteral("ParenLeft")},
		{Qt::Key_ParenRight, QStringLiteral("ParenRight")},
		{Qt::Key_Asterisk, QStringLiteral("Asterisk")},
		{Qt::Key_Plus, QStringLiteral("Plus")},
		{Qt::Key_Comma, QStringLiteral("Comma")},
		{Qt::Key_Minus, QStringLiteral("Minus")},
		{Qt::Key_Period, QStringLiteral("Period")},
		{Qt::Key_Slash, QStringLiteral("Slash")},
		{Qt::Key_0, QStringLiteral("0")},
		{Qt::Key_1, QStringLiteral("1")},
		{Qt::Key_2, QStringLiteral("2")},
		{Qt::Key_3, QStringLiteral("3")},
		{Qt::Key_4, QStringLiteral("4")},
		{Qt::Key_5, QStringLiteral("5")},
		{Qt::Key_6, QStringLiteral("6")},
		{Qt::Key_7, QStringLiteral("7")},
		{Qt::Key_8, QStringLiteral("8")},
		{Qt::Key_9, QStringLiteral("9")},
		{Qt::Key_Colon, QStringLiteral("Colon")},
		{Qt::Key_Semicolon, QStringLiteral("Semicolon")},
		{Qt::Key_Less, QStringLiteral("Less")},
		{Qt::Key_Equal, QStringLiteral("Equal")},
		{Qt::Key_Greater, QStringLiteral("Greater")},
		{Qt::Key_Question, QStringLiteral("Question")},
		{Qt::Key_At, QStringLiteral("At")},
		{Qt::Key_A, QStringLiteral("A")},
		{Qt::Key_B, QStringLiteral("B")},
		{Qt::Key_C, QStringLiteral("C")},
		{Qt::Key_D, QStringLiteral("D")},
		{Qt::Key_E, QStringLiteral("E")},
		{Qt::Key_F, QStringLiteral("F")},
		{Qt::Key_G, QStringLiteral("G")},
		{Qt::Key_H, QStringLiteral("H")},
		{Qt::Key_I, QStringLiteral("I")},
		{Qt::Key_J, QStringLiteral("J")},
		{Qt::Key_K, QStringLiteral("K")},
		{Qt::Key_L, QStringLiteral("L")},
		{Qt::Key_M, QStringLiteral("M")},
		{Qt::Key_N, QStringLiteral("N")},
		{Qt::Key_O, QStringLiteral("O")},
		{Qt::Key_P, QStringLiteral("P")},
		{Qt::Key_Q, QStringLiteral("Q")},
		{Qt::Key_R, QStringLiteral("R")},
		{Qt::Key_S, QStringLiteral("S")},
		{Qt::Key_T, QStringLiteral("T")},
		{Qt::Key_U, QStringLiteral("U")},
		{Qt::Key_V, QStringLiteral("V")},
		{Qt::Key_W, QStringLiteral("W")},
		{Qt::Key_X, QStringLiteral("X")},
		{Qt::Key_Y, QStringLiteral("Y")},
		{Qt::Key_Z, QStringLiteral("Z")},
		{Qt::Key_BracketLeft, QStringLiteral("BracketLeft")},
		{Qt::Key_Backslash, QStringLiteral("Backslash")},
		{Qt::Key_BracketRight, QStringLiteral("BracketRight")},
		{Qt::Key_AsciiCircum, QStringLiteral("AsciiCircum")},
		{Qt::Key_Underscore, QStringLiteral("Underscore")},
		{Qt::Key_QuoteLeft, QStringLiteral("QuoteLeft")},
		{Qt::Key_BraceLeft, QStringLiteral("BraceLeft")},
		{Qt::Key_Bar, QStringLiteral("Bar")},
		{Qt::Key_BraceRight, QStringLiteral("BraceRight")},
		{Qt::Key_AsciiTilde, QStringLiteral("AsciiTilde")},
		{Qt::Key_nobreakspace, QStringLiteral("nobreakspace")},
		{Qt::Key_exclamdown, QStringLiteral("exclamdown")},
		{Qt::Key_cent, QStringLiteral("cent")},
		{Qt::Key_sterling, QStringLiteral("sterling")},
		{Qt::Key_currency, QStringLiteral("currency")},
		{Qt::Key_yen, QStringLiteral("yen")},
		{Qt::Key_brokenbar, QStringLiteral("brokenbar")},
		{Qt::Key_section, QStringLiteral("section")},
		{Qt::Key_diaeresis, QStringLiteral("diaeresis")},
		{Qt::Key_copyright, QStringLiteral("copyright")},
		{Qt::Key_ordfeminine, QStringLiteral("ordfeminine")},
		{Qt::Key_guillemotleft, QStringLiteral("guillemotleft")},
		{Qt::Key_notsign, QStringLiteral("notsign")},
		{Qt::Key_hyphen, QStringLiteral("hyphen")},
		{Qt::Key_registered, QStringLiteral("registered")},
		{Qt::Key_macron, QStringLiteral("macron")},
		{Qt::Key_degree, QStringLiteral("degree")},
		{Qt::Key_plusminus, QStringLiteral("plusminus")},
		{Qt::Key_twosuperior, QStringLiteral("twosuperior")},
		{Qt::Key_threesuperior, QStringLiteral("threesuperior")},
		{Qt::Key_acute, QStringLiteral("acute")},
		{Qt::Key_mu, QStringLiteral("mu")},
		{Qt::Key_paragraph, QStringLiteral("paragraph")},
		{Qt::Key_periodcentered, QStringLiteral("periodcentered")},
		{Qt::Key_cedilla, QStringLiteral("cedilla")},
		{Qt::Key_onesuperior, QStringLiteral("onesuperior")},
		{Qt::Key_masculine, QStringLiteral("masculine")},
		{Qt::Key_guillemotright, QStringLiteral("guillemotright")},
		{Qt::Key_onequarter, QStringLiteral("onequarter")},
		{Qt::Key_onehalf, QStringLiteral("onehalf")},
		{Qt::Key_threequarters, QStringLiteral("threequarters")},
		{Qt::Key_questiondown, QStringLiteral("questiondown")},
		{Qt::Key_Agrave, QStringLiteral("Agrave")},
		{Qt::Key_Aacute, QStringLiteral("Aacute")},
		{Qt::Key_Acircumflex, QStringLiteral("Acircumflex")},
		{Qt::Key_Atilde, QStringLiteral("Atilde")},
		{Qt::Key_Adiaeresis, QStringLiteral("Adiaeresis")},
		{Qt::Key_Aring, QStringLiteral("Aring")},
		{Qt::Key_AE, QStringLiteral("AE")},
		{Qt::Key_Ccedilla, QStringLiteral("Ccedilla")},
		{Qt::Key_Egrave, QStringLiteral("Egrave")},
		{Qt::Key_Eacute, QStringLiteral("Eacute")},
		{Qt::Key_Ecircumflex, QStringLiteral("Ecircumflex")},
		{Qt::Key_Ediaeresis, QStringLiteral("Ediaeresis")},
		{Qt::Key_Igrave, QStringLiteral("Igrave")},
		{Qt::Key_Iacute, QStringLiteral("Iacute")},
		{Qt::Key_Icircumflex, QStringLiteral("Icircumflex")},
		{Qt::Key_Idiaeresis, QStringLiteral("Idiaeresis")},
		{Qt::Key_ETH, QStringLiteral("ETH")},
		{Qt::Key_Ntilde, QStringLiteral("Ntilde")},
		{Qt::Key_Ograve, QStringLiteral("Ograve")},
		{Qt::Key_Oacute, QStringLiteral("Oacute")},
		{Qt::Key_Ocircumflex, QStringLiteral("Ocircumflex")},
		{Qt::Key_Otilde, QStringLiteral("Otilde")},
		{Qt::Key_Odiaeresis, QStringLiteral("Odiaeresis")},
		{Qt::Key_multiply, QStringLiteral("multiply")},
		{Qt::Key_Ooblique, QStringLiteral("Ooblique")},
		{Qt::Key_Ugrave, QStringLiteral("Ugrave")},
		{Qt::Key_Uacute, QStringLiteral("Uacute")},
		{Qt::Key_Ucircumflex, QStringLiteral("Ucircumflex")},
		{Qt::Key_Udiaeresis, QStringLiteral("Udiaeresis")},
		{Qt::Key_Yacute, QStringLiteral("Yacute")},
		{Qt::Key_THORN, QStringLiteral("THORN")},
		{Qt::Key_ssharp, QStringLiteral("ssharp")},
		{Qt::Key_division, QStringLiteral("division")},
		{Qt::Key_ydiaeresis, QStringLiteral("ydiaeresis")},
		{Qt::Key_AltGr, QStringLiteral("AltGr")},
		{Qt::Key_Multi_key, QStringLiteral("Multi_key")},
		{Qt::Key_Codeinput, QStringLiteral("Codeinput")},
		{Qt::Key_SingleCandidate, QStringLiteral("SingleCandidate")},
		{Qt::Key_MultipleCandidate, QStringLiteral("MultipleCandidate")},
		{Qt::Key_PreviousCandidate, QStringLiteral("PreviousCandidate")},
		{Qt::Key_Mode_switch, QStringLiteral("Mode_switch")},
		{Qt::Key_Kanji, QStringLiteral("Kanji")},
		{Qt::Key_Muhenkan, QStringLiteral("Muhenkan")},
		{Qt::Key_Henkan, QStringLiteral("Henkan")},
		{Qt::Key_Romaji, QStringLiteral("Romaji")},
		{Qt::Key_Hiragana, QStringLiteral("Hiragana")},
		{Qt::Key_Katakana, QStringLiteral("Katakana")},
		{Qt::Key_Hiragana_Katakana, QStringLiteral("Hiragana_Katakana")},
		{Qt::Key_Zenkaku, QStringLiteral("Zenkaku")},
		{Qt::Key_Hankaku, QStringLiteral("Hankaku")},
		{Qt::Key_Zenkaku_Hankaku, QStringLiteral("Zenkaku_Hankaku")},
		{Qt::Key_Touroku, QStringLiteral("Touroku")},
		{Qt::Key_Massyo, QStringLiteral("Massyo")},
		{Qt::Key_Kana_Lock, QStringLiteral("Kana_Lock")},
		{Qt::Key_Kana_Shift, QStringLiteral("Kana_Shift")},
		{Qt::Key_Eisu_Shift, QStringLiteral("Eisu_Shift")},
		{Qt::Key_Eisu_toggle, QStringLiteral("Eisu_toggle")},
		{Qt::Key_Hangul, QStringLiteral("Hangul")},
		{Qt::Key_Hangul_Start, QStringLiteral("Hangul_Start")},
		{Qt::Key_Hangul_End, QStringLiteral("Hangul_End")},
		{Qt::Key_Hangul_Hanja, QStringLiteral("Hangul_Hanja")},
		{Qt::Key_Hangul_Jamo, QStringLiteral("Hangul_Jamo")},
		{Qt::Key_Hangul_Romaja, QStringLiteral("Hangul_Romaja")},
		{Qt::Key_Hangul_Jeonja, QStringLiteral("Hangul_Jeonja")},
		{Qt::Key_Hangul_Banja, QStringLiteral("Hangul_Banja")},
		{Qt::Key_Hangul_PreHanja, QStringLiteral("Hangul_PreHanja")},
		{Qt::Key_Hangul_PostHanja, QStringLiteral("Hangul_PostHanja")},
		{Qt::Key_Hangul_Special, QStringLiteral("Hangul_Special")},
		{Qt::Key_Dead_Grave, QStringLiteral("Dead_Grave")},
		{Qt::Key_Dead_Acute, QStringLiteral("Dead_Acute")},
		{Qt::Key_Dead_Circumflex, QStringLiteral("Dead_Circumflex")},
		{Qt::Key_Dead_Tilde, QStringLiteral("Dead_Tilde")},
		{Qt::Key_Dead_Macron, QStringLiteral("Dead_Macron")},
		{Qt::Key_Dead_Breve, QStringLiteral("Dead_Breve")},
		{Qt::Key_Dead_Abovedot, QStringLiteral("Dead_Abovedot")},
		{Qt::Key_Dead_Diaeresis, QStringLiteral("Dead_Diaeresis")},
		{Qt::Key_Dead_Abovering, QStringLiteral("Dead_Abovering")},
		{Qt::Key_Dead_Doubleacute, QStringLiteral("Dead_Doubleacute")},
		{Qt::Key_Dead_Caron, QStringLiteral("Dead_Caron")},
		{Qt::Key_Dead_Cedilla, QStringLiteral("Dead_Cedilla")},
		{Qt::Key_Dead_Ogonek, QStringLiteral("Dead_Ogonek")},
		{Qt::Key_Dead_Iota, QStringLiteral("Dead_Iota")},
		{Qt::Key_Dead_Voiced_Sound, QStringLiteral("Dead_Voiced_Sound")},
		{Qt::Key_Dead_Semivoiced_Sound, QStringLiteral("Dead_Semivoiced_Sound")},
		{Qt::Key_Dead_Belowdot, QStringLiteral("Dead_Belowdot")},
		{Qt::Key_Dead_Hook, QStringLiteral("Dead_Hook")},
		{Qt::Key_Dead_Horn, QStringLiteral("Dead_Horn")},
		{Qt::Key_Back, QStringLiteral("Back")},
		{Qt::Key_Forward, QStringLiteral("Forward")},
		{Qt::Key_Stop, QStringLiteral("Stop")},
		{Qt::Key_Refresh, QStringLiteral("Refresh")},
		{Qt::Key_VolumeDown, QStringLiteral("VolumeDown")},
		{Qt::Key_VolumeMute, QStringLiteral("VolumeMute")},
		{Qt::Key_VolumeUp, QStringLiteral("VolumeUp")},
		{Qt::Key_BassBoost, QStringLiteral("BassBoost")},
		{Qt::Key_BassUp, QStringLiteral("BassUp")},
		{Qt::Key_BassDown, QStringLiteral("BassDown")},
		{Qt::Key_TrebleUp, QStringLiteral("TrebleUp")},
		{Qt::Key_TrebleDown, QStringLiteral("TrebleDown")},
		{Qt::Key_MediaPlay, QStringLiteral("MediaPlay")},
		{Qt::Key_MediaStop, QStringLiteral("MediaStop")},
		{Qt::Key_MediaPrevious, QStringLiteral("MediaPrevious")},
		{Qt::Key_MediaNext, QStringLiteral("MediaNext")},
		{Qt::Key_MediaRecord, QStringLiteral("MediaRecord")},
		{Qt::Key_MediaPause, QStringLiteral("MediaPause")},
		{Qt::Key_MediaTogglePlayPause, QStringLiteral("MediaTogglePlayPause")},
		{Qt::Key_HomePage, QStringLiteral("HomePage")},
		{Qt::Key_Favorites, QStringLiteral("Favorites")},
		{Qt::Key_Search, QStringLiteral("Search")},
		{Qt::Key_Standby, QStringLiteral("Standby")},
		{Qt::Key_OpenUrl, QStringLiteral("OpenUrl")},
		{Qt::Key_LaunchMail, QStringLiteral("LaunchMail")},
		{Qt::Key_LaunchMedia, QStringLiteral("LaunchMedia")},
		{Qt::Key_Launch0, QStringLiteral("Launch0")},
		{Qt::Key_Launch1, QStringLiteral("Launch1")},
		{Qt::Key_Launch2, QStringLiteral("Launch2")},
		{Qt::Key_Launch3, QStringLiteral("Launch3")},
		{Qt::Key_Launch4, QStringLiteral("Launch4")},
		{Qt::Key_Launch5, QStringLiteral("Launch5")},
		{Qt::Key_Launch6, QStringLiteral("Launch6")},
		{Qt::Key_Launch7, QStringLiteral("Launch7")},
		{Qt::Key_Launch8, QStringLiteral("Launch8")},
		{Qt::Key_Launch9, QStringLiteral("Launch9")},
		{Qt::Key_LaunchA, QStringLiteral("LaunchA")},
		{Qt::Key_LaunchB, QStringLiteral("LaunchB")},
		{Qt::Key_LaunchC, QStringLiteral("LaunchC")},
		{Qt::Key_LaunchD, QStringLiteral("LaunchD")},
		{Qt::Key_LaunchE, QStringLiteral("LaunchE")},
		{Qt::Key_LaunchF, QStringLiteral("LaunchF")},
		{Qt::Key_MonBrightnessUp, QStringLiteral("MonBrightnessUp")},
		{Qt::Key_MonBrightnessDown, QStringLiteral("MonBrightnessDown")},
		{Qt::Key_KeyboardLightOnOff, QStringLiteral("KeyboardLightOnOff")},
		{Qt::Key_KeyboardBrightnessUp, QStringLiteral("KeyboardBrightnessUp")},
		{Qt::Key_KeyboardBrightnessDown, QStringLiteral("KeyboardBrightnessDown")},
		{Qt::Key_PowerOff, QStringLiteral("PowerOff")},
		{Qt::Key_WakeUp, QStringLiteral("WakeUp")},
		{Qt::Key_Eject, QStringLiteral("Eject")},
		{Qt::Key_ScreenSaver, QStringLiteral("ScreenSaver")},
		{Qt::Key_WWW, QStringLiteral("WWW")},
		{Qt::Key_Memo, QStringLiteral("Memo")},
		{Qt::Key_LightBulb, QStringLiteral("LightBulb")},
		{Qt::Key_Shop, QStringLiteral("Shop")},
		{Qt::Key_History, QStringLiteral("History")},
		{Qt::Key_AddFavorite, QStringLiteral("AddFavorite")},
		{Qt::Key_HotLinks, QStringLiteral("HotLinks")},
		{Qt::Key_BrightnessAdjust, QStringLiteral("BrightnessAdjust")},
		{Qt::Key_Finance, QStringLiteral("Finance")},
		{Qt::Key_Community, QStringLiteral("Community")},
		{Qt::Key_AudioRewind, QStringLiteral("AudioRewind")},
		{Qt::Key_BackForward, QStringLiteral("BackForward")},
		{Qt::Key_ApplicationLeft, QStringLiteral("ApplicationLeft")},
		{Qt::Key_ApplicationRight, QStringLiteral("ApplicationRight")},
		{Qt::Key_Book, QStringLiteral("Book")},
		{Qt::Key_CD, QStringLiteral("CD")},
		{Qt::Key_Calculator, QStringLiteral("Calculator")},
		{Qt::Key_ToDoList, QStringLiteral("ToDoList")},
		{Qt::Key_ClearGrab, QStringLiteral("ClearGrab")},
		{Qt::Key_Close, QStringLiteral("Close")},
		{Qt::Key_Copy, QStringLiteral("Copy")},
		{Qt::Key_Cut, QStringLiteral("Cut")},
		{Qt::Key_Display, QStringLiteral("Display")},
		{Qt::Key_DOS, QStringLiteral("DOS")},
		{Qt::Key_Documents, QStringLiteral("Documents")},
		{Qt::Key_Excel, QStringLiteral("Excel")},
		{Qt::Key_Explorer, QStringLiteral("Explorer")},
		{Qt::Key_Game, QStringLiteral("Game")},
		{Qt::Key_Go, QStringLiteral("Go")},
		{Qt::Key_iTouch, QStringLiteral("iTouch")},
		{Qt::Key_LogOff, QStringLiteral("LogOff")},
		{Qt::Key_Market, QStringLiteral("Market")},
		{Qt::Key_Meeting, QStringLiteral("Meeting")},
		{Qt::Key_MenuKB, QStringLiteral("MenuKB")},
		{Qt::Key_MenuPB, QStringLiteral("MenuPB")},
		{Qt::Key_MySites, QStringLiteral("MySites")},
		{Qt::Key_News, QStringLiteral("News")},
		{Qt::Key_OfficeHome, QStringLiteral("OfficeHome")},
		{Qt::Key_Option, QStringLiteral("Option")},
		{Qt::Key_Paste, QStringLiteral("Paste")},
		{Qt::Key_Phone, QStringLiteral("Phone")},
		{Qt::Key_Calendar, QStringLiteral("Calendar")},
		{Qt::Key_Reply, QStringLiteral("Reply")},
		{Qt::Key_Reload, QStringLiteral("Reload")},
		{Qt::Key_RotateWindows, QStringLiteral("RotateWindows")},
		{Qt::Key_RotationPB, QStringLiteral("RotationPB")},
		{Qt::Key_RotationKB, QStringLiteral("RotationKB")},
		{Qt::Key_Save, QStringLiteral("Save")},
		{Qt::Key_Send, QStringLiteral("Send")},
		{Qt::Key_Spell, QStringLiteral("Spell")},
		{Qt::Key_SplitScreen, QStringLiteral("SplitScreen")},
		{Qt::Key_Support, QStringLiteral("Support")},
		{Qt::Key_TaskPane, QStringLiteral("TaskPane")},
		{Qt::Key_Terminal, QStringLiteral("Terminal")},
		{Qt::Key_Tools, QStringLiteral("Tools")},
		{Qt::Key_Travel, QStringLiteral("Travel")},
		{Qt::Key_Video, QStringLiteral("Video")},
		{Qt::Key_Word, QStringLiteral("Word")},
		{Qt::Key_Xfer, QStringLiteral("Xfer")},
		{Qt::Key_ZoomIn, QStringLiteral("ZoomIn")},
		{Qt::Key_ZoomOut, QStringLiteral("ZoomOut")},
		{Qt::Key_Away, QStringLiteral("Away")},
		{Qt::Key_Messenger, QStringLiteral("Messenger")},
		{Qt::Key_WebCam, QStringLiteral("WebCam")},
		{Qt::Key_MailForward, QStringLiteral("MailForward")},
		{Qt::Key_Pictures, QStringLiteral("Pictures")},
		{Qt::Key_Music, QStringLiteral("Music")},
		{Qt::Key_Battery, QStringLiteral("Battery")},
		{Qt::Key_Bluetooth, QStringLiteral("Bluetooth")},
		{Qt::Key_WLAN, QStringLiteral("WLAN")},
		{Qt::Key_UWB, QStringLiteral("UWB")},
		{Qt::Key_AudioForward, QStringLiteral("AudioForward")},
		{Qt::Key_AudioRepeat, QStringLiteral("AudioRepeat")},
		{Qt::Key_AudioRandomPlay, QStringLiteral("AudioRandomPlay")},
		{Qt::Key_Subtitle, QStringLiteral("Subtitle")},
		{Qt::Key_AudioCycleTrack, QStringLiteral("AudioCycleTrack")},
		{Qt::Key_Time, QStringLiteral("Time")},
		{Qt::Key_Hibernate, QStringLiteral("Hibernate")},
		{Qt::Key_View, QStringLiteral("View")},
		{Qt::Key_TopMenu, QStringLiteral("TopMenu")},
		{Qt::Key_PowerDown, QStringLiteral("PowerDown")},
		{Qt::Key_Suspend, QStringLiteral("Suspend")},
		{Qt::Key_ContrastAdjust, QStringLiteral("ContrastAdjust")},
		{Qt::Key_LaunchG, QStringLiteral("LaunchG")},
		{Qt::Key_LaunchH, QStringLiteral("LaunchH")},
		{Qt::Key_TouchpadToggle, QStringLiteral("TouchpadToggle")},
		{Qt::Key_TouchpadOn, QStringLiteral("TouchpadOn")},
		{Qt::Key_TouchpadOff, QStringLiteral("TouchpadOff")},
		{Qt::Key_MicMute, QStringLiteral("MicMute")},
		{Qt::Key_Red, QStringLiteral("Red")},
		{Qt::Key_Green, QStringLiteral("Green")},
		{Qt::Key_Yellow, QStringLiteral("Yellow")},
		{Qt::Key_Blue, QStringLiteral("Blue")},
		{Qt::Key_ChannelUp, QStringLiteral("ChannelUp")},
		{Qt::Key_ChannelDown, QStringLiteral("ChannelDown")},
		{Qt::Key_Guide, QStringLiteral("Guide")},
		{Qt::Key_Info, QStringLiteral("Info")},
		{Qt::Key_Settings, QStringLiteral("Settings")},
		{Qt::Key_MicVolumeUp, QStringLiteral("MicVolumeUp")},
		{Qt::Key_MicVolumeDown, QStringLiteral("MicVolumeDown")},
		{Qt::Key_New, QStringLiteral("New")},
		{Qt::Key_Open, QStringLiteral("Open")},
		{Qt::Key_Find, QStringLiteral("Find")},
		{Qt::Key_Undo, QStringLiteral("Undo")},
		{Qt::Key_Redo, QStringLiteral("Redo")},
		{Qt::Key_MediaLast, QStringLiteral("MediaLast")},
		{Qt::Key_Select, QStringLiteral("Select")},
		{Qt::Key_Yes, QStringLiteral("Yes")},
		{Qt::Key_No, QStringLiteral("No")},
		{Qt::Key_Cancel, QStringLiteral("Cancel")},
		{Qt::Key_Printer, QStringLiteral("Printer")},
		{Qt::Key_Execute, QStringLiteral("Execute")},
		{Qt::Key_Sleep, QStringLiteral("Sleep")},
		{Qt::Key_Play, QStringLiteral("Play")},
		{Qt::Key_Zoom, QStringLiteral("Zoom")},
		{Qt::Key_Exit, QStringLiteral("Exit")},
		{Qt::Key_Context1, QStringLiteral("Context1")},
		{Qt::Key_Context2, QStringLiteral("Context2")},
		{Qt::Key_Context3, QStringLiteral("Context3")},
		{Qt::Key_Context4, QStringLiteral("Context4")},
		{Qt::Key_Call, QStringLiteral("Call")},
		{Qt::Key_Hangup, QStringLiteral("Hangup")},
		{Qt::Key_Flip, QStringLiteral("Flip")},
		{Qt::Key_ToggleCallHangup, QStringLiteral("ToggleCallHangup")},
		{Qt::Key_VoiceDial, QStringLiteral("VoiceDial")},
		{Qt::Key_LastNumberRedial, QStringLiteral("LastNumberRedial")},
		{Qt::Key_Camera, QStringLiteral("Camera")},
		{Qt::Key_CameraFocus, QStringLiteral("CameraFocus")}};

	struct QtKeyModifierEntry
	{
		Qt::KeyboardModifier mod;
		Qt::Key key;
		QString name;
	};

	static const std::array<QtKeyModifierEntry, 5> s_qt_key_modifiers = {
		{{Qt::ShiftModifier, Qt::Key_Shift, QStringLiteral("Shift")},
			{Qt::ControlModifier, Qt::Key_Control, QStringLiteral("Control")},
			{Qt::AltModifier, Qt::Key_Alt, QStringLiteral("Alt")},
			{Qt::MetaModifier, Qt::Key_Meta, QStringLiteral("Meta")},
			{Qt::KeypadModifier, static_cast<Qt::Key>(0), QStringLiteral("Keypad")}}};

	QString GetKeyIdentifier(int key)
	{
		const auto it = s_qt_key_names.find(key);
		return it == s_qt_key_names.end() ? QString() : it->second;
	}

	std::optional<int> GetKeyIdForIdentifier(const QString& key_identifier)
	{
		for (const auto& it : s_qt_key_names)
		{
			if (it.second == key_identifier)
				return it.first;
		}

		return std::nullopt;
	}

	QString KeyEventToString(int key, Qt::KeyboardModifiers mods)
	{
		QString key_name = GetKeyIdentifier(key);
		if (key_name.isEmpty())
			return {};

		QString ret;
		for (const QtKeyModifierEntry& mod : s_qt_key_modifiers)
		{
			if (mods & mod.mod && key != mod.key)
			{
				ret.append(mod.name);
				ret.append('+');
			}
		}

		ret.append(key_name);
		return ret;
	}

	std::optional<int> ParseKeyString(const QString& key_str)
	{
		const QStringList sections = key_str.split('+');
		std::optional<int> key_id = GetKeyIdForIdentifier(sections.last());
		if (!key_id)
			return std::nullopt;

		int ret = key_id.value();

		if (sections.size() > 1)
		{
			const int num_modifiers = sections.size() - 1;
			for (int i = 0; i < num_modifiers; i++)
			{
				for (const QtKeyModifierEntry& mod : s_qt_key_modifiers)
				{
					if (sections[i] == mod.name)
					{
						ret |= static_cast<int>(mod.mod);
						break;
					}
				}
			}
		}

		return ret;
	}

	int KeyEventToInt(int key, Qt::KeyboardModifiers mods)
	{
		int val = key;
		if (mods != 0)
		{
			for (const QtKeyModifierEntry& mod : s_qt_key_modifiers)
			{
				if (mods & mod.mod && key != mod.key)
					val |= static_cast<int>(mod.mod);
			}
		}

		return val;
	}

	void OpenURL(QWidget* parent, const QUrl& qurl)
	{
		if (!QDesktopServices::openUrl(qurl))
		{
			QMessageBox::critical(parent, QObject::tr("Failed to open URL"),
				QObject::tr("Failed to open URL.\n\nThe URL was: %1").arg(qurl.toString()));
		}
	}

	void OpenURL(QWidget* parent, const char* url)
	{
		return OpenURL(parent, QUrl::fromEncoded(QByteArray(url, static_cast<int>(std::strlen(url)))));
	}

	void OpenURL(QWidget* parent, const QString& url)
	{
		return OpenURL(parent, QUrl(url));
	}

	QString StringViewToQString(const std::string_view& str)
	{
		return str.empty() ? QString() : QString::fromUtf8(str.data(), str.size());
	}

	void SetWidgetFontForInheritedSetting(QWidget* widget, bool inherited)
	{
		if (widget->font().italic() != inherited)
		{
			QFont new_font(widget->font());
			new_font.setItalic(inherited);
			widget->setFont(new_font);
		}
	}

} // namespace QtUtils
