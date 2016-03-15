/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2010  PCSX2 Dev Team
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
#include "AppGameDatabase.h"

#include <wx/zipstrm.h>
#include <wx/wfstream.h>
#include <wx/imaglist.h>
#include <memory>

#include "MSWstuff.h"

#include "Utilities/EmbeddedImage.h"
#include "Resources/BackgroundLogo.h"
#include "Resources/ButtonIcon_Camera.h"

#include "Resources/ConfigIcon_Cpu.h"
#include "Resources/ConfigIcon_Video.h"
#include "Resources/ConfigIcon_Speedhacks.h"
#include "Resources/ConfigIcon_Gamefixes.h"
#include "Resources/ConfigIcon_Paths.h"
#include "Resources/ConfigIcon_Plugins.h"
#include "Resources/ConfigIcon_MemoryCard.h"
#include "Resources/ConfigIcon_Appearance.h"

#include "Resources/AppIcon16.h"
#include "Resources/AppIcon32.h"
#include "Resources/AppIcon64.h"

const wxImage& LoadImageAny(
	wxImage& dest, bool useTheme, wxFileName& base, const wxChar* filename, IEmbeddedImage& onFail )
{
	if (useTheme && base.DirExists())
	{
		wxFileName pathname(base.GetFullPath(), filename);

		const wxChar* extensions[3] = {L"png", L"jpg", L"bmp"};
		for (size_t i = 0; i < sizeof(extensions)/sizeof(extensions[0]); ++i)
		{
			pathname.SetExt(extensions[i]);
			if (pathname.FileExists() && dest.LoadFile(pathname.GetFullPath()))
				return dest;
		}
	}

	return dest = onFail.Get();
}

RecentIsoList::RecentIsoList(int firstIdForMenuItems_or_wxID_ANY)
{
	Menu = std::unique_ptr<wxMenu>(new wxMenu());
	Menu->Append( MenuId_IsoBrowse, _("Browse..."), _("Browse for an Iso that is not in your recent history.") );
	Manager = std::unique_ptr<RecentIsoManager>(new RecentIsoManager( Menu.get(), firstIdForMenuItems_or_wxID_ANY ));
}

pxAppResources::pxAppResources()
{
}

pxAppResources::~pxAppResources() throw() {}

wxMenu& Pcsx2App::GetRecentIsoMenu()
{
	if (!m_RecentIsoList) m_RecentIsoList = std::unique_ptr<RecentIsoList>(new RecentIsoList( MenuId_RecentIsos_reservedStart ));
	return *m_RecentIsoList->Menu;
}

RecentIsoManager& Pcsx2App::GetRecentIsoManager()
{
	if (!m_RecentIsoList) m_RecentIsoList = std::unique_ptr<RecentIsoList>(new RecentIsoList( MenuId_RecentIsos_reservedStart ));
	return *m_RecentIsoList->Manager;
}

pxAppResources& Pcsx2App::GetResourceCache()
{
	ScopedLock lock( m_mtx_Resources );
	if( !m_Resources )
		m_Resources = std::unique_ptr<pxAppResources>(new pxAppResources());

	return *m_Resources;
}

const wxIconBundle& Pcsx2App::GetIconBundle()
{
	std::unique_ptr<wxIconBundle>& bundle( GetResourceCache().IconBundle );
	if( !bundle )
	{
		bundle = std::unique_ptr<wxIconBundle>(new wxIconBundle());
		bundle->AddIcon( EmbeddedImage<res_AppIcon32>().GetIcon() );
		bundle->AddIcon( EmbeddedImage<res_AppIcon64>().GetIcon() );
		bundle->AddIcon( EmbeddedImage<res_AppIcon16>().GetIcon() );
	}

	return *bundle;
}

const wxBitmap& Pcsx2App::GetLogoBitmap()
{
	std::unique_ptr <wxBitmap>& logo(GetResourceCache().Bitmap_Logo);
	if( logo ) return *logo;

	wxFileName themeDirectory;
	bool useTheme = (g_Conf->DeskTheme != L"default");

	if( useTheme )
	{
		themeDirectory.Assign(wxFileName(PathDefs::GetThemes().ToString()).GetFullPath(), g_Conf->DeskTheme);
#if 0
		wxFileName zipped(themeDirectory);

		zipped.SetExt( L"zip" );
		if( zipped.FileExists() )
		{
			// loading theme from zipfile.
			//wxFileInputStream stream( zipped.ToString() )
			//wxZipInputStream zstream( stream );

			Console.Error( "Loading themes from zipfile is not supported yet.\nFalling back on default theme." );
		}
#endif
	}

	wxImage img;
	EmbeddedImage<res_BackgroundLogo> temp;	// because gcc can't allow non-const temporaries.
	LoadImageAny(img, useTheme, themeDirectory, L"BackgroundLogo", temp);
	float scale = MSW_GetDPIScale(); // 1.0 for non-Windows
	logo = std::unique_ptr<wxBitmap>(new wxBitmap(img.Scale(img.GetWidth() * scale, img.GetHeight() * scale, wxIMAGE_QUALITY_HIGH)));

	return *logo;
}

const wxBitmap& Pcsx2App::GetScreenshotBitmap()
{
	std::unique_ptr<wxBitmap>& screenshot(GetResourceCache().ScreenshotBitmap);
	if (screenshot) return *screenshot;

	wxFileName themeDirectory;
	bool useTheme = (g_Conf->DeskTheme != L"default");

	if (useTheme)
	{
		themeDirectory.Assign(wxFileName(PathDefs::GetThemes().ToString()).GetFullPath(), g_Conf->DeskTheme);
	}

	wxImage img;
	EmbeddedImage<res_ButtonIcon_Camera> temp;	// because gcc can't allow non-const temporaries.
	LoadImageAny(img, useTheme, themeDirectory, L"ButtonIcon_Camera", temp);
	float scale = MSW_GetDPIScale(); // 1.0 for non-Windows
	screenshot = std::unique_ptr<wxBitmap>(new wxBitmap(img.Scale(img.GetWidth() * scale, img.GetHeight() * scale, wxIMAGE_QUALITY_HIGH)));

	return *screenshot;
}

wxImageList& Pcsx2App::GetImgList_Config()
{
	std::unique_ptr<wxImageList>& images( GetResourceCache().ConfigImages );
	if( !images )
	{
		int image_size = MSW_GetDPIScale() * g_Conf->Listbook_ImageSize;
		images = std::unique_ptr<wxImageList>(new wxImageList(image_size, image_size));
		wxFileName themeDirectory;
		bool useTheme = (g_Conf->DeskTheme != L"default");

		if( useTheme )
		{
			themeDirectory.Assign(wxFileName(PathDefs::GetThemes().ToString()).GetFullPath(), g_Conf->DeskTheme);
		}

		wxImage img;

		// GCC Specific: wxT() macro is required when using string token pasting.  For some
		// reason L generates syntax errors. >_<
		// TODO: This can be fixed with something like
		// #define L_STR(x) L_STR2(x)
		// #define L_STR2(x) L ## x
		// but it's probably best to do it everywhere at once. wxWidgets
		// recommends not to use it since v2.9.0.

		#undef  FancyLoadMacro
		#define FancyLoadMacro( name ) \
		{ \
			EmbeddedImage<res_ConfigIcon_##name> temp; \
			LoadImageAny(img, useTheme, themeDirectory, L"ConfigIcon_" wxT(#name), temp); \
			img.Rescale(image_size, image_size, wxIMAGE_QUALITY_HIGH); \
			m_Resources->ImageId.Config.name = images->Add(img); \
		}

		FancyLoadMacro( Paths );
		FancyLoadMacro( Plugins );
		FancyLoadMacro( Gamefixes );
		FancyLoadMacro( Speedhacks );
		FancyLoadMacro( MemoryCard );
		FancyLoadMacro( Video );
		FancyLoadMacro( Cpu );
		FancyLoadMacro( Appearance );
	}
	return *images;
}

// This stuff seems unused?
wxImageList& Pcsx2App::GetImgList_Toolbars()
{
	std::unique_ptr<wxImageList>& images( GetResourceCache().ToolbarImages );

	if( !images )
	{
		const int imgSize = g_Conf->Toolbar_ImageSize ? 64 : 32;
		images = std::unique_ptr<wxImageList>(new wxImageList(imgSize, imgSize));
		wxFileName mess;
		bool useTheme = (g_Conf->DeskTheme != L"default");

		if( useTheme )
		{
			wxDirName theme( PathDefs::GetThemes() + g_Conf->DeskTheme );
			mess = theme.ToString();
		}

		wxImage img;
		#undef  FancyLoadMacro
		#define FancyLoadMacro( name ) \
		{ \
			EmbeddedImage<res_ToolbarIcon_##name> temp( imgSize, imgSize ); \
			m_Resources.ImageId.Toolbars.name = images->Add( LoadImageAny( img, useTheme, mess, L"ToolbarIcon" wxT(#name), temp ) ); \
		}

	}
	return *images;
}

const AppImageIds& Pcsx2App::GetImgId() const
{
	return m_Resources->ImageId;
}
