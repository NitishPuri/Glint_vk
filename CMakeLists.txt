cmake_minimum_required(VERSION 3.20)
project(Glint)

set(CMAKE_CXX_STANDARD 20)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# MSVC defines _DEBUG for debug builds, other compilers don't.
# Config uses it to turn on logging + validation layers by default.
if (NOT MSVC)
    add_compile_definitions($<$<CONFIG:Debug>:_DEBUG>)
endif()

# set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /NODEFAULTLIB:MSVCRT")

# if(MSVC)
#   # Force consistent runtime library for all targets
#   set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL$<$<CONFIG:Debug>:Debug>")
  
#   # Add this to propagate the setting to all targets
#   add_compile_options(
#     $<$<CONFIG:>:/MD>
#     $<$<CONFIG:Debug>:/MDd>
#     $<$<CONFIG:Release>:/MD>
#   )
# endif()


# Vulkan
find_package(Vulkan REQUIRED)
if(NOT Vulkan_FOUND)
    message(FATAL_ERROR "Vulkan not found. Please make sure Vulkan SDK is installed correctly.")
endif()

################## Dependencies ##################
# Add external dependencies
add_subdirectory(ext)
################## Dependencies ##################

################## Shader Compilation ##################
add_subdirectory(src/shaders)
################## Shader Compilation ##################


# Add Glint libraries
add_subdirectory(src/glint_core)
add_subdirectory(src/glint_ui)
# add_subdirectory(src/glint_renderer)

# Sample browser
add_subdirectory(src/samples)
 
add_subdirectory(src/minimal)

#TODO: Add tests