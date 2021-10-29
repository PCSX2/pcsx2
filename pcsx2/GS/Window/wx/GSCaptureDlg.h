/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021 PCSX2 Dev Team
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

#include <wx/wx.h>

#include <ghc/filesystem.h>

#ifdef _WIN32
#include <wil/com.h>
#include <streams.h>
#endif

#pragma once

/// @brief GUI Dialog for Configuring GS Capture Settings
class GSCaptureDlg : public wxDialog
{
#ifdef _WIN32
	struct Codec
	{
		wil::com_ptr_nothrow<IMoniker> moniker;
		wil::com_ptr_nothrow<IBaseFilter> filter;
		std::wstring FriendlyName;
		std::wstring DisplayName;
	};

	std::vector<Codec> m_codecs;

	bool InitSelectedCodec();
#endif

public:
	GSCaptureDlg(wxWindow* parent, bool selectDir = false);

	ghc::filesystem::path GetFilePath()
	{
		return m_filepath;
	};
	int GetColorSpaceSelection()
	{
		return m_colorSpaceSelection;
	};
	std::pair<unsigned int, unsigned int> GetCaptureSize()
	{
		return {m_captureWidth, m_captureHeight};
	};
#ifdef _WIN32
	wil::com_ptr_nothrow<IBaseFilter> GetCodecFilter()
	{
		return m_enc;
	};
#endif

protected:
	void FileEntryChanged(wxCommandEvent& event);
	void BrowseForFile(wxCommandEvent& event);
	void ConfigureCodec(wxCommandEvent& event);
	void CodecSelected(wxCommandEvent& event);
	void ColorSpaceSelected(wxCommandEvent& event);
	void CaptureWidthChanged(wxCommandEvent& event);
	void CaptureHeightChanged(wxCommandEvent& event);
	void ConfirmButtonClicked(wxCommandEvent& event);

private:
	wxTextCtrl* m_filePathInput;
	wxButton* m_browseBtn;
	wxComboBox* m_codecInput;
	wxButton* m_codecConfigBtn;
	wxTextCtrl* m_widthInput;
	wxTextCtrl* m_heightInput;
	wxComboBox* m_colorSpaceInput;
	wxButton* m_cancelBtn;
	wxButton* m_confirmBtn;

	std::vector<std::string> m_colorSpaceOptions = {"YUY2", "RGB32"};

	unsigned int m_captureWidth;
	unsigned int m_captureHeight;
	ghc::filesystem::path m_filepath;
	bool m_selectDir;
	int m_colorSpaceSelection = 0;
#ifdef _WIN32
	wil::com_ptr_nothrow<IBaseFilter> m_enc;
#endif
	void UpdateConfigureButton();
	/// @brief Checks if the confirm button can be clicked, validates the fields
	void UpdateConfirmationButton();
};
