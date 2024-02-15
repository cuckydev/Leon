/*
 * [ Leon ]
 *   Source/Parse.h
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

#include <clang-c/Index.h>

#include <istream>
#include <string>
#include <vector>
#include <unordered_map>

namespace Leon
{
namespace Parse
{

// Get string from a CXString and dispose it
static std::string GetCXString(CXString string)
{
	std::string result = clang_getCString(string);
	clang_disposeString(string);
	return result;
}

// Read a string from a stream
static std::string ParseString(std::istream &stream)
{
	// Read until first quote
	while (1)
	{
		int c = stream.get();
		if (c == std::istream::traits_type::eof())
			return "";
		if (c == '"')
			break;
	}

	// Parse character by character
	auto ctoi = [](char c) -> int {
		if (c >= '0' && c <= '9') return c - '0';
		if (c >= 'a' && c <= 'f') return c - 'a' + 10;
		if (c >= 'A' && c <= 'F') return c - 'A' + 10;
		return -1;
		};

	std::string value;

	while (1)
	{
		int c = stream.get();
		if (c == std::istream::traits_type::eof())
			return "";
		if (c == '"')
			break;

		if (c == '\\')
		{
			// TODO: consider removing this?

			int e0 = stream.get();
			switch (e0)
			{
				case 'a':
					value += '\a';
					break;
				case 'b':
					value += '\b';
					break;
				case 'f':
					value += '\f';
					break;
				case 'n':
					value += '\n';
					break;
				case 'r':
					value += '\r';
					break;
				case 't':
					value += '\t';
					break;
				case 'v':
					value += '\v';
					break;
				case '0':
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
				{
					// Grab first digit
					e0 = e0 - '0';

					// Grab second digit
					int e1;
					if (e1 = ctoi(stream.peek()), (e1 >= 0 && e1 < 8))
					{
						// 2+ digit octal
						stream.get();

						// Grab third digit
						int e2;
						if (e2 = ctoi(stream.peek()), (e2 >= 0 && e2 < 8))
						{
							// 3 digit octal
							stream.get();

							// Write character
							value += (e0 << 6) | (e1 << 3) | e2;
						}
						else
						{
							// 2 digit octal
							value += (e0 << 3) | e1;
						}
					}
					else
					{
						// 1 digit octal
						value += e0;
					}
					break;
				}
				case 'x':
				{
					// Read digits
					char value = 0;
					while (1)
					{
						// Grab digit
						int e = ctoi(stream.peek());
						if (e < 0 || e > 15) break;
						stream.get();

						// Push digit to value
						value = (value << 4) | e;
					}

					// Write character
					value += value;
					break;
				}
				default:
				{
					// Write character
					value += e0;
					break;
				}
			}
		}
		else
		{
			value += c;
		}
	}

	return value;
}

// Leon attributes
struct LeonAttr
{
	enum class Type
	{
		Invalid,
		Flag,
		KeyValue,
	} type = Type::Invalid;

	std::pair<std::string, std::string> kv;
};

// Type registry
struct TypeNode
{
	enum class Type
	{
		Invalid,
		Type,
		LValueReference,
		RValueReference,
		Pointer,
		BlockPointer,
		ObjCObjectPointer,
		MemberPointer,
	} type = Type::Invalid;

	std::string name;

	bool q_const = false, q_volatile = false, q_restrict = false;

	std::string root;
	std::string unqualified_root;
	std::string unqualified;
	std::string pointee;

	struct TemplateArg
	{
		enum class TemplateArgType
		{
			Invalid,
			Type,
			Nullptr,
			Integral,
		} arg_type = TemplateArgType::Invalid;

		std::string type;
		long long integral = 0;
	};

	bool is_template = false;
	std::vector<TemplateArg> template_args;
};

extern std::unordered_map<std::string, TypeNode> type_nodes;

// Enum registry
struct EnumNode
{
	std::string name;
	std::vector<LeonAttr> attrs;
	std::unordered_map<std::string, long long> elems;
};

extern std::unordered_map<std::string, EnumNode> enum_nodes;

// Class registry
struct ClassNode
{
	enum class Visibility
	{
		Invalid,
		Public,
		Protected,
		Private,
	};

	std::string name;
	enum class ClassType
	{
		Invalid,
		Struct,
		Class,
	} class_type = ClassType::Invalid;

	std::vector<LeonAttr> attrs;

	bool q_abstract = false;

	struct Base
	{
		std::string base_class;
		Visibility visibility = Visibility::Invalid;
	};
	
	std::vector<Base> bases;

	struct Member
	{
		std::string name;
		enum class MemberType
		{
			Invalid,
			Member,
			Static,
		} member_type = MemberType::Invalid;

		std::vector<LeonAttr> attrs;

		Visibility visibility = Visibility::Invalid;

		std::string type;
	};
	std::vector<Member> members;

	struct Method
	{
		std::string name;
		enum class MethodType
		{
			Invalid,
			Method,
			Static,
			Friend,
		} method_type = MethodType::Invalid;

		bool q_const = false;

		bool q_virtual = false, q_pure = false;

		std::vector<LeonAttr> attrs;

		Visibility visibility = Visibility::Invalid;

		std::string return_type;

		struct Arg
		{
			std::string type;
			std::string name;

			std::vector<LeonAttr> attrs;
		};
		std::vector<Arg> args;
	};
	std::vector<Method> methods;
};

extern std::unordered_map<std::string, ClassNode> class_nodes;

// Function registry
struct FunctionNode
{
	std::string name;

	std::vector<LeonAttr> attrs;

	std::string return_type;

	struct Arg
	{
		std::string type;
		std::string name;

		std::vector<LeonAttr> attrs;
	};
	std::vector<Arg> args;
};

extern std::unordered_map<std::string, FunctionNode> function_nodes;

// Reset
void Reset();

// Clang cursor visitor
CXChildVisitResult Visitor(CXCursor cursor, CXCursor parent, CXClientData clientData);

}
}
