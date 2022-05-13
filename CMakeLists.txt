cmake_minimum_required(VERSION 3.21)

project(UnnamedRendererDx12 LANGUAGES CXX RC)

if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
	message(FATAL_ERROR "Unnamed Renderer (DX12) requires a 64-bit Windows toolchain.")
endif()

set(SOURCE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/source")
set(INCLUDE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include")
set(SHADER_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
set(BINARY_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")

set(SHADER_FILES
	"${SHADER_DIRECTORY}/RP1ComputeShader.hlsl"
	"${SHADER_DIRECTORY}/RP2ComputeShader.hlsl"
	"${SHADER_DIRECTORY}/RP3ComputeShader.hlsl"
	"${SHADER_DIRECTORY}/RTCommon.hlsli"
)

set(RUNTIME_DLLS
	"${BINARY_DIRECTORY}/dxcompiler.dll"
	"${BINARY_DIRECTORY}/dxil.dll"
)

source_group("Shaders" FILES ${SHADER_FILES})

set_source_files_properties(${SHADER_FILES} PROPERTIES HEADER_FILE_ONLY TRUE VS_TOOL_OVERRIDE "None")

function(add_renderer TARGET_NAME OUTPUT_NAME SCENE_HEADER)
	add_executable(${TARGET_NAME} WIN32
		"${SOURCE_DIRECTORY}/Unnamed Renderer (DX12).cpp"
		"${SOURCE_DIRECTORY}/Unnamed Renderer (DX12).rc"
		"${INCLUDE_DIRECTORY}/DirectXStuff.hpp"
		"${INCLUDE_DIRECTORY}/WinRTStuff.hpp"
		"${INCLUDE_DIRECTORY}/Scene.hpp"
		"${INCLUDE_DIRECTORY}/${SCENE_HEADER}"
		"${INCLUDE_DIRECTORY}/resource.h"
		${SHADER_FILES}
	)

	target_include_directories(${TARGET_NAME} PRIVATE "${INCLUDE_DIRECTORY}")

	target_compile_definitions(${TARGET_NAME} PRIVATE UNICODE _UNICODE $<$<CONFIG:Debug>:_DEBUG> $<$<NOT:$<CONFIG:Debug>>:NDEBUG>
		"UNNAMED_RENDERER_IMAGES_DIRECTORY=\"${CMAKE_CURRENT_SOURCE_DIR}/Images\"" ${ARGN})

	target_compile_options(${TARGET_NAME} PRIVATE /std:c++latest /permissive- /W3 /MP /utf-8)

	target_link_libraries(${TARGET_NAME} PRIVATE d3d12 dxgi dxguid dxcompiler windowscodecs)

	set_target_properties(${TARGET_NAME} PROPERTIES OUTPUT_NAME "${OUTPUT_NAME}")

	add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
		COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${SHADER_FILES} ${RUNTIME_DLLS} "$<TARGET_FILE_DIR:${TARGET_NAME}>"
		COMMAND_EXPAND_LISTS
	)

	add_custom_target(run-${TARGET_NAME}
		COMMAND "$<TARGET_FILE:${TARGET_NAME}>"
		WORKING_DIRECTORY "$<TARGET_FILE_DIR:${TARGET_NAME}>"
		DEPENDS ${TARGET_NAME}
		USES_TERMINAL
	)
endfunction()

add_renderer(CornellBoxRenderer "Unnamed Renderer (DX12) - Cornell Box" "CornellBoxScene.hpp")
add_renderer(RTIOWRenderer "Unnamed Renderer (DX12) - RTIOW" "RTIOWScene.hpp" RTIOW_SCENE)
add_renderer(NeonGalleryRenderer "Unnamed Renderer (DX12) - Neon Gallery" "NeonGalleryScene.hpp" NEON_GALLERY_SCENE)

set_property(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" PROPERTY VS_STARTUP_PROJECT CornellBoxRenderer)
