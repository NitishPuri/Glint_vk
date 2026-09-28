cmake_minimum_required(VERSION 3.25)

set(THIRD_PARTY_DIR "ext")


# GLFW - Pre-compiled binaries
add_library(glfw INTERFACE)
set(GLFW_PATH ${CMAKE_CURRENT_SOURCE_DIR}/glfw-3.4.bin.WIN64)
target_include_directories(glfw INTERFACE ${GLFW_PATH}/include)
target_link_directories(glfw INTERFACE ${GLFW_PATH}/lib-vc2022)
target_link_libraries(glfw INTERFACE glfw3)
set_target_properties(glfw PROPERTIES FOLDER ${THIRD_PARTY_DIR})

# STB - headers only
add_library(stb INTERFACE)
target_include_directories(stb INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/stb)
set_target_properties(stb PROPERTIES 
    FOLDER ${THIRD_PARTY_DIR}
)

# TinyObjLoader - headers only
add_library(tinyobjloader INTERFACE)
target_include_directories(tinyobjloader INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/tinyobjloader)
set_target_properties(tinyobjloader PROPERTIES FOLDER ${THIRD_PARTY_DIR})

# arcball camera - headers only
add_library(arcball_camera INTERFACE)
target_include_directories(arcball_camera INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/arcball_camera)
set_target_properties(arcball_camera PROPERTIES FOLDER ${THIRD_PARTY_DIR})


# Imgui - compiled as a static library
add_library(imgui STATIC
    imgui/imgui.cpp
    # imgui/imgui_demo.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp
    imgui/backends/imgui_impl_glfw.cpp
    imgui/backends/imgui_impl_vulkan.cpp
)

target_include_directories(imgui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui/backends
)

# Find and link required libraries for ImGui
# find_package(Vulkan REQUIRED)
target_link_libraries(imgui PUBLIC Vulkan::Vulkan glfw)
set_target_properties(imgui PROPERTIES FOLDER ${THIRD_PARTY_DIR})


# ktx
set(KTX_DIR ${CMAKE_CURRENT_SOURCE_DIR}/ktx)
set(KTX_SOURCES
    ${KTX_DIR}/lib/texture.c
    ${KTX_DIR}/lib/hashlist.c
    ${KTX_DIR}/lib/checkheader.c
    ${KTX_DIR}/lib/swap.c
    ${KTX_DIR}/lib/memstream.c
    ${KTX_DIR}/lib/filestream.c
    ${KTX_DIR}/lib/vkloader.c)
add_library(ktx STATIC ${KTX_SOURCES})
target_include_directories(ktx PUBLIC 
    ${KTX_DIR}/include
    ${KTX_DIR}/other_include
)
target_link_libraries(ktx PUBLIC Vulkan::Vulkan)
set_target_properties(ktx PROPERTIES FOLDER ${THIRD_PARTY_DIR})

# tinygltf
set(TINYGLTF_DIR ${CMAKE_CURRENT_SOURCE_DIR}/tinygltf)
add_library(tinygltf INTERFACE)
target_include_directories(tinygltf INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/tinygltf)
set_target_properties(tinygltf PROPERTIES FOLDER ${THIRD_PARTY_DIR})
