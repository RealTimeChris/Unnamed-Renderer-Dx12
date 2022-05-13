// Unnamed Renderer (DX12).cpp - Hobby path-tracer.
// Apr 2020
// Chris M.
// https://github.com/RealTimeChris

#include "DirectXStuff.hpp"
#if defined(RTIOW_SCENE)
	#include "RTIOWScene.hpp"
#elif defined(NEON_GALLERY_SCENE)
	#include "NeonGalleryScene.hpp"
#else
	#include "CornellBoxScene.hpp"
#endif
#include "WinRTStuff.hpp"
#include <objbase.h>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <format>
#include <wincodec.h>
#pragma comment(lib, "windowscodecs.lib")

int __stdcall wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int) {
	wchar_t executablePath[MAX_PATH]{};
	GetModuleFileNameW(nullptr, executablePath, MAX_PATH);
	std::filesystem::current_path(std::filesystem::path{ executablePath }.parent_path());

	/*
		GLOBAL APPLICATION/RENDERING STUFF.
	*/

	/*
		COORDINATE SPACES:
			TS - Thread-Space
			PS - Pixel-Space
			WS - World-Space
	*/

	// For verifying function results.
	HRESULT result{ S_OK };

	// For encoding rendered frames to disk as .jpg via WIC.
	result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
	direct_x_stuff::result_check(result, L"CoInitializeEx() failed.", L"COM Initialization Error");

	// Unique DirectX 12 Interfaces.
	direct_x_stuff::factory factory{};
	direct_x_stuff::dxc_library dxcLibrary{};
	direct_x_stuff::dxc_compiler dxcCompiler{};
	direct_x_stuff::d3d12_debug_controller d3dDebugger{};
	direct_x_stuff::device device{ L"Device" };
	direct_x_stuff::command_queue commandQueue{ device.get_interface(), D3D12_COMMAND_QUEUE_PRIORITY_HIGH, L"CommandQueue" };
	direct_x_stuff::fence fence{ device.get_interface(), L"Fence" };

	using scene_stuff::sphere;
	using scene_stuff::rectangle;
	using scene_stuff::triangle_primitive;

	scene_stuff::scene currentScene{ scene_stuff::build_scene() };
	std::vector<sphere>& spheres{ currentScene.spheres };
	std::vector<rectangle>& rectangles{ currentScene.rectangles };
	std::vector<triangle_primitive>& triangles{ currentScene.triangles };

	// View port dimesions, in pixels.
	constexpr uint3 psViewPortDimensions{ 1280u, 720u, 1u };
	constexpr uint bytesPerFinalPixel{ 4u };

	// Values for defining and mapping the workload.
	constexpr uint secondsToRender{ 1u };
	constexpr uint framesPerSecond{ 2u };
	constexpr uint finalFrameCount{ framesPerSecond * secondsToRender };
	constexpr uint samplesPerPixel{ 1000u };
	constexpr uint samplesPerSubmission{ 16u };
	constexpr uint3 tsGridDimensions{ psViewPortDimensions.x, psViewPortDimensions.y, psViewPortDimensions.z };
	constexpr uint3 tsGroupDimensions{ 128u, 8u, 1u };
	constexpr uint3 gridDimensionsByGroup{ direct_x_stuff::set_group_count_per_grid(tsGridDimensions, tsGroupDimensions) };

	// World-Space View Port Values.
	constexpr float wsViewPortAspectRatio{ ( float )psViewPortDimensions.x / ( float )psViewPortDimensions.y };

	// Camera definition values.
	const float vFoVInRadians{ (currentScene.vFoVInDegrees / 180.0f) * ( float )M_PI };
	const float wsViewPortHeight{ 2.0f * ( float )tan(vFoVInRadians / 2.0f) };
	const float wsViewPortWidth{ wsViewPortHeight * wsViewPortAspectRatio };

	// Maximum path-tracing recursion depth.
	constexpr uint maxRecursionDepth{ 10u };

	// Resource Values.
	constexpr uint3 intersectionMapDimensions{ psViewPortDimensions.x, psViewPortDimensions.y, maxRecursionDepth };
	constexpr uint2 accumulationFrameDimensions{ psViewPortDimensions.x, psViewPortDimensions.y };
	constexpr uint2 finalFrameDimensions{ psViewPortDimensions.x, psViewPortDimensions.y };

	// 32-bit Root Constants to be passed from Host to Device/Shader via Root Signature(s).
	struct inline_root_constants {
		uint3 tsGridDimensions;
		float padding00;
		float3 wsCameraLookFrom;
		float padding01;
		float3 wsCameraLookAt;
		float padding02;
		float3 wsCameraUp;
		float padding03;
		float2 wsViewPortDimensions;
		float wsViewPortZCoord;
		float padding04;
		uint maxRecursionDepth;
		uint samplesPerPixel;
		uint maxSampleIndex;
		uint currentSampleIndex;
		float pathMinDistance;
		float pathMaxDistance;
		float2 padding05;
		float3 skyTopColor;
		float padding06;
		float3 skyBottomColor;
		float padding07;
		uint sphereCount;
		uint rectangleCount;
		uint triangleCount;
		float globalTickInRadians;
		float lensRadius;
		float focusDistance;
	};

	constexpr uint rootConstantCount{ sizeof(inline_root_constants) / sizeof(float) };

	inline_root_constants inlineRootConstants{};

	inlineRootConstants.tsGridDimensions	   = tsGridDimensions;
	inlineRootConstants.wsCameraLookFrom	   = currentScene.wsCameraLookFrom;
	inlineRootConstants.wsCameraLookAt		   = currentScene.wsCameraLookAt;
	inlineRootConstants.wsCameraUp			   = currentScene.wsCameraUp;
	inlineRootConstants.wsViewPortDimensions.x = wsViewPortWidth;
	inlineRootConstants.wsViewPortDimensions.y = wsViewPortHeight;
	inlineRootConstants.maxRecursionDepth	   = maxRecursionDepth;
	inlineRootConstants.samplesPerPixel		   = samplesPerPixel;
	inlineRootConstants.maxSampleIndex		   = samplesPerPixel - 1u;
	inlineRootConstants.currentSampleIndex	   = 0u;
	inlineRootConstants.pathMinDistance		   = 0.001f;
	inlineRootConstants.pathMaxDistance		   = 10'000.0f;
	inlineRootConstants.skyTopColor			   = currentScene.skyTopColor;
	inlineRootConstants.skyBottomColor		   = currentScene.skyBottomColor;
	inlineRootConstants.globalTickInRadians	   = 0.0f;
	inlineRootConstants.lensRadius			   = currentScene.aperture * 0.5f;
	inlineRootConstants.focusDistance		   = currentScene.focusDistance;

	inlineRootConstants.sphereCount	   = static_cast<uint>(spheres.size());
	inlineRootConstants.rectangleCount = static_cast<uint>(rectangles.size());
	inlineRootConstants.triangleCount  = static_cast<uint>(triangles.size());

	/*
		GLOBAL PIPELINE RESOURCES/BARRIERS/COPY LOCATIONS:
			-L0 Spheres Buffer, L1 Spheres Buffer
			-L0 Rectangles Buffer, L1 Rectangles Buffer
			-L0 Triangles Buffer, L1 Triangles Buffer
			-L1 Intersection Map 01/02/03 3DTexture (Depth = Recursion Depth)
			-L1 Accumulation Frame 2DTexture
			-Host Final Frame Buffers (Count = Final Frame Count)
			-L0 Final Frame Buffer
			-L1 Final Frame 2DTexture
	*/

	// Spheres upload buffer.

	// Spheres upload buffer.

	direct_x_stuff::buffer_config l0SpheresBufferConfig{};
	l0SpheresBufferConfig.cpuPageProperty	   = D3D12_CPU_PAGE_PROPERTY_WRITE_COMBINE;
	l0SpheresBufferConfig.initialResourceState = D3D12_RESOURCE_STATE_COPY_DEST;
	l0SpheresBufferConfig.memoryPool		   = D3D12_MEMORY_POOL_L0;
	l0SpheresBufferConfig.bufferWidth		   = static_cast<uint32_t>(spheres.size() * sizeof(sphere));

	direct_x_stuff::buffer l0SpheresBuffer{ device.get_interface(), l0SpheresBufferConfig, L"L0SpheresBuffer" };

	D3D12_RESOURCE_BARRIER l0SpheresBufferCopyDestToCopySource{};
	l0SpheresBufferCopyDestToCopySource =
		direct_x_stuff::create_resource_transition_barrier(l0SpheresBuffer.get_interface(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COPY_SOURCE);

	D3D12_RESOURCE_BARRIER l0SpheresBufferCopySourceToCopyDest{};
	l0SpheresBufferCopySourceToCopyDest =
		direct_x_stuff::create_resource_transition_barrier(l0SpheresBuffer.get_interface(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COPY_DEST);

	// Spheres device buffer.
	// Spheres device buffer.
	direct_x_stuff::buffer_config l1SpheresBufferConfig{};
	l1SpheresBufferConfig.cpuPageProperty	   = D3D12_CPU_PAGE_PROPERTY_NOT_AVAILABLE;
	l1SpheresBufferConfig.initialResourceState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	l1SpheresBufferConfig.memoryPool		   = D3D12_MEMORY_POOL_L1;
	l1SpheresBufferConfig.bufferWidth		   = static_cast<uint32_t>(spheres.size() * sizeof(sphere));

	direct_x_stuff::buffer l1SpheresBuffer{ device.get_interface(), l1SpheresBufferConfig, L"L1SpheresBuffer" };

	D3D12_RESOURCE_BARRIER l1SpheresBufferCopyDestToUnorderedAccess{};
	l1SpheresBufferCopyDestToUnorderedAccess =
		direct_x_stuff::create_resource_transition_barrier(l1SpheresBuffer.get_interface(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	D3D12_RESOURCE_BARRIER l1SpheresBufferUnorderedAccessToCopyDest{};
	l1SpheresBufferUnorderedAccessToCopyDest =
		direct_x_stuff::create_resource_transition_barrier(l1SpheresBuffer.get_interface(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_DEST);

	// Rectangles upload buffer.
	direct_x_stuff::buffer_config l0RectanglesBufferConfig{};
	l0RectanglesBufferConfig.cpuPageProperty	  = D3D12_CPU_PAGE_PROPERTY_WRITE_COMBINE;
	l0RectanglesBufferConfig.initialResourceState = D3D12_RESOURCE_STATE_COPY_DEST;
	l0RectanglesBufferConfig.memoryPool			  = D3D12_MEMORY_POOL_L0;
	l0RectanglesBufferConfig.bufferWidth		  = static_cast<uint32_t>(rectangles.size() * sizeof(rectangle));

	direct_x_stuff::buffer l0RectanglesBuffer{ device.get_interface(), l0RectanglesBufferConfig, L"L0RectanglesBuffer" };

	D3D12_RESOURCE_BARRIER l0RectanglesBufferCopyDestToCopySource{};
	l0RectanglesBufferCopyDestToCopySource =
		direct_x_stuff::create_resource_transition_barrier(l0RectanglesBuffer.get_interface(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COPY_SOURCE);

	D3D12_RESOURCE_BARRIER l0RectanglesBufferCopySourceToCopyDest{};
	l0RectanglesBufferCopySourceToCopyDest =
		direct_x_stuff::create_resource_transition_barrier(l0RectanglesBuffer.get_interface(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COPY_DEST);

	// Rectangles device buffer.
	direct_x_stuff::buffer_config l1RectanglesBufferConfig{};
	l1RectanglesBufferConfig.cpuPageProperty	  = D3D12_CPU_PAGE_PROPERTY_NOT_AVAILABLE;
	l1RectanglesBufferConfig.initialResourceState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	l1RectanglesBufferConfig.memoryPool			  = D3D12_MEMORY_POOL_L1;
	l1RectanglesBufferConfig.bufferWidth		  = static_cast<uint32_t>(rectangles.size() * sizeof(rectangle));

	direct_x_stuff::buffer l1RectanglesBuffer{ device.get_interface(), l1RectanglesBufferConfig, L"L1RectanglesBuffer" };

	D3D12_RESOURCE_BARRIER l1RectanglesBufferCopyDestToUnorderedAccess{};
	l1RectanglesBufferCopyDestToUnorderedAccess =
		direct_x_stuff::create_resource_transition_barrier(l1RectanglesBuffer.get_interface(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	D3D12_RESOURCE_BARRIER l1RectanglesBufferUnorderedAccessToCopyDest{};
	l1RectanglesBufferUnorderedAccessToCopyDest =
		direct_x_stuff::create_resource_transition_barrier(l1RectanglesBuffer.get_interface(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_DEST);

	// Triangles upload buffer.
	direct_x_stuff::buffer_config l0TrianglesBufferConfig{};
	l0TrianglesBufferConfig.cpuPageProperty		 = D3D12_CPU_PAGE_PROPERTY_WRITE_COMBINE;
	l0TrianglesBufferConfig.initialResourceState = D3D12_RESOURCE_STATE_COPY_DEST;
	l0TrianglesBufferConfig.memoryPool			 = D3D12_MEMORY_POOL_L0;
	l0TrianglesBufferConfig.bufferWidth			 = static_cast<uint32_t>(triangles.size() * sizeof(triangle_primitive));

	direct_x_stuff::buffer l0TrianglesBuffer{ device.get_interface(), l0TrianglesBufferConfig, L"L0TrianglesBuffer" };

	D3D12_RESOURCE_BARRIER l0TrianglesBufferCopyDestToCopySource{};
	l0TrianglesBufferCopyDestToCopySource =
		direct_x_stuff::create_resource_transition_barrier(l0TrianglesBuffer.get_interface(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COPY_SOURCE);

	D3D12_RESOURCE_BARRIER l0TrianglesBufferCopySourceToCopyDest{};
	l0TrianglesBufferCopySourceToCopyDest =
		direct_x_stuff::create_resource_transition_barrier(l0TrianglesBuffer.get_interface(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COPY_DEST);

	// Triangles device buffer.
	direct_x_stuff::buffer_config l1TrianglesBufferConfig{};
	l1TrianglesBufferConfig.cpuPageProperty		 = D3D12_CPU_PAGE_PROPERTY_NOT_AVAILABLE;
	l1TrianglesBufferConfig.initialResourceState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	l1TrianglesBufferConfig.memoryPool			 = D3D12_MEMORY_POOL_L1;
	l1TrianglesBufferConfig.bufferWidth			 = static_cast<uint32_t>(triangles.size() * sizeof(triangle_primitive));

	direct_x_stuff::buffer l1TrianglesBuffer{ device.get_interface(), l1TrianglesBufferConfig, L"L1TrianglesBuffer" };

	D3D12_RESOURCE_BARRIER l1TrianglesBufferCopyDestToUnorderedAccess{};
	l1TrianglesBufferCopyDestToUnorderedAccess =
		direct_x_stuff::create_resource_transition_barrier(l1TrianglesBuffer.get_interface(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	D3D12_RESOURCE_BARRIER l1TrianglesBufferUnorderedAccessToCopyDest{};
	l1TrianglesBufferUnorderedAccessToCopyDest =
		direct_x_stuff::create_resource_transition_barrier(l1TrianglesBuffer.get_interface(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_DEST);

	// Intersection Maps for recording path-object intersections, to be produced during RP1.
	direct_x_stuff::texture_3d_config l1IntersectionMap013dTextureConfig{};
	l1IntersectionMap013dTextureConfig.cpuPageProperty		= D3D12_CPU_PAGE_PROPERTY_NOT_AVAILABLE;
	l1IntersectionMap013dTextureConfig.memoryPool			= D3D12_MEMORY_POOL_L1;
	l1IntersectionMap013dTextureConfig.nodeMask				= 0u;
	l1IntersectionMap013dTextureConfig.initialResourceState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	l1IntersectionMap013dTextureConfig.textureFormat		= DXGI_FORMAT_R32G32B32A32_FLOAT;
	l1IntersectionMap013dTextureConfig.textureWidth			= intersectionMapDimensions.x;
	l1IntersectionMap013dTextureConfig.textureHeight		= intersectionMapDimensions.y;
	l1IntersectionMap013dTextureConfig.textureDepth			= intersectionMapDimensions.z;

	direct_x_stuff::texture_3d l1IntersectionMap013dTexture{ device.get_interface(), l1IntersectionMap013dTextureConfig, L"L1IntersectionMap013DTexture" };

	direct_x_stuff::texture_3d_config l1IntersectionMap023dTextureConfig{};
	l1IntersectionMap023dTextureConfig.cpuPageProperty		= D3D12_CPU_PAGE_PROPERTY_NOT_AVAILABLE;
	l1IntersectionMap023dTextureConfig.memoryPool			= D3D12_MEMORY_POOL_L1;
	l1IntersectionMap023dTextureConfig.nodeMask				= 0u;
	l1IntersectionMap023dTextureConfig.initialResourceState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	l1IntersectionMap023dTextureConfig.textureFormat		= DXGI_FORMAT_R32G32B32A32_FLOAT;
	l1IntersectionMap023dTextureConfig.textureWidth			= intersectionMapDimensions.x;
	l1IntersectionMap023dTextureConfig.textureHeight		= intersectionMapDimensions.y;
	l1IntersectionMap023dTextureConfig.textureDepth			= intersectionMapDimensions.z;

	direct_x_stuff::texture_3d l1IntersectionMap023dTexture{ device.get_interface(), l1IntersectionMap023dTextureConfig, L"L1IntersectionMap023DTexture" };

	direct_x_stuff::texture_3d_config l1IntersectionMap033dTextureConfig{};
	l1IntersectionMap033dTextureConfig.cpuPageProperty		= D3D12_CPU_PAGE_PROPERTY_NOT_AVAILABLE;
	l1IntersectionMap033dTextureConfig.memoryPool			= D3D12_MEMORY_POOL_L1;
	l1IntersectionMap033dTextureConfig.nodeMask				= 0u;
	l1IntersectionMap033dTextureConfig.initialResourceState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	l1IntersectionMap033dTextureConfig.textureFormat		= DXGI_FORMAT_R32G32B32A32_UINT;
	l1IntersectionMap033dTextureConfig.textureWidth			= intersectionMapDimensions.x;
	l1IntersectionMap033dTextureConfig.textureHeight		= intersectionMapDimensions.y;
	l1IntersectionMap033dTextureConfig.textureDepth			= intersectionMapDimensions.z;

	direct_x_stuff::texture_3d l1IntersectionMap033dTexture{ device.get_interface(), l1IntersectionMap033dTextureConfig, L"L1IntersectionMap033DTexture" };

	// Accumulation Frame for pooling the Samples for each Pixel, to be pushed out during RP2.
	direct_x_stuff::texture_2d_config l1AccumulationFrame2dTextureConfig{};
	l1AccumulationFrame2dTextureConfig.cpuPageProperty		= D3D12_CPU_PAGE_PROPERTY_NOT_AVAILABLE;
	l1AccumulationFrame2dTextureConfig.memoryPool			= D3D12_MEMORY_POOL_L1;
	l1AccumulationFrame2dTextureConfig.nodeMask				= 0u;
	l1AccumulationFrame2dTextureConfig.initialResourceState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	l1AccumulationFrame2dTextureConfig.textureFormat		= DXGI_FORMAT_R32G32B32A32_FLOAT;
	l1AccumulationFrame2dTextureConfig.textureWidth			= accumulationFrameDimensions.x;
	l1AccumulationFrame2dTextureConfig.textureHeight		= accumulationFrameDimensions.y;

	direct_x_stuff::texture_2d l1AccumulationFrame2dTexture{ device.get_interface(), l1AccumulationFrame2dTextureConfig, L"L1AccumulationFrame2DTexture" };

	// Collection of Host-side Final Frame Buffers, to be produced during RP3.
	direct_x_stuff::r8g8b8a8_uint_host_buffer_config* hostFinalFrameBufferConfig[finalFrameCount]{ nullptr };
	uint bufferLengthInElements{ finalFrameDimensions.x * finalFrameDimensions.y };

	for (int64_t i{ 0 }; i < finalFrameCount; i++) {
		unsigned __int8 green{ ( unsigned __int8 )(255.0f * (( float )i / ( float )finalFrameCount)) };
		direct_x_stuff::r8g8b8a8_uint initialColor = { 0u, green, 0u };

		hostFinalFrameBufferConfig[i] = new direct_x_stuff::r8g8b8a8_uint_host_buffer_config{ bufferLengthInElements, initialColor };
	}

	direct_x_stuff::r8g8b8a8_uint_host_buffer* hostFinalFrameBuffers[finalFrameCount]{ nullptr };

	for (int64_t i{ 0 }; i < finalFrameCount; i++) {
		hostFinalFrameBuffers[i] = new direct_x_stuff::r8g8b8a8_uint_host_buffer{ *hostFinalFrameBufferConfig[i] };
	}

	// Final Frame Buffer, upload/download version.
	direct_x_stuff::buffer_config l0FinalFrameBufferConfig{};
	l0FinalFrameBufferConfig.cpuPageProperty	  = D3D12_CPU_PAGE_PROPERTY_WRITE_COMBINE;
	l0FinalFrameBufferConfig.memoryPool			  = D3D12_MEMORY_POOL_L0;
	l0FinalFrameBufferConfig.initialResourceState = D3D12_RESOURCE_STATE_COPY_DEST;
	l0FinalFrameBufferConfig.bufferFormat		  = DXGI_FORMAT_UNKNOWN;
	l0FinalFrameBufferConfig.bufferWidth		  = (finalFrameDimensions.x * finalFrameDimensions.y) * bytesPerFinalPixel;

	direct_x_stuff::buffer l0FinalFrameBuffer{ device.get_interface(), l0FinalFrameBufferConfig, L"L0FinalFrameBuffer" };

	D3D12_RESOURCE_BARRIER l0FinalFrameBufferCopyDestToCopySource{};
	l0FinalFrameBufferCopyDestToCopySource =
		direct_x_stuff::create_resource_transition_barrier(l0FinalFrameBuffer.get_interface(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COPY_SOURCE);

	D3D12_RESOURCE_BARRIER l0FinalFrameBufferCopySourceToCopyDest{};
	l0FinalFrameBufferCopySourceToCopyDest =
		direct_x_stuff::create_resource_transition_barrier(l0FinalFrameBuffer.get_interface(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COPY_DEST);

	D3D12_TEXTURE_COPY_LOCATION l0FinalFrameBufferTextureCopyLocation{};
	l0FinalFrameBufferTextureCopyLocation.pResource							 = l0FinalFrameBuffer.get_interface();
	l0FinalFrameBufferTextureCopyLocation.Type								 = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	l0FinalFrameBufferTextureCopyLocation.PlacedFootprint.Offset			 = 0u;
	l0FinalFrameBufferTextureCopyLocation.PlacedFootprint.Footprint.Format	 = DXGI_FORMAT_R8G8B8A8_UNORM;
	l0FinalFrameBufferTextureCopyLocation.PlacedFootprint.Footprint.Width	 = finalFrameDimensions.x;
	l0FinalFrameBufferTextureCopyLocation.PlacedFootprint.Footprint.Height	 = finalFrameDimensions.y;
	l0FinalFrameBufferTextureCopyLocation.PlacedFootprint.Footprint.Depth	 = 1u;
	l0FinalFrameBufferTextureCopyLocation.PlacedFootprint.Footprint.RowPitch = finalFrameDimensions.x * bytesPerFinalPixel;

	// Final Frame 2DTexture, Device version.
	direct_x_stuff::texture_2d_config l1FinalFrame2dTextureConfig{};
	l1FinalFrame2dTextureConfig.cpuPageProperty		 = D3D12_CPU_PAGE_PROPERTY_NOT_AVAILABLE;
	l1FinalFrame2dTextureConfig.memoryPool			 = D3D12_MEMORY_POOL_L1;
	l1FinalFrame2dTextureConfig.initialResourceState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	l1FinalFrame2dTextureConfig.nodeMask			 = 0u;
	l1FinalFrame2dTextureConfig.textureFormat		 = DXGI_FORMAT_R8G8B8A8_UNORM;
	l1FinalFrame2dTextureConfig.textureWidth		 = finalFrameDimensions.x;
	l1FinalFrame2dTextureConfig.textureHeight		 = finalFrameDimensions.y;

	direct_x_stuff::texture_2d l1FinalFrame2dTexture{ device.get_interface(), l1FinalFrame2dTextureConfig, L"L1FinalFrame2DTexture" };

	D3D12_RESOURCE_BARRIER l1FinalFrame2dTextureUnorderedAccessToCopySource{};
	l1FinalFrame2dTextureUnorderedAccessToCopySource =
		direct_x_stuff::create_resource_transition_barrier(l1FinalFrame2dTexture.get_interface(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);

	D3D12_RESOURCE_BARRIER l1FinalFrame2dTextureCopySourceToUnorderedAccess{};
	l1FinalFrame2dTextureCopySourceToUnorderedAccess =
		direct_x_stuff::create_resource_transition_barrier(l1FinalFrame2dTexture.get_interface(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	D3D12_RESOURCE_BARRIER l1FinalFrame2dTextureUnorderedAccessToCopyDest{};
	l1FinalFrame2dTextureUnorderedAccessToCopyDest =
		direct_x_stuff::create_resource_transition_barrier(l1FinalFrame2dTexture.get_interface(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_DEST);

	D3D12_RESOURCE_BARRIER l1FinalFrame2dTextureCopyDestToCopySource{};
	l1FinalFrame2dTextureCopyDestToCopySource =
		direct_x_stuff::create_resource_transition_barrier(l1FinalFrame2dTexture.get_interface(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COPY_SOURCE);

	D3D12_TEXTURE_COPY_LOCATION l1FinalFrame2dTextureTextureCopyLocation{};
	l1FinalFrame2dTextureTextureCopyLocation.pResource		  = l1FinalFrame2dTexture.get_interface();
	l1FinalFrame2dTextureTextureCopyLocation.Type			  = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	l1FinalFrame2dTextureTextureCopyLocation.SubresourceIndex = 0u;

	/*
		GLOBAL PIPELINE DESCRIPTOR HEAP:
			Resource 0 - L1SpheresBuffer
			Resource 1 - L1RectanglesBuffer
			Resource 2 - L1TrianglesBuffer
			Resource 3 - Unused (null UAV)
			Resource 4 - L1IntersectionMap013DTexture - WStDistance + WSIntersectionPoint
			Resource 5 - L1IntersectionMap023DTexture - Unused + WSIncomingDirection
			Resource 6 - L1IntersectionMap033DTexture - PrimitiveID + ObjectID + MaterialID + CurrentRecursionDepth
			Resource 7 - L1AccumulationFrame2DTexture
			Resource 8 - L1FinalFrame2DTexture
	*/

	const uint globalDescriptorCount{ 9u };
	const uint64_t descriptorHandleIncrementSize{ device.get_interface()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) };
	uint currentDescriptorIndex{ 0u };

	direct_x_stuff::descriptor_heap globalDescriptorHeap{ device.get_interface(), globalDescriptorCount, L"GlobalDescriptorHeap" };

	D3D12_CPU_DESCRIPTOR_HANDLE l1SpheresBufferUavCpuHandle{};

	D3D12_UNORDERED_ACCESS_VIEW_DESC l1SpheresBufferUavDesc{};
	l1SpheresBufferUavDesc.ViewDimension			   = D3D12_UAV_DIMENSION_BUFFER;
	l1SpheresBufferUavDesc.Format					   = l1SpheresBufferConfig.bufferFormat;
	l1SpheresBufferUavDesc.Buffer.FirstElement		   = 0u;
	l1SpheresBufferUavDesc.Buffer.NumElements		   = inlineRootConstants.sphereCount;
	l1SpheresBufferUavDesc.Buffer.CounterOffsetInBytes = 0u;
	l1SpheresBufferUavDesc.Buffer.Flags				   = D3D12_BUFFER_UAV_FLAG_NONE;
	l1SpheresBufferUavDesc.Buffer.StructureByteStride  = sizeof(sphere);

	l1SpheresBufferUavCpuHandle.ptr = globalDescriptorHeap.get_interface()->GetCPUDescriptorHandleForHeapStart().ptr + (currentDescriptorIndex * descriptorHandleIncrementSize);

	device.get_interface()->CreateUnorderedAccessView(l1SpheresBuffer.get_interface(), nullptr, &l1SpheresBufferUavDesc, l1SpheresBufferUavCpuHandle);

	currentDescriptorIndex++;

	D3D12_CPU_DESCRIPTOR_HANDLE l1RectanglesBufferUavCpuHandle{};

	D3D12_UNORDERED_ACCESS_VIEW_DESC l1RectanglesBufferUavDesc{};
	l1RectanglesBufferUavDesc.ViewDimension				  = D3D12_UAV_DIMENSION_BUFFER;
	l1RectanglesBufferUavDesc.Format					  = l1RectanglesBufferConfig.bufferFormat;
	l1RectanglesBufferUavDesc.Buffer.FirstElement		  = 0u;
	l1RectanglesBufferUavDesc.Buffer.NumElements		  = inlineRootConstants.rectangleCount;
	l1RectanglesBufferUavDesc.Buffer.CounterOffsetInBytes = 0u;
	l1RectanglesBufferUavDesc.Buffer.Flags				  = D3D12_BUFFER_UAV_FLAG_NONE;
	l1RectanglesBufferUavDesc.Buffer.StructureByteStride  = sizeof(rectangle);

	l1RectanglesBufferUavCpuHandle.ptr = globalDescriptorHeap.get_interface()->GetCPUDescriptorHandleForHeapStart().ptr + (currentDescriptorIndex * descriptorHandleIncrementSize);

	device.get_interface()->CreateUnorderedAccessView(l1RectanglesBuffer.get_interface(), nullptr, &l1RectanglesBufferUavDesc, l1RectanglesBufferUavCpuHandle);

	currentDescriptorIndex++;

	D3D12_CPU_DESCRIPTOR_HANDLE l1TrianglesBufferUavCpuHandle{};

	D3D12_UNORDERED_ACCESS_VIEW_DESC l1TrianglesBufferUavDesc{};
	l1TrianglesBufferUavDesc.ViewDimension				 = D3D12_UAV_DIMENSION_BUFFER;
	l1TrianglesBufferUavDesc.Format						 = l1TrianglesBufferConfig.bufferFormat;
	l1TrianglesBufferUavDesc.Buffer.FirstElement		 = 0u;
	l1TrianglesBufferUavDesc.Buffer.NumElements			 = inlineRootConstants.triangleCount;
	l1TrianglesBufferUavDesc.Buffer.CounterOffsetInBytes = 0u;
	l1TrianglesBufferUavDesc.Buffer.Flags				 = D3D12_BUFFER_UAV_FLAG_NONE;
	l1TrianglesBufferUavDesc.Buffer.StructureByteStride	 = sizeof(triangle_primitive);

	l1TrianglesBufferUavCpuHandle.ptr = globalDescriptorHeap.get_interface()->GetCPUDescriptorHandleForHeapStart().ptr + (currentDescriptorIndex * descriptorHandleIncrementSize);

	device.get_interface()->CreateUnorderedAccessView(l1TrianglesBuffer.get_interface(), nullptr, &l1TrianglesBufferUavDesc, l1TrianglesBufferUavCpuHandle);

	currentDescriptorIndex++;

	D3D12_CPU_DESCRIPTOR_HANDLE nullUavCpuHandle{};

	D3D12_UNORDERED_ACCESS_VIEW_DESC nullUavDesc{};
	nullUavDesc.ViewDimension		  = D3D12_UAV_DIMENSION_TEXTURE3D;
	nullUavDesc.Format				  = DXGI_FORMAT_R32_FLOAT;
	nullUavDesc.Texture3D.FirstWSlice = 0u;
	nullUavDesc.Texture3D.MipSlice	  = 0u;
	nullUavDesc.Texture3D.WSize		  = 1u;

	nullUavCpuHandle.ptr = globalDescriptorHeap.get_interface()->GetCPUDescriptorHandleForHeapStart().ptr + (currentDescriptorIndex * descriptorHandleIncrementSize);

	device.get_interface()->CreateUnorderedAccessView(nullptr, nullptr, &nullUavDesc, nullUavCpuHandle);

	currentDescriptorIndex++;

	D3D12_CPU_DESCRIPTOR_HANDLE l1IntersectionMap013dTextureUavCpuHandle{};

	D3D12_UNORDERED_ACCESS_VIEW_DESC l1IntersectionMap013dTextureUavDesc{};
	l1IntersectionMap013dTextureUavDesc.ViewDimension		  = D3D12_UAV_DIMENSION_TEXTURE3D;
	l1IntersectionMap013dTextureUavDesc.Format				  = l1IntersectionMap013dTextureConfig.textureFormat;
	l1IntersectionMap013dTextureUavDesc.Texture3D.FirstWSlice = 0u;
	l1IntersectionMap013dTextureUavDesc.Texture3D.MipSlice	  = 0u;
	l1IntersectionMap013dTextureUavDesc.Texture3D.WSize		  = l1IntersectionMap013dTextureConfig.textureDepth;

	l1IntersectionMap013dTextureUavCpuHandle.ptr =
		globalDescriptorHeap.get_interface()->GetCPUDescriptorHandleForHeapStart().ptr + (currentDescriptorIndex * descriptorHandleIncrementSize);

	device.get_interface()->CreateUnorderedAccessView(l1IntersectionMap013dTexture.get_interface(), nullptr, &l1IntersectionMap013dTextureUavDesc,
		l1IntersectionMap013dTextureUavCpuHandle);

	currentDescriptorIndex++;

	D3D12_CPU_DESCRIPTOR_HANDLE l1IntersectionMap023dTextureUavCpuHandle{};

	D3D12_UNORDERED_ACCESS_VIEW_DESC l1IntersectionMap023dTextureUavDesc{};
	l1IntersectionMap023dTextureUavDesc.ViewDimension		  = D3D12_UAV_DIMENSION_TEXTURE3D;
	l1IntersectionMap023dTextureUavDesc.Format				  = l1IntersectionMap023dTextureConfig.textureFormat;
	l1IntersectionMap023dTextureUavDesc.Texture3D.FirstWSlice = 0u;
	l1IntersectionMap023dTextureUavDesc.Texture3D.MipSlice	  = 0u;
	l1IntersectionMap023dTextureUavDesc.Texture3D.WSize		  = l1IntersectionMap023dTextureConfig.textureDepth;

	l1IntersectionMap023dTextureUavCpuHandle.ptr =
		globalDescriptorHeap.get_interface()->GetCPUDescriptorHandleForHeapStart().ptr + (currentDescriptorIndex * descriptorHandleIncrementSize);

	device.get_interface()->CreateUnorderedAccessView(l1IntersectionMap023dTexture.get_interface(), nullptr, &l1IntersectionMap023dTextureUavDesc,
		l1IntersectionMap023dTextureUavCpuHandle);

	currentDescriptorIndex++;

	D3D12_CPU_DESCRIPTOR_HANDLE l1IntersectionMap033dTextureUavCpuHandle{};

	D3D12_UNORDERED_ACCESS_VIEW_DESC l1IntersectionMap033dTextureUavDesc{};
	l1IntersectionMap033dTextureUavDesc.ViewDimension		  = D3D12_UAV_DIMENSION_TEXTURE3D;
	l1IntersectionMap033dTextureUavDesc.Format				  = l1IntersectionMap033dTextureConfig.textureFormat;
	l1IntersectionMap033dTextureUavDesc.Texture3D.FirstWSlice = 0u;
	l1IntersectionMap033dTextureUavDesc.Texture3D.MipSlice	  = 0u;
	l1IntersectionMap033dTextureUavDesc.Texture3D.WSize		  = l1IntersectionMap033dTextureConfig.textureDepth;

	l1IntersectionMap033dTextureUavCpuHandle.ptr =
		globalDescriptorHeap.get_interface()->GetCPUDescriptorHandleForHeapStart().ptr + (currentDescriptorIndex * descriptorHandleIncrementSize);

	device.get_interface()->CreateUnorderedAccessView(l1IntersectionMap033dTexture.get_interface(), nullptr, &l1IntersectionMap033dTextureUavDesc,
		l1IntersectionMap033dTextureUavCpuHandle);

	currentDescriptorIndex++;

	D3D12_CPU_DESCRIPTOR_HANDLE l1AccumulationFrame2dTextureUavCpuHandle{};

	D3D12_UNORDERED_ACCESS_VIEW_DESC l1AccumulationFrame2dTextureUavDesc{};
	l1AccumulationFrame2dTextureUavDesc.ViewDimension		 = D3D12_UAV_DIMENSION_TEXTURE2D;
	l1AccumulationFrame2dTextureUavDesc.Format				 = l1AccumulationFrame2dTextureConfig.textureFormat;
	l1AccumulationFrame2dTextureUavDesc.Texture2D.MipSlice	 = 0u;
	l1AccumulationFrame2dTextureUavDesc.Texture2D.PlaneSlice = 0u;

	l1AccumulationFrame2dTextureUavCpuHandle.ptr =
		globalDescriptorHeap.get_interface()->GetCPUDescriptorHandleForHeapStart().ptr + (currentDescriptorIndex * descriptorHandleIncrementSize);

	device.get_interface()->CreateUnorderedAccessView(l1AccumulationFrame2dTexture.get_interface(), nullptr, &l1AccumulationFrame2dTextureUavDesc,
		l1AccumulationFrame2dTextureUavCpuHandle);

	currentDescriptorIndex++;

	D3D12_CPU_DESCRIPTOR_HANDLE l1FinalFrame2dTextureUavCpuHandle{};

	D3D12_UNORDERED_ACCESS_VIEW_DESC l1FinalFrame2dTextureUavDesc{};
	l1FinalFrame2dTextureUavDesc.ViewDimension		  = D3D12_UAV_DIMENSION_TEXTURE2D;
	l1FinalFrame2dTextureUavDesc.Format				  = l1FinalFrame2dTextureConfig.textureFormat;
	l1FinalFrame2dTextureUavDesc.Texture2D.MipSlice	  = 0u;
	l1FinalFrame2dTextureUavDesc.Texture2D.PlaneSlice = 0u;

	l1FinalFrame2dTextureUavCpuHandle.ptr =
		globalDescriptorHeap.get_interface()->GetCPUDescriptorHandleForHeapStart().ptr + (currentDescriptorIndex * descriptorHandleIncrementSize);

	device.get_interface()->CreateUnorderedAccessView(l1FinalFrame2dTexture.get_interface(), nullptr, &l1FinalFrame2dTextureUavDesc, l1FinalFrame2dTextureUavCpuHandle);

	currentDescriptorIndex++;

	/*
		GLOBAL PIPELINE ROOT SIGNATURE:
			Parameter 0 - Descriptor Table with a single UAV Descriptor Range
			Parameter 1 - 32-bit Inline Root Constants (Max = 60)
	*/

	direct_x_stuff::blob rootSignatureBlob{}, rootSignatureErrorBlob{};
	direct_x_stuff::root_signature globalRootSignature{ device.get_interface(), rootSignatureBlob.get_interface(), rootSignatureErrorBlob.get_interface(), globalDescriptorCount,
		L"GlobalRootSignature" };

	/*
		RENDER-PASS 1: Compute Shader, Pipeline State, Command Allocator, Graphics Command List
			-Generates an Intersection Map to be used for later recursion to collect final sample/pixel colors.
			-IntersectionMaps = Records Path-Object intersections as Incoming Path Direction and Object Id.
	*/

	direct_x_stuff::shader_config rp1ComputeShaderConfig{};
	rp1ComputeShaderConfig.shaderFileName	= L"RP1ComputeShader.hlsl";
	rp1ComputeShaderConfig.shaderEntryPoint = L"compute_main";
	rp1ComputeShaderConfig.targetProfile	= L"cs_6_3";

	direct_x_stuff::shader rp1ComputeShader{ dxcLibrary.get_interface(), dxcCompiler.get_interface(), rp1ComputeShaderConfig };

	direct_x_stuff::pipeline_state rp1PipelineState{ device.get_interface(), globalRootSignature.get_interface(), rp1ComputeShader.get_shader_byte_code_size(),
		rp1ComputeShader.get_shader_byte_code(), L"RP1PipelineState" };

	direct_x_stuff::command_allocator rp1CommandAllocator{ device.get_interface(), L"RP1CommandAllocator" };

	direct_x_stuff::graphics_command_list rp1GraphicsCommandList{ device.get_interface(), rp1CommandAllocator.get_interface(), L"RP1GraphicsCommandList" };

	/*
		RENDER-PASS 2: Compute Shader, Pipeline State, Command Allocator, Graphics Command List
			-Generates an Accumulation Frame. One per Final Frame.
			-Accumulation Frame = Summation of Samples on a Per-Color-Channel-Per-Pixel basis.
			-Recurses backwards through the Intersection Maps to collect the final Pixel-Colors.
	*/

	direct_x_stuff::shader_config rp2ComputeShaderConfig{};
	rp2ComputeShaderConfig.shaderFileName	= L"RP2ComputeShader.hlsl";
	rp2ComputeShaderConfig.shaderEntryPoint = L"compute_main";
	rp2ComputeShaderConfig.targetProfile	= L"cs_6_3";

	direct_x_stuff::shader rp2ComputeShader{ dxcLibrary.get_interface(), dxcCompiler.get_interface(), rp2ComputeShaderConfig };

	direct_x_stuff::pipeline_state rp2PipelineState{ device.get_interface(), globalRootSignature.get_interface(), rp2ComputeShader.get_shader_byte_code_size(),
		rp2ComputeShader.get_shader_byte_code(), L"RP2PipelineState" };

	/*
		RENDER-PASS 3: Compute Shader, Pipeline State, Command Allocator, Graphics Command List
			-Collects the final values from each Accumulation Frame, and properly converts them
				into the correct format, then saves them in Final Frames.
	*/

	direct_x_stuff::shader_config rp3ComputeShaderConfig{};
	rp3ComputeShaderConfig.shaderFileName	= L"RP3ComputeShader.hlsl";
	rp3ComputeShaderConfig.shaderEntryPoint = L"compute_main";
	rp3ComputeShaderConfig.targetProfile	= L"cs_6_3";

	direct_x_stuff::shader rp3ComputeShader{ dxcLibrary.get_interface(), dxcCompiler.get_interface(), rp3ComputeShaderConfig };

	direct_x_stuff::pipeline_state rp3PipelineState{ device.get_interface(), globalRootSignature.get_interface(), rp3ComputeShader.get_shader_byte_code_size(),
		rp3ComputeShader.get_shader_byte_code(), L"RP3PipelineState" };

	direct_x_stuff::command_allocator rp3CommandAllocator{ device.get_interface(), L"RP3CommandAllocator" };

	direct_x_stuff::graphics_command_list rp3GraphicsCommandList{ device.get_interface(), rp3CommandAllocator.get_interface(), L"RP3GraphicsCommandList" };

	/*
		PRESENTATION: Theatre Window Class, Theatre Window, Factory, Swap Chain, Back Buffer Resources,
			Present Command Allocator, Present Graphics Command List
			-Will be used to Present Final Frames to the Theatre Window.
	*/

	// Prep for Presentation.
	win_rt_stuff::window_class theatreWindowClass{};
	win_rt_stuff::render_window theatreWindow{ theatreWindowClass.report_class_name(), psViewPortDimensions.x, psViewPortDimensions.y, L"Unnamed Renderer" };

	const uint swapChainBackBufferCount{ 2u };
	const uint swapChainSyncInterval{ 1u };
	uint currentBackBufferIndex{ 0u };

	direct_x_stuff::swap_chain swapChain{ factory.get_interface(), commandQueue.get_interface(), theatreWindow.report_window_handle(), psViewPortDimensions.x,
		psViewPortDimensions.y, swapChainBackBufferCount, l1FinalFrame2dTextureConfig.textureFormat };

	ID3D12Resource* swapChainBackBuffers[swapChainBackBufferCount]{ nullptr };

	currentBackBufferIndex = swapChain.get_interface()->GetCurrentBackBufferIndex();

	result = swapChain.get_interface()->GetBuffer(( UINT )currentBackBufferIndex, __uuidof(ID3D12Resource), std::bit_cast<void**>(&swapChainBackBuffers[currentBackBufferIndex]));
	direct_x_stuff::result_check(result, L"GetBuffer() failed.", L"SwapChain Prep Error");

	result = swapChainBackBuffers[currentBackBufferIndex]->SetName(L"SwapChainBackBuffer00");
	direct_x_stuff::result_check(result, L"SetName() failed.", L"SwapChain Prep Error");

	result = swapChain.get_interface()->Present(swapChainSyncInterval, NULL);
	direct_x_stuff::result_check(result, L"Present() failed.", L"SwapChain Prep Error");

	fence.flush_command_queue(commandQueue.get_interface());

	currentBackBufferIndex = swapChain.get_interface()->GetCurrentBackBufferIndex();

	result = swapChain.get_interface()->GetBuffer(( UINT )currentBackBufferIndex, __uuidof(ID3D12Resource), std::bit_cast<void**>(&swapChainBackBuffers[currentBackBufferIndex]));
	direct_x_stuff::result_check(result, L"GetBuffer() failed.", L"SwapChain Prep Error");

	result = swapChainBackBuffers[currentBackBufferIndex]->SetName(L"SwapChainBackBuffer01");
	direct_x_stuff::result_check(result, L"SetName() failed.", L"SwapChain Prep Error");

	result = swapChain.get_interface()->Present(swapChainSyncInterval, NULL);
	direct_x_stuff::result_check(result, L"Present() failed.", L"SwapChain Prep Error");

	fence.flush_command_queue(commandQueue.get_interface());

	D3D12_RESOURCE_BARRIER swapChainBackBufferPresentToCopyDestBarriers[swapChainBackBufferCount]{};

	for (int64_t i{ 0 }; i < swapChainBackBufferCount; i++) {
		swapChainBackBufferPresentToCopyDestBarriers[i] =
			direct_x_stuff::create_resource_transition_barrier(swapChainBackBuffers[i], D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_DEST);
	}

	D3D12_RESOURCE_BARRIER swapChainBackBufferCopyDestToPresentBarriers[swapChainBackBufferCount]{};

	for (int64_t i{ 0 }; i < swapChainBackBufferCount; i++) {
		swapChainBackBufferCopyDestToPresentBarriers[i] =
			direct_x_stuff::create_resource_transition_barrier(swapChainBackBuffers[i], D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PRESENT);
	}

	direct_x_stuff::command_allocator presentCommandAllocator{ device.get_interface(), L"PresentCommandAllocator" };

	direct_x_stuff::graphics_command_list presentGraphicsCommandList{ device.get_interface(), presentCommandAllocator.get_interface(), L"PresentGraphicsCommandList" };

	/*
		APPLICATION EVENT-LOOP:
	*/

	MSG messageStruct{};
	const uint messageFilterMin{ 0u }, messageFilterMax{ 0u };

	const __int32 maxRenderIndex{ ( __int32 )finalFrameCount - 1 };
	__int32 currentRenderIndex{ 0 };
	bool currentlyRendering{ false };
	bool hasFinishedRendering{ false };// Once true, Left Mouse Button saves the current frame instead of restarting the render.
	std::chrono::steady_clock::time_point renderStartTime{};
	std::chrono::milliseconds renderDuration{ 0 };

	const __int32 maxPresentIndex{ ( __int32 )finalFrameCount - 1 };
	__int32 currentPresentIndex{ 0 };
	bool currentlyPresenting{ true };

	auto pGlobalDescriptorHeap = globalDescriptorHeap.get_interface();

	D3D12_RESOURCE_BARRIER globalUavBarrier{};
	globalUavBarrier.Type			 = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	globalUavBarrier.Flags			 = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	globalUavBarrier.UAV.pResource	 = nullptr;
	auto pRP1GraphicsCommandList	 = rp1GraphicsCommandList.get_list_for_submission();
	auto pRP3GraphicsCommandList	 = rp3GraphicsCommandList.get_list_for_submission();
	auto pPresentGraphicsCommandList = presentGraphicsCommandList.get_list_for_submission();

	// Encodes an R8G8B8A8 host-side frame buffer to a .jpg file via WIC.
	auto save_final_frame_as_jpeg = [&](void* pPixelData, const wchar_t* filePath) -> void {
		HRESULT localResult{ S_OK };

		IWICImagingFactory* pWICFactory{ nullptr };
		localResult = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pWICFactory));
		direct_x_stuff::result_check(localResult, L"CoCreateInstance() failed.", L"SaveFinalFrameAsJpeg Error");

		IWICBitmap* pWICBitmap{ nullptr };
		localResult = pWICFactory->CreateBitmapFromMemory(finalFrameDimensions.x, finalFrameDimensions.y, GUID_WICPixelFormat32bppRGBA, finalFrameDimensions.x * bytesPerFinalPixel,
			( UINT )(finalFrameDimensions.x * finalFrameDimensions.y * bytesPerFinalPixel), std::bit_cast<BYTE*>(pPixelData), &pWICBitmap);
		direct_x_stuff::result_check(localResult, L"CreateBitmapFromMemory() failed.", L"SaveFinalFrameAsJpeg Error");

		IWICStream* pWICStream{ nullptr };
		localResult = pWICFactory->CreateStream(&pWICStream);
		direct_x_stuff::result_check(localResult, L"CreateStream() failed.", L"SaveFinalFrameAsJpeg Error");

		localResult = pWICStream->InitializeFromFilename(filePath, GENERIC_WRITE);
		direct_x_stuff::result_check(localResult, L"InitializeFromFilename() failed.", L"SaveFinalFrameAsJpeg Error");

		IWICBitmapEncoder* pWICEncoder{ nullptr };
		localResult = pWICFactory->CreateEncoder(GUID_ContainerFormatJpeg, nullptr, &pWICEncoder);
		direct_x_stuff::result_check(localResult, L"CreateEncoder() failed.", L"SaveFinalFrameAsJpeg Error");

		localResult = pWICEncoder->Initialize(pWICStream, WICBitmapEncoderNoCache);
		direct_x_stuff::result_check(localResult, L"Initialize() failed.", L"SaveFinalFrameAsJpeg Error");

		IWICBitmapFrameEncode* pWICFrameEncode{ nullptr };
		IPropertyBag2* pWICPropertyBag{ nullptr };
		localResult = pWICEncoder->CreateNewFrame(&pWICFrameEncode, &pWICPropertyBag);
		direct_x_stuff::result_check(localResult, L"CreateNewFrame() failed.", L"SaveFinalFrameAsJpeg Error");

		localResult = pWICFrameEncode->Initialize(pWICPropertyBag);
		direct_x_stuff::result_check(localResult, L"Initialize() failed.", L"SaveFinalFrameAsJpeg Error");

		localResult = pWICFrameEncode->SetSize(finalFrameDimensions.x, finalFrameDimensions.y);
		direct_x_stuff::result_check(localResult, L"SetSize() failed.", L"SaveFinalFrameAsJpeg Error");

		WICPixelFormatGUID jpegPixelFormat{ GUID_WICPixelFormat24bppBGR };
		localResult = pWICFrameEncode->SetPixelFormat(&jpegPixelFormat);
		direct_x_stuff::result_check(localResult, L"SetPixelFormat() failed.", L"SaveFinalFrameAsJpeg Error");

		IWICFormatConverter* pWICConverter{ nullptr };
		localResult = pWICFactory->CreateFormatConverter(&pWICConverter);
		direct_x_stuff::result_check(localResult, L"CreateFormatConverter() failed.", L"SaveFinalFrameAsJpeg Error");

		localResult = pWICConverter->Initialize(pWICBitmap, jpegPixelFormat, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);
		direct_x_stuff::result_check(localResult, L"Initialize() failed.", L"SaveFinalFrameAsJpeg Error");

		localResult = pWICFrameEncode->WriteSource(pWICConverter, nullptr);
		direct_x_stuff::result_check(localResult, L"WriteSource() failed.", L"SaveFinalFrameAsJpeg Error");

		localResult = pWICFrameEncode->Commit();
		direct_x_stuff::result_check(localResult, L"Commit() failed.", L"SaveFinalFrameAsJpeg Error");

		localResult = pWICEncoder->Commit();
		direct_x_stuff::result_check(localResult, L"Commit() failed.", L"SaveFinalFrameAsJpeg Error");

		if (pWICConverter != nullptr) {
			pWICConverter->Release();
			pWICConverter = nullptr;
		}

		if (pWICPropertyBag != nullptr) {
			pWICPropertyBag->Release();
			pWICPropertyBag = nullptr;
		}

		if (pWICFrameEncode != nullptr) {
			pWICFrameEncode->Release();
			pWICFrameEncode = nullptr;
		}

		if (pWICEncoder != nullptr) {
			pWICEncoder->Release();
			pWICEncoder = nullptr;
		}

		if (pWICStream != nullptr) {
			pWICStream->Release();
			pWICStream = nullptr;
		}

		if (pWICBitmap != nullptr) {
			pWICBitmap->Release();
			pWICBitmap = nullptr;
		}

		if (pWICFactory != nullptr) {
			pWICFactory->Release();
			pWICFactory = nullptr;
		}
	};

	MessageBox(theatreWindow.report_window_handle(), L"Begin Rendering: Left Mouse Button", L"Message", NULL);

	while (true == true) {
		while (PeekMessageW(&messageStruct, NULL, messageFilterMin, messageFilterMax, PM_REMOVE)) {
			TranslateMessage(&messageStruct);
			DispatchMessageW(&messageStruct);
		}

		if (IsWindowVisible(theatreWindow.report_window_handle()) == false) {
			break;
		}

		// Rendering activation, or (once a render has completed) saving the currently-displayed frame to disk.
		if (messageStruct.message == WM_LBUTTONUP) {
			if (hasFinishedRendering == false) {
				currentlyPresenting = false;
				currentRenderIndex	= 0;
				currentlyRendering	= true;
				renderStartTime		= std::chrono::steady_clock::now();
			} else {
				const std::chrono::zoned_time localTime{ std::chrono::current_zone(), std::chrono::system_clock::now() };
				const std::filesystem::path imagesDirectory{ UNNAMED_RENDERER_IMAGES_DIRECTORY };
				std::filesystem::create_directories(imagesDirectory);
				const std::wstring outputFileStem{ std::format(L"{}-{}SPP-{}B-{}x{}-{:%Y%m%d}", currentScene.name, samplesPerPixel, maxRecursionDepth, psViewPortDimensions.x,
					psViewPortDimensions.y, localTime) };
				auto is_image_counter_taken = [&](const std::wstring& counterPrefix) -> bool {
					for (const std::filesystem::directory_entry& entry: std::filesystem::directory_iterator{ imagesDirectory }) {
						const std::wstring fileName{ entry.path().filename().wstring() };
						if (fileName.starts_with(counterPrefix + L"-") || fileName == counterPrefix + L".jpg") {
							return true;
						}
					}
					return false;
				};
				uint imageCounter{ 1u };
				while (is_image_counter_taken(std::format(L"{}-{:02}", outputFileStem, imageCounter))) {
					imageCounter++;
				}
				const std::filesystem::path outputFilePath{ imagesDirectory / std::format(L"{}-{:02}-{}ms.jpg", outputFileStem, imageCounter, renderDuration.count()) };
				save_final_frame_as_jpeg(hostFinalFrameBuffers[currentPresentIndex]->get_pointer_to_buffer_start(), outputFilePath.c_str());
			}

			// Prevent this Left Mouse Button event from being re-handled on subsequent loop iterations
			// before the next real message overwrites MessageStruct.
			messageStruct.message = 0u;
		}

		// Rendering Logic.
		if (currentlyRendering == true) {
			if (inlineRootConstants.currentSampleIndex == 0u) {
				// Set the Inline Root Constants and Scene Objects:
				inlineRootConstants.globalTickInRadians = 2.0f * (( float )currentRenderIndex / (( float )maxRenderIndex));

				// Interpolation of the geometry values for spheres, then rectangles, then triangles.
				// Interpolation of the geometry values for spheres, then rectangles, then triangles.
				for (int64_t i{ 0 }; i < ( int64_t )spheres.size(); i++) {
					spheres[i].wsOrigin.x = linear_interpolation(inlineRootConstants.globalTickInRadians / 2.0f, spheres[i].wsOriginStart.x, spheres[i].wsOriginEnd.x);
					spheres[i].wsOrigin.y = linear_interpolation(inlineRootConstants.globalTickInRadians / 2.0f, spheres[i].wsOriginStart.y, spheres[i].wsOriginEnd.y);
					spheres[i].wsOrigin.z = linear_interpolation(inlineRootConstants.globalTickInRadians / 2.0f, spheres[i].wsOriginStart.z, spheres[i].wsOriginEnd.z);

					spheres[i].wsRadius = linear_interpolation(inlineRootConstants.globalTickInRadians / 2.0f, spheres[i].wsRadiusStart, spheres[i].wsRadiusEnd);

					spheres[i].color.x = linear_interpolation(inlineRootConstants.globalTickInRadians / 2.0f, spheres[i].colorStart.x, spheres[i].colorEnd.x);
					spheres[i].color.y = linear_interpolation(inlineRootConstants.globalTickInRadians / 2.0f, spheres[i].colorStart.y, spheres[i].colorEnd.y);
					spheres[i].color.z = linear_interpolation(inlineRootConstants.globalTickInRadians / 2.0f, spheres[i].colorStart.z, spheres[i].colorEnd.z);

					spheres[i].materialScalar = linear_interpolation(inlineRootConstants.globalTickInRadians / 2.0f, spheres[i].materialScalarStart, spheres[i].materialScalarEnd);
				}

				for (int64_t i{ 0 }; i < ( int64_t )rectangles.size(); i++) {
					rectangles[i].q1.x = linear_interpolation(inlineRootConstants.globalTickInRadians / 2.0f, rectangles[i].q1Start.x, rectangles[i].q1End.x);
					rectangles[i].q1.y = linear_interpolation(inlineRootConstants.globalTickInRadians / 2.0f, rectangles[i].q1Start.y, rectangles[i].q1End.y);
					rectangles[i].q1.z = linear_interpolation(inlineRootConstants.globalTickInRadians / 2.0f, rectangles[i].q1Start.z, rectangles[i].q1End.z);

					rectangles[i].q2.x = linear_interpolation(inlineRootConstants.globalTickInRadians / 2.0f, rectangles[i].q2Start.x, rectangles[i].q2End.x);
					rectangles[i].q2.y = linear_interpolation(inlineRootConstants.globalTickInRadians / 2.0f, rectangles[i].q2Start.y, rectangles[i].q2End.y);
					rectangles[i].q2.z = linear_interpolation(inlineRootConstants.globalTickInRadians / 2.0f, rectangles[i].q2Start.z, rectangles[i].q2End.z);

					rectangles[i].q3.x = linear_interpolation(inlineRootConstants.globalTickInRadians / 2.0f, rectangles[i].q3Start.x, rectangles[i].q3End.x);
					rectangles[i].q3.y = linear_interpolation(inlineRootConstants.globalTickInRadians / 2.0f, rectangles[i].q3Start.y, rectangles[i].q3End.y);
					rectangles[i].q3.z = linear_interpolation(inlineRootConstants.globalTickInRadians / 2.0f, rectangles[i].q3Start.z, rectangles[i].q3End.z);

					rectangles[i].q4.x = linear_interpolation(inlineRootConstants.globalTickInRadians / 2.0f, rectangles[i].q4Start.x, rectangles[i].q4End.x);
					rectangles[i].q4.y = linear_interpolation(inlineRootConstants.globalTickInRadians / 2.0f, rectangles[i].q4Start.y, rectangles[i].q4End.y);
					rectangles[i].q4.z = linear_interpolation(inlineRootConstants.globalTickInRadians / 2.0f, rectangles[i].q4Start.z, rectangles[i].q4End.z);

					rectangles[i].color.x = linear_interpolation(inlineRootConstants.globalTickInRadians / 2.0f, rectangles[i].colorStart.x, rectangles[i].colorEnd.x);
					rectangles[i].color.y = linear_interpolation(inlineRootConstants.globalTickInRadians / 2.0f, rectangles[i].colorStart.y, rectangles[i].colorEnd.y);
					rectangles[i].color.z = linear_interpolation(inlineRootConstants.globalTickInRadians / 2.0f, rectangles[i].colorStart.z, rectangles[i].colorEnd.z);

					rectangles[i].materialScalar =
						linear_interpolation(inlineRootConstants.globalTickInRadians / 2.0f, rectangles[i].materialScalarStart, rectangles[i].materialScalarEnd);
				}

				for (int64_t i{ 0 }; i < ( int64_t )triangles.size(); i++) {
					triangles[i].v1.x = linear_interpolation(inlineRootConstants.globalTickInRadians / 2.0f, triangles[i].v1Start.x, triangles[i].v1End.x);
					triangles[i].v1.y = linear_interpolation(inlineRootConstants.globalTickInRadians / 2.0f, triangles[i].v1Start.y, triangles[i].v1End.y);
					triangles[i].v1.z = linear_interpolation(inlineRootConstants.globalTickInRadians / 2.0f, triangles[i].v1Start.z, triangles[i].v1End.z);

					triangles[i].v2.x = linear_interpolation(inlineRootConstants.globalTickInRadians / 2.0f, triangles[i].v2Start.x, triangles[i].v2End.x);
					triangles[i].v2.y = linear_interpolation(inlineRootConstants.globalTickInRadians / 2.0f, triangles[i].v2Start.y, triangles[i].v2End.y);
					triangles[i].v2.z = linear_interpolation(inlineRootConstants.globalTickInRadians / 2.0f, triangles[i].v2Start.z, triangles[i].v2End.z);

					triangles[i].v3.x = linear_interpolation(inlineRootConstants.globalTickInRadians / 2.0f, triangles[i].v3Start.x, triangles[i].v3End.x);
					triangles[i].v3.y = linear_interpolation(inlineRootConstants.globalTickInRadians / 2.0f, triangles[i].v3Start.y, triangles[i].v3End.y);
					triangles[i].v3.z = linear_interpolation(inlineRootConstants.globalTickInRadians / 2.0f, triangles[i].v3Start.z, triangles[i].v3End.z);

					triangles[i].color.x = linear_interpolation(inlineRootConstants.globalTickInRadians / 2.0f, triangles[i].colorStart.x, triangles[i].colorEnd.x);
					triangles[i].color.y = linear_interpolation(inlineRootConstants.globalTickInRadians / 2.0f, triangles[i].colorStart.y, triangles[i].colorEnd.y);
					triangles[i].color.z = linear_interpolation(inlineRootConstants.globalTickInRadians / 2.0f, triangles[i].colorStart.z, triangles[i].colorEnd.z);

					triangles[i].materialScalar =
						linear_interpolation(inlineRootConstants.globalTickInRadians / 2.0f, triangles[i].materialScalarStart, triangles[i].materialScalarEnd);
				}

				// Move the Host data into L0 shared memory: Spheres, Rectangles, and Triangles.
				void* pL0SpheresBuffer{ nullptr };

				l0SpheresBuffer.get_interface()->Map(0u, nullptr, &pL0SpheresBuffer);

				memcpy_s(pL0SpheresBuffer, l0SpheresBufferConfig.bufferWidth, spheres.data(), spheres.size() * sizeof(sphere));

				l0SpheresBuffer.get_interface()->Unmap(0u, nullptr);

				pL0SpheresBuffer = nullptr;

				void* pL0RectanglesBuffer{ nullptr };

				l0RectanglesBuffer.get_interface()->Map(0u, nullptr, &pL0RectanglesBuffer);

				memcpy_s(pL0RectanglesBuffer, l0RectanglesBufferConfig.bufferWidth, rectangles.data(), rectangles.size() * sizeof(rectangle));

				l0RectanglesBuffer.get_interface()->Unmap(0u, nullptr);

				pL0RectanglesBuffer = nullptr;

				void* pL0TrianglesBuffer{ nullptr };

				l0TrianglesBuffer.get_interface()->Map(0u, nullptr, &pL0TrianglesBuffer);

				memcpy_s(pL0TrianglesBuffer, l0TrianglesBufferConfig.bufferWidth, triangles.data(), triangles.size() * sizeof(triangle_primitive));

				l0TrianglesBuffer.get_interface()->Unmap(0u, nullptr);

				pL0TrianglesBuffer = nullptr;
			}

			if (inlineRootConstants.currentSampleIndex <= inlineRootConstants.maxSampleIndex) {
				// Render-passes 1 and 2, batched: Generate the Intersection Maps, then accumulate them into the Accumulation Frame.
				rp1CommandAllocator.get_interface()->Reset();

				rp1GraphicsCommandList.get_interface()->Reset(rp1CommandAllocator.get_interface(), rp1PipelineState.get_interface());

				rp1GraphicsCommandList.get_interface()->ResourceBarrier(1u, &l0SpheresBufferCopyDestToCopySource);

				rp1GraphicsCommandList.get_interface()->ResourceBarrier(1u, &l1SpheresBufferUnorderedAccessToCopyDest);

				rp1GraphicsCommandList.get_interface()->CopyResource(l1SpheresBuffer.get_interface(), l0SpheresBuffer.get_interface());

				rp1GraphicsCommandList.get_interface()->ResourceBarrier(1u, &l0SpheresBufferCopySourceToCopyDest);

				rp1GraphicsCommandList.get_interface()->ResourceBarrier(1u, &l1SpheresBufferCopyDestToUnorderedAccess);

				rp1GraphicsCommandList.get_interface()->ResourceBarrier(1u, &l0RectanglesBufferCopyDestToCopySource);

				rp1GraphicsCommandList.get_interface()->ResourceBarrier(1u, &l1RectanglesBufferUnorderedAccessToCopyDest);

				rp1GraphicsCommandList.get_interface()->CopyResource(l1RectanglesBuffer.get_interface(), l0RectanglesBuffer.get_interface());

				rp1GraphicsCommandList.get_interface()->ResourceBarrier(1u, &l0RectanglesBufferCopySourceToCopyDest);

				rp1GraphicsCommandList.get_interface()->ResourceBarrier(1u, &l1RectanglesBufferCopyDestToUnorderedAccess);

				rp1GraphicsCommandList.get_interface()->ResourceBarrier(1u, &l0TrianglesBufferCopyDestToCopySource);

				rp1GraphicsCommandList.get_interface()->ResourceBarrier(1u, &l1TrianglesBufferUnorderedAccessToCopyDest);

				rp1GraphicsCommandList.get_interface()->CopyResource(l1TrianglesBuffer.get_interface(), l0TrianglesBuffer.get_interface());

				rp1GraphicsCommandList.get_interface()->ResourceBarrier(1u, &l0TrianglesBufferCopySourceToCopyDest);

				rp1GraphicsCommandList.get_interface()->ResourceBarrier(1u, &l1TrianglesBufferCopyDestToUnorderedAccess);

				rp1GraphicsCommandList.get_interface()->SetComputeRootSignature(globalRootSignature.get_interface());

				rp1GraphicsCommandList.get_interface()->SetDescriptorHeaps(1u, &pGlobalDescriptorHeap);

				rp1GraphicsCommandList.get_interface()->SetComputeRootDescriptorTable(0u, globalDescriptorHeap.get_interface()->GetGPUDescriptorHandleForHeapStart());

				const uint submissionEndSampleIndex{ std::min(inlineRootConstants.currentSampleIndex + samplesPerSubmission, inlineRootConstants.maxSampleIndex + 1u) };

				for (; inlineRootConstants.currentSampleIndex < submissionEndSampleIndex; inlineRootConstants.currentSampleIndex++) {
					rp1GraphicsCommandList.get_interface()->SetComputeRoot32BitConstants(1u, rootConstantCount, ( void* )&inlineRootConstants, 0u);

					rp1GraphicsCommandList.get_interface()->SetPipelineState(rp1PipelineState.get_interface());

					rp1GraphicsCommandList.get_interface()->Dispatch(gridDimensionsByGroup.x, gridDimensionsByGroup.y, gridDimensionsByGroup.z);

					rp1GraphicsCommandList.get_interface()->ResourceBarrier(1u, &globalUavBarrier);

					rp1GraphicsCommandList.get_interface()->SetPipelineState(rp2PipelineState.get_interface());

					rp1GraphicsCommandList.get_interface()->Dispatch(gridDimensionsByGroup.x, gridDimensionsByGroup.y, gridDimensionsByGroup.z);

					rp1GraphicsCommandList.get_interface()->ResourceBarrier(1u, &globalUavBarrier);
				}

				rp1GraphicsCommandList.get_interface()->Close();

				commandQueue.get_interface()->ExecuteCommandLists(1u, &pRP1GraphicsCommandList);

				fence.flush_command_queue(commandQueue.get_interface());
			}

			if (inlineRootConstants.currentSampleIndex > inlineRootConstants.maxSampleIndex) {
				// Render-pass 3: Generate and store the Final Frame
				rp3CommandAllocator.get_interface()->Reset();

				rp3GraphicsCommandList.get_interface()->Reset(rp3CommandAllocator.get_interface(), rp3PipelineState.get_interface());

				rp3GraphicsCommandList.get_interface()->SetComputeRootSignature(globalRootSignature.get_interface());

				rp3GraphicsCommandList.get_interface()->SetDescriptorHeaps(1u, &pGlobalDescriptorHeap);

				rp3GraphicsCommandList.get_interface()->SetComputeRootDescriptorTable(0u, globalDescriptorHeap.get_interface()->GetGPUDescriptorHandleForHeapStart());

				rp3GraphicsCommandList.get_interface()->SetComputeRoot32BitConstants(1u, rootConstantCount, ( void* )&inlineRootConstants, 0u);

				rp3GraphicsCommandList.get_interface()->SetPipelineState(rp3PipelineState.get_interface());

				rp3GraphicsCommandList.get_interface()->Dispatch(gridDimensionsByGroup.x, gridDimensionsByGroup.y, gridDimensionsByGroup.z);

				rp3GraphicsCommandList.get_interface()->ResourceBarrier(1u, &l1FinalFrame2dTextureUnorderedAccessToCopySource);

				rp3GraphicsCommandList.get_interface()->CopyTextureRegion(&l0FinalFrameBufferTextureCopyLocation, 0u, 0u, 0u, &l1FinalFrame2dTextureTextureCopyLocation, nullptr);

				rp3GraphicsCommandList.get_interface()->ResourceBarrier(1u, &l1FinalFrame2dTextureCopySourceToUnorderedAccess);

				rp3GraphicsCommandList.get_interface()->ResourceBarrier(1u, &l0FinalFrameBufferCopyDestToCopySource);

				rp3GraphicsCommandList.get_interface()->ResourceBarrier(1u, &l0FinalFrameBufferCopySourceToCopyDest);

				rp3GraphicsCommandList.get_interface()->Close();

				commandQueue.get_interface()->ExecuteCommandLists(1u, &pRP3GraphicsCommandList);

				fence.flush_command_queue(commandQueue.get_interface());

				void* pL0FinalFrameBuffer{ nullptr };

				l0FinalFrameBuffer.get_interface()->Map(0u, nullptr, &pL0FinalFrameBuffer);

				memcpy_s(hostFinalFrameBuffers[currentRenderIndex]->get_pointer_to_buffer_start(), hostFinalFrameBuffers[currentRenderIndex]->get_buffer_size_in_bytes(),
					pL0FinalFrameBuffer, l0FinalFrameBufferConfig.bufferWidth);

				l0FinalFrameBuffer.get_interface()->Unmap(0u, nullptr);

				pL0FinalFrameBuffer = nullptr;

				inlineRootConstants.currentSampleIndex = 0u;

				currentRenderIndex++;

				if (currentRenderIndex > maxRenderIndex) {
					currentlyRendering	 = false;
					currentlyPresenting	 = true;
					hasFinishedRendering = true;
					renderDuration		 = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - renderStartTime);
				}
			}
		}

		// Present Logic.
		if (currentlyPresenting == true) {
			void* pL0FinalFrameBuffer{ nullptr };

			l0FinalFrameBuffer.get_interface()->Map(0u, nullptr, &pL0FinalFrameBuffer);

			memcpy_s(pL0FinalFrameBuffer, l0FinalFrameBufferConfig.bufferWidth, hostFinalFrameBuffers[currentPresentIndex]->get_pointer_to_buffer_start(),
				hostFinalFrameBuffers[currentPresentIndex]->get_buffer_size_in_bytes());

			l0FinalFrameBuffer.get_interface()->Unmap(0u, nullptr);

			pL0FinalFrameBuffer = nullptr;

			presentCommandAllocator.get_interface()->Reset();

			presentGraphicsCommandList.get_interface()->Reset(presentCommandAllocator.get_interface(), nullptr);

			presentGraphicsCommandList.get_interface()->ResourceBarrier(1u, &l0FinalFrameBufferCopyDestToCopySource);

			presentGraphicsCommandList.get_interface()->ResourceBarrier(1u, &l1FinalFrame2dTextureUnorderedAccessToCopyDest);

			presentGraphicsCommandList.get_interface()->CopyTextureRegion(&l1FinalFrame2dTextureTextureCopyLocation, 0u, 0u, 0u, &l0FinalFrameBufferTextureCopyLocation, nullptr);

			presentGraphicsCommandList.get_interface()->ResourceBarrier(1u, &l0FinalFrameBufferCopySourceToCopyDest);

			presentGraphicsCommandList.get_interface()->ResourceBarrier(1u, &l1FinalFrame2dTextureCopyDestToCopySource);

			currentBackBufferIndex = swapChain.get_interface()->GetCurrentBackBufferIndex();

			presentGraphicsCommandList.get_interface()->ResourceBarrier(1u, &swapChainBackBufferPresentToCopyDestBarriers[currentBackBufferIndex]);

			presentGraphicsCommandList.get_interface()->CopyResource(swapChainBackBuffers[currentBackBufferIndex], l1FinalFrame2dTexture.get_interface());

			presentGraphicsCommandList.get_interface()->ResourceBarrier(1u, &l1FinalFrame2dTextureCopySourceToUnorderedAccess);

			presentGraphicsCommandList.get_interface()->ResourceBarrier(1u, &swapChainBackBufferCopyDestToPresentBarriers[currentBackBufferIndex]);

			presentGraphicsCommandList.get_interface()->Close();

			commandQueue.get_interface()->ExecuteCommandLists(1u, &pPresentGraphicsCommandList);

			swapChain.get_interface()->Present(1u, NULL);

			fence.flush_command_queue(commandQueue.get_interface());

			currentPresentIndex++;

			if (currentPresentIndex > maxPresentIndex) {
				currentPresentIndex = 0;
			}
		}
	}

	/*
		GLOBAL CLEANUP/REPORTING/EXIT:
			-FILO
	*/

	// Development message.
	if (DEBUG_ENABLED) {
		MessageBox(NULL, L"Program Success!", L"Debug Mode Message", NULL);
	}

	// Report the DirectX debug info.
	direct_x_stuff::report_dxgi_debug_info();

	if (pPresentGraphicsCommandList != nullptr) {
		pPresentGraphicsCommandList = nullptr;
	}

	if (pRP3GraphicsCommandList != nullptr) {
		pRP3GraphicsCommandList = nullptr;
	}

	if (pRP1GraphicsCommandList != nullptr) {
		pRP1GraphicsCommandList = nullptr;
	}

	if (pGlobalDescriptorHeap != nullptr) {
		pGlobalDescriptorHeap = nullptr;
	}

	for (int64_t i{ swapChainBackBufferCount - 1 }; i >= 0; i--) {
		if (swapChainBackBuffers[i] != nullptr) {
			swapChainBackBuffers[i]->Release();
			swapChainBackBuffers[i] = nullptr;
		}
	}

	for (int64_t i{ finalFrameCount - 1 }; i >= 0; i--) {
		if (hostFinalFrameBuffers[i] != nullptr) {
			delete hostFinalFrameBuffers[i];
			hostFinalFrameBuffers[i] = nullptr;
		}
	}

	for (int64_t i{ finalFrameCount - 1 }; i >= 0; i--) {
		if (hostFinalFrameBufferConfig[i] != nullptr) {
			delete hostFinalFrameBufferConfig[i];
			hostFinalFrameBufferConfig[i] = nullptr;
		}
	}

	CoUninitialize();

	// Exit.
	int successExitCode{ 128 };
	return successExitCode;
}
