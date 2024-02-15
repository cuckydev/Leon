/*
 * [ Leon ]
 *   Source/Parse.cpp
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

#include "Parse.h"

#include <istream>
#include <iostream>
#include <sstream>

namespace Leon
{
namespace Parse
{

// Parse a @leon attribute
static LeonAttr ParseAttribute(const std::string &src)
{
	LeonAttr attr;

	// Read initial token
	std::stringstream src_stream(src);
	{
		std::string token;
		src_stream >> token;

		if (token == "@leon")
			attr.type = LeonAttr::Type::Flag;
		else
			attr.type = LeonAttr::Type::KeyValue;
	}

	switch (attr.type)
	{
		case LeonAttr::Type::KeyValue:
		{
			attr.kv.first = ParseString(src_stream);
			attr.kv.second = ParseString(src_stream);
			if (attr.kv.first.empty() || attr.kv.second.empty())
			{
				attr.type = LeonAttr::Type::Invalid;
				throw std::runtime_error("LEON_KV malformed");
			}
			break;
		}
		default:
			break;
	}

	return attr;
}

// Parse attributes from a cursor
static std::vector<LeonAttr> ParseCXCursorAttributes(CXCursor cursor)
{
	struct VisitorClient
	{
		std::vector<LeonAttr> attrs;
	} client;

	auto visitor = [](CXCursor cursor, CXCursor parent, CXClientData clientData) -> CXChildVisitResult
		{
			auto &client = *(reinterpret_cast<VisitorClient *>(clientData));

			if (cursor.kind == CXCursor_AnnotateAttr)
			{
				std::string src = GetCXString(clang_getCursorSpelling(cursor));
				LeonAttr attr = ParseAttribute(src);
				if (attr.type != LeonAttr::Type::Invalid)
					client.attrs.push_back(attr);
				return CXChildVisit_Continue;
			}

			return CXChildVisit_Continue;
		};

	clang_visitChildren(cursor, visitor, &client);

	return client.attrs;
}

// Get the full name of a cursor declaration
static std::string GetCXCursorName(CXCursor cx_cursor)
{
	std::string name = GetCXString(clang_getCursorSpelling(cx_cursor));

	while (1)
	{
		// Get the parent cursor to step up a level
		cx_cursor = clang_getCursorSemanticParent(cx_cursor);
		if (clang_isInvalid(cx_cursor.kind) || clang_isTranslationUnit(cx_cursor.kind))
			break;
		if (clang_isUnexposed(cx_cursor.kind))
			continue;

		// Append parent to name
		name = GetCXString(clang_getCursorSpelling(cx_cursor)) + "::" + name;
	}

	return name;
}

// Get the name of a cursor
static std::string GetCXCursorString(CXCursor cursor)
{
	return GetCXString(clang_getCursorKindSpelling(cursor.kind)) + " (" + GetCXCursorName(cursor) + ")";
}

// Check a CXType for support
static void CheckCXType(CXType cx_type)
{
	if (clang_getNumArgTypes(cx_type) > 0 || clang_getResultType(cx_type).kind != CXType_Invalid)
		throw std::runtime_error("Function types currently unsupported: " + GetCXString(clang_getTypeSpelling(cx_type)));
}

// Get the root type from a CXType, without any references or pointers
static CXType GetCXTypeRoot(CXType cx_type)
{
	while (1)
	{
		if (cx_type.kind == CXType_LValueReference || cx_type.kind == CXType_RValueReference)
		{
			// Resolve reference
			cx_type = clang_getNonReferenceType(cx_type);
		}
		else if (cx_type.kind == CXType_Pointer || cx_type.kind == CXType_BlockPointer || cx_type.kind == CXType_ObjCObjectPointer || cx_type.kind == CXType_MemberPointer || cx_type.kind == CXType_Auto /* || cx_type.kind == CXType_DeducedTemplateSpecialization*/)
		{
			// Resolve pointer
			CXType cx_try = clang_getPointeeType(cx_type);
			if (cx_try.kind == CXType_Invalid)
				break;
			cx_type = cx_try;
		}
		else
		{
			break;
		}
	}
	return cx_type;
}

// Get the qualification string of a CXType
static std::string GetQualString(CXType cx_type)
{
	std::string out = "";
	if (clang_isConstQualifiedType(cx_type))
	{
		out += "const";
	}
	if (clang_isVolatileQualifiedType(cx_type))
	{
		if (!out.empty())
			out += ' ';
		out += "volatile";
	}
	if (clang_isRestrictQualifiedType(cx_type))
	{
		if (!out.empty())
			out += ' ';
		out += "restrict";
	}
	return out;
}

// Get the full name of a CXType
static std::string GetCXTypeName(CXType cx_type)
{
	// Get global name
	CXType root = GetCXTypeRoot(cx_type);
	CheckCXType(root);

	CXCursor cursor = clang_getTypeDeclaration(root);

	std::string global_name;
	if (!clang_isInvalid(cursor.kind))
	{
		global_name = GetCXCursorName(cursor);
	}
	else
	{
		global_name = GetCXString(clang_getTypeSpelling(clang_getUnqualifiedType(root)));
	}

	// Apply template parameters
	if (!clang_isInvalid(cursor.kind))
	{
		int template_num = clang_Cursor_getNumTemplateArguments(cursor);

		if (template_num >= 0)
		{
			global_name += '<';

			for (unsigned int t = 0; t < template_num; t++)
			{
				if (t)
					global_name += ", ";

				auto t_kind = clang_Cursor_getTemplateArgumentKind(cursor, t);
				switch (t_kind)
				{
					case CXTemplateArgumentKind_Type:
						global_name += GetCXTypeName(clang_Cursor_getTemplateArgumentType(cursor, t));
						break;
					case CXTemplateArgumentKind_NullPtr:
						global_name += "nullptr";
						break;
					case CXTemplateArgumentKind_Integral:
						global_name += std::to_string(clang_Cursor_getTemplateArgumentValue(cursor, t));
						break;
					case CXTemplateArgumentKind_Null:
						throw std::runtime_error("CXTemplateArgumentKind_Null: " + global_name);
					case CXTemplateArgumentKind_Declaration:
						throw std::runtime_error("CXTemplateArgumentKind_Declaration: " + global_name);
					case CXTemplateArgumentKind_Template:
						throw std::runtime_error("CXTemplateArgumentKind_Template: " + global_name);
					case CXTemplateArgumentKind_TemplateExpansion:
						throw std::runtime_error("CXTemplateArgumentKind_TemplateExpansion: " + global_name);
					case CXTemplateArgumentKind_Expression:
						throw std::runtime_error("CXTemplateArgumentKind_Expression: " + global_name);
					case CXTemplateArgumentKind_Pack:
						throw std::runtime_error("CXTemplateArgumentKind_Pack: " + global_name);
					case CXTemplateArgumentKind_Invalid:
						throw std::runtime_error("Could not deduce template argument type: " + global_name);
				}
			}

			global_name += '>';
		}
	}

	// Apply qualifications
	std::string lqual = GetQualString(root);
	if (!lqual.empty())
		lqual += ' ';

	std::string rqual = "";

	while (1)
	{
		if (cx_type.kind == CXType_LValueReference)
		{
			// Resolve reference
			rqual = " " + ("&" + GetQualString(cx_type)) + rqual;
			cx_type = clang_getNonReferenceType(cx_type);
		}
		else if (cx_type.kind == CXType_RValueReference)
		{
			// Resolve reference
			rqual = " " + ("&&" + GetQualString(cx_type)) + rqual;
			cx_type = clang_getNonReferenceType(cx_type);
		}
		else if (cx_type.kind == CXType_Pointer || cx_type.kind == CXType_BlockPointer || cx_type.kind == CXType_ObjCObjectPointer || cx_type.kind == CXType_MemberPointer /* || cx_type.kind == CXType_Auto || cx_type.kind == CXType_DeducedTemplateSpecialization*/)
		{
			// Resolve pointer
			CXType cx_try = clang_getPointeeType(cx_type);
			if (cx_try.kind == CXType_Invalid)
				break;

			rqual = " " + ("*" + GetQualString(cx_type)) + rqual;
			cx_type = cx_try;
		}
		else
		{
			break;
		}
	}

	return lqual + global_name + rqual;
}

// Type registry
std::unordered_map<std::string, TypeNode> type_nodes;

std::string RegisterType(CXType cx_type)
{
	std::string name = GetCXTypeName(cx_type);

	// Check if type was already registered
	auto it = type_nodes.find(name);
	if (it != type_nodes.end())
		return it->first;

	// Register new type
	auto &node = type_nodes[name];

	node.name = name;

	switch (cx_type.kind)
	{
		case CXType_LValueReference:
			node.type = TypeNode::Type::LValueReference;
			break;
		case CXType_RValueReference:
			node.type = TypeNode::Type::RValueReference;
			break;
		case CXType_Pointer:
			node.type = TypeNode::Type::Pointer;
			break;
		case CXType_BlockPointer:
			node.type = TypeNode::Type::BlockPointer;
			break;
		case CXType_ObjCObjectPointer:
			node.type = TypeNode::Type::ObjCObjectPointer;
			break;
		case CXType_MemberPointer:
			node.type = TypeNode::Type::MemberPointer;
			break;
		default:
			node.type = TypeNode::Type::Type;
			// std::cout << "!!!!! " <<  name << " is " << GetCXString(clang_getTypeKindSpelling(cx_type.kind)) << std::endl;
			break;
	}

	node.q_const = clang_isConstQualifiedType(cx_type);
	node.q_volatile = clang_isVolatileQualifiedType(cx_type);
	node.q_restrict = clang_isRestrictQualifiedType(cx_type);

	CXType root = GetCXTypeRoot(cx_type);
	node.root = RegisterType(root);

	CXCursor cursor = clang_getTypeDeclaration(root);

	node.unqualified = RegisterType(clang_getUnqualifiedType(cx_type));
	if (!clang_isInvalid(cursor.kind))
	{
		node.unqualified_root = RegisterType(clang_getCursorType(cursor));
	}
	else
	{
		node.unqualified_root = RegisterType(clang_getUnqualifiedType(cx_type));
	}

	if (cx_type.kind == CXType_LValueReference || cx_type.kind == CXType_RValueReference)
	{
		// Resolve reference
		cx_type = clang_getNonReferenceType(cx_type);
		node.pointee = RegisterType(cx_type);
	}
	else if (cx_type.kind == CXType_Pointer || cx_type.kind == CXType_BlockPointer || cx_type.kind == CXType_ObjCObjectPointer || cx_type.kind == CXType_MemberPointer /* || cx_type.kind == CXType_Auto || cx_type.kind == CXType_DeducedTemplateSpecialization*/)
	{
		// Resolve pointer
		CXType cx_try = clang_getPointeeType(cx_type);
		if (cx_try.kind != CXType_Invalid)
		{
			cx_type = cx_try;
			node.pointee = RegisterType(cx_type);
		}
	}

	// Get template parameters
	if (!clang_isInvalid(cursor.kind))
	{
		int template_num = clang_Cursor_getNumTemplateArguments(cursor);

		if (template_num >= 0)
		{
			node.is_template = true;

			for (unsigned int t = 0; t < template_num; t++)
			{
				TypeNode::TemplateArg arg;

				auto t_kind = clang_Cursor_getTemplateArgumentKind(cursor, t);
				switch (t_kind)
				{
					case CXTemplateArgumentKind_Type:
						arg.arg_type = TypeNode::TemplateArg::TemplateArgType::Type;
						arg.type = RegisterType(clang_Cursor_getTemplateArgumentType(cursor, t));
						break;
					case CXTemplateArgumentKind_NullPtr:
						arg.arg_type = TypeNode::TemplateArg::TemplateArgType::Nullptr;
						break;
					case CXTemplateArgumentKind_Integral:
						arg.arg_type = TypeNode::TemplateArg::TemplateArgType::Integral;
						arg.integral = clang_Cursor_getTemplateArgumentValue(cursor, t);
						break;
					case CXTemplateArgumentKind_Null:
						throw std::runtime_error("CXTemplateArgumentKind_Null: " + name);
					case CXTemplateArgumentKind_Declaration:
						throw std::runtime_error("CXTemplateArgumentKind_Declaration: " + name);
					case CXTemplateArgumentKind_Template:
						throw std::runtime_error("CXTemplateArgumentKind_Template: " + name);
					case CXTemplateArgumentKind_TemplateExpansion:
						throw std::runtime_error("CXTemplateArgumentKind_TemplateExpansion: " + name);
					case CXTemplateArgumentKind_Expression:
						throw std::runtime_error("CXTemplateArgumentKind_Expression: " + name);
					case CXTemplateArgumentKind_Pack:
						throw std::runtime_error("CXTemplateArgumentKind_Pack: " + name);
					case CXTemplateArgumentKind_Invalid:
						throw std::runtime_error("Could not deduce template argument type: " + name);
				}

				node.template_args.emplace_back(std::move(arg));
			}
		}
	}

	return name;
}

// Enum registry
std::unordered_map<std::string, EnumNode> enum_nodes;

static std::string RegisterEnum(CXCursor cursor)
{
	std::string name = GetCXCursorName(cursor);

	// Check if enum was already registered
	auto it = enum_nodes.find(name);
	if (it != enum_nodes.end())
		return it->first;

	// Visit enum children
	struct VisitorClient
	{
		EnumNode node;
		std::string last_elem;
		long long current_value = 0;
	} client;

	client.node.attrs = ParseCXCursorAttributes(cursor);

	if (client.node.attrs.size())
	{
		client.node.name = name;

		auto visitor = [](CXCursor cursor, CXCursor parent, CXClientData clientData) -> CXChildVisitResult
			{
				auto &client = *(reinterpret_cast<VisitorClient *>(clientData));

				// Start enum element
				if (cursor.kind == CXCursor_EnumConstantDecl)
				{
					std::string name = GetCXString(clang_getCursorSpelling(cursor));
					client.last_elem = name;
					client.node.elems[name] = client.current_value++;
					return CXChildVisit_Recurse;
				}

				// Evaluate operators
				CXEvalResult result = clang_Cursor_Evaluate(cursor);
				if (result != nullptr)
				{
					CXEvalResultKind result_kind = clang_EvalResult_getKind(result);
					switch (result_kind)
					{
						case CXEval_Int:
						{
							long long value = clang_EvalResult_getAsLongLong(result);
							client.node.elems[client.last_elem] = value;
							client.current_value = value + 1;
							break;
						}
						default:
						{
							throw std::runtime_error("Unexpected EvalResult kind for enum element");
						}
					}
				}

				return CXChildVisit_Continue;
			};

		clang_visitChildren(cursor, visitor, &client);

		enum_nodes[name] = std::move(client.node);
	}

	return "";
}

// Class registry
std::unordered_map<std::string, ClassNode> class_nodes;

std::string RegisterClass(CXCursor cursor)
{
	std::string name = GetCXCursorName(cursor);

	// Check if class was already registered
	auto it = class_nodes.find(name);
	if (it != class_nodes.end())
		return it->first;

	struct VisitorClient
	{
		ClassNode node;
		int friend_decl = 0;

		ClassNode::Method *current_method = nullptr;
		ClassNode::Method::Arg *current_arg = nullptr;
	} client;

	client.node.attrs = ParseCXCursorAttributes(cursor);

	if (client.node.attrs.size())
	{
		client.node.name = name;

		switch (cursor.kind)
		{
			case CXCursor_ClassDecl:
				client.node.class_type = ClassNode::ClassType::Class;
				break;
			case CXCursor_StructDecl:
				client.node.class_type = ClassNode::ClassType::Struct;
				break;
			default:
				throw std::runtime_error("Unexpected cursor kind for RegisterClass");
		}

		auto visitor = [](CXCursor cursor, CXCursor parent, CXClientData clientData) -> CXChildVisitResult
			{
				auto &client = *(reinterpret_cast<VisitorClient *>(clientData));

				if (client.friend_decl)
					client.friend_decl--;

				// Add base class
				if (cursor.kind == CXCursor_CXXBaseSpecifier)
				{
					ClassNode::Base base;

					auto access = clang_getCXXAccessSpecifier(cursor);
					switch (access)
					{
						case CX_CXXPublic:
							base.visibility = ClassNode::Visibility::Public;
							break;
						case CX_CXXProtected:
							base.visibility = ClassNode::Visibility::Protected;
							break;
						case CX_CXXPrivate:
							base.visibility = ClassNode::Visibility::Private;
							break;
						default:
							throw std::runtime_error("Unexpected access specifier for base specifier");
					}

					auto type = clang_getCursorType(cursor);
					if (type.kind == CXType_Invalid)
						throw std::runtime_error("Type not found for base specifier");

					auto decl = clang_getTypeDeclaration(type);
					if (clang_isInvalid(decl.kind))
						throw std::runtime_error("Type not found for base specifier");

					base.base_class = GetCXCursorName(decl);

					client.node.bases.emplace_back(std::move(base));

					return CXChildVisit_Continue;
				}

				// Start nested classes and structs
				if (cursor.kind == CXCursor_ClassDecl || cursor.kind == CXCursor_StructDecl)
				{
					RegisterClass(cursor);
					return CXChildVisit_Continue;
				}

				// Start nested enum
				if (cursor.kind == CXCursor_EnumDecl)
				{
					RegisterEnum(cursor);
					return CXChildVisit_Continue;
				}

				// Members
				if (cursor.kind == CXCursor_FieldDecl)
				{
					auto access = clang_getCXXAccessSpecifier(cursor);

					ClassNode::Member member;
					member.attrs = ParseCXCursorAttributes(cursor);

					if (member.attrs.size())
					{
						member.name = GetCXString(clang_getCursorSpelling(cursor));

						switch (access)
						{
							case CX_CXXPublic:
								member.visibility = ClassNode::Visibility::Public;
								break;
							case CX_CXXProtected:
								member.visibility = ClassNode::Visibility::Protected;
								break;
							case CX_CXXPrivate:
								member.visibility = ClassNode::Visibility::Private;
								break;
							default:
								throw std::runtime_error("Unexpected access specifier for member");
						}

						member.member_type = ClassNode::Member::MemberType::Member;

						member.type = RegisterType(clang_getCursorType(cursor));

						client.node.members.emplace_back(std::move(member));
					}

					return CXChildVisit_Continue;
				}

				// Variables
				if (cursor.kind == CXCursor_VarDecl)
				{
					auto access = clang_getCXXAccessSpecifier(cursor);

					ClassNode::Member member;
					member.attrs = ParseCXCursorAttributes(cursor);

					if (member.attrs.size())
					{
						member.name = GetCXString(clang_getCursorSpelling(cursor));

						switch (access)
						{
							case CX_CXXPublic:
								member.visibility = ClassNode::Visibility::Public;
								break;
							case CX_CXXProtected:
								member.visibility = ClassNode::Visibility::Protected;
								break;
							case CX_CXXPrivate:
								member.visibility = ClassNode::Visibility::Private;
								break;
							default:
								throw std::runtime_error("Unexpected access specifier for variable");
						}

						member.member_type = ClassNode::Member::MemberType::Static;

						member.type = RegisterType(clang_getCursorType(cursor));

						client.node.members.emplace_back(std::move(member));
					}

					return CXChildVisit_Continue;
				}

				// Functions
				if (cursor.kind == CXCursor_FunctionDecl)
				{
					auto access = clang_getCXXAccessSpecifier(cursor);
					auto storage = clang_Cursor_getStorageClass(cursor);

					ClassNode::Method method;
					method.attrs = ParseCXCursorAttributes(cursor);

					if (method.attrs.size())
					{
						method.name = GetCXString(clang_getCursorSpelling(cursor));

						switch (access)
						{
							case CX_CXXPublic:
								method.visibility = ClassNode::Visibility::Public;
								break;
							case CX_CXXProtected:
								method.visibility = ClassNode::Visibility::Protected;
								break;
							case CX_CXXPrivate:
								method.visibility = ClassNode::Visibility::Private;
								break;
							default:
								throw std::runtime_error("Unexpected access specifier for function");
						}

						if (client.friend_decl)
							method.method_type = ClassNode::Method::MethodType::Friend;
						else
							throw std::runtime_error("FunctionDecl in class without FriendDecl");

						method.return_type = RegisterType(clang_getCursorResultType(cursor));

						client.current_method = &(client.node.methods.emplace_back(std::move(method)));
						client.current_arg = nullptr;
						return CXChildVisit_Recurse;
					}
					else
					{
						client.current_method = nullptr;
						client.current_arg = nullptr;
						return CXChildVisit_Continue;
					}
				}

				// Methods
				if (cursor.kind == CXCursor_CXXMethod)
				{
					auto access = clang_getCXXAccessSpecifier(cursor);
					auto storage = clang_Cursor_getStorageClass(cursor);

					ClassNode::Method method;
					method.attrs = ParseCXCursorAttributes(cursor);

					if (method.attrs.size())
					{
						method.name = GetCXString(clang_getCursorSpelling(cursor));

						switch (access)
						{
							case CX_CXXPublic:
								method.visibility = ClassNode::Visibility::Public;
								break;
							case CX_CXXProtected:
								method.visibility = ClassNode::Visibility::Protected;
								break;
							case CX_CXXPrivate:
								method.visibility = ClassNode::Visibility::Private;
								break;
							default:
								throw std::runtime_error("Unexpected access specifier for method");
						}

						switch (storage)
						{
							case CX_SC_None:
								method.method_type = ClassNode::Method::MethodType::Method;
								break;
							case CX_SC_Static:
								method.method_type = ClassNode::Method::MethodType::Static;
								break;
							case CX_SC_Extern:
							case CX_SC_PrivateExtern:
							case CX_SC_OpenCLWorkGroupLocal:
							case CX_SC_Auto:
							case CX_SC_Register:
								throw std::runtime_error("Invalid CXXMethod storage class");
								break;
							default:
								throw std::runtime_error("Unexpected storage class for method");
						}

						method.q_const = clang_CXXMethod_isConst(cursor);
						method.q_virtual = clang_CXXMethod_isVirtual(cursor);
						method.q_pure = clang_CXXMethod_isPureVirtual(cursor);

						method.return_type = RegisterType(clang_getCursorResultType(cursor));

						client.current_method = &(client.node.methods.emplace_back(std::move(method)));
						client.current_arg = nullptr;
						return CXChildVisit_Recurse;
					}
					else
					{
						client.current_method = nullptr;
						client.current_arg = nullptr;
						return CXChildVisit_Continue;
					}
				}

				// Parameters
				if (cursor.kind == CXCursor_ParmDecl)
				{
					if (client.current_method == nullptr)
						throw std::runtime_error("ParmDecl without CXXMethod or FunctionDecl");
					client.current_arg = &(client.current_method->args.emplace_back());

					client.current_arg->name = GetCXString(clang_getCursorSpelling(cursor));
					client.current_arg->type = RegisterType(clang_getCursorType(cursor));

					client.current_arg->attrs = ParseCXCursorAttributes(cursor);

					return CXChildVisit_Continue;
				}

				// Update friend decl
				if (cursor.kind == CXCursor_FriendDecl)
					client.friend_decl = 2;

				return CXChildVisit_Recurse;
			};

		clang_visitChildren(cursor, visitor, &client);

		// Determine if this is an abstract class
		client.node.q_abstract = false;
		for (auto &i : client.node.methods)
		{
			if (i.q_pure)
				client.node.q_abstract = true;
		}

		class_nodes[name] = std::move(client.node);
	}

	return name;
}

// Function registry
std::unordered_map<std::string, FunctionNode> function_nodes;

static std::string RegisterFunction(CXCursor cursor)
{
	std::string name = GetCXCursorName(cursor);

	// Check if class was already registered
	auto it = class_nodes.find(name);
	if (it != class_nodes.end())
		return it->first;

	struct VisitorClient
	{
		FunctionNode node;

		FunctionNode::Arg *current_arg = nullptr;
	} client;

	client.node.attrs = ParseCXCursorAttributes(cursor);

	if (client.node.attrs.size())
	{
		client.node.name = name;

		client.node.return_type = RegisterType(clang_getCursorResultType(cursor));

		auto visitor = [](CXCursor cursor, CXCursor parent, CXClientData clientData) -> CXChildVisitResult
			{
				auto &client = *(reinterpret_cast<VisitorClient *>(clientData));

				// Parameters
				if (cursor.kind == CXCursor_ParmDecl)
				{
					client.current_arg = &(client.node.args.emplace_back());

					client.current_arg->name = GetCXString(clang_getCursorSpelling(cursor));
					client.current_arg->type = RegisterType(clang_getCursorType(cursor));

					client.current_arg->attrs = ParseCXCursorAttributes(cursor);

					return CXChildVisit_Continue;
				}

				return CXChildVisit_Recurse;
			};

		clang_visitChildren(cursor, visitor, &client);

		function_nodes[name] = std::move(client.node);
	}

	return name;
}

// Reset
void Reset()
{
	type_nodes.clear();
	enum_nodes.clear();
	class_nodes.clear();
	function_nodes.clear();
}

// Visitor
CXChildVisitResult Visitor(CXCursor cursor, CXCursor parent, CXClientData clientData)
{
	CXSourceLocation location = clang_getCursorLocation(cursor);

	if (!clang_Location_isFromMainFile(location))
		return CXChildVisit_Continue;

	unsigned int level_current = *(reinterpret_cast<unsigned int *>(clientData));
	unsigned int level_next = level_current + 1;
	std::string level_string = std::string(level_current, '-');

	if (cursor.kind == CXCursor_ClassTemplate)
	{
		// TODO
		return CXChildVisit_Continue;
	}

	if (cursor.kind == CXCursor_ClassTemplatePartialSpecialization)
	{
		// TODO
		return CXChildVisit_Continue;
	}

	if (cursor.kind == CXCursor_ClassDecl || cursor.kind == CXCursor_StructDecl)
	{
		RegisterClass(cursor);
		return CXChildVisit_Continue;
	}

	if (cursor.kind == CXCursor_EnumDecl)
	{
		RegisterEnum(cursor);
		return CXChildVisit_Continue;
	}

	if (cursor.kind == CXCursor_FunctionDecl)
	{
		RegisterFunction(cursor);
		return CXChildVisit_Continue;
	}

	clang_visitChildren(cursor, Visitor, &level_next);

	return CXChildVisit_Continue;
}

}
}
