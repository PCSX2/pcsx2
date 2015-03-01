/*  LilyPad - Pad plugin for PS2 Emulator
 *  Copyright (C) 2002-2014  PCSX2 Dev Team/ChickenLiver
 *
 *  File imported from SPU2-X (and tranformed to object)
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the
 *  terms of the GNU Lesser General Public License as published by the Free
 *  Software Found- ation, either version 3 of the License, or (at your option)
 *  any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY
 *  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 *  FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 *  details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with PCSX2.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "Global.h"
#include <wx/fileconf.h>

extern void CfgSetSettingsDir(const char* dir);

class CfgHelper {
	wxFileConfig* m_config;
	static wxString m_path;

	void setIni(const wchar_t* Section);

	public:
	CfgHelper();
	~CfgHelper();

	void		WriteBool(const wchar_t* Section, const wchar_t* Name, bool Value);
	void		WriteInt(const wchar_t* Section, const wchar_t* Name, int Value);
	void		WriteFloat(const wchar_t* Section, const wchar_t* Name, float Value);
	void		WriteStr(const wchar_t* Section, const wchar_t* Name, const wxString& Data);

	bool		ReadBool(const wchar_t *Section,const wchar_t* Name, bool Default = false);
	int			ReadStr(const wchar_t* Section, const wchar_t* Name, wxString& Data, const wchar_t* Default = 0);
	int			ReadStr(const wchar_t* Section, const wchar_t* Name, wchar_t* Data, const wchar_t* Default = 0);
	int			ReadInt(const wchar_t* Section, const wchar_t* Name,int Default = 0);
	float		ReadFloat(const wchar_t* Section, const wchar_t* Name, float Default = 0.0f);

	static void SetSettingsDir(const char* dir);

};
