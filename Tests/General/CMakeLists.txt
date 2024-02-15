# Compile MyCoolGame
project(MyCoolGame LANGUAGES CXX)

add_executable(MyCoolGame
	"Source/MyCoolGame.cpp"

	"Source/AppleComponent.h"
	"Source/CoolComponent.h"
)

target_include_directories(MyCoolGame PUBLIC "Source")

target_link_libraries(MyCoolGame PUBLIC Leon)

leon_target(MyCoolGame_Leon "${CMAKE_CURRENT_BINARY_DIR}/LeonProject" MyCoolGame "${CMAKE_CURRENT_SOURCE_DIR}/Process.lua" ".cpp" ".cpp"
	"Source/AppleComponent.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/Source/CoolComponent.h"
)
leon_target_outputs(MyCoolGame_Leon MyCoolGame)
leon_target_glue(MyCoolGame_Leon MyCoolGame)

add_dependencies(MyCoolGame MyCoolGame_Leon)
