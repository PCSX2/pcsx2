#include "PrecompiledHeader.h"

#include "MemoryTypes.h"// use"g_FrameCount"
#include "Counters.h"	// use"g_FrameCount"
#include "App.h"
#include "GSFrame.h"

#include "AppSaveStates.h"

#include "TAS/MovieControle.h"
#include "TAS/KeyMovie.h"

#include "LuaManager.h"
#include "LuaEngine.h"

#include "DebugTools/DebugInterface.h"


extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}


//=============================================
// emu_frameadvance
//=============================================
static int emu_frameadvance(lua_State *L)
{
	LuaEngine *pLua = g_Lua.getLuaEnginPtr(L);
	if (pLua == NULL)return 0;
	if (pLua->getState() != LuaEngine::RUNNING)return 0;

	g_MovieControle.FrameAdvance();
	pLua->setState(LuaEngine::RESUME);
	return lua_yield(L, 0);
}


//=============================================
// emu
//=============================================
static int emu_pause(lua_State *L)
{
	g_MovieControle.Pause();
	return 0;
}
static int emu_unpause(lua_State *L)
{
	g_MovieControle.UnPause();
	return 0;
}
static int emu_getframecount(lua_State *L)
{
	lua_pushinteger(L, g_FrameCount);
	return 1;
}
static int emu_registerbefore(lua_State *L)
{
	if (lua_iscfunction(L, 1)) {
		luaL_error(L, "Invalid input function.");
	}
	LuaEngine *pLua = g_Lua.getLuaEnginPtr(L);
	if (pLua == NULL)return 0;

	pLua->unRegistryBefore();
	pLua->registryBefore(luaL_ref(L, LUA_REGISTRYINDEX));
	return 0;
}

static int emu_registerafter(lua_State *L)
{
	if (lua_iscfunction(L, 1)) {
		luaL_error(L, "Invalid input function.");
	}
	LuaEngine *pLua = g_Lua.getLuaEnginPtr(L);
	if (pLua == NULL)return 0;

	pLua->unRegistryAfter();
	pLua->registryAfter(luaL_ref(L, LUA_REGISTRYINDEX));
	return 0;
}
static int emu_registerexit(lua_State *L)
{
	if (lua_iscfunction(L, 1)) {
		luaL_error(L, "Invalid input function.");
	}
	LuaEngine *pLua = g_Lua.getLuaEnginPtr(L);
	if (pLua == NULL)return 0;
	
	pLua->unRegistryExit();
	pLua->registryExit(luaL_ref(L, LUA_REGISTRYINDEX));
	return 0;
}


//=============================================
// memory
//=============================================
static int memory_readbyte(lua_State *L)
{
	wxString cpu = luaL_checkstring(L, 2);
	DebugInterface &mem = r3000Debug;
	if (cpu == "r5900") {
		mem = r5900Debug;
	}
	lua_pushinteger(L, mem.read8(luaL_checkinteger(L, 1)));
	return 1;
}
static int memory_readbytesigned(lua_State *L)
{
	wxString cpu = luaL_checkstring(L, 2);
	DebugInterface &mem = r3000Debug;
	if (cpu == "r5900") {
		mem = r5900Debug;
	}
	unsigned long address = luaL_checkinteger(L, 1);
	lua_pushinteger(L, (signed)mem.read8(address));
	return 1;
}
static int memory_readword(lua_State *L)
{
	wxString cpu = luaL_checkstring(L, 2);
	DebugInterface &mem = r3000Debug;
	if (cpu == "r5900") {
		mem = r5900Debug;
	}
	unsigned long address = luaL_checkinteger(L, 1);
	lua_pushinteger(L, mem.read16(address));
	return 1;
}
static int memory_readwordsigned(lua_State *L)
{
	wxString cpu = luaL_checkstring(L, 2);
	DebugInterface &mem = r3000Debug;
	if (cpu == "r5900") {
		mem = r5900Debug;
	}
	unsigned long address = luaL_checkinteger(L, 1);
	lua_pushinteger(L, (signed)mem.read16(address));
	return 1;
}
static int memory_readdword(lua_State *L)
{
	wxString cpu = luaL_checkstring(L, 2);
	DebugInterface &mem = r3000Debug;
	if (cpu == "r5900") {
		mem = r5900Debug;
	}
	unsigned long address = luaL_checkinteger(L, 1);
	lua_pushinteger(L, mem.read32(address));
	return 1;
}
static int memory_readdwordsigned(lua_State *L)
{
	wxString cpu = luaL_checkstring(L, 2);
	DebugInterface &mem = r3000Debug;
	if (cpu == "r5900") {
		mem = r5900Debug;
	}
	unsigned long address = luaL_checkinteger(L, 1);
	lua_pushinteger(L, (signed)mem.read32(address));
	return 1;
}
static int memory_writebyte(lua_State *L)
{
	wxString cpu = luaL_checkstring(L, 3);
	DebugInterface &mem = r3000Debug;
	if (cpu == "r5900") {
		mem = r5900Debug;
	}
	mem.write8(luaL_checkinteger(L, 1), luaL_checkinteger(L, 2));
	return 0;
}
static int memory_writedword(lua_State *L)
{
	wxString cpu = luaL_checkstring(L, 3);
	DebugInterface &mem = r3000Debug;
	if (cpu == "r5900") {
		mem = r5900Debug;
	}
	mem.write32(luaL_checkinteger(L, 1), luaL_checkinteger(L, 2));
	return 0;
}



//=============================================
// joypad
//=============================================
static int joypad_get(lua_State *L)
{
	// Reads the joypads as inputted by the user
	int which = luaL_checkinteger(L, 1);

	if (which < 0 || 2 < which )
	{
		luaL_error(L, "Invalid input port (valid range 0-1, specified %d)", which);
	}
	lua_newtable(L);
	const PadData & pad = g_Lua.getNowFramePadData();
	auto normalKeys = pad.getNormalKeys(which);
	for (auto it = normalKeys.begin(); it != normalKeys.end(); ++it)
	{
		lua_pushstring(L, it->first);
		lua_pushboolean(L, it->second);
		lua_settable(L, -3);
	}
	auto analogKeys = pad.getAnalogKeys(which);
	for (auto it = analogKeys.begin(); it != analogKeys.end(); ++it)
	{
		lua_pushstring(L, it->first);
		lua_pushinteger(L, it->second);
		lua_settable(L, -3);
		//lua_pushinteger(L, it->second);
		//lua_setfield(L, -2, it->first);
	}
	return 1;
}
static int joypad_set(lua_State *L)
{
	// Reads the joypads as inputted by the user
	int which = luaL_checkinteger(L, 1);
	if (which < 0 || 2 <= which)
	{
		luaL_error(L, "Invalid input port (valid range 0-1, specified %d)", which);
	}

	// And the table of buttons.
	luaL_checktype(L, 2, LUA_TTABLE);

	PadData & pad = g_Lua.getNowFramePadData();
	auto normalKeys = pad.getNormalKeys(which);
	for (auto it = normalKeys.begin(); it != normalKeys.end(); ++it)
	{
		lua_getfield(L, 2, it->first.c_str() );
		if (!lua_isnil(L, -1))
		{
			normalKeys.at(it->first) = (lua_toboolean(L, -1) != 0);
		}
		lua_pop(L, 1);
	}
	pad.setNormalKeys(which,normalKeys);
	auto analogKeys = pad.getAnalogKeys(which);
	for (auto it = analogKeys.begin(); it != analogKeys.end(); ++it)
	{
		lua_getfield(L, 2, it->first.c_str());
		if (!lua_isnil(L, -1))
		{
			analogKeys.at(it->first) = lua_tointeger(L, -1);
		}
		lua_pop(L, 1);
	}
	pad.setAnalogKeys(which, analogKeys);
	g_Lua.setNowFramePadData(pad);
	return 0;
}

//=============================================
// savestate
//=============================================
static int savestate_save(lua_State *L)
{
	int slot = luaL_checkinteger(L, 1);
	if (slot < 0 || 9 < slot)
	{
		luaL_error(L, "Invalid input slot (valid range 0-9, specified %d)", slot);
	}
	States_SetCurrentSlot(slot);
	States_FreezeCurrentSlot();
	return 0;
}
static int savestate_load(lua_State *L)
{
	int slot = luaL_checkinteger(L, 1);
	if (slot < 0 || 9 < slot)
	{
		luaL_error(L, "Invalid input slot (valid range 0-9, specified %d)", slot);
	}
	States_SetCurrentSlot(slot);
	States_DefrostCurrentSlot();
	return 0;
}
//=============================================
// movie
//=============================================
static int movie_getmode(lua_State *L)
{
	if (g_KeyMovie.getModeState() == KeyMovie::RECORD) {
		lua_pushstring(L, "record");
		return 1;
	}
	else if (g_KeyMovie.getModeState() == KeyMovie::REPLAY) {
		lua_pushstring(L, "playback");
		return 1;
	}
	lua_pushnil(L);
	return 1;
}
static int movie_getlength(lua_State *L)
{
	if (g_KeyMovie.getModeState() == KeyMovie::NONE) {
		lua_pushinteger(L, 0);
		return 1;
	}
	lua_pushinteger(L, g_KeyMovieData.getMaxFrame());
	return 1;
}

static int movie_getauthor(lua_State *L)
{
	if (g_KeyMovie.getModeState() == KeyMovie::NONE) {
		lua_pushnil(L);
		return 1;
	}
	lua_pushstring(L, g_KeyMovieHeader.author);
	return 1;
}
static int movie_getcdrom(lua_State *L)
{
	if (g_KeyMovie.getModeState() == KeyMovie::NONE) {
		lua_pushnil(L);
		return 1;
	}
	lua_pushstring(L, g_KeyMovieHeader.cdrom);
	return 1;
}
static int movie_getfilename(lua_State *L)
{
	if (g_KeyMovie.getModeState() == KeyMovie::NONE) {
		lua_pushnil(L);
		return 1;
	}
	lua_pushstring(L, g_KeyMovieData.getFilename());
	return 1;
}
static int movie_rerecordcount(lua_State *L)
{
	if (g_KeyMovie.getModeState() == KeyMovie::NONE) {
		lua_pushinteger(L, 0);
		return 1;
	}
	lua_pushinteger(L, g_KeyMovieData.getUndoCount());
	return 1;
}
static int movie_stop(lua_State *L)
{
	g_KeyMovie.Stop();
	return 0;
}

//=============================================
// gui
//=============================================
// Constructs a wxColor from the stack, using base as the first value. The alpha component is read only if alpha is true
static wxColor buildColorFromInt(lua_State *L, int base, bool alpha = false)
{
	int r = luaL_checkinteger(L, base), g = luaL_checknumber(L, base+1), b = luaL_checknumber(L, base+2);

	if (r < 0 || r > 255)
		luaL_error(L, "Invalid red color range");
	if (g < 0 || g > 255)
		luaL_error(L, "Invalid green color range");
	if (b < 0 || b > 255)
		luaL_error(L, "Invalid blue color range");

	int alphaValue = 255;
	if (alpha) {
		alphaValue = luaL_checkinteger(L, base+3);
		if (alphaValue < 0 || alphaValue > 255)
			luaL_error(L, "Invalid alpha color range");
	}

	return wxColor(r, g, b, alphaValue);
}

// pair: color built and last argument read
static std::pair<wxColor, int> buildColor(lua_State *L, int base, wxColor default = *wxBLACK)
{
	if (lua_isnone(L, base))
		return std::make_pair(default, base-1);
	else if (lua_isinteger(L, base) && lua_isinteger(L, base + 1) && lua_isinteger(L, base + 2)) {
		if (lua_isinteger(L, base + 3))
			return std::make_pair(buildColorFromInt(L, base, true), base + 3);
		else
			return std::make_pair(buildColorFromInt(L, base), base + 2);
	}
	else if (!lua_isinteger(L, base) && lua_isstring(L, base)) {
		return std::make_pair(wxColor(luaL_checkstring(L, base)), base);
	}
	else
		luaL_error(L, "Invalid color description");
}

static int gui_text(lua_State *L)
{
	auto *panel = wxGetApp().GetGsFrame().GetGui();

	int x = luaL_checkinteger(L, 1), y = luaL_checkinteger(L, 2);
	wxString message = luaL_checkstring(L, 3);
	auto foreground = buildColor(L, 4);
	auto background = buildColor(L, foreground.second + 1, wxTransparentColor);

	int startFont = background.second + 1;
	int charSize = 12, family = wxFontFamily::wxFONTFAMILY_DEFAULT;
	int style = wxFontStyle::wxFONTSTYLE_NORMAL, weight = wxFontWeight::wxFONTWEIGHT_NORMAL;

	if (lua_isinteger(L, startFont))
	{
		charSize = luaL_checkinteger(L, startFont);
		if (lua_isstring(L, startFont + 1))
		{
			wxString fam = luaL_checkstring(L, startFont + 1);
			fam.MakeLower();
			if (fam == "default")
				family = wxFontFamily::wxFONTFAMILY_DEFAULT;
			else if (fam == "decorative")
				family = wxFontFamily::wxFONTFAMILY_DECORATIVE;
			else if (fam == "roman")
				family = wxFontFamily::wxFONTFAMILY_ROMAN;
			else if (fam == "script")
				family = wxFontFamily::wxFONTFAMILY_SCRIPT;
			else if (fam == "swiss")
				family = wxFontFamily::wxFONTFAMILY_SWISS;
			else if (fam == "modern")
				family = wxFontFamily::wxFONTFAMILY_MODERN;
			else if (fam == "teletype")
				family = wxFontFamily::wxFONTFAMILY_TELETYPE;
			else
				luaL_error(L, fam + " is an invalid font family");

			if (lua_isstring(L, startFont + 2))
			{
				wxString sty = luaL_checkstring(L, startFont + 2);

				if (sty == "normal")
					style = wxFontStyle::wxFONTSTYLE_NORMAL;
				else if (sty == "italic")
					style = wxFontStyle::wxFONTSTYLE_ITALIC;
				else if (sty == "slant")
					style = wxFontStyle::wxFONTSTYLE_SLANT;
				else
					luaL_error(L, sty + " is an invalid font style");

				if (lua_isstring(L, startFont + 3))
				{
					wxString wei = luaL_checkstring(L, startFont + 3);

					if (wei == "normal")
						weight = wxFontWeight::wxFONTWEIGHT_NORMAL;
					else if (wei == "light")
						weight = wxFontWeight::wxFONTWEIGHT_LIGHT;
					else if (wei == "bold")
						weight = wxFontWeight::wxFONTWEIGHT_BOLD;
					else
						luaL_error(L, wei + " is an invalid font weight");
				}
			}
		}
	}

	panel->DrawText(x, y, message, foreground.first, background.first, charSize, family, style, weight);

	return 0;
}

static int gui_drawbox(lua_State *L)
{
	auto *panel = wxGetApp().GetGsFrame().GetGui();

	int x1 = luaL_checkinteger(L, 1), y1 = luaL_checkinteger(L, 2);
	int x2 = luaL_checkinteger(L, 3), y2 = luaL_checkinteger(L, 4);
	auto line = buildColor(L, 5);
	auto background = buildColor(L, line.second + 1, wxTransparentColor);

	panel->DrawBox(x1, y1, x2, y2, line.first, background.first);

	return 0;
}

static int gui_drawrectangle(lua_State *L)
{
	auto *panel = wxGetApp().GetGsFrame().GetGui();

	int x = luaL_checkinteger(L, 1), y = luaL_checkinteger(L, 2);
	int width = luaL_checkinteger(L, 3), height = luaL_checkinteger(L, 4);
	auto line = buildColor(L, 5);
	auto background = buildColor(L, line.second + 1, wxTransparentColor);

	panel->DrawRectangle(x, y, width, height, line.first, background.first);

	return 0;
}

static int gui_drawline(lua_State *L)
{
	auto* panel = wxGetApp().GetGsFrame().GetGui();

	int x1 = luaL_checkinteger(L, 1), y1 = luaL_checkinteger(L, 2);
	int x2 = luaL_checkinteger(L, 3), y2 = luaL_checkinteger(L, 4);

	auto penColor = buildColor(L, 5);

	panel->DrawLine(x1, y1, x2, y2, penColor.first);
	return 0;
}

static int gui_drawpixel(lua_State *L)
{
	auto *panel = wxGetApp().GetGsFrame().GetGui();

	int x = luaL_checkinteger(L, 1), y = luaL_checkinteger(L, 2);
	auto color = buildColor(L, 3);

	panel->DrawPixel(x, y, color.first);

	return 0;
}

static int gui_drawellipse(lua_State *L)
{
	auto *panel = wxGetApp().GetGsFrame().GetGui();

	int x = luaL_checkinteger(L, 1), y = luaL_checkinteger(L, 2);
	int width = luaL_checkinteger(L, 3), height = luaL_checkinteger(L, 4);
	auto line = buildColor(L, 5);
	auto background = buildColor(L, line.second + 1, wxTransparentColor);

	panel->DrawEllipse(x, y, width, height, line.first, background.first);
	return 0;
}

static int gui_drawcircle(lua_State *L)
{
	auto *panel = wxGetApp().GetGsFrame().GetGui();

	int x = luaL_checkinteger(L, 1), y = luaL_checkinteger(L, 2);
	int radius = luaL_checkinteger(L, 3);
	auto line = buildColor(L, 4);
	auto background = buildColor(L, line.second + 1, wxTransparentColor);

	panel->DrawCircle(x, y, radius, line.first, background.first);
	return 0;
}

//=============================================
// lua
//=============================================
static int lua_func_close(lua_State *L)
{
	LuaEngine *pLua = g_Lua.getLuaEnginPtr(L);
	if (pLua == NULL)return 0;
	pLua->Close();
	return 0;
}

//=============================================
// other
//=============================================
int lua_function_print(lua_State *L)
{
	wxString str = luaL_checkstring(L, 1);
	Console.WriteLn(Color_StrongBlue, str);
	return 0;
}

//=============================================
// lua library name
//=============================================
static const struct luaL_Reg emulib[] =
{
	{ "frameadvance", emu_frameadvance },
	//{"speedmode", emu_speedmode},
	//{"wait", emu_wait},
	{"pause", emu_pause},
	{"unpause", emu_unpause},
	{"resume", emu_unpause},
	//{"emulateframe", emu_emulateframe},
	//{"emulateframefastnoskipping", emu_emulateframefastnoskipping},
	//{"emulateframefast", emu_emulateframefast},
	//{"emulateframeinvisible", emu_emulateframeinvisible},
	//{"redraw", emu_redraw},
	{"framecount", emu_getframecount},
	//{"lagcount", emu_getlagcount},
	//{"lagged", emu_lagged},
	//{"emulating", emu_emulating},
	//{"atframeboundary", emu_atframeboundary},
	{"registerbefore", emu_registerbefore},
	{"registerafter", emu_registerafter},
	//{"registerstart", emu_registerstart},
	{"registerexit", emu_registerexit},
	//{ "persistglobalvariables", emu_persistglobalvariables },
	//{ "message", emu_message },
	//{ "print", print }, // sure, why not
	//{"openscript", emu_openscript},
	//{"loadrom", emu_loadrom},
	//{"reset", emu_reset},
	// alternative names
	//{"openrom", emu_loadrom},
	{ NULL, NULL }
};
const struct luaL_Reg *lua_function_emulib = emulib;

static const struct luaL_Reg memorylib[] = {
	{ "readbyte",				memory_readbyte },
	{ "readbytesigned",			memory_readbytesigned },
	{ "readword",				memory_readword },
	{ "readwordsigned",			memory_readwordsigned },
	{ "readdword",				memory_readdword },
	{ "readdwordsigned",		memory_readdwordsigned },
	//{ "readbyterange",			memory_readbyterange },
	{ "writebyte",				memory_writebyte },
	//{ "writeword",				memory_writeword },
	{ "writedword",				memory_writedword },
	/*{ "getregister",			memory_getregister },
	{ "setregister",			memory_setregister },
	{ "gbromreadbyte",			memory_gbromreadbyte },
	{ "gbromreadbytesigned",	memory_gbromreadbytesigned },
	{ "gbromreadword",			memory_gbromreadword },
	{ "gbromreadwordsigned",	memory_gbromreadwordsigned },
	{ "gbromreaddword",			memory_gbromreaddword },
	{ "gbromreaddwordsigned",	memory_gbromreaddwordsigned },
	{ "gbromreadbyterange",		memory_gbromreadbyterange },

	// alternate naming scheme for word and double-word and unsigned
	{ "readbyteunsigned",		memory_readbyte },
	{ "readwordunsigned",		memory_readword },
	{ "readdwordunsigned",		memory_readdword },
	{ "readshort",				memory_readword },
	{ "readshortunsigned",		memory_readword },
	{ "readshortsigned",		memory_readwordsigned },
	{ "readlong",				memory_readdword },
	{ "readlongunsigned",		memory_readdword },
	{ "readlongsigned",			memory_readdwordsigned },
	{ "writeshort",				memory_writeword },
	{ "writelong",				memory_writedword },
	{ "gbromreadbyteunsigned",	memory_gbromreadbyte },
	{ "gbromreadwordunsigned",	memory_gbromreadword },
	{ "gbromreaddwordunsigned",	memory_gbromreaddword },
	{ "gbromreadshort",			memory_gbromreadword },
	{ "gbromreadshortunsigned",	memory_gbromreadword },
	{ "gbromreadshortsigned",	memory_gbromreadwordsigned },
	{ "gbromreadlong",			memory_gbromreaddword },
	{ "gbromreadlongunsigned",	memory_gbromreaddword },
	{ "gbromreadlongsigned",	memory_gbromreaddwordsigned },

	// memory hooks
	{ "registerwrite",	   memory_registerwrite },
	{ "registerread",	   memory_registerread },
	{ "registerexec",	   memory_registerexec },
	// alternate names
	{ "register",		   memory_registerwrite },
	{ "registerrun",	   memory_registerexec },
	{ "registerexecute",   memory_registerexec },*/

	{ NULL,				   NULL }
};
const struct luaL_Reg *lua_function_memorylib = memorylib;


static const struct luaL_Reg joypadlib[] = {
	{ "get",	  joypad_get },
	{ "set",	  joypad_set },
	{ NULL,		  NULL }
};
const struct luaL_Reg *lua_function_joypadlib= joypadlib;

static const struct luaL_Reg savestatelib[] = {
	{ "saveslot",	savestate_save },
	{ "loadslot",	savestate_load },

	{ NULL,		NULL }
};
const struct luaL_Reg *lua_function_savestatelib= savestatelib;


static const struct luaL_Reg movielib[] = {
	{ "mode",			  movie_getmode },
	{ "length",			  movie_getlength },
	{ "author",			  movie_getauthor },
	{ "getauthor",		  movie_getauthor },
	{ "name",			  movie_getfilename },
	{ "getname",		  movie_getfilename },
	{ "cdrom",			  movie_getcdrom },
	{ "getcdrom",		  movie_getcdrom },
	{ "rerecordcount",	  movie_rerecordcount },
	{ "stop",			  movie_stop },
	{ "close",			  movie_stop },
	{ NULL,				  NULL }
};
extern const struct luaL_Reg *lua_function_movielib = movielib;

static const struct luaL_Reg guilib[] = {
	/*{ "register",	  gui_register },*/
	{ "text",		  gui_text },
	{ "box",		  gui_drawbox },
	{ "rectangle",	  gui_drawrectangle},
	{ "line",		  gui_drawline },
	{ "pixel",		  gui_drawpixel },
	{ "ellipse",      gui_drawellipse},
	{ "circle",       gui_drawcircle},
	/*
	{ "opacity",	  gui_setopacity },
	{ "transparency", gui_transparency },
	{ "popup",		  gui_popup },
	{ "parsecolor",	  gui_parsecolor },
	{ "gdscreenshot", gui_gdscreenshot },
	{ "gdoverlay",	  gui_gdoverlay },
	{ "getpixel",	  gui_getpixel },
	*/

	// alternative names
	{ "drawText",	  gui_text },
	{ "drawBox",	  gui_drawbox },
	{ "drawRectangle",gui_drawrectangle},
	{ "drawLine",	  gui_drawline },
	{ "drawPixel",	  gui_drawpixel },
	{ "setPixel",	  gui_drawpixel },
	{ "writePixel",	  gui_drawpixel },
	{ "drawEllipse",  gui_drawellipse},
	{ "drawCircle",   gui_drawcircle},
	/*
	{ "drawimage",	  gui_gdoverlay },
	{ "image",		  gui_gdoverlay },
	{ "readpixel",	  gui_getpixel },*/
	{ NULL,			  NULL }
};
const struct luaL_Reg *lua_function_guilib = guilib;


static const struct luaL_Reg soundlib[] = {
	//{ "get",  sound_get },

	{ NULL,	  NULL }
};
const struct luaL_Reg *lua_function_soundlib = soundlib;

static const struct luaL_Reg lualib[] = {
	{ "close",  lua_func_close },

	{ NULL,	  NULL }
};
const struct luaL_Reg *lua_function_lualib = lualib;
