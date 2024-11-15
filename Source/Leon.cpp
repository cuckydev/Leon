/*
 * [ Leon ]
 *   Source/Leon.cpp
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
#include "Process.h"

#include <sstream>
#include <fstream>
#include <filesystem>
#include <cstring>
#include <memory>

// Get standardized path
struct StdPath
{
	std::filesystem::path path;
	std::string utf8;
};

static StdPath GetStdPath(const std::string &src)
{
	StdPath out;
	out.path = std::filesystem::path(src);
	if (!std::filesystem::exists(out.path))
		throw std::runtime_error("File \"" + src + "\" doesn't exist");
	else
		out.path = std::filesystem::canonical(out.path);

	// TODO: I think this throws exceptions with non-ANSI characters with MSVC
	// I don't have a particularly convenient way to test that right now though.
	out.utf8 = out.path.string();
	for (auto &i : out.utf8)
		if (i == '\\')
			i = '/';

	return out;
}

// Parse a CMake list
static std::vector<std::string> ParseCMakeList(const std::string &src)
{
	std::vector<std::string> result;
	std::stringstream ss(src);
	std::string token;

	while (std::getline(ss, token, ';'))
		result.push_back(token);

	return result;
}

// Clean a path so that it's always relative
static void CleanPath(std::filesystem::path &path)
{
	if (path.has_root_path())
	{
		auto temp = path.native();
		for (auto &i : temp)
		{
			if (i == '/' || i == '\\')
				i = '_';
			if (i == ':')
				i = '_';
		}
		path = std::filesystem::path(temp);
	}
}

// Entry point
int main(int argc, char **argv)
{
	try
	{
		// Print Leon information
		std::cout << "========================================" << '\n';
		std::cout << "Leon (" LEON_VERSION ")" << '\n';
		std::cout << "libclang: " << Leon::Parse::GetCXString(clang_getClangVersion()) << '\n';
		std::cout << "========================================" << std::endl;

		if (argc < 4)
		{
			std::cout << "Usage: " << argv[0] << " <binary_dir> <process.lua> [options] <source>" << std::endl;
			return -1;
		}

		int argi = 1;

		std::filesystem::path binary_dir = std::filesystem::path(std::string(argv[argi++]));
		StdPath lua_std = GetStdPath(argv[argi++]);

		std::vector<std::string> in_includes;
		std::vector<std::string> in_defines;

		std::vector<StdPath> source_std;

		// Create project directory
		std::filesystem::create_directories(binary_dir);
		
		// Parse options
		std::string out_extension, glue_extension;

		std::string current_option;

		for (; argi < argc; argi++)
		{
			std::string args(argv[argi]);

			if (current_option.empty())
			{
				if (args == "-include")
					current_option = args;
				else if (args == "-define")
					current_option = args;
				else if (args == "-out_extension")
					current_option = args;
				else if (args == "-glue_extension")
					current_option = args;
				else
					break;
			}
			else
			{
				if (current_option == "-include")
				{
					// Parse includes
					auto arg_includes = ParseCMakeList(args);
					in_includes.insert(in_includes.end(), arg_includes.begin(), arg_includes.end());
				}
				else if (current_option == "-define")
				{
					// Parse defines
					auto arg_defines = ParseCMakeList(args);
					in_defines.insert(in_defines.end(), arg_defines.begin(), arg_defines.end());
				}
				else if (current_option == "-out_extension")
				{
					out_extension = args;
				}
				else if (current_option == "-glue_extension")
				{
					glue_extension = args;
				}
				current_option.clear();
			}
		}

		// Setup arguments
		static_assert(sizeof(std::unique_ptr<char[]>) == sizeof(char *));
		std::vector<std::unique_ptr<char[]>> args;
		{
			auto args_c = [&args](const char *str) -> void
				{
					size_t size = strlen(str) + 1;
					std::unique_ptr<char[]> data = std::make_unique<char[]>(size);
					memcpy(data.get(), str, size);
					args.emplace_back(std::move(data));
				};
			auto args_s = [&args_c, &args](const std::string &str) -> void
				{
					args_c(str.c_str());
				};

			args_c("-x"); args_c("c++");
			args_c("-std=c++20");

			args_c("-D_LEON_PROC");

			args_c("-fhosted");
			args_c("-fcxx-exceptions");
			args_c("-fexceptions");

			// Include our system headers
#define LEON_SYSTEM_INCLUDE_FRAME(header) args_c("-isystem"); args_c( header );
#include <LeonSystemIncludeFrame.h>
#undef LEON_SYSTEM_INCLUDE_FRAME

			// Include our provided headers
			for (auto &i : in_includes)
				args_s(std::string("-I") + i);

			// Define our provided defines
			for (auto &d : in_defines)
				args_s(std::string("-D") + d);
		}

		// Decide where to put the glue
		std::filesystem::path glue_name = binary_dir / ("glue" + glue_extension);
		bool rebuild_glue = false;

		if (!std::filesystem::exists(glue_name))
			rebuild_glue = true;
		else if (std::filesystem::last_write_time(lua_std.path) > std::filesystem::last_write_time(glue_name))
			rebuild_glue = true;

		// Parse source arguments
		struct SourceArgument
		{
			StdPath std;
			std::filesystem::path binary_dir;
			std::filesystem::path out_name;
			bool rebuild = false;
		};

		std::vector<SourceArgument> source_args;

		for (; argi < argc; argi++)
		{
			auto sources = ParseCMakeList(std::string(argv[argi]));
			for (auto &i : sources)
			{
				// Get standard path of source file
				SourceArgument source_arg;
				source_arg.std = GetStdPath(i);

				// Get the binary path of the source file
				std::filesystem::path in_path = source_arg.std.path;
				CleanPath(in_path);

				source_arg.binary_dir = binary_dir / in_path;
				std::filesystem::create_directories(source_arg.binary_dir);

				// Check if we should rebuild the output file
				source_arg.out_name = source_arg.binary_dir / ("out" + out_extension);

				if (!std::filesystem::exists(source_arg.out_name))
				{
					source_arg.rebuild = true;
				}
				else
				{
					bool source_modified = std::filesystem::last_write_time(source_arg.std.path) > std::filesystem::last_write_time(source_arg.out_name);
					bool process_modified = std::filesystem::last_write_time(lua_std.path) > std::filesystem::last_write_time(source_arg.out_name);

					if (source_modified || process_modified)
						source_arg.rebuild = true;
				}

				source_args.emplace_back(std::move(source_arg));
			}
		}

		if (source_args.size() == 0)
			throw std::runtime_error("Given no sources.");

		// Load and compile lua source
		std::stringstream lua_sstream;
		{
			std::ifstream lua_stream(lua_std.path);
			lua_sstream << lua_stream.rdbuf();
		}

		std::unique_ptr<lua_State, void (*)(lua_State *)> GL(luaL_newstate(), lua_close);
		luaL_openlibs(GL.get());

		lua_State *L = lua_newthread(GL.get());

		std::string bytecode = Luau::compile(lua_sstream.str());
		if (luau_load(L, "=in", bytecode.data(), bytecode.size(), 0) != 0)
		{
			size_t len;
			const char *msg = lua_tolstring(L, -1, &len);

			std::string error(msg, len);
			lua_pop(L, 1);

			throw std::runtime_error("Lua process failed to compile: " + error);
		}

		// Setup thread
		// The stack now contains the function that will execute the loaded bytecode
		lua_State *T = lua_newthread(L);
		lua_pushvalue(L, -2);
		lua_remove(L, -3);
		lua_xmove(L, T, 1);

		int thread_status = lua_resume(T, nullptr, 0);

		lua_pop(L, 1); // Remove thread off main stack

		if (thread_status != LUA_OK)
		{
			std::string error;
			if (thread_status == LUA_YIELD)
				error = "thread yielded unexpectedly";
			else if (const char *str = lua_tostring(T, -1))
				error = str;

			error += "\nstack backtrace:\n";
			error += lua_debugtrace(T);

			throw std::runtime_error("Lua process failed to execute: " + error);
		}

		// Check for the table off the stack
		if (lua_gettop(T) == 0 || !lua_istable(T, -1))
			throw std::runtime_error("Lua process did not return `table`");

		// Parse sources
		for (auto &source : source_args)
		{
			// Get shorthand name
			std::string short_name = source.std.path.filename().string();

			if (!source.rebuild)
			{
				std::cout << "[ `" << short_name << "` up to date ]" << '\n';
				continue;
			}
			else
			{
				std::cout << "[ Generating `" << short_name << "` ]" << '\n';
			}

			// Parse in libclang
			{
				CXIndex index = clang_createIndex(0, 0);
				CXTranslationUnit tu;
				CXErrorCode ec;

				// Load up the source file
				CXTranslationUnit_Flags flags = static_cast<CXTranslationUnit_Flags>(CXTranslationUnit_SkipFunctionBodies | CXTranslationUnit_Incomplete);
				ec = clang_parseTranslationUnit2(index, source.std.path.string().c_str(), reinterpret_cast<const char *const *>(args.data()), args.size(), nullptr, 0, flags, &tu);
				
				// Check diagnostics
				size_t num_diagnostics = clang_getNumDiagnostics(tu);
				if (num_diagnostics != 0)
				{
					std::cout << std::flush;

					for (unsigned int i = 0; i < num_diagnostics; i++)
					{
						auto diagnostic = clang_getDiagnostic(tu, i);
						auto severity = clang_getDiagnosticSeverity(diagnostic);
						switch (severity)
						{
							case CXDiagnostic_Ignored:
								break;
							case CXDiagnostic_Note:
							case CXDiagnostic_Warning:
							case CXDiagnostic_Error:
							case CXDiagnostic_Fatal:
								std::cerr << Leon::Parse::GetCXString(clang_formatDiagnostic(diagnostic, clang_defaultDiagnosticDisplayOptions())) << '\n';
								break;
						}
						clang_disposeDiagnostic(diagnostic);

						if (severity == CXDiagnostic_Error || severity == CXDiagnostic_Fatal)
							throw std::runtime_error("Source parsing ran into a fatal error. See above.");
					}

					std::cerr << std::flush;
				}

				// Check if the translation unit failed, but wasn't caught by a diagnostic
				if (ec != CXError_Success)
				{
					std::string problem;
					switch (ec)
					{
						case CXError_Failure:
							problem = "Failure";
							break;
						case CXError_Crashed:
							problem = "Crashed";
							break;
						case CXError_InvalidArguments:
							problem = "Invalid Arguments";
							break;
						case CXError_ASTReadError:
							problem = "AST Read Error";
							break;
						default:
							problem = std::to_string(ec);
							break;
					}
					throw std::runtime_error(problem + " wasn't caught by a diagnostic.");
				}

				// Parse the AST
				Leon::Parse::Reset();

				CXCursor rootCursor = clang_getTranslationUnitCursor(tu);

				unsigned int treeLevel = 0;

				clang_visitChildren(rootCursor, Leon::Parse::Visitor, &treeLevel);

				clang_disposeTranslationUnit(tu);
				clang_disposeIndex(index);
			}

			// Process in Lua process
			{
				// Get SourceProcess function
				lua_pushstring(T, "SourceProcess");
				lua_gettable(T, -2);

				// Run SourceProcess
				lua_pushstring(T, source.std.utf8.c_str()); // source
				Leon::Process::ConstructLuaTables(T); // types, enums, classes, functions

				thread_status = lua_pcall(T, 5, 1, 0);

				if (thread_status == 0)
				{
					// Get result string
					if (!lua_isstring(T, -1))
						throw std::runtime_error("Lua process did not return `string`");
					std::string output = lua_tostring(T, -1);
					lua_pop(T, 1);

					// Write output file
					std::ofstream output_stream(source.out_name, std::ios::binary);
					if (!output_stream)
						throw std::runtime_error("Failed to open output: " + source.out_name.string());

					output_stream.write(output.data(), output.size());
				}
				else
				{
					std::string error;
					if (thread_status == LUA_YIELD)
						error = "thread yielded unexpectedly";
					else if (const char *str = lua_tostring(T, -1))
						error = str;

					error += "\nstack backtrace:\n";
					error += lua_debugtrace(T);

					throw std::runtime_error("Lua process failed to execute: " + error);
				}
			}
		}

		// Generate glue
		if (!rebuild_glue)
		{
			std::cout << "[ `glue` up to date ]" << '\n';
		}
		else
		{
			std::cout << "[ Generating `glue` ]" << '\n';

			// Get GlueProcess function
			lua_pushstring(T, "GlueProcess");
			lua_gettable(T, -2);

			// Build sources table
			lua_newtable(T);

			int source_i = 1;
			for (auto &source : source_args)
			{
				lua_pushnumber(T, source_i++);
				lua_newtable(T);

				Leon::Process::LuaTableSetString(T, -1, "source", source.std.utf8.c_str());

				StdPath out_std_path = GetStdPath(source.out_name.string());
				Leon::Process::LuaTableSetString(T, -1, "out", out_std_path.utf8.c_str());

				lua_settable(T, -3);
			}

			// Run SourceProcess
			thread_status = lua_pcall(T, 1, 1, 0);

			if (thread_status == 0)
			{
				// Get result string
				if (!lua_isstring(T, -1))
					throw std::runtime_error("Lua process did not return `string`");
				std::string output = lua_tostring(T, -1);

				// Write output file
				std::ofstream output_stream(glue_name, std::ios::binary);
				if (!output_stream)
					throw std::runtime_error("Failed to open output: " + glue_name.string());

				output_stream.write(output.data(), output.size());
			}
			else
			{
				std::string error;
				if (thread_status == LUA_YIELD)
					error = "thread yielded unexpectedly";
				else if (const char *str = lua_tostring(T, -1))
					error = str;

				error += "\nstack backtrace:\n";
				error += lua_debugtrace(T);

				throw std::runtime_error("Lua process failed to execute: " + error);
			}
		}
	}
	catch (std::exception &e)
	{
		std::cout << std::flush;
		std::cerr << std::endl;

		std::cerr << "========================================" << '\n';
		std::cerr << "Leon generator failed!" << '\n';
		std::cerr << "========================================" << '\n';
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}

	return 0;
}
