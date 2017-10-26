#pragma once
#ifndef __LUA_FUNCTIONS_H__
#define __LUA_FUNCTIONS_H__

extern const struct luaL_Reg *lua_function_emulib;
extern const struct luaL_Reg *lua_function_memorylib;
extern const struct luaL_Reg *lua_function_joypadlib;
extern const struct luaL_Reg *lua_function_savestatelib;
extern const struct luaL_Reg *lua_function_movielib;
extern const struct luaL_Reg *lua_function_guilib;
extern const struct luaL_Reg *lua_function_lualib;

extern int lua_function_print(lua_State *L);


#endif
