// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "QtUtils.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QFileInfo>
#include <QtCore/QMetaObject>
#include <QtGui/QAction>
#include <QtGui/QGuiApplication>
#include <QtGui/QDesktopServices>
#include <QtGui/QKeyEvent>
#include <QtGui/QScreen>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QInputDialog>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QSlider>
#include <QtWidgets/QStyle>
#include <QtWidgets/QTableView>
#include <QtWidgets/QTreeView>

#include <algorithm>
#include <array>
#include <map>

#include "common/CocoaTools.h"
#include "common/Console.h"

#if defined(_WIN32)
#include "common/RedtapeWindows.h"
#include <Shlobj.h>
#elif !defined(APPLE)
#include <qpa/qplatformnativeinterface.h>
#endif

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

	void ShowInFileExplorer(QWidget* parent, const QFileInfo& file)
	{
#if defined(_WIN32)
		std::wstring wstr = QDir::toNativeSeparators(file.absoluteFilePath()).toStdWString();
		bool ok = false;
		if (PIDLIST_ABSOLUTE pidl = ILCreateFromPath(wstr.c_str()))
		{
			ok = SUCCEEDED(SHOpenFolderAndSelectItems(pidl, 0, nullptr, 0));
			ILFree(pidl);
		}
#elif defined(__APPLE__)
		bool ok = CocoaTools::ShowInFinder(file.absoluteFilePath().toStdString());
#else
		bool ok = QDesktopServices::openUrl(QUrl::fromLocalFile(file.absolutePath()));
#endif
		if (!ok)
		{
			QMessageBox::critical(parent, QCoreApplication::translate("FileOperations", "Failed to show file"),
				QCoreApplication::translate("FileOperations", "Failed to show file in file explorer.\n\nThe file was: %1").arg(file.absoluteFilePath()));
		}
	}

	QString GetShowInFileExplorerMessage()
	{
#if defined(_WIN32)
		//: Windows action to show a file in Windows Explorer
		return QCoreApplication::translate("FileOperations", "Show in Folder");
#elif defined(__APPLE__)
		//: macOS action to show a file in Finder
		return QCoreApplication::translate("FileOperations", "Show in Finder");
#else
		//: Opens the system file manager to the directory containing a selected file
		return QCoreApplication::translate("FileOperations", "Open Containing Directory");
#endif
	}

	void OpenURL(QWidget* parent, const QUrl& qurl)
	{
		if (!QDesktopServices::openUrl(qurl))
		{
			QMessageBox::critical(parent, QCoreApplication::translate("FileOperations", "Failed to open URL"),
				QCoreApplication::translate("FileOperations", "Failed to open URL.\n\nThe URL was: %1").arg(qurl.toString()));
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

	QString StringViewToQString(const std::string_view str)
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

	void BindLabelToSlider(QSlider* slider, QLabel* label, float range /*= 1.0f*/)
	{
		auto update_label = [label, range](int new_value) {
			label->setText(QString::number(static_cast<int>(new_value) / range));
		};
		update_label(slider->value());
		QObject::connect(slider, &QSlider::valueChanged, label, std::move(update_label));
	}

	void SetWindowResizeable(QWidget* widget, bool resizeable)
	{
		if (QMainWindow* window = qobject_cast<QMainWindow*>(widget); window)
		{
			// update status bar grip if present
			if (QStatusBar* sb = window->statusBar(); sb)
				sb->setSizeGripEnabled(resizeable);
		}

		if ((widget->sizePolicy().horizontalPolicy() == QSizePolicy::Preferred) != resizeable)
		{
			if (resizeable)
			{
				// Min/max numbers come from uic.
				widget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
				widget->setMinimumSize(1, 1);
				widget->setMaximumSize(16777215, 16777215);
			}
			else
			{
				widget->setFixedSize(widget->size());
				widget->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
			}
		}
	}

	void ResizePotentiallyFixedSizeWindow(QWidget* widget, int width, int height)
	{
		width = std::max(width, 1);
		height = std::max(height, 1);
		if (widget->sizePolicy().horizontalPolicy() == QSizePolicy::Fixed)
			widget->setFixedSize(width, height);

		widget->resize(width, height);
	}

	qreal GetDevicePixelRatioForWidget(const QWidget* widget)
	{
		const QScreen* screen_for_ratio = widget->screen();
		if (!screen_for_ratio)
			screen_for_ratio = QGuiApplication::primaryScreen();

		return screen_for_ratio ? screen_for_ratio->devicePixelRatio() : static_cast<qreal>(1);
	}

	std::optional<WindowInfo> GetWindowInfoForWidget(QWidget* widget)
	{
		WindowInfo wi;

		// Windows and Apple are easy here since there's no display connection.
#if defined(_WIN32)
		wi.type = WindowInfo::Type::Win32;
		wi.window_handle = reinterpret_cast<void*>(widget->winId());
#elif defined(__APPLE__)
		wi.type = WindowInfo::Type::MacOS;
		wi.window_handle = reinterpret_cast<void*>(widget->winId());
#else
		QPlatformNativeInterface* pni = QGuiApplication::platformNativeInterface();
		const QString platform_name = QGuiApplication::platformName();
		if (platform_name == QStringLiteral("xcb"))
		{
			// Can't get a handle for an unmapped window in X, it doesn't like it.
			if (!widget->isVisible())
			{
				Console.WriteLn("Returning null window info for widget because it is not visible.");
				return std::nullopt;
			}

			wi.type = WindowInfo::Type::X11;
			wi.display_connection = pni->nativeResourceForWindow("display", widget->windowHandle());
			wi.window_handle = reinterpret_cast<void*>(widget->winId());
		}
		else if (platform_name == QStringLiteral("wayland"))
		{
			wi.type = WindowInfo::Type::Wayland;
			wi.display_connection = pni->nativeResourceForWindow("display", widget->windowHandle());
			wi.window_handle = pni->nativeResourceForWindow("surface", widget->windowHandle());
		}
		else
		{
			Console.WriteLn("Unknown PNI platform '%s'.", platform_name.toUtf8().constData());
			return std::nullopt;
		}
#endif

		const qreal dpr = GetDevicePixelRatioForWidget(widget);
		wi.surface_width = static_cast<u32>(static_cast<qreal>(widget->width()) * dpr);
		wi.surface_height = static_cast<u32>(static_cast<qreal>(widget->height()) * dpr);
		wi.surface_scale = static_cast<float>(dpr);
		return wi;
	}

	QString AbstractItemModelToCSV(QAbstractItemModel* model, int role, bool useQuotes)
	{
		QString csv;
		// Header
		for (int col = 0; col < model->columnCount(); col++)
		{
			// Encapsulate value in quotes so that commas don't break the column count.
			QString headerLine = model->headerData(col, Qt::Horizontal, Qt::DisplayRole).toString();
			csv += useQuotes ? QString("\"%1\"").arg(headerLine) : headerLine;
			if (col < model->columnCount() - 1)
				csv += ",";
		}

		csv += "\n";

		// Data
		for (int row = 0; row < model->rowCount(); row++)
		{
			for (int col = 0; col < model->columnCount(); col++)
			{
				// Encapsulate value in quotes so that commas don't break the column count.
				QString dataLine = model->data(model->index(row, col), role).toString();
				csv += useQuotes ? QString("\"%1\"").arg(dataLine) : dataLine;

				if (col < model->columnCount() - 1)
					csv += ",";
			}
			csv += "\n";
		}
		return csv;
	}
} // namespace QtUtils
