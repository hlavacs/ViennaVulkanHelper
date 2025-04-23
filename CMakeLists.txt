cmake_minimum_required (VERSION 3.5.0)
project (
    "Vulkan Helper"
    VERSION 1.0.0
    DESCRIPTION "A set of wrapper functions over theVulkan API."
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

message("Compiler: " ${CMAKE_CXX_COMPILER_ID})

if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  	add_compile_options(/D IMGUI_IMPL_VULKAN_NO_PROTOTYPES)
	add_compile_options(/D_CRT_SECURE_NO_WARNINGS) #for assimp
	add_compile_options(/EHsc /DEBUG /Zi /Istb /MP) #for assimp
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
	add_compile_options(-D IMGUI_IMPL_VULKAN_NO_PROTOTYPES)
	add_compile_options(-Wno-nullability-completeness)
	add_compile_options(-D_CRT_SECURE_NO_WARNINGS) #for assimp
	add_compile_options(-fexperimental-library)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	add_compile_options(-D IMGUI_IMPL_VULKAN_NO_PROTOTYPES)
	add_compile_options(-Wno-nullability-completeness)
	add_compile_options(-D_CRT_SECURE_NO_WARNINGS) #for assimp
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	add_compile_options(-D IMGUI_IMPL_VULKAN_NO_PROTOTYPES)
	add_compile_options(-Wno-nullability-completeness)
	add_compile_options(-D_CRT_SECURE_NO_WARNINGS) #for assimp
endif()


#--------------------------------- Find VulkanSDK ---------------------------------#
# Vulkan SDK
find_package(Vulkan REQUIRED)
set(VULKAN_SDK $ENV{VULKAN_SDK})

if (WIN32)
	set(VULKAN_INCLUDE Include)
	set(VULKAN_LIB Lib)
elseif (APPLE)
	set(VULKAN_INCLUDE include)
    set(VULKAN_LIB lib)
elseif (UNIX AND NOT APPLE)
	set(VULKAN_INCLUDE include)
	set(VULKAN_LIB lib)
endif()

include_directories(${VULKAN_SDK}/${VULKAN_INCLUDE})
include_directories(${VULKAN_SDK}/${VULKAN_INCLUDE}/volk)
include_directories(${VULKAN_SDK}/${VULKAN_INCLUDE}/vma)
link_directories(${VULKAN_SDK}/lib)

#--------------------------------- Fetch necessary Git repos ---------------------------------#

include(FetchContent) # enable Fetch Content

#--------------------------------- Fetch SDL3 ---------------------------------#

set(SDL_STATIC ON)
set(SDL_SHARED OFF)
FetchContent_Declare(sdl3
                     GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
                     GIT_TAG main
)
FetchContent_MakeAvailable(sdl3)
include_directories (${sdl3_SOURCE_DIR}/include)

#--------------------------------- Fetch SDL Mixer ---------------------------------#

set(SDL3_DIR ${SDL3_BINARY_DIR})
set(BUILD_SHARED_LIBS OFF)
FetchContent_Declare(sdlmixer
                    GIT_REPOSITORY https://github.com/libsdl-org/SDL_mixer.git
                    GIT_TAG main
)
FetchContent_MakeAvailable(sdlmixer)
include_directories (${sdlmixer_SOURCE_DIR}/include)

#--------------------------------- Fetch ImGui ---------------------------------#

FetchContent_Declare(imgui
                     GIT_REPOSITORY https://github.com/ocornut/imgui.git 
                     GIT_TAG master)

FetchContent_MakeAvailable(Imgui)
# ImGui does not provide CMakeLists.txt
add_library(imgui)
target_include_directories(imgui SYSTEM PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
target_include_directories(imgui SYSTEM PRIVATE ${Vulkan_INCLUDE_DIRS})
target_compile_definitions(imgui PRIVATE IMGUI_IMPL_VULKAN_NO_PROTOTYPES)
target_link_libraries(imgui SDL3::SDL3)
target_sources(imgui PRIVATE
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
)
# available now as imgui

#--------------------------------- Fetch vk-bootstrap ---------------------------------#
FetchContent_Declare(vkbootstrap
                     GIT_REPOSITORY https://github.com/charles-lunarg/vk-bootstrap.git 
                     GIT_TAG main)
FetchContent_MakeAvailable(vkbootstrap)
# available now as vk-bootstrap::vk-bootstrap

#--------------------------------- Fetch glm ---------------------------------#

FetchContent_Declare(glm
                     GIT_REPOSITORY https://github.com/g-truc/glm.git 
                     GIT_TAG master)
FetchContent_MakeAvailable(glm)
include_directories(${glm_SOURCE_DIR}/src)

# Fetch GitHub projects and set variables for include directories
include(FetchContent) # enable Fetch Content

#--------------------------------- Fetch stb image ---------------------------------#
FetchContent_Declare(stb_image
                     GIT_REPOSITORY https://github.com/nothings/stb.git
                     GIT_TAG master
)
FetchContent_MakeAvailable(stb_image)
include_directories (${stb_image_SOURCE_DIR})

#--------------------------------- Main Target ---------------------------------#

set(INCLUDE ${PROJECT_SOURCE_DIR}/include)
include_directories(${INCLUDE})
add_subdirectory(examples)

