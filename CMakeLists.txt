# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.8)

set(CMAKE_CXX_STANDARD 17)

project ("Final184ShadersTest")

add_library(imgui
	imgui/imgui.cpp
	imgui/imgui_demo.cpp
	imgui/imgui_draw.cpp
	imgui/imgui_widgets.cpp
)
target_include_directories(imgui INTERFACE imgui)

add_subdirectory(Foundation)
add_subdirectory(SPIRV-Cross)
add_subdirectory(RHI)

find_package(SDL2 REQUIRED)
find_package(glm REQUIRED)

include_directories(${GLM_INCLUDE_DIRS})
link_directories(${GLM_LIBRARY_DIRS})
add_definitions(${GLM_DEFINITIONS})

find_package(ASSIMP REQUIRED)
message(STATUS "Found ASSIMP in ${ASSIMP_INCLUDE_DIR}")
include_directories(${ASSIMP_INCLUDE_DIR})

add_executable (Final184ShadersTest "src/Final184ShadersTest.cpp" "src/imgui_impl_sdl.cpp")
target_compile_definitions(Final184ShadersTest PRIVATE -DAPP_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(Final184ShadersTest ${ASSIMP_LIBRARIES})
target_link_libraries(Final184ShadersTest Foundation)
target_link_libraries(Final184ShadersTest RHI)

find_package(SDL2 CONFIG REQUIRED)
target_link_libraries(Final184ShadersTest SDL2::SDL2 SDL2::SDL2main)
