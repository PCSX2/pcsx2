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

#include "PrecompiledHeader.h"
#include "GS.h"
#include "GSCaptureDlg.h"
#include "GS/GSExtra.h"
#include "common/StringUtil.h"
#include <commdlg.h>

// Ideally this belongs in WIL, but CAUUID is used by a *single* COM function in WinAPI.
// That's presumably why it's omitted and is unlikely to make it to upstream WIL.
static void __stdcall CloseCAUUID(_Inout_ CAUUID* cauuid) WI_NOEXCEPT
{
	::CoTaskMemFree(cauuid->pElems);
}

using unique_cauuid = wil::unique_struct<CAUUID, decltype(&::CloseCAUUID), ::CloseCAUUID>;
using unique_olestr = wil::unique_any<LPOLESTR, decltype(&::CoTaskMemFree), ::CoTaskMemFree>;

template<typename Func>
static void EnumSysDev(const GUID& clsid, Func&& f)
{
	if (auto devEnum = wil::CoCreateInstanceNoThrow<ICreateDevEnum>(CLSID_SystemDeviceEnum))
	{
		wil::com_ptr_nothrow<IEnumMoniker> classEnum;
		if (SUCCEEDED(devEnum->CreateClassEnumerator(clsid, classEnum.put(), 0)))
		{
			wil::com_ptr_nothrow<IMoniker> moniker;
			while (classEnum->Next(1, moniker.put(), nullptr) == S_OK)
			{
				std::forward<Func>(f)(moniker.get());
			}
		}
	}
}

void GSCaptureDlg::InvalidFile()
{
	const std::wstring message = L"GS couldn't open file for capturing: " + m_filename + L".\nCapture aborted.";
	MessageBox(GetActiveWindow(), message.c_str(), L"GS System Message", MB_OK | MB_SETFOREGROUND);
}

GSCaptureDlg::GSCaptureDlg()
	: GSDialog(IDD_CAPTURE)
{
	m_width = theApp.GetConfigI("CaptureWidth");
	m_height = theApp.GetConfigI("CaptureHeight");
	m_filename = StringUtil::UTF8StringToWideString(theApp.GetConfigS("CaptureFileName"));
}

int GSCaptureDlg::GetSelCodec(Codec& c)
{
	INT_PTR data = 0;

	if (ComboBoxGetSelData(IDC_CODECS, data))
	{
		if (data == 0)
			return 2;

		c = *(Codec*)data;

		if (!c.filter)
		{
			c.moniker->BindToObject(NULL, NULL, IID_PPV_ARGS(c.filter.put()));

			if (!c.filter)
				return 0;
		}

		return 1;
	}

	return 0;
}

void GSCaptureDlg::UpdateConfigureButton()
{
	Codec c;
	bool enable = false;

	if (GetSelCodec(c) != 1)
	{
		EnableWindow(GetDlgItem(m_hWnd, IDC_CONFIGURE), false);
		return;
	}

	if (auto pSPP = c.filter.try_query<ISpecifyPropertyPages>())
	{
		unique_cauuid caGUID;
		enable = SUCCEEDED(pSPP->GetPages(&caGUID));
	}
	else if (auto pAMVfWCD = c.filter.try_query<IAMVfwCompressDialogs>())
	{
		enable = pAMVfWCD->ShowDialog(VfwCompressDialog_QueryConfig, nullptr) == S_OK;
	}
	EnableWindow(GetDlgItem(m_hWnd, IDC_CONFIGURE), enable);
}

void GSCaptureDlg::OnInit()
{
	__super::OnInit();

	SetTextAsInt(IDC_WIDTH, m_width);
	SetTextAsInt(IDC_HEIGHT, m_height);
	SetText(IDC_FILENAME, m_filename.c_str());

	m_codecs.clear();

	const std::wstring selected = StringUtil::UTF8StringToWideString(theApp.GetConfigS("CaptureVideoCodecDisplayName"));

	ComboBoxAppend(IDC_CODECS, "Uncompressed", 0, true);
	ComboBoxAppend(IDC_COLORSPACE, "YUY2", 0, true);
	ComboBoxAppend(IDC_COLORSPACE, "RGB32", 1, false);

	CoInitialize(0); // this is obviously wrong here, each thread should call this on start, and where is CoUninitalize?

	EnumSysDev(CLSID_VideoCompressorCategory, [&](IMoniker* moniker)
	{
		Codec c;

		c.moniker = moniker;

		unique_olestr str;
		if (FAILED(moniker->GetDisplayName(NULL, NULL, str.put())))
			return;

		std::wstring prefix;
		if      (wcsstr(str.get(), L"@device:dmo:")) prefix = L"(DMO) ";
		else if (wcsstr(str.get(), L"@device:sw:"))  prefix = L"(DS) ";
		else if (wcsstr(str.get(), L"@device:cm:"))  prefix = L"(VfW) ";

		
		c.DisplayName = str.get();

		wil::com_ptr_nothrow<IPropertyBag> pPB;
		if (FAILED(moniker->BindToStorage(0, 0, IID_PPV_ARGS(pPB.put()))))
			return;

		wil::unique_variant var;
		if (FAILED(pPB->Read(L"FriendlyName", &var, nullptr)))
			return;

		c.FriendlyName = prefix + var.bstrVal;

		m_codecs.push_back(c);

		ComboBoxAppend(IDC_CODECS, c.FriendlyName.c_str(), (LPARAM)&m_codecs.back(), c.DisplayName == selected);
	});
	UpdateConfigureButton();
}

bool GSCaptureDlg::OnCommand(HWND hWnd, UINT id, UINT code)
{
	switch (id)
	{
		case IDC_FILENAME:
		{
			EnableWindow(GetDlgItem(m_hWnd, IDOK), GetText(IDC_FILENAME).length() != 0);
			return false;
		}
		case IDC_BROWSE:
		{
			if (code == BN_CLICKED)
			{
				wchar_t buff[MAX_PATH] = {0};

				OPENFILENAME ofn;
				memset(&ofn, 0, sizeof(ofn));

				ofn.lStructSize = sizeof(ofn);
				ofn.hwndOwner = m_hWnd;
				ofn.lpstrFile = buff;
				ofn.nMaxFile = std::size(buff);
				ofn.lpstrFilter = L"Avi files (*.avi)\0*.avi\0";
				ofn.Flags = OFN_EXPLORER | OFN_ENABLESIZING | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;

				wcscpy(ofn.lpstrFile, m_filename.c_str());
				if (GetSaveFileName(&ofn))
				{
					m_filename = ofn.lpstrFile;
					SetText(IDC_FILENAME, m_filename.c_str());
				}

				return true;
			}
			break;
		}
		case IDC_CONFIGURE:
		{
			if (code == BN_CLICKED)
			{
				Codec c;
				if (GetSelCodec(c) == 1)
				{
					if (auto pSPP = c.filter.try_query<ISpecifyPropertyPages>())
					{
						unique_cauuid caGUID;
						if (SUCCEEDED(pSPP->GetPages(&caGUID)))
						{
							auto lpUnk = pSPP.try_query<IUnknown>();
							OleCreatePropertyFrame(m_hWnd, 0, 0, c.FriendlyName.c_str(), 1, lpUnk.addressof(), caGUID.cElems, caGUID.pElems, 0, 0, NULL);
						}
					}
					else if (auto pAMVfWCD = c.filter.try_query<IAMVfwCompressDialogs>())
					{
						if (pAMVfWCD->ShowDialog(VfwCompressDialog_QueryConfig, NULL) == S_OK)
							pAMVfWCD->ShowDialog(VfwCompressDialog_Config, m_hWnd);
					}
				}
				return true;
			}
			break;
		}
		case IDC_CODECS:
		{
			UpdateConfigureButton();
			break;
		}
		case IDOK:
		{
			m_width = GetTextAsInt(IDC_WIDTH);
			m_height = GetTextAsInt(IDC_HEIGHT);
			m_filename = GetText(IDC_FILENAME);
			ComboBoxGetSelData(IDC_COLORSPACE, m_colorspace);

			Codec c;
			int ris = GetSelCodec(c);
			if (ris == 0)
				return false;

			m_enc = c.filter;

			theApp.SetConfig("CaptureWidth", m_width);
			theApp.SetConfig("CaptureHeight", m_height);
			theApp.SetConfig("CaptureFileName", StringUtil::WideStringToUTF8String(m_filename).c_str());

			if (ris != 2)
				theApp.SetConfig("CaptureVideoCodecDisplayName", StringUtil::WideStringToUTF8String(c.DisplayName).c_str());
			else
				theApp.SetConfig("CaptureVideoCodecDisplayName", "");
			break;
		}
		default:
			break;
	}
	return __super::OnCommand(hWnd, id, code);
}
