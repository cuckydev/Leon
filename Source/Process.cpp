/*
 * [ Leon ]
 *   Source/Process.cpp
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

#include "Process.h"

#include "Parse.h"

#include <iostream>
#include <stdexcept>

namespace Leon
{
namespace Process
{

// Lua tables
static void ConstructLuaAttributes(lua_State *T, const std::vector<Leon::Parse::LeonAttr> &attrs)
{
	lua_newtable(T);
	for (auto &i : attrs)
	{
		if (i.type == Leon::Parse::LeonAttr::Type::KeyValue)
		{
			lua_pushstring(T, i.kv.first.c_str());
			lua_pushstring(T, i.kv.second.c_str());
			lua_settable(T, -3);
		}
	}
}

void ConstructLuaTables(lua_State *T)
{
	// Create types table
	lua_newtable(T);
	for (auto &i : Leon::Parse::type_nodes)
	{
		lua_pushstring(T, i.first.c_str());
		lua_newtable(T);
		lua_settable(T, -3);
	}

	for (auto &i : Leon::Parse::type_nodes)
	{
		lua_pushstring(T, i.first.c_str());
		lua_gettable(T, -2);

		switch (i.second.type)
		{
			case Leon::Parse::TypeNode::Type::Invalid:
				throw std::runtime_error("Invalid type node");
			case Leon::Parse::TypeNode::Type::Type:
				LuaTableSetString(T, -1, "type_type", "type");
				break;
			case Leon::Parse::TypeNode::Type::LValueReference:
				LuaTableSetString(T, -1, "type_type", "lvalue_reference");
				break;
			case Leon::Parse::TypeNode::Type::RValueReference:
				LuaTableSetString(T, -1, "type_type", "rvalue_reference");
				break;
			case Leon::Parse::TypeNode::Type::Pointer:
				LuaTableSetString(T, -1, "type_type", "pointer");
				break;
			case Leon::Parse::TypeNode::Type::BlockPointer:
				LuaTableSetString(T, -1, "type_type", "block_pointer");
				break;
			case Leon::Parse::TypeNode::Type::ObjCObjectPointer:
				LuaTableSetString(T, -1, "type_type", "objc_object_pointer");
				break;
			case Leon::Parse::TypeNode::Type::MemberPointer:
				LuaTableSetString(T, -1, "type_type", "member_pointer");
				break;
		}

		LuaTableSetBoolean(T, -1, "const", i.second.q_const);
		LuaTableSetBoolean(T, -1, "volatile", i.second.q_volatile);
		LuaTableSetBoolean(T, -1, "restrict", i.second.q_restrict);

		LuaTableSetString(T, -1, "name", i.second.name.c_str());
		LuaTableSetFromByString(T, -1, "root", -2, i.second.root.c_str());
		LuaTableSetFromByString(T, -1, "unqualified_root", -2, i.second.unqualified_root.c_str());
		LuaTableSetFromByString(T, -1, "unqualified", -2, i.second.unqualified.c_str());
		LuaTableSetFromByString(T, -1, "pointee", -2, i.second.pointee.c_str());

		LuaTableSetBoolean(T, -1, "is_template", i.second.is_template);

		if (i.second.is_template)
		{
			lua_pushstring(T, "template_arguments");
			lua_newtable(T);

			int template_i = 1;
			for (auto &t : i.second.template_args)
			{
				lua_pushnumber(T, template_i++);
				lua_newtable(T);

				switch (t.arg_type)
				{
					case Leon::Parse::TypeNode::TemplateArg::TemplateArgType::Invalid:
						throw std::runtime_error("Invalid type node");
					case Leon::Parse::TypeNode::TemplateArg::TemplateArgType::Type:
						LuaTableSetString(T, -1, "argument_type", "type");
						LuaTableSetFromByString(T, -1, "type", -6, t.type.c_str());
						break;
					case Leon::Parse::TypeNode::TemplateArg::TemplateArgType::Nullptr:
						LuaTableSetString(T, -1, "argument_type", "nullptr");
						break;
					case Leon::Parse::TypeNode::TemplateArg::TemplateArgType::Integral:
						LuaTableSetString(T, -1, "argument_type", "integral");
						LuaTableSetString(T, -1, "integral", std::to_string(t.integral).c_str());
						break;
				}

				lua_settable(T, -3);
			}

			lua_settable(T, -3);
		}

		lua_pop(T, 1);
	}

	// Create enums table
	lua_newtable(T);

	for (auto &i : Leon::Parse::enum_nodes)
	{
		lua_pushstring(T, i.first.c_str());
		lua_newtable(T);

		LuaTableSetString(T, -1, "name", i.second.name.c_str());

		lua_pushstring(T, "attributes");
		ConstructLuaAttributes(T, i.second.attrs);
		lua_settable(T, -3);

		lua_pushstring(T, "elements");
		lua_newtable(T);
		for (auto &v : i.second.elems)
		{
			lua_pushstring(T, v.first.c_str());
			lua_pushstring(T, std::to_string(v.second).c_str());
			lua_settable(T, -3);
		}
		lua_settable(T, -3);

		// Push to enums table
		lua_settable(T, -3);
	}

	// Create classes table
	lua_newtable(T);
	for (auto &i : Leon::Parse::class_nodes)
	{
		lua_pushstring(T, i.first.c_str());
		lua_newtable(T);
		lua_settable(T, -3);
	}

	for (auto &i : Leon::Parse::class_nodes)
	{
		lua_pushstring(T, i.first.c_str());
		lua_gettable(T, -2);

		LuaTableSetString(T, -1, "name", i.second.name.c_str());

		switch (i.second.class_type)
		{
			case Leon::Parse::ClassNode::ClassType::Invalid:
				throw std::runtime_error("Invalid class type");
			case Leon::Parse::ClassNode::ClassType::Class:
				LuaTableSetString(T, -1, "class_type", "class");
				break;
			case Leon::Parse::ClassNode::ClassType::Struct:
				LuaTableSetString(T, -1, "class_type", "struct");
				break;
		}

		lua_pushstring(T, "attributes");
		ConstructLuaAttributes(T, i.second.attrs);
		lua_settable(T, -3);

		LuaTableSetBoolean(T, -1, "abstract", i.second.q_abstract);

		lua_pushstring(T, "bases");
		lua_newtable(T);
		for (auto &v : i.second.bases)
		{
			lua_pushstring(T, v.base_class.c_str());
			lua_newtable(T);

			LuaTableSetFromByString(T, -1, "class", -6, v.base_class.c_str());

			switch (v.visibility)
			{
				case Leon::Parse::ClassNode::Visibility::Invalid:
					throw std::runtime_error("Invalid visibility");
				case Leon::Parse::ClassNode::Visibility::Public:
					LuaTableSetString(T, -1, "visibility", "public");
					break;
				case Leon::Parse::ClassNode::Visibility::Protected:
					LuaTableSetString(T, -1, "visibility", "protected");
					break;
				case Leon::Parse::ClassNode::Visibility::Private:
					LuaTableSetString(T, -1, "visibility", "private");
					break;
			}

			lua_settable(T, -3);
		}
		lua_settable(T, -3);

		lua_pushstring(T, "members");
		lua_newtable(T);
		for (auto &v : i.second.members)
		{
			lua_pushstring(T, v.name.c_str());
			lua_newtable(T);

			LuaTableSetString(T, -1, "name", v.name.c_str());

			switch (v.member_type)
			{
				case Leon::Parse::ClassNode::Member::MemberType::Invalid:
					throw std::runtime_error("Invalid member type");
				case Leon::Parse::ClassNode::Member::MemberType::Member:
					LuaTableSetString(T, -1, "member_type", "member");
					break;
				case Leon::Parse::ClassNode::Member::MemberType::Static:
					LuaTableSetString(T, -1, "member_type", "static");
					break;
			}

			lua_pushstring(T, "attributes");
			ConstructLuaAttributes(T, v.attrs);
			lua_settable(T, -3);

			switch (v.visibility)
			{
				case Leon::Parse::ClassNode::Visibility::Invalid:
					throw std::runtime_error("Invalid visibility");
				case Leon::Parse::ClassNode::Visibility::Public:
					LuaTableSetString(T, -1, "visibility", "public");
					break;
				case Leon::Parse::ClassNode::Visibility::Protected:
					LuaTableSetString(T, -1, "visibility", "protected");
					break;
				case Leon::Parse::ClassNode::Visibility::Private:
					LuaTableSetString(T, -1, "visibility", "private");
					break;
			}

			LuaTableSetFromByString(T, -1, "type", -8, v.type.c_str());

			lua_settable(T, -3);
		}
		lua_settable(T, -3);

		lua_pushstring(T, "methods");
		lua_newtable(T);
		for (auto &v : i.second.methods)
		{
			lua_pushstring(T, v.name.c_str());
			lua_newtable(T);

			LuaTableSetString(T, -1, "name", v.name.c_str());

			switch (v.method_type)
			{
				case Leon::Parse::ClassNode::Method::MethodType::Invalid:
					throw std::runtime_error("Invalid method type");
				case Leon::Parse::ClassNode::Method::MethodType::Method:
					LuaTableSetString(T, -1, "method_type", "method");
					break;
				case Leon::Parse::ClassNode::Method::MethodType::Friend:
					LuaTableSetString(T, -1, "method_type", "friend");
					break;
				case Leon::Parse::ClassNode::Method::MethodType::Static:
					LuaTableSetString(T, -1, "method_type", "static");
					break;
			}

			lua_pushstring(T, "attributes");
			ConstructLuaAttributes(T, v.attrs);
			lua_settable(T, -3);

			switch (v.visibility)
			{
				case Leon::Parse::ClassNode::Visibility::Invalid:
					throw std::runtime_error("Invalid visibility");
				case Leon::Parse::ClassNode::Visibility::Public:
					LuaTableSetString(T, -1, "visibility", "public");
					break;
				case Leon::Parse::ClassNode::Visibility::Protected:
					LuaTableSetString(T, -1, "visibility", "protected");
					break;
				case Leon::Parse::ClassNode::Visibility::Private:
					LuaTableSetString(T, -1, "visibility", "private");
					break;
			}

			LuaTableSetBoolean(T, -1, "const", v.q_const);
			LuaTableSetBoolean(T, -1, "virtual", v.q_virtual);
			LuaTableSetBoolean(T, -1, "pure", v.q_pure);

			LuaTableSetFromByString(T, -1, "return_type", -8, v.return_type.c_str());

			lua_pushstring(T, "arguments");
			lua_newtable(T);
			int arg_i = 1;
			for (auto &a : v.args)
			{
				lua_pushnumber(T, arg_i++);
				lua_newtable(T);

				LuaTableSetFromByString(T, -1, "type", -12, a.type.c_str());

				LuaTableSetString(T, -1, "name", a.name.c_str());

				lua_pushstring(T, "attributes");
				ConstructLuaAttributes(T, a.attrs);
				lua_settable(T, -3);

				lua_settable(T, -3);
			}
			lua_settable(T, -3);

			lua_settable(T, -3);
		}
		lua_settable(T, -3);

		lua_pop(T, 1);
	}

	// Create functions table
	lua_newtable(T);

	for (auto &i : Leon::Parse::function_nodes)
	{
		lua_pushstring(T, i.first.c_str());
		lua_newtable(T);

		LuaTableSetString(T, -1, "name", i.second.name.c_str());

		lua_pushstring(T, "attributes");
		ConstructLuaAttributes(T, i.second.attrs);
		lua_settable(T, -3);

		lua_pushstring(T, "return_type");
		LuaTableGetString(T, -7, i.second.return_type.c_str());
		lua_settable(T, -3);

		lua_pushstring(T, "arguments");
		lua_newtable(T);
		int arg_i = 1;
		for (auto &a : i.second.args)
		{
			lua_pushnumber(T, arg_i++);
			lua_newtable(T);

			lua_pushstring(T, "type");
			LuaTableGetString(T, -11, a.type.c_str());
			lua_settable(T, -3);

			LuaTableSetString(T, -1, "name", a.name.c_str());

			lua_pushstring(T, "attributes");
			ConstructLuaAttributes(T, a.attrs);
			lua_settable(T, -3);

			lua_settable(T, -3);
		}
		lua_settable(T, -3);

		lua_settable(T, -3);
	}
}

}
}
