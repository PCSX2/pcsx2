// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "QtUtils.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QLocale>
#include <QtCore/QtGlobal>
#include <QtCore/QMetaObject>
#include <QtCore/QStandardPaths>
#include <QtGui/QAction>
#include <QtGui/QDesktopServices>
#include <QtGui/QIcon>
#include <QtGui/QKeyEvent>
#include <QtGui/QPainter>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QFileDialog>
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

#include <fmt/format.h>

#ifdef Q_OS_LINUX
#include <QtGui/private/qtx11extras_p.h>
#endif

#include <algorithm>
#include <array>
#include <cstdlib>
#include <map>

#include "common/CocoaTools.h"
#include "common/Console.h"
#include "common/FileSystem.h"
#include "common/Path.h"
#include "common/StringUtil.h"
#include "pcsx2/Config.h"
#include "QtHost.h"

#if defined(_WIN32)
#include "common/RedtapeWindows.h"
#include "common/RedtapeWilCom.h"
#include <Shlobj.h>
#include <shobjidl.h>
#include <comdef.h>
#else
#include <sys/stat.h>
#include <cerrno>
#include <cstring>
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
		if (!pm || pm->width() <= 0 || pm->height() <= 0)
			return;

		if (dpr <= 0.0)
		{
			Console.ErrorFmt("resizeAndScalePixmap: Invalid device pixel ratio ({}) - pixmap will be null", dpr);
			*pm = QPixmap();
			return;
		}

		const int dpr_expected_width = qRound(expected_width * dpr);
		const int dpr_expected_height = qRound(expected_height * dpr);

		if (pm->width() == dpr_expected_width &&
			pm->height() == dpr_expected_height &&
			qFuzzyCompare(pm->devicePixelRatio(), dpr) &&
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
					Qt::SmoothTransformation);

				const QRectF scaledSrcRect(0, 0, pm->width(), pm->height());

				QSizeF logicalSize = pm->size() / dpr;
				QRectF destRect(QPointF(0, 0), logicalSize);

				destRect.moveCenter(painterRect.center());

				painter.drawPixmap(destRect, *pm, scaledSrcRect);
				break;
			}
			case ScalingMode::Stretch:
			{
				painter.drawPixmap(painterRect, *pm, srcRect);
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

				if (pm->devicePixelRatio() == dpr)
				{
					QBrush tileBrush(*pm);
					painter.fillRect(painterRect, tileBrush);
				}
				else
				{
					QPixmap tileSource = pm->scaled(tileWidth, tileHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation);
					tileSource.setDevicePixelRatio(dpr);
					QBrush tileBrush(tileSource);
					painter.fillRect(painterRect, tileBrush);
				}
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

	void SetWindowResizeable(QWindow* window, bool resizeable)
	{
		if (resizeable)
		{
			// Min/max numbers come from uic.
			window->setMinimumWidth(1);
			window->setMinimumHeight(1);
			window->setMaximumWidth(16777215);
			window->setMaximumHeight(16777215);
		}
		else
		{
			window->setMinimumWidth(window->width());
			window->setMinimumHeight(window->height());
			window->setMaximumWidth(window->width());
			window->setMaximumHeight(window->height());
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

	void ResizePotentiallyFixedSizeWindow(QWindow* window, int width, int height)
	{
		width = std::max(width, 1);
		height = std::max(height, 1);

		if (window->minimumHeight() == window->maximumHeight())
		{
			window->setMinimumWidth(width);
			window->setMinimumHeight(height);
			window->setMaximumWidth(width);
			window->setMaximumHeight(height);
		}

		window->resize(width, height);
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

	bool IsRunningInFlatpak()
	{
		// Checks for the existence of the `.flatpak-info` file which seems to be always present inside flatpak sandboxes.
		return FileSystem::FileExists("/.flatpak-info");
	}

	bool IsRunningInAppImage()
	{
		// The AppImage runtime sets APPIMAGE environment variable so we can check for that.
		return std::getenv("APPIMAGE") != nullptr;
	}

	void CreateShortcut(QWidget* parent, const std::string& name, const std::string& game_path,
		std::vector<std::string> passed_cli_args, const std::string& custom_args,
		const std::string& icon_path, bool is_desktop, bool prompt_for_destination)
	{
		const auto tr_msg = [](const char* str) {
			return QCoreApplication::translate("CreateShortcut", str);
		};

#if defined(_WIN32)
		if (name.empty())
		{
			Console.Error("Cannot create shortcuts without a name.");
			return;
		}

		// Sanitize filename
		const std::string clean_name = Path::SanitizeFileName(name);
		std::string clean_path = game_path.empty() ? std::string() : Path::ToNativePath(Path::RealPath(game_path));
		if (!Path::IsValidFileName(clean_name))
		{
			QMessageBox::critical(parent, tr_msg("Failed to create shortcut"), tr_msg("Filename contains illegal character."), QMessageBox::StandardButton::Ok, QMessageBox::StandardButton::Ok);
			return;
		}

		// Get path to Desktop or per-user Start Menu\Programs directory
		// https://superuser.com/questions/1489874/how-can-i-get-the-real-path-of-desktop-in-windows-explorer/1789849#1789849
		// https://learn.microsoft.com/en-us/windows/win32/api/shlobj_core/nf-shlobj_core-shgetknownfolderpath
		// https://learn.microsoft.com/en-us/windows/win32/shell/knownfolderid
		std::string link_file;
		if (wil::unique_cotaskmem_string directory; SUCCEEDED(SHGetKnownFolderPath(is_desktop ? FOLDERID_Desktop : FOLDERID_Programs, 0, NULL, &directory)))
		{
			std::string directory_utf8 = StringUtil::WideStringToUTF8String(directory.get());

			if (is_desktop)
				link_file = Path::ToNativePath(fmt::format("{}/{}.lnk", directory_utf8, clean_name));
			else
			{
				const std::string pcsx2_start_menu_dir = Path::ToNativePath(fmt::format("{}/PCSX2", directory_utf8));
				if (!FileSystem::EnsureDirectoryExists(pcsx2_start_menu_dir.c_str(), false))
				{
					QMessageBox::critical(parent, tr_msg("Failed to create shortcut"), tr_msg("Could not create start menu directory."), QMessageBox::StandardButton::Ok, QMessageBox::StandardButton::Ok);
					return;
				}

				link_file = Path::ToNativePath(fmt::format("{}/{}.lnk", pcsx2_start_menu_dir, clean_name));
			}
		}
		else
		{
			QMessageBox::critical(parent, tr_msg("Failed to create shortcut"), is_desktop ? tr_msg("'Desktop' directory not found") : tr_msg("User's 'Start Menu\\Programs' directory not found"), QMessageBox::StandardButton::Ok, QMessageBox::StandardButton::Ok);
			return;
		}

		// Check if the same shortcut already exists
		if (prompt_for_destination && FileSystem::FileExists(link_file.c_str()))
		{
			QMessageBox::critical(parent, tr_msg("Failed to create shortcut"), tr_msg("A shortcut with the same name already exists."), QMessageBox::StandardButton::Ok, QMessageBox::StandardButton::Ok);
			return;
		}

		// Shortcut CmdLine Args
		bool lossless = true;
		for (std::string& arg : passed_cli_args)
			lossless &= EscapeShortcutCommandLine(&arg);
		if (!clean_path.empty())
			lossless &= EscapeShortcutCommandLine(&clean_path);

		if (!lossless)
		{
			QMessageBox::critical(parent, tr_msg("Failed to create shortcut"), tr_msg("File path contains invalid character(s)."), QMessageBox::StandardButton::Ok, QMessageBox::StandardButton::Ok);
			return;
		}

		std::string final_args = StringUtil::JoinString(passed_cli_args.begin(), passed_cli_args.end(), " ");
		if (!custom_args.empty())
		{
			if (!final_args.empty())
				final_args += ' ';
			final_args += custom_args;
		}
		if (!clean_path.empty())
		{
			if (!final_args.empty())
				final_args += ' ';
			final_args += fmt::format("-- {}", clean_path);
		}

		Console.WriteLnFmt("Creating a shortcut '{}' with arguments '{}'", link_file, final_args);

		const auto str_error = [](HRESULT hr) -> std::string {
			_com_error err(hr);
			const TCHAR* errMsg = err.ErrorMessage();
			return fmt::format("{} [{}]", StringUtil::WideStringToUTF8String(errMsg), hr);
		};

		// Construct the shortcut
		// https://stackoverflow.com/questions/3906974/how-to-programmatically-create-a-shortcut-using-win32
		HRESULT res = CoInitialize(NULL);
		if (FAILED(res))
		{
			Console.ErrorFmt("Failed to create shortcut: CoInitialize failed ({})", str_error(res));
			QMessageBox::critical(parent, tr_msg("Failed to create shortcut"), tr_msg("CoInitialize failed (%1)").arg(QString::fromStdString(str_error(res))), QMessageBox::StandardButton::Ok, QMessageBox::StandardButton::Ok);
			return;
		}

		wil::unique_couninitialize_call co_cleanup;

		const auto report_error = [&](const QString& reason) {
			Console.ErrorFmt("Failed to create shortcut: {}", reason.toStdString());
			QMessageBox::critical(parent, tr_msg("Failed to create shortcut"), reason, QMessageBox::StandardButton::Ok, QMessageBox::StandardButton::Ok);
		};

		wil::com_ptr_nothrow<IShellLink> pShellLink = wil::CoCreateInstanceNoThrow<IShellLink>(CLSID_ShellLink);
		wil::com_ptr_nothrow<IPersistFile> pPersistFile;

		if (!pShellLink)
		{
			report_error(tr_msg("CoCreateInstance failed"));
			return;
		}

		// Set path to the executable
		const std::wstring target_file = StringUtil::UTF8StringToWideString(FileSystem::GetProgramPath());
		res = pShellLink->SetPath(target_file.c_str());
		if (FAILED(res))
		{
			report_error(tr_msg("SetPath failed (%1)").arg(QString::fromStdString(str_error(res))));
			return;
		}

		// Set the working directory
		const std::wstring working_dir = StringUtil::UTF8StringToWideString(Path::GetDirectory(FileSystem::GetProgramPath()));
		res = pShellLink->SetWorkingDirectory(working_dir.c_str());
		if (FAILED(res))
		{
			report_error(tr_msg("SetWorkingDirectory failed (%1)").arg(QString::fromStdString(str_error(res))));
			return;
		}

		// Set the description (shown as the shortcut's tooltip)
		const std::wstring description = tr_msg("PlayStation 2 Emulator").toStdWString();
		res = pShellLink->SetDescription(description.c_str());
		if (FAILED(res))
		{
			report_error(tr_msg("SetDescription failed (%1)").arg(QString::fromStdString(str_error(res))));
			return;
		}

		// Set the launch arguments
		if (!final_args.empty())
		{
			const std::wstring target_cli_args = StringUtil::UTF8StringToWideString(final_args);
			res = pShellLink->SetArguments(target_cli_args.c_str());
			if (FAILED(res))
			{
				report_error(tr_msg("SetArguments failed (%1)").arg(QString::fromStdString(str_error(res))));
				return;
			}
		}

		// Set the icon
		std::string final_icon_path;
		if (!icon_path.empty())
		{
			final_icon_path = Path::ToNativePath(icon_path);
			if (!FileSystem::FileExists(final_icon_path.c_str()))
			{
				QMessageBox::critical(parent, tr_msg("Failed to create shortcut"), tr_msg("The selected icon file does not exist."), QMessageBox::StandardButton::Ok, QMessageBox::StandardButton::Ok);
				return;
			}
		}
		else
		{
			final_icon_path = Path::ToNativePath(Path::Combine(Path::GetDirectory(FileSystem::GetProgramPath()), "resources/icons/AppIconLarge.ico"));
		}

		const std::wstring w_icon_path = StringUtil::UTF8StringToWideString(final_icon_path);
		res = pShellLink->SetIconLocation(w_icon_path.c_str(), 0);
		if (FAILED(res))
		{
			report_error(tr_msg("SetIconLocation failed (%1)").arg(QString::fromStdString(str_error(res))));
			return;
		}

		// Use the IPersistFile object to save the shell link
		res = pShellLink.query_to(&pPersistFile);
		if (FAILED(res))
		{
			report_error(tr_msg("QueryInterface failed (%1)").arg(QString::fromStdString(str_error(res))));
			return;
		}

		// Save shortcut link to disk
		const std::wstring w_link_file = StringUtil::UTF8StringToWideString(link_file);
		res = pPersistFile->Save(w_link_file.c_str(), TRUE);
		if (FAILED(res))
		{
			report_error(tr_msg("Failed to save the shortcut (%1)").arg(QString::fromStdString(str_error(res))));
			return;
		}

#else

		if (name.empty())
		{
			QMessageBox::critical(parent, tr_msg("Failed to create shortcut"), tr_msg("Cannot create a shortcut without a title."), QMessageBox::StandardButton::Ok, QMessageBox::StandardButton::Ok);
			return;
		}

		const bool is_flatpak = IsRunningInFlatpak();

		// Sanitize filename and game path
		const std::string clean_name = Path::SanitizeFileName(name);
		std::string clean_path = game_path.empty() ? std::string() : Path::Canonicalize(Path::RealPath(game_path));
		if (!Path::IsValidFileName(clean_name))
		{
			QMessageBox::critical(parent, tr_msg("Failed to create shortcut"), tr_msg("Filename contains illegal character."), QMessageBox::StandardButton::Ok, QMessageBox::StandardButton::Ok);
			return;
		}

		// Find the executable path
		std::string executable_path = FileSystem::GetPackagePath();
		if (executable_path.empty())
		{
			QMessageBox::critical(parent, tr_msg("Failed to create shortcut"), tr_msg("Executable path is empty."), QMessageBox::StandardButton::Ok, QMessageBox::StandardButton::Ok);
			return;
		}

		// Find the destination directory
		const QString link_dir = QStandardPaths::writableLocation(is_desktop ? QStandardPaths::DesktopLocation : QStandardPaths::ApplicationsLocation);
		if (link_dir.isEmpty())
		{
			QMessageBox::critical(parent, tr_msg("Failed to create shortcut"), tr_msg("Could not determine the shortcut destination directory."), QMessageBox::StandardButton::Ok, QMessageBox::StandardButton::Ok);
			return;
		}
		const std::string link_path = fmt::format("{}/{}.desktop", link_dir.toStdString(), clean_name);

		std::string icon_name;
		if (!icon_path.empty())
		{
			if (!FileSystem::FileExists(icon_path.c_str()))
			{
				QMessageBox::critical(parent, tr_msg("Failed to create shortcut"), tr_msg("The selected icon file does not exist."), QMessageBox::StandardButton::Ok, QMessageBox::StandardButton::Ok);
				return;
			}
			icon_name = icon_path;
		}
		else
		{
			if (is_flatpak) // Flatpak
			{
				executable_path = "flatpak run net.pcsx2.PCSX2";
				icon_name = "net.pcsx2.PCSX2";
			}
			else
			{
				// Copy PCSX2 icon
				icon_name = "PCSX2";
				const std::string icon_dest = fmt::format("{}/icons/hicolor/512x512/apps/", QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation).toStdString());
				const std::string icon_path_dest = fmt::format("{}/{}.png", icon_dest, icon_name);
				if (FileSystem::EnsureDirectoryExists(icon_dest.c_str(), true))
					FileSystem::CopyFilePath(Path::Combine(EmuFolders::Resources, "icons/AppIconLarge.png").c_str(), icon_path_dest.c_str(), true);
			}
		}

		// Shortcut CmdLine Args
		bool lossless = true;
		for (std::string& arg : passed_cli_args)
			lossless &= EscapeShortcutCommandLine(&arg);
		if (!is_flatpak)
			lossless &= EscapeShortcutCommandLine(&executable_path);
		if (!clean_path.empty())
			lossless &= EscapeShortcutCommandLine(&clean_path);

		if (!lossless)
		{
			QMessageBox::critical(parent, tr_msg("Failed to create shortcut"), tr_msg("File path contains invalid character(s)."), QMessageBox::StandardButton::Ok, QMessageBox::StandardButton::Ok);
			return;
		}

		std::string cmdline = StringUtil::JoinString(passed_cli_args.begin(), passed_cli_args.end(), " ");

		// Assembling the .desktop file
		std::string final_args = executable_path;
		if (!cmdline.empty())
			final_args += fmt::format(" {}", cmdline);
		if (!custom_args.empty())
			final_args += fmt::format(" {}", custom_args);
		if (!clean_path.empty())
			final_args += fmt::format(" -- {}", clean_path);
		std::string file_content =
			"[Desktop Entry]\n"
			"Version=1.0\n"
			"Type=Application\n"
			"Terminal=false\n"
			"StartupWMClass=pcsx2-qt\n"
			"Exec=" +
			final_args + "\n" +
			"Name=" +
			name + "\n" +
			"GenericName=PlayStation 2 Emulator\n"
			"Icon=" +
			icon_name + "\n" +
			"Comment=Sony PlayStation 2 Emulator\n"
			"Keywords=game;emulator;\n"
			"Categories=Game;Emulator;\n";
		std::string_view sv(file_content);

		QString final_path = QString::fromStdString(link_path);
		if (prompt_for_destination)
		{
			const QString filter(tr_msg("Desktop Shortcut Files (*.desktop)"));
			final_path = QDir::toNativeSeparators(QFileDialog::getSaveFileName(parent, tr_msg("Select Shortcut Save Destination"), final_path, filter));
			if (final_path.isEmpty())
				return;
		}
		else
		{
			const std::string link_dir(Path::GetDirectory(link_path));
			if (!link_dir.empty() && !FileSystem::EnsureDirectoryExists(link_dir.c_str(), true))
			{
				QMessageBox::critical(parent, tr_msg("Failed to create shortcut"), tr_msg("Could not create the shortcut directory."), QMessageBox::StandardButton::Ok, QMessageBox::StandardButton::Ok);
				return;
			}
		}

		// Write to .desktop file
		if (!FileSystem::WriteStringToFile(final_path.toStdString().c_str(), sv))
		{
			QMessageBox::critical(parent, tr_msg("Failed to create shortcut"), tr_msg("Failed to create .desktop file"), QMessageBox::StandardButton::Ok, QMessageBox::StandardButton::Ok);
			return;
		}

		if (chmod(final_path.toStdString().c_str(), S_IRWXU) != 0) // enables user to execute file
			Console.ErrorFmt("Failed to change file permissions for .desktop file: {} ({})", strerror(errno), errno);
#endif
	}

	bool EscapeShortcutCommandLine(std::string* arg)
	{
#ifdef _WIN32
		if (!arg->empty() && arg->find_first_of(" \t\n\v\"") == std::string::npos)
			return true;

		std::string temp;
		temp.reserve(arg->length() + 10);
		temp += '"';

		for (auto it = arg->begin();; ++it)
		{
			int backslash_count = 0;
			while (it != arg->end() && *it == '\\')
			{
				++it;
				++backslash_count;
			}

			if (it == arg->end())
			{
				temp.append(backslash_count * 2, '\\');
				break;
			}

			if (*it == '"')
			{
				temp.append(backslash_count * 2 + 1, '\\');
				temp += '"';
			}
			else
			{
				temp.append(backslash_count, '\\');
				temp += *it;
			}
		}

		temp += '"';
		*arg = std::move(temp);
		return true;
#else
		const char* carg = arg->c_str();
		const char* cend = carg + arg->size();
		const char* RESERVED_CHARS = " \t\n\\\"'\\\\><~|%&;$*?#()`"
									 "\x01\x02\x03\x04\x05\x06\x07\x08\x0b\x0c\x0d\x0e\x0f"
									 "\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f\x7f";
		const char* next = carg + std::strcspn(carg, RESERVED_CHARS);

		if (next == cend)
			return true; // No escaping needed, don't modify

		bool lossless = true;
		std::string temp = "\"";
		const char* NOT_VALID_IN_QUOTE = "%`$\"\\\n"
										 "\x01\x02\x03\x04\x05\x06\x07\x08\x0b\x0c\x0d\x0e\x0f"
										 "\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f\x7f";

		while (true)
		{
			next = carg + std::strcspn(carg, NOT_VALID_IN_QUOTE);
			temp.append(carg, next);
			carg = next;

			if (carg == cend)
				break;

			switch (*carg)
			{
				case '"':
				case '`':
					temp.push_back('\\');
					temp.push_back(*carg);
					break;
				case '\\':
					temp.append("\\\\\\\\");
					break;
				case '$':
					temp.push_back('\\');
					temp.push_back('\\');
					temp.push_back(*carg);
					break;
				case '%':
					temp.push_back('%');
					temp.push_back(*carg);
					break;
				default:
					temp.push_back(' ');
					lossless = false;
					break;
			}
			++carg;
		}

		temp.push_back('"');
		*arg = std::move(temp);
		return lossless;
#endif
	}
} // namespace QtUtils
