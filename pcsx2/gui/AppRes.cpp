/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021  PCSX2 Dev Team
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
#include "MainFrame.h"

#include <wx/zipstrm.h>
#include <wx/wfstream.h>
#include <wx/imaglist.h>
#include <memory>

#include "MSWstuff.h"

#include "gui/EmbeddedImage.h"
#include "gui/Resources/BackgroundLogo.h"
#include "gui/Resources/ButtonIcon_Camera.h"

#include "gui/Resources/ConfigIcon_Cpu.h"
#include "gui/Resources/ConfigIcon_Video.h"
#include "gui/Resources/ConfigIcon_Speedhacks.h"
#include "gui/Resources/ConfigIcon_Gamefixes.h"
#include "gui/Resources/ConfigIcon_Paths.h"
#include "gui/Resources/ConfigIcon_MemoryCard.h"

#include "gui/Resources/AppIcon16.h"
#include "gui/Resources/AppIcon32.h"
#include "gui/Resources/AppIcon64.h"

RecentIsoList::RecentIsoList(int firstIdForMenuItems_or_wxID_ANY)
{
	Menu = std::unique_ptr<wxMenu>(new wxMenu());
	Menu->Append(MenuId_IsoBrowse, _("Browse..."), _("Browse for an ISO that is not in your recent history."));
	Menu->AppendSeparator();
	Menu->AppendCheckItem(MenuId_Ask_On_Booting, _("Always ask when booting"), _("Manually select an ISO upon boot ignoring the selection from recent ISO list."));
	Menu->Check(MenuId_Ask_On_Booting, g_Conf->AskOnBoot);

	Manager = std::unique_ptr<RecentIsoManager>(new RecentIsoManager(Menu.get(), firstIdForMenuItems_or_wxID_ANY));
}

pxAppResources::pxAppResources()
{
}

pxAppResources::~pxAppResources() = default;

wxMenu& Pcsx2App::GetRecentIsoMenu()
{
	if (!m_RecentIsoList)
		m_RecentIsoList = std::unique_ptr<RecentIsoList>(new RecentIsoList(MenuId_RecentIsos_reservedStart));
	return *m_RecentIsoList->Menu;
}

RecentIsoManager& Pcsx2App::GetRecentIsoManager()
{
	if (!m_RecentIsoList)
		m_RecentIsoList = std::unique_ptr<RecentIsoList>(new RecentIsoList(MenuId_RecentIsos_reservedStart));
	return *m_RecentIsoList->Manager;
}

wxMenu& Pcsx2App::GetDriveListMenu()
{
	if (!m_DriveList)
	{
		m_DriveList = std::unique_ptr<DriveList>(new DriveList());
	}

	return *m_DriveList->Menu;
}

pxAppResources& Pcsx2App::GetResourceCache()
{
	ScopedLock lock(m_mtx_Resources);
	if (!m_Resources)
		m_Resources = std::unique_ptr<pxAppResources>(new pxAppResources());

	return *m_Resources;
}

const wxIconBundle& Pcsx2App::GetIconBundle()
{
	std::unique_ptr<wxIconBundle>& bundle(GetResourceCache().IconBundle);
	if (!bundle)
	{
		bundle = std::unique_ptr<wxIconBundle>(new wxIconBundle());
		bundle->AddIcon(EmbeddedImage<res_AppIcon32>().GetIcon());
		bundle->AddIcon(EmbeddedImage<res_AppIcon64>().GetIcon());
		bundle->AddIcon(EmbeddedImage<res_AppIcon16>().GetIcon());
	}

	return *bundle;
}

const wxBitmap& Pcsx2App::GetLogoBitmap()
{
	std::unique_ptr<wxBitmap>& logo(GetResourceCache().Bitmap_Logo);
	if (logo)
		return *logo;

	wxImage img = EmbeddedImage<res_BackgroundLogo>().Get();
	float scale = MSW_GetDPIScale(); // 1.0 for non-Windows
	logo = std::unique_ptr<wxBitmap>(new wxBitmap(img.Scale(img.GetWidth() * scale, img.GetHeight() * scale, wxIMAGE_QUALITY_HIGH)));

	return *logo;
}

const wxBitmap& Pcsx2App::GetScreenshotBitmap()
{
	std::unique_ptr<wxBitmap>& screenshot(GetResourceCache().ScreenshotBitmap);
	if (screenshot)
		return *screenshot;

	wxImage img = EmbeddedImage<res_ButtonIcon_Camera>().Get();
	float scale = MSW_GetDPIScale(); // 1.0 for non-Windows
	screenshot = std::unique_ptr<wxBitmap>(new wxBitmap(img.Scale(img.GetWidth() * scale, img.GetHeight() * scale, wxIMAGE_QUALITY_HIGH)));

	return *screenshot;
}

wxImageList& Pcsx2App::GetImgList_Config()
{
	std::unique_ptr<wxImageList>& images(GetResourceCache().ConfigImages);
	if (!images)
	{
		int image_size = MSW_GetDPIScale() * g_Conf->Listbook_ImageSize;
		images = std::unique_ptr<wxImageList>(new wxImageList(image_size, image_size));

#undef FancyLoadMacro
#define FancyLoadMacro(name) \
	{ \
		wxImage img = EmbeddedImage<res_ConfigIcon_##name>().Get(); \
		img.Rescale(image_size, image_size, wxIMAGE_QUALITY_HIGH); \
		m_Resources->ImageId.Config.name = images->Add(img); \
	}

		FancyLoadMacro(Paths);
		FancyLoadMacro(Gamefixes);
		FancyLoadMacro(Speedhacks);
		FancyLoadMacro(MemoryCard);
		FancyLoadMacro(Video);
		FancyLoadMacro(Cpu);
	}
	return *images;
}

// This stuff seems unused?
wxImageList& Pcsx2App::GetImgList_Toolbars()
{
	std::unique_ptr<wxImageList>& images(GetResourceCache().ToolbarImages);

	if (!images)
	{
		const int imgSize = g_Conf->Toolbar_ImageSize ? 64 : 32;
		images = std::unique_ptr<wxImageList>(new wxImageList(imgSize, imgSize));

#undef FancyLoadMacro
#define FancyLoadMacro(name) \
	{ \
		wxImage img = EmbeddedImage<res_ToolbarIcon_##name>(imgSize, imgSize).Get(); \
		m_Resources.ImageId.Toolbars.name = images->Add(img); \
	}
	}
	return *images;
}

const AppImageIds& Pcsx2App::GetImgId() const
{
	return m_Resources->ImageId;
}
