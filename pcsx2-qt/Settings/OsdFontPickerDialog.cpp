// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "OsdFontPickerDialog.h"
#include "QtHost.h"

#include "common/Console.h"
#include "common/FileSystem.h"
#include "common/Path.h"

#include "pcsx2/Config.h"

#include <QtCore/QtCore>
#include <QtGui/QtGui>
#include <QtWidgets/QtWidgets>

#if defined(__APPLE__)
#include <CoreText/CoreText.h>
#endif

#if defined(__linux__)
#include <fontconfig/fontconfig.h>
#endif

static constexpr const char* FONT_METADATA_INDEX_URL =
	"https://raw.githubusercontent.com/fontsource/google-font-metadata/main/data/google-fonts-v2.json";
static constexpr const char* FONT_LICENSE_INDEX_URL =
	"https://raw.githubusercontent.com/fontsource/google-font-metadata/main/data/licenses.json";
static constexpr const char* OSD_DEFAULT_FONT_RESOURCE = "fonts" FS_OSPATH_SEPARATOR_STR "RobotoMono-Medium.ttf";

static QString resolveBundledDefaultFontFamily()
{
	static const QString s_default_family = []() {
		const std::string font_path = EmuFolders::GetOverridableResourcePath(OSD_DEFAULT_FONT_RESOURCE);
		const QString qpath = QString::fromStdString(font_path);
		if (!QFileInfo::exists(qpath))
			return QStringLiteral("Roboto Mono");

		const int font_id = QFontDatabase::addApplicationFont(qpath);
		if (font_id < 0)
			return QStringLiteral("Roboto Mono");

		const QStringList families = QFontDatabase::applicationFontFamilies(font_id);
		QFontDatabase::removeApplicationFont(font_id);
		return families.empty() ? QStringLiteral("Roboto Mono") : families.front();
	}();

	return s_default_family;
}

static const QStringList& fontExtensions()
{
	static const QStringList s_extensions = {QStringLiteral("ttf"), QStringLiteral("otf"), QStringLiteral("ttc"),
		QStringLiteral("otc")};
	return s_extensions;
}

static QString normalizePathForCompare(const QString& path)
{
	if (path.isEmpty())
		return QString();

	const QFileInfo fi(path);
	const QString canonical = fi.canonicalFilePath();
	if (!canonical.isEmpty())
		return QDir::cleanPath(canonical);

	return QDir::cleanPath(fi.absoluteFilePath());
}

static bool pathsReferToSameFile(const QString& lhs, const QString& rhs)
{
	const QString left = normalizePathForCompare(lhs);
	const QString right = normalizePathForCompare(rhs);
	if (left.isEmpty() || right.isEmpty())
		return false;

#ifdef _WIN32
	return left.compare(right, Qt::CaseInsensitive) == 0;
#else
	return left == right;
#endif
}

static QString encodeFamilyForUrl(const QString& family)
{
	QString encoded = QString::fromUtf8(QUrl::toPercentEncoding(family));
	encoded.replace(QStringLiteral("%20"), QStringLiteral("+"));
	return encoded;
}

static QString buildFontSourceUrl(const QString& family)
{
	const QString encoded = encodeFamilyForUrl(family);
	return QStringLiteral("https://fonts.google.com/specimen/%1").arg(encoded);
}

OSDFontPickerDialog::OSDFontPickerDialog(QWidget* parent, QString current_font_path, bool bold_preview)
	: QDialog(parent)
	, m_selected_font_path(std::move(current_font_path))
	, m_bold_preview(bold_preview)
{
	m_ui.setupUi(this);

	setWindowTitle(tr("Select OSD Font"));
	setWindowIcon(QtHost::GetAppIcon());
	setModal(false);
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
	m_ui.splitter->setStretchFactor(0, 3);
	m_ui.splitter->setStretchFactor(1, 2);
	m_ui.preview->setAlignment(Qt::AlignLeft | Qt::AlignTop);

	connect(m_ui.buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(m_ui.buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
	connect(this, &QDialog::accepted, this, &OSDFontPickerDialog::onDialogAccepted);

	connect(m_ui.search, &QLineEdit::textChanged, this, [this]() { populateFamilyList(); });
	connect(m_ui.showDownloadedOnly, &QCheckBox::toggled, this, [this]() { populateFamilyList(); });
	connect(m_ui.familyList, &QListWidget::itemSelectionChanged, this, &OSDFontPickerDialog::onFamilySelectionChanged);
	connect(m_ui.systemSearch, &QLineEdit::textChanged, this, [this]() { populateSystemFamilyList(); });
	connect(m_ui.systemFamilyList, &QListWidget::itemSelectionChanged, this, &OSDFontPickerDialog::onSystemFamilySelectionChanged);
	connect(m_ui.sourceTabs, &QTabWidget::currentChanged, this, &OSDFontPickerDialog::onSourceTabChanged);
	connect(m_ui.refreshCatalog, &QPushButton::clicked, this, &OSDFontPickerDialog::onRefreshCatalogClicked);
	connect(m_ui.downloadSelected, &QPushButton::clicked, this, &OSDFontPickerDialog::onDownloadSelectedClicked);
	connect(m_ui.chooseLocal, &QPushButton::clicked, this, &OSDFontPickerDialog::onChooseLocalClicked);
	connect(m_ui.useDefault, &QPushButton::clicked, this, &OSDFontPickerDialog::onUseDefaultClicked);

	m_ui.downloadSelected->setEnabled(false);
	m_ui.familyList->setEnabled(false);
	m_ui.search->setEnabled(false);

	m_ui.chooseLocal->setEnabled(true);
	m_ui.useDefault->setEnabled(true);
	m_ui.catalogInfo->setText(tr("Catalog unavailable."));
	m_ui.familyInfo->setText(tr("No family selected."));

	m_ui.systemInfo->setText(tr("System font families: loading..."));

	populateSystemFamilyList();
	m_ui.sourceTabs->setCurrentIndex(0);

	setSelectedFontPath(m_selected_font_path);
}

OSDFontPickerDialog::~OSDFontPickerDialog()
{
	if (m_preview_font_id >= 0)
		QFontDatabase::removeApplicationFont(m_preview_font_id);
}

QString OSDFontPickerDialog::selectedFontPath() const
{
	return m_selected_font_path;
}

void OSDFontPickerDialog::populateFamilyList()
{
	m_ui.familyList->clear();

	const bool have_license_index = ensureLicenseIndexLoaded();

	const bool downloaded_only = m_ui.showDownloadedOnly->isChecked();
	QSet<QString> downloaded_keys;
	if (downloaded_only)
	{
		const QDir cache_dir(getCatalogFontCacheDir());
		QStringList filters;
		for (const QString& ext : fontExtensions())
			filters.push_back(QStringLiteral("*.%1").arg(ext));

		const QFileInfoList files = cache_dir.entryInfoList(filters, QDir::Files | QDir::Readable, QDir::Name);

		for (const QFileInfo& fi : files)
		{
			const QString base = fi.completeBaseName();
			const int regular_pos = base.indexOf(QStringLiteral("-Regular"), 0, Qt::CaseInsensitive);
			if (regular_pos > 0)
				downloaded_keys.insert(base.left(regular_pos));
		}
	}

	const QString filter = m_ui.search->text().trimmed();
	int visible_count = 0;
	for (int i = 0; i < m_catalog.size(); i++)
	{
		const CatalogFontEntry& entry = m_catalog.at(i);
		if (have_license_index)
		{
			const auto lic_it = m_license_index.constFind(entry.id);
			if (lic_it == m_license_index.constEnd())
				continue;
		}
		if (downloaded_only)
		{
			const QString key = QString::fromStdString(Path::SanitizeFileName(entry.family.toStdString()));
			if (!downloaded_keys.contains(key))
				continue;
		}
		if (!filter.isEmpty() && !entry.family.contains(filter, Qt::CaseInsensitive))
			continue;

		QListWidgetItem* item = new QListWidgetItem(QStringLiteral("%1 (%2)").arg(entry.family, entry.category), m_ui.familyList);
		item->setData(Qt::UserRole, i);
		visible_count++;
	}

	m_ui.catalogInfo->setText(tr("Catalog families: %1")
			.arg(visible_count));
}

void OSDFontPickerDialog::populateSystemFamilyList()
{
	m_ui.systemFamilyList->clear();

	const QString filter = m_ui.systemSearch->text().trimmed();

	const QStringList families = QFontDatabase::families();
	int visible_count = 0;
	int selected_row = -1;

	for (const QString& family : families)
	{
		if (!filter.isEmpty() && !family.contains(filter, Qt::CaseInsensitive))
			continue;

		QListWidgetItem* item = new QListWidgetItem(family, m_ui.systemFamilyList);
		if (!m_selected_system_family.isEmpty() &&
			family.compare(m_selected_system_family, Qt::CaseInsensitive) == 0)
		{
			selected_row = m_ui.systemFamilyList->row(item);
		}

		visible_count++;
	}

	if (selected_row >= 0)
		m_ui.systemFamilyList->setCurrentRow(selected_row);

	m_ui.systemInfo->setText(tr("System font families: %1").arg(visible_count));
}

void OSDFontPickerDialog::updateFamilyInfoLabel(const CatalogFontEntry& entry)
{
	const QString family = entry.family.toHtmlEscaped();
	const QString category = entry.category.toHtmlEscaped();
	const QString license_text = normalizeLicenseForDisplay(entry.license).toHtmlEscaped();
	const QString source_url = buildFontSourceUrl(entry.family).toHtmlEscaped();

	QString license_html = license_text;
	if (!entry.license_url.isEmpty())
	{
		const QString license_url = entry.license_url.toHtmlEscaped();
		license_html = QStringLiteral("<a href=\"%1\">%2</a>").arg(license_url, license_text);
	}

	QString copyright_html;
	if (ensureLicenseIndexLoaded())
	{
		const auto lic_it = m_license_index.constFind(entry.id);
		if (lic_it != m_license_index.constEnd() && !lic_it->original.trimmed().isEmpty())
			copyright_html =
				QStringLiteral("<br><br>%1").arg(lic_it->original.toHtmlEscaped());
	}

	const QString html = tr("Family: %1<br>Category: %2<br>License: %3<br>Source: <a href=\"%4\">%4</a>%5")
	                         .arg(family, category, license_html, source_url, copyright_html);

	m_ui.familyInfo->setTextFormat(Qt::RichText);
	m_ui.familyInfo->setTextInteractionFlags(Qt::TextBrowserInteraction);
	m_ui.familyInfo->setOpenExternalLinks(true);
	m_ui.familyInfo->setText(html);
}

OSDFontPickerDialog::CatalogFontEntry* OSDFontPickerDialog::getSelectedCatalogEntry()
{
	QListWidgetItem* item = m_ui.familyList->currentItem();
	if (!item)
		return nullptr;

	const int row = item->data(Qt::UserRole).toInt();
	if (row < 0 || row >= m_catalog.size())
		return nullptr;

	return &m_catalog[row];
}

void OSDFontPickerDialog::onFamilySelectionChanged()
{
	CatalogFontEntry* entry = getSelectedCatalogEntry();
	m_selected_system_family.clear();
	m_ui.systemFamilyList->clearSelection();
	if (!entry)
	{
		m_ui.familyInfo->setText(tr("No family selected."));
		refreshPreview();
		updateDownloadSelectedButton();
		return;
	}

	if (ensureLicenseIndexLoaded())
	{
		const auto lic_it = m_license_index.constFind(entry->id);
		if (lic_it != m_license_index.constEnd())
		{
			entry->license = normalizeLicenseForDisplay(lic_it->type);
			if (entry->license_url.isEmpty() && !lic_it->url.trimmed().isEmpty())
				entry->license_url = lic_it->url.trimmed();
		}
	}

	updateFamilyInfoLabel(*entry);

	const QString cached_font_path = findCachedFamilyFontPath(entry->family);
	if (!cached_font_path.isEmpty())
		setPreviewFontFromPath(cached_font_path);
	else
		refreshPreview();

	updateDownloadSelectedButton();
}

void OSDFontPickerDialog::onSystemFamilySelectionChanged()
{
	QListWidgetItem* item = m_ui.systemFamilyList->currentItem();

	if (!item)
	{
		m_selected_system_family.clear();
		refreshPreview();
		m_ui.familyInfo->setText(tr("No family selected."));
		return;
	}

	m_selected_system_family = item->text();

	m_ui.familyList->clearSelection();
	m_ui.downloadSelected->setEnabled(false);

	refreshPreview();
}

void OSDFontPickerDialog::onRefreshCatalogClicked()
{
	if (!ensureCatalogLoaded(true, true))
	{
		m_ui.status->setText(tr("Could not refresh online font index. Cached catalog remains available."));
		return;
	}

	populateFamilyList();
	m_ui.status->setText(tr("Catalog refreshed from online font index."));
}

void OSDFontPickerDialog::onDownloadSelectedClicked()
{
	CatalogFontEntry* entry = getSelectedCatalogEntry();
	if (!entry)
		return;

	if (!ensureLicenseIndexLoaded())
	{
		QMessageBox::warning(this, tr("Download Failed"),
			tr("Could not load license metadata for this font."));
		return;
	}

	const auto lic_it = m_license_index.constFind(entry->id);
	if (lic_it == m_license_index.constEnd() || lic_it->type.trimmed().isEmpty())
	{
		QMessageBox::warning(this, tr("Download Failed"),
			tr("Missing license metadata for this font entry."));
		return;
	}

	entry->license = normalizeLicenseForDisplay(lic_it->type);
	if (entry->license_url.isEmpty() && !lic_it->url.trimmed().isEmpty())
		entry->license_url = lic_it->url.trimmed();

	const QString cache_dir = getCatalogFontCacheDir();
	if (!FileSystem::DirectoryExists(cache_dir.toUtf8().constData()) &&
		!FileSystem::CreateDirectoryPath(cache_dir.toUtf8().constData(), true))
	{
		QMessageBox::critical(this, tr("Download Failed"), tr("Failed to create font cache directory."));
		return;
	}

	QString download_url = entry->regular_url;
	if (download_url.startsWith(QStringLiteral("//")))
		download_url.prepend(QStringLiteral("https:"));

	const QString ext = download_url.contains(".otf", Qt::CaseInsensitive) ? QStringLiteral("otf") : QStringLiteral("ttf");
	const QString base_name = QStringLiteral("%1-Regular")
	                              .arg(QString::fromStdString(Path::SanitizeFileName(entry->family.toStdString())));
	QString out_path = QStringLiteral("%1/%2.%3").arg(cache_dir, base_name, ext);
	for (int i = 1; QFileInfo::exists(out_path); i++)
		out_path = QStringLiteral("%1/%2-%3.%4").arg(cache_dir, base_name).arg(i).arg(ext);

	std::vector<u8> font_data;
	if (!QtHost::DownloadFile(this, tr("Downloading Font"), download_url.toStdString(), &font_data).value_or(false) || font_data.empty())
		return;

	QSaveFile out_file(out_path);
	if (!out_file.open(QIODevice::WriteOnly))
	{
		QMessageBox::critical(this, tr("Download Failed"),
			tr("Failed to open output file '%1': %2").arg(out_path, out_file.errorString()));
		return;
	}

	const qint64 write_size = static_cast<qint64>(font_data.size());
	if (out_file.write(reinterpret_cast<const char*>(font_data.data()), write_size) != write_size || !out_file.commit())
	{
		QMessageBox::critical(this, tr("Download Failed"),
			tr("Failed to write downloaded data to '%1': %2").arg(out_path, out_file.errorString()));
		return;
	}

	if (!validateFontFile(out_path))
	{
		FileSystem::DeleteFilePath(out_path.toUtf8().constData(), nullptr);
		QMessageBox::critical(this, tr("Invalid Font"), tr("Downloaded font file could not be loaded."));
		return;
	}

	if (!writeDownloadedFontLicenseNotice(*entry, out_path, download_url))
	{
		FileSystem::DeleteFilePath(out_path.toUtf8().constData(), nullptr);
		QMessageBox::critical(this, tr("Download Failed"),
			tr("Downloaded font license metadata could not be written."));
		return;
	}

	setSelectedFontPath(out_path);
	populateFamilyList();
	m_ui.status->setText(tr("Downloaded '%1'.").arg(entry->family));
}

void OSDFontPickerDialog::onChooseLocalClicked()
{
	const QString selected = QFileDialog::getOpenFileName(this, tr("Select OSD Font"), QString(),
		tr("Font Files (*.ttf *.ttc *.otf *.otc);;All Files (*.*)"));
	if (selected.isEmpty())
		return;

	if (!validateFontFile(selected))
	{
		QMessageBox::warning(this, tr("Invalid Font"), tr("The selected file could not be loaded as a font."));
		return;
	}

	m_ui.familyList->clearSelection();
	setSelectedFontPath(QDir::toNativeSeparators(selected));
}

void OSDFontPickerDialog::onUseDefaultClicked()
{
	m_ui.familyList->clearSelection();
	m_ui.systemFamilyList->clearSelection();
	m_selected_system_family.clear();
	setSelectedFontPath(QString());
	const QString default_family = resolveBundledDefaultFontFamily();
	m_ui.status->setText(tr("Using default font: %1.").arg(default_family));
}

void OSDFontPickerDialog::onDialogAccepted()
{
	if (!m_selected_system_family.trimmed().isEmpty())
	{
		const QString resolved_path = resolveSystemFontPath(m_selected_system_family);
		if (!resolved_path.isEmpty() && !pathsReferToSameFile(m_selected_font_path, resolved_path))
		{
			setSelectedFontPath(resolved_path);
			m_ui.status->setText(tr("Using system font: %1.").arg(m_selected_system_family));
		}
		else if (resolved_path.isEmpty())
		{
			m_ui.status->setText(
				tr("Could not locate a font file for system family '%1'. Using bundled default.").arg(m_selected_system_family));
			setSelectedFontPath(QString());
		}
	}
	else if (CatalogFontEntry* entry = getSelectedCatalogEntry())
	{
		const QString cached_font_path = findCachedFamilyFontPath(entry->family);
		if (!cached_font_path.isEmpty() && !pathsReferToSameFile(m_selected_font_path, cached_font_path))
			setSelectedFontPath(cached_font_path);
	}
}

bool OSDFontPickerDialog::ensureCatalogLoaded(bool allow_download, bool force_download)
{
	const QString cache_path = getCatalogCachePath();

	if (allow_download && (force_download || !m_catalog_loaded))
	{
		const bool previously_loaded = m_catalog_loaded;

		const QFileInfo cache_info(cache_path);
		if (!cache_info.dir().exists())
			FileSystem::CreateDirectoryPath(cache_info.dir().absolutePath().toUtf8().constData(), true);

		QtHost::DownloadFile(this, tr("Downloading Files"), FONT_METADATA_INDEX_URL, cache_path.toStdString());

		const bool loaded = loadCatalogFromFile(cache_path);
		if (loaded)
		{
			m_catalog_loaded = true;
			ensureLicenseIndexLoaded();
		}
		else if (!previously_loaded)
			m_catalog_loaded = false;
	}

	if (!m_catalog_loaded)
	{
		m_catalog_loaded = loadCatalogFromFile(cache_path);
	}

	m_ui.familyList->setEnabled(m_catalog_loaded);
	m_ui.search->setEnabled(m_catalog_loaded);
	updateDownloadSelectedButton();
	if (m_catalog_loaded)
		m_ui.catalogInfo->setText(tr("Catalog ready."));
	else
		m_ui.catalogInfo->setText(tr("Catalog unavailable."));
	return m_catalog_loaded;
}

bool OSDFontPickerDialog::loadCatalogFromFile(const QString& path)
{
	QFile file(path);
	if (!file.open(QIODevice::ReadOnly))
		return false;

	QByteArray raw = file.readAll();
	QJsonParseError err;
	const QJsonDocument doc = QJsonDocument::fromJson(raw, &err);
	if (err.error != QJsonParseError::NoError || !doc.isObject())
		return false;

	m_catalog.clear();
	const QJsonObject root = doc.object();
	if (root.isEmpty())
		return false;

	for (auto it = root.begin(); it != root.end(); ++it)
	{
		const QString id = it.key().trimmed();
		const QJsonObject obj = it.value().toObject();
		if (id.isEmpty() || obj.isEmpty())
			continue;

		const QString family = obj.value(QStringLiteral("family")).toString().trimmed();
		if (family.isEmpty())
			continue;

		const QString category = obj.value(QStringLiteral("category")).toString(QStringLiteral("unknown"));
		const QString def_subset = obj.value(QStringLiteral("defSubset")).toString().trimmed();
		const QJsonObject variants = obj.value(QStringLiteral("variants")).toObject();
		if (variants.isEmpty())
			continue;

		auto select_url_for_variant = [](const QJsonObject& subset_obj) -> QString {
			const QJsonObject url_obj = subset_obj.value(QStringLiteral("url")).toObject();
			if (url_obj.isEmpty())
				return QString();

			const QString ttf = url_obj.value(QStringLiteral("truetype")).toString().trimmed();
			if (!ttf.isEmpty())
				return ttf;

			const QString otf = url_obj.value(QStringLiteral("opentype")).toString().trimmed();
			if (!otf.isEmpty())
				return otf;

			const QString woff2 = url_obj.value(QStringLiteral("woff2")).toString().trimmed();
			if (!woff2.isEmpty())
				return woff2;

			return QString();
		};

		QString regular_url;

		const QJsonObject weight_400 = variants.value(QStringLiteral("400")).toObject();
		if (!weight_400.isEmpty())
		{
			const QJsonObject styles = weight_400.value(QStringLiteral("normal")).toObject();
			if (!styles.isEmpty())
			{
				QJsonObject subset_obj;
				if (!def_subset.isEmpty())
					subset_obj = styles.value(def_subset).toObject();
				if (subset_obj.isEmpty() && !styles.isEmpty())
					subset_obj = styles.begin().value().toObject();

				regular_url = select_url_for_variant(subset_obj);
			}
		}

		if (regular_url.isEmpty())
		{
			for (auto weight_it = variants.begin(); weight_it != variants.end() && regular_url.isEmpty(); ++weight_it)
			{
				const QJsonObject styles = weight_it.value().toObject();
				for (auto style_it = styles.begin(); style_it != styles.end() && regular_url.isEmpty(); ++style_it)
				{
					const QJsonObject subsets = style_it.value().toObject();
					for (auto subset_it = subsets.begin(); subset_it != subsets.end() && regular_url.isEmpty(); ++subset_it)
					{
						const QJsonObject subset_obj = subset_it.value().toObject();
						regular_url = select_url_for_variant(subset_obj);
					}
				}
			}
		}

		if (regular_url.isEmpty())
			continue;

		CatalogFontEntry entry;
		entry.id = id;
		entry.family = family;
		entry.category = category;
		entry.license.clear();
		entry.regular_url = regular_url;
		entry.license_url.clear();
		m_catalog.push_back(std::move(entry));
	}

	return !m_catalog.isEmpty();
}

void OSDFontPickerDialog::setSelectedFontPath(const QString& path)
{
	m_selected_font_path = path.trimmed();
	if (!m_selected_font_path.isEmpty())
	{
		m_selected_system_family.clear();
	}
	refreshPreview();
	refreshSelectionValidity();
}

void OSDFontPickerDialog::refreshPreview()
{
	if (!m_selected_system_family.trimmed().isEmpty())
	{
		m_ui.selectedPath->setText(tr("Selected font: %1 (system font)").arg(m_selected_system_family));
		setPreviewFontFromSystemFamily(m_selected_system_family);
		return;
	}

	if (m_selected_font_path.isEmpty())
	{
		m_ui.selectedPath->setText(tr("Selected font: %1 (default)").arg(resolveBundledDefaultFontFamily()));
	}
	else
	{
		const QString display_path = QDir::toNativeSeparators(m_selected_font_path);
		m_ui.selectedPath->setText(tr("Selected font file:\n%1").arg(display_path));
	}

	setPreviewFontFromPath(m_selected_font_path);
}

void OSDFontPickerDialog::setPreviewSampleText()
{
	m_ui.preview->setText(tr("Saving state to slot 1...\n"
							 "Saved state to slot 1.\n"
							 "Loaded state from slot 1.\n"
							 "Saving single frame GS dump with Zstandard compression to 'gsdump_0001.gs.zst'\n"
							 "Saved GS dump to 'gsdump_0001.gs.zst'.\n"
							 "No save state found in slot 2.\n"
							 "Target speed set to 120%."));
}

void OSDFontPickerDialog::setPreviewFontFromPath(const QString& font_path)
{
	if (m_preview_font_id >= 0)
	{
		QFontDatabase::removeApplicationFont(m_preview_font_id);
		m_preview_font_id = -1;
	}

	QFont preview_font;
	preview_font.setFamily(resolveBundledDefaultFontFamily());
	preview_font.setBold(m_bold_preview);

	if (!font_path.isEmpty())
	{
		const QFileInfo fi(font_path);
		if (fi.exists() && fi.isFile())
		{
			m_preview_font_id = QFontDatabase::addApplicationFont(font_path);
			if (m_preview_font_id >= 0)
			{
				const QStringList families = QFontDatabase::applicationFontFamilies(m_preview_font_id);
				if (!families.empty())
					preview_font.setFamily(families.front());
			}
		}
	}

	m_ui.preview->setFont(preview_font);
	setPreviewSampleText();
}

void OSDFontPickerDialog::setPreviewFontFromSystemFamily(const QString& family)
{
	QFont preview_font;
	if (!family.trimmed().isEmpty())
		preview_font.setFamily(family);
	else
		preview_font.setFamily(resolveBundledDefaultFontFamily());

	preview_font.setBold(m_bold_preview);
	m_ui.preview->setFont(preview_font);
	setPreviewSampleText();
}

void OSDFontPickerDialog::refreshSelectionValidity()
{
	bool valid = true;
	if (!m_selected_font_path.isEmpty())
		valid = validateFontFile(m_selected_font_path);

	QPushButton* ok = m_ui.buttonBox->button(QDialogButtonBox::Ok);
	if (ok)
		ok->setEnabled(valid);

	if (!valid)
		m_ui.status->setText(tr("Selected font is invalid. Choose another font or use default."));
}

bool OSDFontPickerDialog::validateFontFile(const QString& path)
{
	if (path.isEmpty())
		return true;

	const QFileInfo fi(path);
	if (!fi.exists() || !fi.isFile())
		return false;

	const int font_id = QFontDatabase::addApplicationFont(path);
	if (font_id < 0)
		return false;

	QFontDatabase::removeApplicationFont(font_id);
	return true;
}

QString OSDFontPickerDialog::getCatalogCachePath() const
{
	const QString dir = getCatalogFontCacheDir();
	return QString::fromStdString(Path::Combine(dir.toStdString(), "font-catalog.json"));
}

void OSDFontPickerDialog::onSourceTabChanged(int index)
{
	m_ui.status->clear();

	if (index == 0)
	{
		if (m_ui.systemFamilyList->count() == 0)
			populateSystemFamilyList();
		return;
	}

	if (!m_catalog_loaded)
	{
		if (ensureCatalogLoaded(true))
		{
			populateFamilyList();
			m_ui.status->setText(tr("Catalog loaded from online font index."));
		}
		else
		{
			m_ui.status->setText(
				tr("Catalog unavailable. Use 'Choose Local' to select a font file or 'Use Default' for the bundled font."));
		}
	}
}

QString OSDFontPickerDialog::getCatalogFontCacheDir() const
{
	return QString::fromStdString(Path::Combine(EmuFolders::Cache, "fonts"));
}

void OSDFontPickerDialog::updateDownloadSelectedButton()
{
	const bool enabled = m_catalog_loaded && (m_ui.familyList->currentItem() != nullptr);
	m_ui.downloadSelected->setEnabled(enabled);
}

static bool fontFileHasFamily(const QString& path, const QString& family)
{
	int font_id = QFontDatabase::addApplicationFont(path);
	if (font_id < 0)
		return false;

	const QStringList families = QFontDatabase::applicationFontFamilies(font_id);
	QFontDatabase::removeApplicationFont(font_id);

	for (const QString& f : families)
	{
		if (f.compare(family, Qt::CaseInsensitive) == 0)
			return true;
	}

	return false;
}

#if defined(_WIN32)
static QString resolveSystemFontPathViaWindowsRegistry(const QString& family)
{
	const QString trimmed = family.trimmed();
	if (trimmed.isEmpty())
		return QString();

	const QString normalized_family = trimmed.toCaseFolded();
	const QString windows_dir = qEnvironmentVariable("WINDIR");
	const QString fonts_dir = windows_dir.isEmpty() ? QStringLiteral("C:/Windows/Fonts") :
	                                                  QDir(windows_dir).filePath(QStringLiteral("Fonts"));

	QSettings reg(QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Fonts"),
		QSettings::NativeFormat);
	const QStringList keys = reg.allKeys();
	for (const QString& key : keys)
	{
		const int paren_pos = key.indexOf('(');
		const QString key_base = (paren_pos >= 0) ? key.left(paren_pos).trimmed() : key.trimmed();
		if (key_base.isEmpty())
			continue;
		if (!key_base.toCaseFolded().startsWith(normalized_family))
			continue;

		QString value = reg.value(key).toString().trimmed();
		if (value.isEmpty())
			continue;

		if (QDir::isRelativePath(value))
			value = QDir(fonts_dir).filePath(value);

		if (!QFileInfo::exists(value))
			continue;

		if (fontFileHasFamily(value, trimmed))
			return value;
	}

	return QString();
}
#endif

#if defined(__linux__)
static QString resolveSystemFontPathViaFontconfig(const QString& family)
{
	const QString trimmed = family.trimmed();
	if (trimmed.isEmpty())
		return QString();

	if (!FcInit())
		return QString();

	const QByteArray family_utf8 = trimmed.toUtf8();
	FcPattern* pattern = FcPatternCreate();
	if (!pattern)
		return QString();

	FcPatternAddString(pattern, FC_FAMILY, reinterpret_cast<const FcChar8*>(family_utf8.constData()));
	FcConfigSubstitute(nullptr, pattern, FcMatchPattern);
	FcDefaultSubstitute(pattern);

	FcResult result = FcResultNoMatch;
	FcPattern* match = FcFontMatch(nullptr, pattern, &result);
	FcPatternDestroy(pattern);
	if (!match)
		return QString();

	FcChar8* file_path = nullptr;
	QString resolved;
	if (FcPatternGetString(match, FC_FILE, 0, &file_path) == FcResultMatch && file_path)
		resolved = QString::fromUtf8(reinterpret_cast<const char*>(file_path));
	FcPatternDestroy(match);
	return resolved;
}
#endif

#if defined(__APPLE__)
static QString resolveSystemFontPathViaCoreText(const QString& family)
{
	const QString trimmed = family.trimmed();
	if (trimmed.isEmpty())
		return QString();

	const QByteArray family_utf8 = trimmed.toUtf8();
	CFStringRef cf_family =
		CFStringCreateWithCString(kCFAllocatorDefault, family_utf8.constData(), kCFStringEncodingUTF8);
	if (!cf_family)
		return QString();

	CFMutableDictionaryRef attributes = CFDictionaryCreateMutable(kCFAllocatorDefault, 1,
		&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	if (!attributes)
	{
		CFRelease(cf_family);
		return QString();
	}

	CFDictionarySetValue(attributes, kCTFontFamilyNameAttribute, cf_family);
	CTFontDescriptorRef descriptor = CTFontDescriptorCreateWithAttributes(attributes);
	CFRelease(attributes);
	CFRelease(cf_family);

	if (!descriptor)
		return QString();

	CTFontRef font = CTFontCreateWithFontDescriptor(descriptor, 0.0, nullptr);
	CFRelease(descriptor);

	if (!font)
		return QString();

	CFTypeRef url_value = CTFontCopyAttribute(font, kCTFontURLAttribute);
	CFRelease(font);

	if (!url_value)
		return QString();

	QString result;
	CFURLRef url = nullptr;
	if (CFGetTypeID(url_value) == CFURLGetTypeID())
		url = static_cast<CFURLRef>(url_value);

	if (url)
	{
		char path_buffer[PATH_MAX];
		if (CFURLGetFileSystemRepresentation(
				url, true, reinterpret_cast<UInt8*>(path_buffer), sizeof(path_buffer)))
		{
			result = QString::fromUtf8(path_buffer);
		}
	}

	CFRelease(url_value);
	return result;
}
#endif

QString OSDFontPickerDialog::resolveSystemFontPath(const QString& family) const
{
	const QString key = family.trimmed().toCaseFolded();
	if (key.isEmpty())
		return QString();

	const auto it = m_system_font_path_cache.constFind(key);
	if (it != m_system_font_path_cache.constEnd())
		return it.value();

#if defined(_WIN32)
	const QString platform_path = resolveSystemFontPathViaWindowsRegistry(family);
#elif defined(__APPLE__)
	const QString platform_path = resolveSystemFontPathViaCoreText(family);
#elif defined(__linux__)
	const QString platform_path = resolveSystemFontPathViaFontconfig(family);
#else
	const QString platform_path; // well, this should never happen...
#endif
	if (!platform_path.isEmpty())
	{
		m_system_font_path_cache.insert(key, platform_path);
		return platform_path;
	}

	QStringList font_locations = QStandardPaths::standardLocations(QStandardPaths::FontsLocation);
	QStringList filters;
	for (const QString& ext : fontExtensions())
		filters.push_back(QStringLiteral("*.%1").arg(ext));

	for (const QString& dir_path : font_locations)
	{
		QDir dir(dir_path);
		if (!dir.exists())
			continue;

		const QFileInfoList entries = dir.entryInfoList(filters, QDir::Files | QDir::Readable, QDir::Name);
		for (const QFileInfo& entry : entries)
		{
			const QString file_path = entry.absoluteFilePath();
			if (fontFileHasFamily(file_path, family))
			{
				m_system_font_path_cache.insert(key, file_path);
				return file_path;
			}
		}
	}

	Console.WarningFmt("OSDFontPickerDialog: could not resolve system font family '{}'", family.toStdString());
	return QString();
}

QString OSDFontPickerDialog::normalizeLicenseForDisplay(const QString& license) const
{
	const QString trimmed = license.trimmed();
	if (trimmed.isEmpty())
		return tr("Not provided by catalog");
	return trimmed;
}

bool OSDFontPickerDialog::ensureLicenseIndexLoaded()
{
	if (!m_license_index_loaded && !m_license_index_failed && m_license_index.isEmpty())
	{
		std::vector<u8> data;
		if (QtHost::DownloadFile(this, tr("Downloading Files"), FONT_LICENSE_INDEX_URL, &data).value_or(false) &&
			!data.empty())
		{
			QByteArray raw(reinterpret_cast<const char*>(data.data()), static_cast<qsizetype>(data.size()));
			QJsonParseError err;
			const QJsonDocument doc = QJsonDocument::fromJson(raw, &err);
			if (err.error == QJsonParseError::NoError && doc.isObject())
			{
				const QJsonObject root = doc.object();
				for (auto it_index = root.begin(); it_index != root.end(); ++it_index)
				{
					const QString entry_id = it_index.key().trimmed();
					const QJsonObject obj = it_index.value().toObject();
					if (entry_id.isEmpty() || obj.isEmpty())
						continue;

					const QJsonObject license_obj = obj.value(QStringLiteral("license")).toObject();
					if (license_obj.isEmpty())
						continue;

					LicenseInfo info;
					info.type = license_obj.value(QStringLiteral("type")).toString().trimmed();
					info.url = license_obj.value(QStringLiteral("url")).toString().trimmed();
					info.original = obj.value(QStringLiteral("original")).toString().trimmed();
					if (info.type.isEmpty())
						continue;

					m_license_index.insert(entry_id, info);
				}

				if (!m_license_index.isEmpty())
				{
					m_license_index_loaded = true;
				}
				else
				{
					m_license_index_failed = true;
				}
			}
			else
			{
				m_license_index_failed = true;
			}
		}
		else
		{
			m_license_index_failed = true;
		}
	}

	return m_license_index_loaded;
}

bool OSDFontPickerDialog::writeDownloadedFontLicenseNotice(
	const CatalogFontEntry& entry, const QString& font_path, const QString& download_url)
{
	const QString license_type = entry.license.trimmed();
	if (license_type.isEmpty())
		return false;

	const QFileInfo font_info(font_path);
	if (!font_info.exists() || !font_info.isFile())
		return false;

	const QString notice_path = font_info.dir().absoluteFilePath(
		QStringLiteral("%1.LICENSE.txt").arg(font_info.completeBaseName()));
	QSaveFile notice_file(notice_path);
	if (!notice_file.open(QIODevice::WriteOnly | QIODevice::Text))
		return false;

	QString original_text;
	const auto lic_it = m_license_index.constFind(entry.id);
	if (lic_it != m_license_index.constEnd())
		original_text = lic_it->original.trimmed();

	const QString source_url = buildFontSourceUrl(entry.family);
	const QString content = QStringLiteral(
		"Font Family: %1\n"
		"Downloaded From: %2\n"
		"Font File URL: %3\n"
		"License: %4\n"
		"License URL: %5\n"
		"\n"
		"Copyright/Original:\n"
		"%6\n")
	                            .arg(entry.family, source_url, download_url, license_type, entry.license_url, original_text);

	if (notice_file.write(content.toUtf8()) < 0)
		return false;

	return notice_file.commit();
}

QString OSDFontPickerDialog::findCachedFamilyFontPath(const QString& family) const
{
	const QString cache_dir = getCatalogFontCacheDir();
	const QDir dir(cache_dir);
	if (!dir.exists())
		return QString();

	const QString base_name = QStringLiteral("%1-Regular")
	                              .arg(QString::fromStdString(Path::SanitizeFileName(family.toStdString())));

	for (const QString& ext : fontExtensions())
	{
		const QString exact = dir.absoluteFilePath(QStringLiteral("%1.%2").arg(base_name, ext));
		if (QFileInfo::exists(exact))
			return exact;
	}

	QStringList patterns;
	for (const QString& ext : fontExtensions())
		patterns.push_back(QStringLiteral("%1-*.%2").arg(base_name, ext));

	const QFileInfoList matches = dir.entryInfoList(patterns, QDir::Files | QDir::Readable, QDir::Time);
	if (!matches.empty())
		return matches.front().absoluteFilePath();

	return QString();
}

#include "moc_OsdFontPickerDialog.cpp"
