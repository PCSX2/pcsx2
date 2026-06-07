// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include <QtWidgets/QDialog>
#include <QtCore/QHash>

#include "ui_OsdFontPickerDialog.h"

class OSDFontPickerDialog final : public QDialog
{
	Q_OBJECT

public:
	OSDFontPickerDialog(QWidget* parent, QString current_font_path, bool bold_preview);
	~OSDFontPickerDialog();

	QString selectedFontPath() const;

private Q_SLOTS:
	void onFamilySelectionChanged();
	void onSystemFamilySelectionChanged();
	void onRefreshCatalogClicked();
	void onDownloadSelectedClicked();
	void onChooseLocalClicked();
	void onUseDefaultClicked();
	void onSourceTabChanged(int index);
	void onDialogAccepted();

private:
	struct CatalogFontEntry
	{
		QString id;
		QString family;
		QString category;
		QString license;
		QString regular_url;
		QString license_url;
	};

	struct LicenseInfo
	{
		QString type;
		QString url;
		QString original;
	};

	void populateFamilyList();
	void populateSystemFamilyList();
	bool ensureCatalogLoaded(bool allow_download, bool force_download = false);
	bool loadCatalogFromFile(const QString& path);
	void setSelectedFontPath(const QString& path);
	void refreshPreview();
	void refreshSelectionValidity();
	bool validateFontFile(const QString& path);
	QString getCatalogCachePath() const;
	QString getCatalogFontCacheDir() const;
	void updateFamilyInfoLabel(const CatalogFontEntry& entry);
	QString normalizeLicenseForDisplay(const QString& license) const;
	bool ensureLicenseIndexLoaded();
	bool writeDownloadedFontLicenseNotice(const CatalogFontEntry& entry, const QString& font_path, const QString& download_url);
	QString findCachedFamilyFontPath(const QString& family) const;
	void setPreviewFontFromPath(const QString& font_path);
	void setPreviewFontFromSystemFamily(const QString& family);
	void setPreviewSampleText();
	CatalogFontEntry* getSelectedCatalogEntry();

	QString resolveSystemFontPath(const QString& family) const;
	void updateDownloadSelectedButton();

	Ui::OsdFontPickerDialog m_ui;

	QList<CatalogFontEntry> m_catalog;
	QHash<QString, LicenseInfo> m_license_index;
	QString m_selected_font_path;
	QString m_selected_system_family;
	bool m_catalog_loaded = false;
	bool m_license_index_loaded = false;
	bool m_license_index_failed = false;
	mutable QHash<QString, QString> m_system_font_path_cache;
	bool m_bold_preview = false;
	int m_preview_font_id = -1;
};
