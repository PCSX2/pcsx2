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

#include "usb-python2-btools.h"

namespace usb_python2
{
	namespace btools
	{
		class Python2BtoolsConfigDialog : public wxDialog
		{
			wxChoice* gameListChoice;
			std::vector<wxString> gameList;

		public:
			Python2BtoolsConfigDialog(std::vector<wxString> gameList)
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

		void ConfigurePython2Btools(Python2DlgConfig &config)
		{
			ScopedCoreThreadPause paused_core;

			TSTDSTRING selectedDevice;
			LoadSetting(Python2Device::TypeName(), config.port, "python2", N_DEVICE, selectedDevice);

			Python2BtoolsConfigDialog dialog(config.devList);
			dialog.Load(config, selectedDevice);

			if (dialog.ShowModal() == wxID_OK)
			{
				const auto selectedIdx = dialog.GetSelectedGame();

				std::wstring selectedGameEntry = config.devListGroups[selectedIdx].ToStdWstring();
				SaveSetting(Python2Device::TypeName(), config.port, "python2", N_DEVICE, selectedGameEntry);
			}

			paused_core.AllowResume();
		}
	} // namespace btools
} // namespace usb_python2
