/*
 * [ Leon ]
 *   Source/Process.h
 * Author(s): Regan Green
 * Date: 2024-02-13
 *
 * Copyright (C) 2024 Regan "CKDEV" Green
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#pragma once

#include <Luau/Compiler.h>
#include <lua.h>
#include <lualib.h>

#include <iostream>

namespace Leon
{
namespace Process
{

// Dump Lua stack
inline void DumpStack(lua_State *state)
{
	// Dump marker
	std::cout << "Lua::DumpStack(" << (void*)state << ")" << std::endl;

	// Get stack element count
	int top = lua_gettop(state);
	for (int i = 1; i <= top; i++)
	{
		// Print index and type name
		std::cout << '\t' << i << '\t' << luaL_typename(state, i) << '\t';

		// Print value
		switch (lua_type(state, i))
		{
			case LUA_TNUMBER:
				std::cout << lua_tonumber(state, i) << std::endl;
				break;
			case LUA_TSTRING:
				std::cout << '"' << lua_tostring(state, i) << '"' << std::endl;
				break;
			case LUA_TBOOLEAN:
				if (lua_toboolean(state, i))
					std::cout << "true" << std::endl;
				else
					std::cout << "false" << std::endl;
				break;
			case LUA_TNIL:
				std::cout << "nil" << std::endl;
				break;
			default:
				std::cout << (void *)lua_topointer(state, i) << std::endl;
				break;
		}
	}
}

// Lua helpers
static void LuaTableSetBoolean(lua_State *T, int idx, const char *name, int v)
{
	lua_pushstring(T, name);
	lua_pushboolean(T, v);
	lua_settable(T, idx - 2);
}

static void LuaTableSetString(lua_State *T, int idx, const char *name, const char *v)
{
	lua_pushstring(T, name);
	lua_pushstring(T, v);
	lua_settable(T, idx - 2);
}

static void LuaTableSetFromByString(lua_State *T, int idx_dst, const char *name_dst, int idx_src, const char *name_src)
{
	lua_pushstring(T, name_dst);
	lua_pushstring(T, name_src);
	lua_gettable(T, idx_src - 2);
	if (lua_isnil(T, -1) && name_src[0] != '\0')
	{
		lua_pop(T, 1);
		lua_pushstring(T, name_src);
	}
	lua_settable(T, idx_dst - 2);
}

static void LuaTableGetString(lua_State *T, int idx, const char *name)
{
	lua_pushstring(T, name);
	lua_gettable(T, idx - 1);
}

// Lua process functions
/*
This pushes the following tables onto the stack
 - types
 - enums
 - classes
 - functions
*/
void ConstructLuaTables(lua_State *T);

}
}
