/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2020  PCSX2 Dev Team
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

#include <stdio.h>

#include <vector>

#include <wx/wx.h>
#include <wx/collpane.h>
#include <wx/filepicker.h>
#include <wx/slider.h>
#include <wx/spinctrl.h>
#include <wx/gbsizer.h>

#include "gui/AppCoreThread.h"

#include "gui/AppConfig.h"

#include "usb-python2-passthrough.h"

namespace usb_python2
{
	namespace passthrough
	{
		class Python2PassthroughConfigDialog : public wxDialog
		{
			wxChoice* gameListChoice;
			std::vector<wxString> gameList;

		public:
			Python2PassthroughConfigDialog(std::vector<wxString> gameList)
				: wxDialog(nullptr, wxID_ANY, _("Python 2 Configuration"), wxDefaultPosition, wxDefaultSize, wxCAPTION | wxCLOSE_BOX | wxRESIZE_BORDER)
				, gameList(gameList)
			{
				auto* padding = new wxBoxSizer(wxVERTICAL);
				auto* topBox = new wxBoxSizer(wxVERTICAL);

				auto* gameListSizer = new wxStaticBoxSizer(wxVERTICAL, this, _("Game Entry"));
				gameListSizer->AddSpacer(1);

				wxArrayString gameListArray;
				for (auto& game : gameList)
					gameListArray.Add(game);

				auto* gameListBox = new wxGridBagSizer(1, 12);
				gameListChoice = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, gameListArray);

				gameListBox->Add(gameListChoice, wxGBPosition(0, 0), wxGBSpan(1, 12), wxEXPAND);

				gameListBox->AddGrowableCol(1);
				gameListSizer->Add(gameListBox, wxSizerFlags().Expand());

				topBox->Add(gameListSizer, wxSizerFlags().Expand());
				topBox->Add(CreateStdDialogButtonSizer(wxOK | wxCANCEL), wxSizerFlags().Right());

				padding->Add(topBox, wxSizerFlags().Expand().Border(wxALL, 5));
				SetSizerAndFit(padding);

				SetMaxSize(wxSize(wxDefaultCoord, GetMinSize().y));
			}

			void Load(Python2DlgConfig& config, wxString selectedDevice)
			{
				for (size_t i = 0; i != config.devList.size(); i++)
				{
					if (config.devListGroups[i] == selectedDevice)
					{
						gameListChoice->SetSelection(i);
						break;
					}
				}
			}

			int GetSelectedGame()
			{
				return gameListChoice->GetSelection();
			}
		};

		void ConfigurePython2Passthrough(Python2DlgConfig &config)
		{
			ScopedCoreThreadPause paused_core;

			TSTDSTRING selectedDevice;
			LoadSetting(Python2Device::TypeName(), config.port, "python2", N_DEVICE, selectedDevice);

			Python2PassthroughConfigDialog dialog(config.devList);
			dialog.Load(config, selectedDevice);

			if (dialog.ShowModal() == wxID_OK)
			{
				const auto selectedIdx = dialog.GetSelectedGame();

#ifdef _WIN32
				std::wstring selectedGameEntry = config.devListGroups[selectedIdx].ToStdWstring();
				SaveSetting<std::wstring>(Python2Device::TypeName(), config.port, "python2", N_DEVICE, selectedGameEntry);
#else
				std::string selectedGameEntry = config.devListGroups[selectedIdx].ToStdString();
				SaveSetting<std::string>(Python2Device::TypeName(), config.port, "python2", N_DEVICE, selectedGameEntry);
#endif
			}

			paused_core.AllowResume();
		}
	} // namespace passthrough
} // namespace usb_python2
