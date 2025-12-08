// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "QtUtils.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QFileInfo>
#include <QtCore/QLocale>
#include <QtCore/QtGlobal>
#include <QtCore/QMetaObject>
#include <QtGui/QAction>
#include <QtGui/QDesktopServices>
#include <QtGui/QIcon>
#include <QtGui/QKeyEvent>
#include <QtGui/QPainter>
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

#ifdef Q_OS_LINUX
#include <QtGui/private/qtx11extras_p.h>
#endif

#include <algorithm>
#include <array>
#include <map>

#include "common/CocoaTools.h"
#include "common/Console.h"
#include "QtHost.h"

#if defined(_WIN32)
#include "common/RedtapeWindows.h"
#include <Shlobj.h>
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

	void resizeAndScalePixmap(QPixmap* pm, const int expected_width, const int expected_height, const qreal dpr, const ScalingMode scaling_mode, const float opacity)
	{
		if (!pm || pm->isNull() || pm->width() <= 0 || pm->height() <= 0)
			return;

		const int dpr_expected_width = qRound(expected_width * dpr);
		const int dpr_expected_height = qRound(expected_height * dpr);

		if (pm->width() == dpr_expected_width &&
			pm->height() == dpr_expected_height &&
			pm->devicePixelRatio() == dpr &&
			opacity == 100.0f)
		{
			switch (scaling_mode)
			{
				case ScalingMode::Fit:
				case ScalingMode::Stretch:
				case ScalingMode::Center:
					return;

				case ScalingMode::Fill:
				case ScalingMode::Tile:
				default:
					break;
			}
		}

		QPixmap final_pixmap(dpr_expected_width, dpr_expected_height);
		final_pixmap.setDevicePixelRatio(dpr);
		final_pixmap.fill(Qt::transparent);

		QPainter painter;
		painter.begin(&final_pixmap);
		painter.setRenderHint(QPainter::Antialiasing, true);
		painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
		painter.setOpacity(opacity / 100.0f);
		painter.setCompositionMode(QPainter::CompositionMode_SourceOver);

		const QRectF srcRect(0, 0, pm->width(), pm->height());
		const QRectF painterRect(0, 0, expected_width, expected_height);

		switch (scaling_mode)
		{
			case ScalingMode::Fit:
			case ScalingMode::Fill:
			{
				auto const aspect_mode = (scaling_mode == ScalingMode::Fit) ?
				                             Qt::KeepAspectRatio :
				                             Qt::KeepAspectRatioByExpanding;

				QSizeF scaledSize(pm->width(), pm->height());
				scaledSize.scale(dpr_expected_width, dpr_expected_height, aspect_mode);

				*pm = pm->scaled(
					qRound(scaledSize.width()),
					qRound(scaledSize.height()),
					Qt::IgnoreAspectRatio,
					Qt::SmoothTransformation
				);

				const QRectF scaledSrcRect(0, 0, pm->width(), pm->height());

				QSizeF logicalSize = pm->size() / dpr;
				QRectF destRect(QPointF(0, 0), logicalSize);

				destRect.moveCenter(painterRect.center());

				painter.drawPixmap(destRect, *pm, scaledSrcRect);
				break;
			}
			case ScalingMode::Stretch:
			{
				*pm = pm->scaled(
					dpr_expected_width,
					dpr_expected_height,
					Qt::IgnoreAspectRatio,
					Qt::SmoothTransformation
				);

				const QRectF scaledSrcRect(0, 0, pm->width(), pm->height());

				painter.drawPixmap(painterRect, *pm, scaledSrcRect);
				break;
			}
			case ScalingMode::Center:
			{
				const qreal pmWidth = pm->width() / dpr;
				const qreal pmHeight = pm->height() / dpr;

				QRectF destRect(0, 0, pmWidth, pmHeight);

				destRect.moveCenter(painterRect.center());

				painter.drawPixmap(destRect, *pm, srcRect);
				break;
			}
			case ScalingMode::Tile:
			{
				const qreal tileWidth = pm->width() / dpr;
				const qreal tileHeight = pm->height() / dpr;

				if (tileWidth <= 0 || tileHeight <= 0)
					break;

				QPixmap tileSource = pm->scaled(tileWidth, tileHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation);
				tileSource.setDevicePixelRatio(dpr);

				QBrush tileBrush(tileSource);
				tileBrush.setTextureImage(tileSource.toImage());

				painter.fillRect(painterRect, tileBrush);
				break;
			}
			default:
				break;
		}
		painter.end();
		*pm = std::move(final_pixmap);
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
		return QCoreApplication::translate("FileOperations", "Show in Explorer");
#elif defined(__APPLE__)
		//: macOS action to show a file in Finder
		return QCoreApplication::translate("FileOperations", "Show in Finder");
#else
		//: Linux/*NIX: Opens the system file manager to the directory containing a selected file
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

	bool IsCompositorManagerRunning()
	{
		if (qEnvironmentVariableIsSet("PCSX2_NO_COMPOSITING"))
			return false;

#ifdef Q_OS_LINUX
		if (QX11Info::isPlatformX11() && !QX11Info::isCompositingManagerRunning())
			return false;
#endif

		return true;
	}

	class IconVariableDpiFilter : QObject
	{
	public:
		explicit IconVariableDpiFilter(QLabel* lbl, const QIcon& icon, const QSize& size, QObject* parent = nullptr)
			: QObject(parent)
			, m_lbl{lbl}
			, m_icn{icon}
			, m_size{size}
		{
			lbl->installEventFilter(this);
			m_lbl->setPixmap(m_icn.pixmap(m_size, m_lbl->devicePixelRatioF()));
		}

	protected:
		bool eventFilter(QObject* object, QEvent* event) override
		{
			if (object == m_lbl && event->type() == QEvent::DevicePixelRatioChange)
				m_lbl->setPixmap(m_icn.pixmap(m_size, m_lbl->devicePixelRatioF()));
			// Don't block the event
			return false;
		}

	private:
		QLabel* m_lbl;
		QIcon m_icn;
		QSize m_size;
	};

	void SetScalableIcon(QLabel* lbl, const QIcon& icon, const QSize& size)
	{
		new IconVariableDpiFilter(lbl, icon, size, lbl);
	}

	QString GetSystemLanguageCode()
	{
		std::vector<std::pair<QString, QString>> available = QtHost::GetAvailableLanguageList();
		QString locale = QLocale::system().name();
		locale.replace('_', '-');
		for (const std::pair<QString, QString>& entry : available)
		{
			if (entry.second == locale)
				return locale;
		}
		QStringView lang = QStringView(locale);
		lang = lang.left(lang.indexOf('-'));
		for (const std::pair<QString, QString>& entry : available)
		{
			QStringView avail = QStringView(entry.second);
			avail = avail.left(avail.indexOf('-'));
			if (avail == lang)
				return entry.second;
		}
		// No matches, default to English
		return QStringLiteral("en-US");
	}

	QIcon GetFlagIconForLanguage(const QString& language_code)
	{
		QString actual_language_code = language_code;
		if (language_code == QStringLiteral("system"))
		{
			actual_language_code = GetSystemLanguageCode();
		}

		QString country_code;

		const int dash_index = actual_language_code.indexOf('-');
		if (dash_index > 0 && dash_index < actual_language_code.length() - 1)
		{
			country_code = actual_language_code.mid(dash_index + 1);
		}
		else
		{
			if (actual_language_code == QStringLiteral("en"))
				country_code = QStringLiteral("US");
			else
				return QIcon(); // No flag available
		}

		// Special cases
		if (actual_language_code == QStringLiteral("es-419"))
		{
			// Latin America (es-419) use Mexico flag as representative
			country_code = QStringLiteral("MX");
		}
		else if (actual_language_code == QStringLiteral("sr-SP"))
		{
			// Serbia (SP) is not a valid ISO code, use RS (Serbia)
			country_code = QStringLiteral("RS");
		}

		const QString flag_path = QStringLiteral("%1/icons/flags/%2.svg").arg(QtHost::GetResourcesBasePath()).arg(country_code.toLower());
		return QIcon(flag_path);
	}
} // namespace QtUtils
