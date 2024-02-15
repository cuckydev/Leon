# Leon
Leon provides C++ reflection utilizing Lua scripting to process the data.

This project is still in a very early state, so contributions are very much welcome.

# Building and usage
Simply add Leon as a Git submodule and import it in your project as a CMake subdirectory.

All Leon needs is a Lua script and the headers you want it to process.

# Dependencies
Leon depends on LLVM libclang 16.0.0+.
It will attempt to find an install on your system, otherwise you can provide one yourself in [ThirdParty/libclang](ThirdParty/libclang).
