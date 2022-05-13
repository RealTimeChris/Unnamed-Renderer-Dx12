// Unnamed Renderer (DX12).cpp - Hobby path-tracer.
// Apr 2020
// Chris M.
// https://github.com/RealTimeChris

#include "DirectXStuff.hpp"
#if defined(RTIOW_SCENE)
	#include "RTIOWScene.hpp"
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
	wchar_t ExecutablePath[MAX_PATH]{};
	GetModuleFileNameW(nullptr, ExecutablePath, MAX_PATH);
	std::filesystem::current_path(std::filesystem::path{ ExecutablePath }.parent_path());

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
	HRESULT Result{ S_OK };

	// For encoding rendered frames to disk as .jpg via WIC.
	Result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
	DirectXStuff::ResultCheck(Result, L"CoInitializeEx() failed.", L"COM Initialization Error");

	// Unique DirectX 12 Interfaces.
	DirectXStuff::Factory Factory{};
	DirectXStuff::DXCLibrary DXCLibrary{};
	DirectXStuff::DXCCompiler DXCCompiler{};
	DirectXStuff::D3D12DebugController D3DDebugger{};
	DirectXStuff::Device Device{ L"Device" };
	DirectXStuff::CommandQueue CommandQueue{ Device.GetInterface(), D3D12_COMMAND_QUEUE_PRIORITY_HIGH, L"CommandQueue" };
	DirectXStuff::Fence Fence{ Device.GetInterface(), L"Fence" };

	using SceneStuff::Sphere;
	using SceneStuff::Rectangle;
	using SceneStuff::Triangle;

	SceneStuff::Scene CurrentScene{ SceneStuff::BuildScene() };
	std::vector<Sphere>& Spheres{ CurrentScene.Spheres };
	std::vector<Rectangle>& Rectangles{ CurrentScene.Rectangles };
	std::vector<Triangle>& Triangles{ CurrentScene.Triangles };

	// View port dimesions, in pixels.
	constexpr uint3 PSViewPortDimensions{ 1280u, 720u, 1u };
	constexpr uint BytesPerFinalPixel{ 4u };

	// Values for defining and mapping the workload.
	constexpr uint SecondsToRender{ 1u };
	constexpr uint FramesPerSecond{ 2u };
	constexpr uint FinalFrameCount{ FramesPerSecond * SecondsToRender };
	constexpr uint SamplesPerPixel{ 1000u };
	constexpr uint SamplesPerSubmission{ 16u };
	constexpr uint3 TSGridDimensions{ PSViewPortDimensions.x, PSViewPortDimensions.y, PSViewPortDimensions.z };
	constexpr uint3 TSGroupDimensions{ 128u, 8u, 1u };
	constexpr uint3 GridDimensionsByGroup{ DirectXStuff::SetGroupCountPerGrid(TSGridDimensions, TSGroupDimensions) };

	// World-Space View Port Values.
	constexpr float WSViewPortAspectRatio{ ( float )PSViewPortDimensions.x / ( float )PSViewPortDimensions.y };

	// Camera definition values.
	const float VFoVInRadians{ (CurrentScene.VFoVInDegrees / 180.0f) * ( float )M_PI };
	const float WSViewPortHeight{ 2.0f * ( float )tan(VFoVInRadians / 2.0f) };
	const float WSViewPortWidth{ WSViewPortHeight * WSViewPortAspectRatio };

	// Maximum path-tracing recursion depth.
	constexpr uint MaxRecursionDepth{ 30u };

	// Resource Values.
	constexpr uint3 IntersectionMapDimensions{ PSViewPortDimensions.x, PSViewPortDimensions.y, MaxRecursionDepth };
	constexpr uint2 AccumulationFrameDimensions{ PSViewPortDimensions.x, PSViewPortDimensions.y };
	constexpr uint2 FinalFrameDimensions{ PSViewPortDimensions.x, PSViewPortDimensions.y };

	// 32-bit Root Constants to be passed from Host to Device/Shader via Root Signature(s).
	struct InlineRootConstants {
		uint3 TSGridDimensions;
		float Padding00;
		float3 WSCameraLookFrom;
		float Padding01;
		float3 WSCameraLookAt;
		float Padding02;
		float3 WSCameraUp;
		float Padding03;
		float2 WSViewPortDimensions;
		float WSViewPortZCoord;
		float Padding04;
		uint MaxRecursionDepth;
		uint SamplesPerPixel;
		uint MaxSampleIndex;
		uint CurrentSampleIndex;
		float PathMinDistance;
		float PathMaxDistance;
		float2 Padding05;
		float3 SkyTopColor;
		float Padding06;
		float3 SkyBottomColor;
		float Padding07;
		uint SphereCount;
		uint RectangleCount;
		uint TriangleCount;
		float GlobalTickInRadians;
		float LensRadius;
		float FocusDistance;
	};

	constexpr uint RootConstantCount{ sizeof(InlineRootConstants) / sizeof(float) };

	InlineRootConstants InlineRootConstants{};

	InlineRootConstants.TSGridDimensions = TSGridDimensions;
	InlineRootConstants.WSCameraLookFrom = CurrentScene.WSCameraLookFrom;
	InlineRootConstants.WSCameraLookAt = CurrentScene.WSCameraLookAt;
	InlineRootConstants.WSCameraUp = CurrentScene.WSCameraUp;
	InlineRootConstants.WSViewPortDimensions.x = WSViewPortWidth;
	InlineRootConstants.WSViewPortDimensions.y = WSViewPortHeight;
	InlineRootConstants.MaxRecursionDepth = MaxRecursionDepth;
	InlineRootConstants.SamplesPerPixel = SamplesPerPixel;
	InlineRootConstants.MaxSampleIndex = SamplesPerPixel - 1u;
	InlineRootConstants.CurrentSampleIndex = 0u;
	InlineRootConstants.PathMinDistance = 0.001f;
	InlineRootConstants.PathMaxDistance = 10'000.0f;
	InlineRootConstants.SkyTopColor = CurrentScene.SkyTopColor;
	InlineRootConstants.SkyBottomColor = CurrentScene.SkyBottomColor;
	InlineRootConstants.GlobalTickInRadians = 0.0f;
	InlineRootConstants.LensRadius = CurrentScene.Aperture * 0.5f;
	InlineRootConstants.FocusDistance = CurrentScene.FocusDistance;

	InlineRootConstants.SphereCount = ( uint )Spheres.size();
	InlineRootConstants.RectangleCount = ( uint )Rectangles.size();
	InlineRootConstants.TriangleCount = ( uint )Triangles.size();




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

	DirectXStuff::BufferConfig L0SpheresBufferConfig{};
	L0SpheresBufferConfig.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_COMBINE;
	L0SpheresBufferConfig.InitialResourceState = D3D12_RESOURCE_STATE_COPY_DEST;
	L0SpheresBufferConfig.MemoryPool = D3D12_MEMORY_POOL_L0;
	L0SpheresBufferConfig.BufferWidth = static_cast<uint32_t>(Spheres.size() * sizeof(Sphere));

	DirectXStuff::Buffer L0SpheresBuffer{ Device.GetInterface(), L0SpheresBufferConfig, L"L0SpheresBuffer" };

	D3D12_RESOURCE_BARRIER L0SpheresBufferCopyDestToCopySource{};
	L0SpheresBufferCopyDestToCopySource =
		DirectXStuff::CreateResourceTransitionBarrier(L0SpheresBuffer.GetInterface(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COPY_SOURCE);

	D3D12_RESOURCE_BARRIER L0SpheresBufferCopySourceToCopyDest{};
	L0SpheresBufferCopySourceToCopyDest =
		DirectXStuff::CreateResourceTransitionBarrier(L0SpheresBuffer.GetInterface(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COPY_DEST);

	// Spheres device buffer.
	// Spheres device buffer.
	DirectXStuff::BufferConfig L1SpheresBufferConfig{};
	L1SpheresBufferConfig.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_NOT_AVAILABLE;
	L1SpheresBufferConfig.InitialResourceState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	L1SpheresBufferConfig.MemoryPool = D3D12_MEMORY_POOL_L1;
	L1SpheresBufferConfig.BufferWidth = static_cast<uint32_t>(Spheres.size() * sizeof(Sphere));

	DirectXStuff::Buffer L1SpheresBuffer{ Device.GetInterface(), L1SpheresBufferConfig, L"L1SpheresBuffer" };

	D3D12_RESOURCE_BARRIER L1SpheresBufferCopyDestToUnorderedAccess{};
	L1SpheresBufferCopyDestToUnorderedAccess =
		DirectXStuff::CreateResourceTransitionBarrier(L1SpheresBuffer.GetInterface(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	D3D12_RESOURCE_BARRIER L1SpheresBufferUnorderedAccessToCopyDest{};
	L1SpheresBufferUnorderedAccessToCopyDest =
		DirectXStuff::CreateResourceTransitionBarrier(L1SpheresBuffer.GetInterface(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_DEST);

	// Rectangles upload buffer.
	DirectXStuff::BufferConfig L0RectanglesBufferConfig{};
	L0RectanglesBufferConfig.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_COMBINE;
	L0RectanglesBufferConfig.InitialResourceState = D3D12_RESOURCE_STATE_COPY_DEST;
	L0RectanglesBufferConfig.MemoryPool = D3D12_MEMORY_POOL_L0;
	L0RectanglesBufferConfig.BufferWidth = static_cast<uint32_t>(Rectangles.size() * sizeof(Rectangle));

	DirectXStuff::Buffer L0RectanglesBuffer{ Device.GetInterface(), L0RectanglesBufferConfig, L"L0RectanglesBuffer" };

	D3D12_RESOURCE_BARRIER L0RectanglesBufferCopyDestToCopySource{};
	L0RectanglesBufferCopyDestToCopySource =
		DirectXStuff::CreateResourceTransitionBarrier(L0RectanglesBuffer.GetInterface(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COPY_SOURCE);

	D3D12_RESOURCE_BARRIER L0RectanglesBufferCopySourceToCopyDest{};
	L0RectanglesBufferCopySourceToCopyDest =
		DirectXStuff::CreateResourceTransitionBarrier(L0RectanglesBuffer.GetInterface(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COPY_DEST);

	// Rectangles device buffer.
	DirectXStuff::BufferConfig L1RectanglesBufferConfig{};
	L1RectanglesBufferConfig.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_NOT_AVAILABLE;
	L1RectanglesBufferConfig.InitialResourceState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	L1RectanglesBufferConfig.MemoryPool = D3D12_MEMORY_POOL_L1;
	L1RectanglesBufferConfig.BufferWidth = static_cast<uint32_t>(Rectangles.size() * sizeof(Rectangle));

	DirectXStuff::Buffer L1RectanglesBuffer{ Device.GetInterface(), L1RectanglesBufferConfig, L"L1RectanglesBuffer" };

	D3D12_RESOURCE_BARRIER L1RectanglesBufferCopyDestToUnorderedAccess{};
	L1RectanglesBufferCopyDestToUnorderedAccess =
		DirectXStuff::CreateResourceTransitionBarrier(L1RectanglesBuffer.GetInterface(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	D3D12_RESOURCE_BARRIER L1RectanglesBufferUnorderedAccessToCopyDest{};
	L1RectanglesBufferUnorderedAccessToCopyDest =
		DirectXStuff::CreateResourceTransitionBarrier(L1RectanglesBuffer.GetInterface(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_DEST);

	// Triangles upload buffer.
	DirectXStuff::BufferConfig L0TrianglesBufferConfig{};
	L0TrianglesBufferConfig.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_COMBINE;
	L0TrianglesBufferConfig.InitialResourceState = D3D12_RESOURCE_STATE_COPY_DEST;
	L0TrianglesBufferConfig.MemoryPool = D3D12_MEMORY_POOL_L0;
	L0TrianglesBufferConfig.BufferWidth = static_cast<uint32_t>(Triangles.size() * sizeof(Triangle));

	DirectXStuff::Buffer L0TrianglesBuffer{ Device.GetInterface(), L0TrianglesBufferConfig, L"L0TrianglesBuffer" };

	D3D12_RESOURCE_BARRIER L0TrianglesBufferCopyDestToCopySource{};
	L0TrianglesBufferCopyDestToCopySource =
		DirectXStuff::CreateResourceTransitionBarrier(L0TrianglesBuffer.GetInterface(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COPY_SOURCE);

	D3D12_RESOURCE_BARRIER L0TrianglesBufferCopySourceToCopyDest{};
	L0TrianglesBufferCopySourceToCopyDest =
		DirectXStuff::CreateResourceTransitionBarrier(L0TrianglesBuffer.GetInterface(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COPY_DEST);

	// Triangles device buffer.
	DirectXStuff::BufferConfig L1TrianglesBufferConfig{};
	L1TrianglesBufferConfig.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_NOT_AVAILABLE;
	L1TrianglesBufferConfig.InitialResourceState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	L1TrianglesBufferConfig.MemoryPool = D3D12_MEMORY_POOL_L1;
	L1TrianglesBufferConfig.BufferWidth = static_cast<uint32_t>(Triangles.size() * sizeof(Triangle));

	DirectXStuff::Buffer L1TrianglesBuffer{ Device.GetInterface(), L1TrianglesBufferConfig, L"L1TrianglesBuffer" };

	D3D12_RESOURCE_BARRIER L1TrianglesBufferCopyDestToUnorderedAccess{};
	L1TrianglesBufferCopyDestToUnorderedAccess =
		DirectXStuff::CreateResourceTransitionBarrier(L1TrianglesBuffer.GetInterface(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	D3D12_RESOURCE_BARRIER L1TrianglesBufferUnorderedAccessToCopyDest{};
	L1TrianglesBufferUnorderedAccessToCopyDest =
		DirectXStuff::CreateResourceTransitionBarrier(L1TrianglesBuffer.GetInterface(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_DEST);

	// Intersection Maps for recording path-object intersections, to be produced during RP1.
	DirectXStuff::Texture3DConfig L1IntersectionMap013DTextureConfig{};
	L1IntersectionMap013DTextureConfig.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_NOT_AVAILABLE;
	L1IntersectionMap013DTextureConfig.MemoryPool = D3D12_MEMORY_POOL_L1;
	L1IntersectionMap013DTextureConfig.NodeMask = 0u;
	L1IntersectionMap013DTextureConfig.InitialResourceState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	L1IntersectionMap013DTextureConfig.TextureFormat = DXGI_FORMAT_R32G32B32A32_FLOAT;
	L1IntersectionMap013DTextureConfig.TextureWidth = IntersectionMapDimensions.x;
	L1IntersectionMap013DTextureConfig.TextureHeight = IntersectionMapDimensions.y;
	L1IntersectionMap013DTextureConfig.TextureDepth = IntersectionMapDimensions.z;

	DirectXStuff::Texture3D L1IntersectionMap013DTexture{ Device.GetInterface(), L1IntersectionMap013DTextureConfig, L"L1IntersectionMap013DTexture" };

	DirectXStuff::Texture3DConfig L1IntersectionMap023DTextureConfig{};
	L1IntersectionMap023DTextureConfig.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_NOT_AVAILABLE;
	L1IntersectionMap023DTextureConfig.MemoryPool = D3D12_MEMORY_POOL_L1;
	L1IntersectionMap023DTextureConfig.NodeMask = 0u;
	L1IntersectionMap023DTextureConfig.InitialResourceState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	L1IntersectionMap023DTextureConfig.TextureFormat = DXGI_FORMAT_R32G32B32A32_FLOAT;
	L1IntersectionMap023DTextureConfig.TextureWidth = IntersectionMapDimensions.x;
	L1IntersectionMap023DTextureConfig.TextureHeight = IntersectionMapDimensions.y;
	L1IntersectionMap023DTextureConfig.TextureDepth = IntersectionMapDimensions.z;

	DirectXStuff::Texture3D L1IntersectionMap023DTexture{ Device.GetInterface(), L1IntersectionMap023DTextureConfig, L"L1IntersectionMap023DTexture" };

	DirectXStuff::Texture3DConfig L1IntersectionMap033DTextureConfig{};
	L1IntersectionMap033DTextureConfig.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_NOT_AVAILABLE;
	L1IntersectionMap033DTextureConfig.MemoryPool = D3D12_MEMORY_POOL_L1;
	L1IntersectionMap033DTextureConfig.NodeMask = 0u;
	L1IntersectionMap033DTextureConfig.InitialResourceState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	L1IntersectionMap033DTextureConfig.TextureFormat = DXGI_FORMAT_R32G32B32A32_UINT;
	L1IntersectionMap033DTextureConfig.TextureWidth = IntersectionMapDimensions.x;
	L1IntersectionMap033DTextureConfig.TextureHeight = IntersectionMapDimensions.y;
	L1IntersectionMap033DTextureConfig.TextureDepth = IntersectionMapDimensions.z;

	DirectXStuff::Texture3D L1IntersectionMap033DTexture{ Device.GetInterface(), L1IntersectionMap033DTextureConfig, L"L1IntersectionMap033DTexture" };

	// Accumulation Frame for pooling the Samples for each Pixel, to be pushed out during RP2.
	DirectXStuff::Texture2DConfig L1AccumulationFrame2DTextureConfig{};
	L1AccumulationFrame2DTextureConfig.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_NOT_AVAILABLE;
	L1AccumulationFrame2DTextureConfig.MemoryPool = D3D12_MEMORY_POOL_L1;
	L1AccumulationFrame2DTextureConfig.NodeMask = 0u;
	L1AccumulationFrame2DTextureConfig.InitialResourceState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	L1AccumulationFrame2DTextureConfig.TextureFormat = DXGI_FORMAT_R32G32B32A32_FLOAT;
	L1AccumulationFrame2DTextureConfig.TextureWidth = AccumulationFrameDimensions.x;
	L1AccumulationFrame2DTextureConfig.TextureHeight = AccumulationFrameDimensions.y;

	DirectXStuff::Texture2D L1AccumulationFrame2DTexture{ Device.GetInterface(), L1AccumulationFrame2DTextureConfig, L"L1AccumulationFrame2DTexture" };

	// Collection of Host-side Final Frame Buffers, to be produced during RP3.
	DirectXStuff::R8G8B8A8UintHostBufferConfig* HostFinalFrameBufferConfig[FinalFrameCount]{ nullptr };
	uint BufferLengthInElements{ FinalFrameDimensions.x * FinalFrameDimensions.y };

	for (__int64 i{ 0 }; i < FinalFrameCount; i++) {
		unsigned __int8 Green{ ( unsigned __int8 )(255.0f * (( float )i / ( float )FinalFrameCount)) };
		DirectXStuff::R8G8B8A8Uint InitialColor = { 0u, Green, 0u };

		HostFinalFrameBufferConfig[i] = new DirectXStuff::R8G8B8A8UintHostBufferConfig{ BufferLengthInElements, InitialColor };
	}

	DirectXStuff::R8G8B8A8UintHostBuffer* HostFinalFrameBuffers[FinalFrameCount]{ nullptr };

	for (__int64 i{ 0 }; i < FinalFrameCount; i++) {
		HostFinalFrameBuffers[i] = new DirectXStuff::R8G8B8A8UintHostBuffer{ *HostFinalFrameBufferConfig[i] };
	}

	// Final Frame Buffer, upload/download version.
	DirectXStuff::BufferConfig L0FinalFrameBufferConfig{};
	L0FinalFrameBufferConfig.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_COMBINE;
	L0FinalFrameBufferConfig.MemoryPool = D3D12_MEMORY_POOL_L0;
	L0FinalFrameBufferConfig.InitialResourceState = D3D12_RESOURCE_STATE_COPY_DEST;
	L0FinalFrameBufferConfig.BufferFormat = DXGI_FORMAT_UNKNOWN;
	L0FinalFrameBufferConfig.BufferWidth = (FinalFrameDimensions.x * FinalFrameDimensions.y) * BytesPerFinalPixel;

	DirectXStuff::Buffer L0FinalFrameBuffer{ Device.GetInterface(), L0FinalFrameBufferConfig, L"L0FinalFrameBuffer" };

	D3D12_RESOURCE_BARRIER L0FinalFrameBufferCopyDestToCopySource{};
	L0FinalFrameBufferCopyDestToCopySource =
		DirectXStuff::CreateResourceTransitionBarrier(L0FinalFrameBuffer.GetInterface(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COPY_SOURCE);

	D3D12_RESOURCE_BARRIER L0FinalFrameBufferCopySourceToCopyDest{};
	L0FinalFrameBufferCopySourceToCopyDest =
		DirectXStuff::CreateResourceTransitionBarrier(L0FinalFrameBuffer.GetInterface(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COPY_DEST);

	D3D12_TEXTURE_COPY_LOCATION L0FinalFrameBufferTextureCopyLocation{};
	L0FinalFrameBufferTextureCopyLocation.pResource = L0FinalFrameBuffer.GetInterface();
	L0FinalFrameBufferTextureCopyLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	L0FinalFrameBufferTextureCopyLocation.PlacedFootprint.Offset = 0u;
	L0FinalFrameBufferTextureCopyLocation.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	L0FinalFrameBufferTextureCopyLocation.PlacedFootprint.Footprint.Width = FinalFrameDimensions.x;
	L0FinalFrameBufferTextureCopyLocation.PlacedFootprint.Footprint.Height = FinalFrameDimensions.y;
	L0FinalFrameBufferTextureCopyLocation.PlacedFootprint.Footprint.Depth = 1u;
	L0FinalFrameBufferTextureCopyLocation.PlacedFootprint.Footprint.RowPitch = FinalFrameDimensions.x * BytesPerFinalPixel;

	// Final Frame 2DTexture, Device version.
	DirectXStuff::Texture2DConfig L1FinalFrame2DTextureConfig{};
	L1FinalFrame2DTextureConfig.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_NOT_AVAILABLE;
	L1FinalFrame2DTextureConfig.MemoryPool = D3D12_MEMORY_POOL_L1;
	L1FinalFrame2DTextureConfig.InitialResourceState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	L1FinalFrame2DTextureConfig.NodeMask = 0u;
	L1FinalFrame2DTextureConfig.TextureFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	L1FinalFrame2DTextureConfig.TextureWidth = FinalFrameDimensions.x;
	L1FinalFrame2DTextureConfig.TextureHeight = FinalFrameDimensions.y;

	DirectXStuff::Texture2D L1FinalFrame2DTexture{ Device.GetInterface(), L1FinalFrame2DTextureConfig, L"L1FinalFrame2DTexture" };

	D3D12_RESOURCE_BARRIER L1FinalFrame2DTextureUnorderedAccessToCopySource{};
	L1FinalFrame2DTextureUnorderedAccessToCopySource = DirectXStuff::CreateResourceTransitionBarrier(
		L1FinalFrame2DTexture.GetInterface(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);

	D3D12_RESOURCE_BARRIER L1FinalFrame2DTextureCopySourceToUnorderedAccess{};
	L1FinalFrame2DTextureCopySourceToUnorderedAccess = DirectXStuff::CreateResourceTransitionBarrier(
		L1FinalFrame2DTexture.GetInterface(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	D3D12_RESOURCE_BARRIER L1FinalFrame2DTextureUnorderedAccessToCopyDest{};
	L1FinalFrame2DTextureUnorderedAccessToCopyDest = DirectXStuff::CreateResourceTransitionBarrier(
		L1FinalFrame2DTexture.GetInterface(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_DEST);

	D3D12_RESOURCE_BARRIER L1FinalFrame2DTextureCopyDestToCopySource{};
	L1FinalFrame2DTextureCopyDestToCopySource =
		DirectXStuff::CreateResourceTransitionBarrier(L1FinalFrame2DTexture.GetInterface(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COPY_SOURCE);

	D3D12_TEXTURE_COPY_LOCATION L1FinalFrame2DTextureTextureCopyLocation{};
	L1FinalFrame2DTextureTextureCopyLocation.pResource = L1FinalFrame2DTexture.GetInterface();
	L1FinalFrame2DTextureTextureCopyLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	L1FinalFrame2DTextureTextureCopyLocation.SubresourceIndex = 0u;




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

	const uint GlobalDescriptorCount{ 9u };
	const unsigned __int64 DescriptorHandleIncrementSize{ Device.GetInterface()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) };
	uint CurrentDescriptorIndex{ 0u };

	DirectXStuff::DescriptorHeap GlobalDescriptorHeap{ Device.GetInterface(), GlobalDescriptorCount, L"GlobalDescriptorHeap" };

	D3D12_CPU_DESCRIPTOR_HANDLE L1SpheresBufferUAVCPUHandle{};

	D3D12_UNORDERED_ACCESS_VIEW_DESC L1SpheresBufferUAVDesc{};
	L1SpheresBufferUAVDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	L1SpheresBufferUAVDesc.Format = L1SpheresBufferConfig.BufferFormat;
	L1SpheresBufferUAVDesc.Buffer.FirstElement = 0u;
	L1SpheresBufferUAVDesc.Buffer.NumElements = InlineRootConstants.SphereCount;
	L1SpheresBufferUAVDesc.Buffer.CounterOffsetInBytes = 0u;
	L1SpheresBufferUAVDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
	L1SpheresBufferUAVDesc.Buffer.StructureByteStride = sizeof(Sphere);

	L1SpheresBufferUAVCPUHandle.ptr =
		GlobalDescriptorHeap.GetInterface()->GetCPUDescriptorHandleForHeapStart().ptr + (CurrentDescriptorIndex * DescriptorHandleIncrementSize);

	Device.GetInterface()->CreateUnorderedAccessView(L1SpheresBuffer.GetInterface(), nullptr, &L1SpheresBufferUAVDesc, L1SpheresBufferUAVCPUHandle);

	CurrentDescriptorIndex++;

	D3D12_CPU_DESCRIPTOR_HANDLE L1RectanglesBufferUAVCPUHandle{};

	D3D12_UNORDERED_ACCESS_VIEW_DESC L1RectanglesBufferUAVDesc{};
	L1RectanglesBufferUAVDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	L1RectanglesBufferUAVDesc.Format = L1RectanglesBufferConfig.BufferFormat;
	L1RectanglesBufferUAVDesc.Buffer.FirstElement = 0u;
	L1RectanglesBufferUAVDesc.Buffer.NumElements = InlineRootConstants.RectangleCount;
	L1RectanglesBufferUAVDesc.Buffer.CounterOffsetInBytes = 0u;
	L1RectanglesBufferUAVDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
	L1RectanglesBufferUAVDesc.Buffer.StructureByteStride = sizeof(Rectangle);

	L1RectanglesBufferUAVCPUHandle.ptr =
		GlobalDescriptorHeap.GetInterface()->GetCPUDescriptorHandleForHeapStart().ptr + (CurrentDescriptorIndex * DescriptorHandleIncrementSize);

	Device.GetInterface()->CreateUnorderedAccessView(L1RectanglesBuffer.GetInterface(), nullptr, &L1RectanglesBufferUAVDesc, L1RectanglesBufferUAVCPUHandle);

	CurrentDescriptorIndex++;

	D3D12_CPU_DESCRIPTOR_HANDLE L1TrianglesBufferUAVCPUHandle{};

	D3D12_UNORDERED_ACCESS_VIEW_DESC L1TrianglesBufferUAVDesc{};
	L1TrianglesBufferUAVDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	L1TrianglesBufferUAVDesc.Format = L1TrianglesBufferConfig.BufferFormat;
	L1TrianglesBufferUAVDesc.Buffer.FirstElement = 0u;
	L1TrianglesBufferUAVDesc.Buffer.NumElements = InlineRootConstants.TriangleCount;
	L1TrianglesBufferUAVDesc.Buffer.CounterOffsetInBytes = 0u;
	L1TrianglesBufferUAVDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
	L1TrianglesBufferUAVDesc.Buffer.StructureByteStride = sizeof(Triangle);

	L1TrianglesBufferUAVCPUHandle.ptr =
		GlobalDescriptorHeap.GetInterface()->GetCPUDescriptorHandleForHeapStart().ptr + (CurrentDescriptorIndex * DescriptorHandleIncrementSize);

	Device.GetInterface()->CreateUnorderedAccessView(L1TrianglesBuffer.GetInterface(), nullptr, &L1TrianglesBufferUAVDesc, L1TrianglesBufferUAVCPUHandle);

	CurrentDescriptorIndex++;

	D3D12_CPU_DESCRIPTOR_HANDLE NullUAVCPUHandle{};

	D3D12_UNORDERED_ACCESS_VIEW_DESC NullUAVDesc{};
	NullUAVDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
	NullUAVDesc.Format = DXGI_FORMAT_R32_FLOAT;
	NullUAVDesc.Texture3D.FirstWSlice = 0u;
	NullUAVDesc.Texture3D.MipSlice = 0u;
	NullUAVDesc.Texture3D.WSize = 1u;

	NullUAVCPUHandle.ptr = GlobalDescriptorHeap.GetInterface()->GetCPUDescriptorHandleForHeapStart().ptr + (CurrentDescriptorIndex * DescriptorHandleIncrementSize);

	Device.GetInterface()->CreateUnorderedAccessView(nullptr, nullptr, &NullUAVDesc, NullUAVCPUHandle);

	CurrentDescriptorIndex++;

	D3D12_CPU_DESCRIPTOR_HANDLE L1IntersectionMap013DTextureUAVCPUHandle{};

	D3D12_UNORDERED_ACCESS_VIEW_DESC L1IntersectionMap013DTextureUAVDesc{};
	L1IntersectionMap013DTextureUAVDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
	L1IntersectionMap013DTextureUAVDesc.Format = L1IntersectionMap013DTextureConfig.TextureFormat;
	L1IntersectionMap013DTextureUAVDesc.Texture3D.FirstWSlice = 0u;
	L1IntersectionMap013DTextureUAVDesc.Texture3D.MipSlice = 0u;
	L1IntersectionMap013DTextureUAVDesc.Texture3D.WSize = L1IntersectionMap013DTextureConfig.TextureDepth;

	L1IntersectionMap013DTextureUAVCPUHandle.ptr =
		GlobalDescriptorHeap.GetInterface()->GetCPUDescriptorHandleForHeapStart().ptr + (CurrentDescriptorIndex * DescriptorHandleIncrementSize);

	Device.GetInterface()->CreateUnorderedAccessView(
		L1IntersectionMap013DTexture.GetInterface(), nullptr, &L1IntersectionMap013DTextureUAVDesc, L1IntersectionMap013DTextureUAVCPUHandle);

	CurrentDescriptorIndex++;

	D3D12_CPU_DESCRIPTOR_HANDLE L1IntersectionMap023DTextureUAVCPUHandle{};

	D3D12_UNORDERED_ACCESS_VIEW_DESC L1IntersectionMap023DTextureUAVDesc{};
	L1IntersectionMap023DTextureUAVDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
	L1IntersectionMap023DTextureUAVDesc.Format = L1IntersectionMap023DTextureConfig.TextureFormat;
	L1IntersectionMap023DTextureUAVDesc.Texture3D.FirstWSlice = 0u;
	L1IntersectionMap023DTextureUAVDesc.Texture3D.MipSlice = 0u;
	L1IntersectionMap023DTextureUAVDesc.Texture3D.WSize = L1IntersectionMap023DTextureConfig.TextureDepth;

	L1IntersectionMap023DTextureUAVCPUHandle.ptr =
		GlobalDescriptorHeap.GetInterface()->GetCPUDescriptorHandleForHeapStart().ptr + (CurrentDescriptorIndex * DescriptorHandleIncrementSize);

	Device.GetInterface()->CreateUnorderedAccessView(
		L1IntersectionMap023DTexture.GetInterface(), nullptr, &L1IntersectionMap023DTextureUAVDesc, L1IntersectionMap023DTextureUAVCPUHandle);

	CurrentDescriptorIndex++;

	D3D12_CPU_DESCRIPTOR_HANDLE L1IntersectionMap033DTextureUAVCPUHandle{};

	D3D12_UNORDERED_ACCESS_VIEW_DESC L1IntersectionMap033DTextureUAVDesc{};
	L1IntersectionMap033DTextureUAVDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
	L1IntersectionMap033DTextureUAVDesc.Format = L1IntersectionMap033DTextureConfig.TextureFormat;
	L1IntersectionMap033DTextureUAVDesc.Texture3D.FirstWSlice = 0u;
	L1IntersectionMap033DTextureUAVDesc.Texture3D.MipSlice = 0u;
	L1IntersectionMap033DTextureUAVDesc.Texture3D.WSize = L1IntersectionMap033DTextureConfig.TextureDepth;

	L1IntersectionMap033DTextureUAVCPUHandle.ptr =
		GlobalDescriptorHeap.GetInterface()->GetCPUDescriptorHandleForHeapStart().ptr + (CurrentDescriptorIndex * DescriptorHandleIncrementSize);

	Device.GetInterface()->CreateUnorderedAccessView(
		L1IntersectionMap033DTexture.GetInterface(), nullptr, &L1IntersectionMap033DTextureUAVDesc, L1IntersectionMap033DTextureUAVCPUHandle);

	CurrentDescriptorIndex++;

	D3D12_CPU_DESCRIPTOR_HANDLE L1AccumulationFrame2DTextureUAVCPUHandle{};

	D3D12_UNORDERED_ACCESS_VIEW_DESC L1AccumulationFrame2DTextureUAVDesc{};
	L1AccumulationFrame2DTextureUAVDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	L1AccumulationFrame2DTextureUAVDesc.Format = L1AccumulationFrame2DTextureConfig.TextureFormat;
	L1AccumulationFrame2DTextureUAVDesc.Texture2D.MipSlice = 0u;
	L1AccumulationFrame2DTextureUAVDesc.Texture2D.PlaneSlice = 0u;

	L1AccumulationFrame2DTextureUAVCPUHandle.ptr =
		GlobalDescriptorHeap.GetInterface()->GetCPUDescriptorHandleForHeapStart().ptr + (CurrentDescriptorIndex * DescriptorHandleIncrementSize);

	Device.GetInterface()->CreateUnorderedAccessView(
		L1AccumulationFrame2DTexture.GetInterface(), nullptr, &L1AccumulationFrame2DTextureUAVDesc, L1AccumulationFrame2DTextureUAVCPUHandle);

	CurrentDescriptorIndex++;

	D3D12_CPU_DESCRIPTOR_HANDLE L1FinalFrame2DTextureUAVCPUHandle{};

	D3D12_UNORDERED_ACCESS_VIEW_DESC L1FinalFrame2DTextureUAVDesc{};
	L1FinalFrame2DTextureUAVDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	L1FinalFrame2DTextureUAVDesc.Format = L1FinalFrame2DTextureConfig.TextureFormat;
	L1FinalFrame2DTextureUAVDesc.Texture2D.MipSlice = 0u;
	L1FinalFrame2DTextureUAVDesc.Texture2D.PlaneSlice = 0u;

	L1FinalFrame2DTextureUAVCPUHandle.ptr =
		GlobalDescriptorHeap.GetInterface()->GetCPUDescriptorHandleForHeapStart().ptr + (CurrentDescriptorIndex * DescriptorHandleIncrementSize);

	Device.GetInterface()->CreateUnorderedAccessView(
		L1FinalFrame2DTexture.GetInterface(), nullptr, &L1FinalFrame2DTextureUAVDesc, L1FinalFrame2DTextureUAVCPUHandle);

	CurrentDescriptorIndex++;




	/*
		GLOBAL PIPELINE ROOT SIGNATURE:
			Parameter 0 - Descriptor Table with a single UAV Descriptor Range
			Parameter 1 - 32-bit Inline Root Constants (Max = 60)
	*/

	DirectXStuff::Blob RootSignatureBlob{}, RootSignatureErrorBlob{};
	DirectXStuff::RootSignature GlobalRootSignature{ Device.GetInterface(), RootSignatureBlob.GetInterface(), RootSignatureErrorBlob.GetInterface(),
		GlobalDescriptorCount, L"GlobalRootSignature" };




	/*
		RENDER-PASS 1: Compute Shader, Pipeline State, Command Allocator, Graphics Command List
			-Generates an Intersection Map to be used for later recursion to collect final sample/pixel colors.
			-IntersectionMaps = Records Path-Object intersections as Incoming Path Direction and Object Id.
	*/

	DirectXStuff::ShaderConfig RP1ComputeShaderConfig{};
	RP1ComputeShaderConfig.ShaderFileName = L"RP1ComputeShader.hlsl";
	RP1ComputeShaderConfig.ShaderEntryPoint = L"ComputeMain";
	RP1ComputeShaderConfig.TargetProfile = L"cs_6_3";

	DirectXStuff::Shader RP1ComputeShader{ DXCLibrary.GetInterface(), DXCCompiler.GetInterface(), RP1ComputeShaderConfig };

	DirectXStuff::PipelineState RP1PipelineState{ Device.GetInterface(), GlobalRootSignature.GetInterface(), RP1ComputeShader.GetShaderByteCodeSize(),
		RP1ComputeShader.GetShaderByteCode(), L"RP1PipelineState" };

	DirectXStuff::CommandAllocator RP1CommandAllocator{ Device.GetInterface(), L"RP1CommandAllocator" };

	DirectXStuff::GraphicsCommandList RP1GraphicsCommandList{ Device.GetInterface(), RP1CommandAllocator.GetInterface(), L"RP1GraphicsCommandList" };




	/*
		RENDER-PASS 2: Compute Shader, Pipeline State, Command Allocator, Graphics Command List
			-Generates an Accumulation Frame. One per Final Frame.
			-Accumulation Frame = Summation of Samples on a Per-Color-Channel-Per-Pixel basis.
			-Recurses backwards through the Intersection Maps to collect the final Pixel-Colors.
	*/

	DirectXStuff::ShaderConfig RP2ComputeShaderConfig{};
	RP2ComputeShaderConfig.ShaderFileName = L"RP2ComputeShader.hlsl";
	RP2ComputeShaderConfig.ShaderEntryPoint = L"ComputeMain";
	RP2ComputeShaderConfig.TargetProfile = L"cs_6_3";

	DirectXStuff::Shader RP2ComputeShader{ DXCLibrary.GetInterface(), DXCCompiler.GetInterface(), RP2ComputeShaderConfig };

	DirectXStuff::PipelineState RP2PipelineState{ Device.GetInterface(), GlobalRootSignature.GetInterface(), RP2ComputeShader.GetShaderByteCodeSize(),
		RP2ComputeShader.GetShaderByteCode(), L"RP2PipelineState" };





	/*
		RENDER-PASS 3: Compute Shader, Pipeline State, Command Allocator, Graphics Command List
			-Collects the final values from each Accumulation Frame, and properly converts them
				into the correct format, then saves them in Final Frames.
	*/

	DirectXStuff::ShaderConfig RP3ComputeShaderConfig{};
	RP3ComputeShaderConfig.ShaderFileName = L"RP3ComputeShader.hlsl";
	RP3ComputeShaderConfig.ShaderEntryPoint = L"ComputeMain";
	RP3ComputeShaderConfig.TargetProfile = L"cs_6_3";

	DirectXStuff::Shader RP3ComputeShader{ DXCLibrary.GetInterface(), DXCCompiler.GetInterface(), RP3ComputeShaderConfig };

	DirectXStuff::PipelineState RP3PipelineState{ Device.GetInterface(), GlobalRootSignature.GetInterface(), RP3ComputeShader.GetShaderByteCodeSize(),
		RP3ComputeShader.GetShaderByteCode(), L"RP3PipelineState" };

	DirectXStuff::CommandAllocator RP3CommandAllocator{ Device.GetInterface(), L"RP3CommandAllocator" };

	DirectXStuff::GraphicsCommandList RP3GraphicsCommandList{ Device.GetInterface(), RP3CommandAllocator.GetInterface(), L"RP3GraphicsCommandList" };




	/*
		PRESENTATION: Theatre Window Class, Theatre Window, Factory, Swap Chain, Back Buffer Resources,
			Present Command Allocator, Present Graphics Command List
			-Will be used to Present Final Frames to the Theatre Window.
	*/

	// Prep for Presentation.
	WinRTStuff::WindowClass TheatreWindowClass{};
	WinRTStuff::RenderWindow TheatreWindow{ TheatreWindowClass.ReportClassName(), PSViewPortDimensions.x, PSViewPortDimensions.y, L"Unnamed Renderer" };

	const uint SwapChainBackBufferCount{ 2u };
	const uint SwapChainSyncInterval{ 1u };
	uint CurrentBackBufferIndex{ 0u };

	DirectXStuff::SwapChain SwapChain{ Factory.GetInterface(), CommandQueue.GetInterface(), TheatreWindow.ReportWindowHandle(), PSViewPortDimensions.x,
		PSViewPortDimensions.y, SwapChainBackBufferCount, L1FinalFrame2DTextureConfig.TextureFormat };

	ID3D12Resource* SwapChainBackBuffers[SwapChainBackBufferCount]{ nullptr };

	CurrentBackBufferIndex = SwapChain.GetInterface()->GetCurrentBackBufferIndex();

	Result = SwapChain.GetInterface()->GetBuffer(
		( UINT )CurrentBackBufferIndex, __uuidof(ID3D12Resource), std::bit_cast<void**>(&SwapChainBackBuffers[CurrentBackBufferIndex]));
	DirectXStuff::ResultCheck(Result, L"GetBuffer() failed.", L"SwapChain Prep Error");

	Result = SwapChainBackBuffers[CurrentBackBufferIndex]->SetName(L"SwapChainBackBuffer00");
	DirectXStuff::ResultCheck(Result, L"SetName() failed.", L"SwapChain Prep Error");

	Result = SwapChain.GetInterface()->Present(SwapChainSyncInterval, NULL);
	DirectXStuff::ResultCheck(Result, L"Present() failed.", L"SwapChain Prep Error");

	Fence.FlushCommandQueue(CommandQueue.GetInterface());

	CurrentBackBufferIndex = SwapChain.GetInterface()->GetCurrentBackBufferIndex();

	Result = SwapChain.GetInterface()->GetBuffer(
		( UINT )CurrentBackBufferIndex, __uuidof(ID3D12Resource), std::bit_cast<void**>(&SwapChainBackBuffers[CurrentBackBufferIndex]));
	DirectXStuff::ResultCheck(Result, L"GetBuffer() failed.", L"SwapChain Prep Error");

	Result = SwapChainBackBuffers[CurrentBackBufferIndex]->SetName(L"SwapChainBackBuffer01");
	DirectXStuff::ResultCheck(Result, L"SetName() failed.", L"SwapChain Prep Error");

	Result = SwapChain.GetInterface()->Present(SwapChainSyncInterval, NULL);
	DirectXStuff::ResultCheck(Result, L"Present() failed.", L"SwapChain Prep Error");

	Fence.FlushCommandQueue(CommandQueue.GetInterface());

	D3D12_RESOURCE_BARRIER SwapChainBackBufferPresentToCopyDestBarriers[SwapChainBackBufferCount]{};

	for (__int64 i{ 0 }; i < SwapChainBackBufferCount; i++) {
		SwapChainBackBufferPresentToCopyDestBarriers[i] =
			DirectXStuff::CreateResourceTransitionBarrier(SwapChainBackBuffers[i], D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_DEST);
	}

	D3D12_RESOURCE_BARRIER SwapChainBackBufferCopyDestToPresentBarriers[SwapChainBackBufferCount]{};

	for (__int64 i{ 0 }; i < SwapChainBackBufferCount; i++) {
		SwapChainBackBufferCopyDestToPresentBarriers[i] =
			DirectXStuff::CreateResourceTransitionBarrier(SwapChainBackBuffers[i], D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PRESENT);
	}

	DirectXStuff::CommandAllocator PresentCommandAllocator{ Device.GetInterface(), L"PresentCommandAllocator" };

	DirectXStuff::GraphicsCommandList PresentGraphicsCommandList{ Device.GetInterface(), PresentCommandAllocator.GetInterface(),
		L"PresentGraphicsCommandList" };




	/*
		APPLICATION EVENT-LOOP:
	*/

	MSG MessageStruct{};
	const uint MessageFilterMin{ 0u }, MessageFilterMax{ 0u };

	const __int32 MaxRenderIndex{ ( __int32 )FinalFrameCount - 1 };
	__int32 CurrentRenderIndex{ 0 };
	bool CurrentlyRendering{ false };
	bool HasFinishedRendering{ false };// Once true, Left Mouse Button saves the current frame instead of restarting the render.
	std::chrono::steady_clock::time_point RenderStartTime{};
	std::chrono::milliseconds RenderDuration{ 0 };

	const __int32 MaxPresentIndex{ ( __int32 )FinalFrameCount - 1 };
	__int32 CurrentPresentIndex{ 0 };
	bool CurrentlyPresenting{ true };

	auto pGlobalDescriptorHeap = GlobalDescriptorHeap.GetInterface();

	D3D12_RESOURCE_BARRIER GlobalUAVBarrier{};
	GlobalUAVBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	GlobalUAVBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	GlobalUAVBarrier.UAV.pResource = nullptr;
	auto pRP1GraphicsCommandList = RP1GraphicsCommandList.GetListForSubmission();
	auto pRP3GraphicsCommandList = RP3GraphicsCommandList.GetListForSubmission();
	auto pPresentGraphicsCommandList = PresentGraphicsCommandList.GetListForSubmission();

	// Encodes an R8G8B8A8 host-side frame buffer to a .jpg file via WIC.
	auto SaveFinalFrameAsJpeg = [&](void* pPixelData, const wchar_t* FilePath) -> void {
		HRESULT LocalResult{ S_OK };

		IWICImagingFactory* pWICFactory{ nullptr };
		LocalResult = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pWICFactory));
		DirectXStuff::ResultCheck(LocalResult, L"CoCreateInstance() failed.", L"SaveFinalFrameAsJpeg Error");

		IWICBitmap* pWICBitmap{ nullptr };
		LocalResult = pWICFactory->CreateBitmapFromMemory(FinalFrameDimensions.x, FinalFrameDimensions.y, GUID_WICPixelFormat32bppRGBA,
			FinalFrameDimensions.x * BytesPerFinalPixel, ( UINT )(FinalFrameDimensions.x * FinalFrameDimensions.y * BytesPerFinalPixel),
			std::bit_cast<BYTE*>(pPixelData), &pWICBitmap);
		DirectXStuff::ResultCheck(LocalResult, L"CreateBitmapFromMemory() failed.", L"SaveFinalFrameAsJpeg Error");

		IWICStream* pWICStream{ nullptr };
		LocalResult = pWICFactory->CreateStream(&pWICStream);
		DirectXStuff::ResultCheck(LocalResult, L"CreateStream() failed.", L"SaveFinalFrameAsJpeg Error");

		LocalResult = pWICStream->InitializeFromFilename(FilePath, GENERIC_WRITE);
		DirectXStuff::ResultCheck(LocalResult, L"InitializeFromFilename() failed.", L"SaveFinalFrameAsJpeg Error");

		IWICBitmapEncoder* pWICEncoder{ nullptr };
		LocalResult = pWICFactory->CreateEncoder(GUID_ContainerFormatJpeg, nullptr, &pWICEncoder);
		DirectXStuff::ResultCheck(LocalResult, L"CreateEncoder() failed.", L"SaveFinalFrameAsJpeg Error");

		LocalResult = pWICEncoder->Initialize(pWICStream, WICBitmapEncoderNoCache);
		DirectXStuff::ResultCheck(LocalResult, L"Initialize() failed.", L"SaveFinalFrameAsJpeg Error");

		IWICBitmapFrameEncode* pWICFrameEncode{ nullptr };
		IPropertyBag2* pWICPropertyBag{ nullptr };
		LocalResult = pWICEncoder->CreateNewFrame(&pWICFrameEncode, &pWICPropertyBag);
		DirectXStuff::ResultCheck(LocalResult, L"CreateNewFrame() failed.", L"SaveFinalFrameAsJpeg Error");

		LocalResult = pWICFrameEncode->Initialize(pWICPropertyBag);
		DirectXStuff::ResultCheck(LocalResult, L"Initialize() failed.", L"SaveFinalFrameAsJpeg Error");

		LocalResult = pWICFrameEncode->SetSize(FinalFrameDimensions.x, FinalFrameDimensions.y);
		DirectXStuff::ResultCheck(LocalResult, L"SetSize() failed.", L"SaveFinalFrameAsJpeg Error");

		WICPixelFormatGUID JpegPixelFormat{ GUID_WICPixelFormat24bppBGR };
		LocalResult = pWICFrameEncode->SetPixelFormat(&JpegPixelFormat);
		DirectXStuff::ResultCheck(LocalResult, L"SetPixelFormat() failed.", L"SaveFinalFrameAsJpeg Error");

		IWICFormatConverter* pWICConverter{ nullptr };
		LocalResult = pWICFactory->CreateFormatConverter(&pWICConverter);
		DirectXStuff::ResultCheck(LocalResult, L"CreateFormatConverter() failed.", L"SaveFinalFrameAsJpeg Error");

		LocalResult =
			pWICConverter->Initialize(pWICBitmap, JpegPixelFormat, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);
		DirectXStuff::ResultCheck(LocalResult, L"Initialize() failed.", L"SaveFinalFrameAsJpeg Error");

		LocalResult = pWICFrameEncode->WriteSource(pWICConverter, nullptr);
		DirectXStuff::ResultCheck(LocalResult, L"WriteSource() failed.", L"SaveFinalFrameAsJpeg Error");

		LocalResult = pWICFrameEncode->Commit();
		DirectXStuff::ResultCheck(LocalResult, L"Commit() failed.", L"SaveFinalFrameAsJpeg Error");

		LocalResult = pWICEncoder->Commit();
		DirectXStuff::ResultCheck(LocalResult, L"Commit() failed.", L"SaveFinalFrameAsJpeg Error");

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

	MessageBox(TheatreWindow.ReportWindowHandle(), L"Begin Rendering: Left Mouse Button", L"Message", NULL);

	while (true == true) {
		while (PeekMessageW(&MessageStruct, NULL, MessageFilterMin, MessageFilterMax, PM_REMOVE)) {
			TranslateMessage(&MessageStruct);
			DispatchMessageW(&MessageStruct);
		}

		if (IsWindowVisible(TheatreWindow.ReportWindowHandle()) == false) {
			break;
		}

		// Rendering activation, or (once a render has completed) saving the currently-displayed frame to disk.
		if (MessageStruct.message == WM_LBUTTONUP) {
			if (HasFinishedRendering == false) {
				CurrentlyPresenting = false;
				CurrentRenderIndex = 0;
				CurrentlyRendering = true;
				RenderStartTime = std::chrono::steady_clock::now();
			} else {
				const std::chrono::zoned_time LocalTime{ std::chrono::current_zone(), std::chrono::system_clock::now() };
				const std::filesystem::path ImagesDirectory{ UNNAMED_RENDERER_IMAGES_DIRECTORY };
				std::filesystem::create_directories(ImagesDirectory);
				const std::wstring OutputFileStem{ std::format(L"{}-{}SPP-{}B-{}x{}-{:%Y%m%d}", CurrentScene.Name, SamplesPerPixel, MaxRecursionDepth,
					PSViewPortDimensions.x, PSViewPortDimensions.y, LocalTime) };
				auto IsImageCounterTaken = [&](const std::wstring& CounterPrefix) -> bool {
					for (const std::filesystem::directory_entry& Entry : std::filesystem::directory_iterator{ ImagesDirectory }) {
						const std::wstring FileName{ Entry.path().filename().wstring() };
						if (FileName.starts_with(CounterPrefix + L"-") || FileName == CounterPrefix + L".jpg") {
							return true;
						}
					}
					return false;
				};
				uint ImageCounter{ 1u };
				while (IsImageCounterTaken(std::format(L"{}-{:02}", OutputFileStem, ImageCounter))) {
					ImageCounter++;
				}
				const std::filesystem::path OutputFilePath{ ImagesDirectory / std::format(L"{}-{:02}-{}ms.jpg", OutputFileStem, ImageCounter, RenderDuration.count()) };
				SaveFinalFrameAsJpeg(HostFinalFrameBuffers[CurrentPresentIndex]->GetPointerToBufferStart(), OutputFilePath.c_str());
			}

			// Prevent this Left Mouse Button event from being re-handled on subsequent loop iterations
			// before the next real message overwrites MessageStruct.
			MessageStruct.message = 0u;
		}

		// Rendering Logic.
		if (CurrentlyRendering == true) {
			if (InlineRootConstants.CurrentSampleIndex == 0u) {
				// Set the Inline Root Constants and Scene Objects:
				InlineRootConstants.GlobalTickInRadians = 2.0f * (( float )CurrentRenderIndex / (( float )MaxRenderIndex));

				// Interpolation of the geometry values for spheres, then rectangles, then triangles.
				// Interpolation of the geometry values for spheres, then rectangles, then triangles.
				for (__int64 i{ 0 }; i < ( __int64 )Spheres.size(); i++) {
					Spheres[i].WSOrigin.x =
						LinearInterpolation(InlineRootConstants.GlobalTickInRadians / 2.0f, Spheres[i].WSOriginStart.x, Spheres[i].WSOriginEnd.x);
					Spheres[i].WSOrigin.y =
						LinearInterpolation(InlineRootConstants.GlobalTickInRadians / 2.0f, Spheres[i].WSOriginStart.y, Spheres[i].WSOriginEnd.y);
					Spheres[i].WSOrigin.z =
						LinearInterpolation(InlineRootConstants.GlobalTickInRadians / 2.0f, Spheres[i].WSOriginStart.z, Spheres[i].WSOriginEnd.z);

					Spheres[i].WSRadius = LinearInterpolation(InlineRootConstants.GlobalTickInRadians / 2.0f, Spheres[i].WSRadiusStart, Spheres[i].WSRadiusEnd);

					Spheres[i].Color.x = LinearInterpolation(InlineRootConstants.GlobalTickInRadians / 2.0f, Spheres[i].ColorStart.x, Spheres[i].ColorEnd.x);
					Spheres[i].Color.y = LinearInterpolation(InlineRootConstants.GlobalTickInRadians / 2.0f, Spheres[i].ColorStart.y, Spheres[i].ColorEnd.y);
					Spheres[i].Color.z = LinearInterpolation(InlineRootConstants.GlobalTickInRadians / 2.0f, Spheres[i].ColorStart.z, Spheres[i].ColorEnd.z);

					Spheres[i].MaterialScalar =
						LinearInterpolation(InlineRootConstants.GlobalTickInRadians / 2.0f, Spheres[i].MaterialScalarStart, Spheres[i].MaterialScalarEnd);
				}

				for (__int64 i{ 0 }; i < ( __int64 )Rectangles.size(); i++) {
					Rectangles[i].Q1.x = LinearInterpolation(InlineRootConstants.GlobalTickInRadians / 2.0f, Rectangles[i].Q1Start.x, Rectangles[i].Q1End.x);
					Rectangles[i].Q1.y = LinearInterpolation(InlineRootConstants.GlobalTickInRadians / 2.0f, Rectangles[i].Q1Start.y, Rectangles[i].Q1End.y);
					Rectangles[i].Q1.z = LinearInterpolation(InlineRootConstants.GlobalTickInRadians / 2.0f, Rectangles[i].Q1Start.z, Rectangles[i].Q1End.z);

					Rectangles[i].Q2.x = LinearInterpolation(InlineRootConstants.GlobalTickInRadians / 2.0f, Rectangles[i].Q2Start.x, Rectangles[i].Q2End.x);
					Rectangles[i].Q2.y = LinearInterpolation(InlineRootConstants.GlobalTickInRadians / 2.0f, Rectangles[i].Q2Start.y, Rectangles[i].Q2End.y);
					Rectangles[i].Q2.z = LinearInterpolation(InlineRootConstants.GlobalTickInRadians / 2.0f, Rectangles[i].Q2Start.z, Rectangles[i].Q2End.z);

					Rectangles[i].Q3.x = LinearInterpolation(InlineRootConstants.GlobalTickInRadians / 2.0f, Rectangles[i].Q3Start.x, Rectangles[i].Q3End.x);
					Rectangles[i].Q3.y = LinearInterpolation(InlineRootConstants.GlobalTickInRadians / 2.0f, Rectangles[i].Q3Start.y, Rectangles[i].Q3End.y);
					Rectangles[i].Q3.z = LinearInterpolation(InlineRootConstants.GlobalTickInRadians / 2.0f, Rectangles[i].Q3Start.z, Rectangles[i].Q3End.z);

					Rectangles[i].Q4.x = LinearInterpolation(InlineRootConstants.GlobalTickInRadians / 2.0f, Rectangles[i].Q4Start.x, Rectangles[i].Q4End.x);
					Rectangles[i].Q4.y = LinearInterpolation(InlineRootConstants.GlobalTickInRadians / 2.0f, Rectangles[i].Q4Start.y, Rectangles[i].Q4End.y);
					Rectangles[i].Q4.z = LinearInterpolation(InlineRootConstants.GlobalTickInRadians / 2.0f, Rectangles[i].Q4Start.z, Rectangles[i].Q4End.z);

					Rectangles[i].Color.x =
						LinearInterpolation(InlineRootConstants.GlobalTickInRadians / 2.0f, Rectangles[i].ColorStart.x, Rectangles[i].ColorEnd.x);
					Rectangles[i].Color.y =
						LinearInterpolation(InlineRootConstants.GlobalTickInRadians / 2.0f, Rectangles[i].ColorStart.y, Rectangles[i].ColorEnd.y);
					Rectangles[i].Color.z =
						LinearInterpolation(InlineRootConstants.GlobalTickInRadians / 2.0f, Rectangles[i].ColorStart.z, Rectangles[i].ColorEnd.z);

					Rectangles[i].MaterialScalar =
						LinearInterpolation(InlineRootConstants.GlobalTickInRadians / 2.0f, Rectangles[i].MaterialScalarStart, Rectangles[i].MaterialScalarEnd);
				}

				for (__int64 i{ 0 }; i < ( __int64 )Triangles.size(); i++) {
					Triangles[i].V1.x = LinearInterpolation(InlineRootConstants.GlobalTickInRadians / 2.0f, Triangles[i].V1Start.x, Triangles[i].V1End.x);
					Triangles[i].V1.y = LinearInterpolation(InlineRootConstants.GlobalTickInRadians / 2.0f, Triangles[i].V1Start.y, Triangles[i].V1End.y);
					Triangles[i].V1.z = LinearInterpolation(InlineRootConstants.GlobalTickInRadians / 2.0f, Triangles[i].V1Start.z, Triangles[i].V1End.z);

					Triangles[i].V2.x = LinearInterpolation(InlineRootConstants.GlobalTickInRadians / 2.0f, Triangles[i].V2Start.x, Triangles[i].V2End.x);
					Triangles[i].V2.y = LinearInterpolation(InlineRootConstants.GlobalTickInRadians / 2.0f, Triangles[i].V2Start.y, Triangles[i].V2End.y);
					Triangles[i].V2.z = LinearInterpolation(InlineRootConstants.GlobalTickInRadians / 2.0f, Triangles[i].V2Start.z, Triangles[i].V2End.z);

					Triangles[i].V3.x = LinearInterpolation(InlineRootConstants.GlobalTickInRadians / 2.0f, Triangles[i].V3Start.x, Triangles[i].V3End.x);
					Triangles[i].V3.y = LinearInterpolation(InlineRootConstants.GlobalTickInRadians / 2.0f, Triangles[i].V3Start.y, Triangles[i].V3End.y);
					Triangles[i].V3.z = LinearInterpolation(InlineRootConstants.GlobalTickInRadians / 2.0f, Triangles[i].V3Start.z, Triangles[i].V3End.z);

					Triangles[i].Color.x =
						LinearInterpolation(InlineRootConstants.GlobalTickInRadians / 2.0f, Triangles[i].ColorStart.x, Triangles[i].ColorEnd.x);
					Triangles[i].Color.y =
						LinearInterpolation(InlineRootConstants.GlobalTickInRadians / 2.0f, Triangles[i].ColorStart.y, Triangles[i].ColorEnd.y);
					Triangles[i].Color.z =
						LinearInterpolation(InlineRootConstants.GlobalTickInRadians / 2.0f, Triangles[i].ColorStart.z, Triangles[i].ColorEnd.z);

					Triangles[i].MaterialScalar =
						LinearInterpolation(InlineRootConstants.GlobalTickInRadians / 2.0f, Triangles[i].MaterialScalarStart, Triangles[i].MaterialScalarEnd);
				}

				// Move the Host data into L0 shared memory: Spheres, Rectangles, and Triangles.
				void* pL0SpheresBuffer{ nullptr };

				L0SpheresBuffer.GetInterface()->Map(0u, nullptr, &pL0SpheresBuffer);

				memcpy_s(pL0SpheresBuffer, L0SpheresBufferConfig.BufferWidth, Spheres.data(), Spheres.size() * sizeof(Sphere));

				L0SpheresBuffer.GetInterface()->Unmap(0u, nullptr);

				pL0SpheresBuffer = nullptr;

				void* pL0RectanglesBuffer{ nullptr };

				L0RectanglesBuffer.GetInterface()->Map(0u, nullptr, &pL0RectanglesBuffer);

				memcpy_s(pL0RectanglesBuffer, L0RectanglesBufferConfig.BufferWidth, Rectangles.data(), Rectangles.size() * sizeof(Rectangle));

				L0RectanglesBuffer.GetInterface()->Unmap(0u, nullptr);

				pL0RectanglesBuffer = nullptr;

				void* pL0TrianglesBuffer{ nullptr };

				L0TrianglesBuffer.GetInterface()->Map(0u, nullptr, &pL0TrianglesBuffer);

				memcpy_s(pL0TrianglesBuffer, L0TrianglesBufferConfig.BufferWidth, Triangles.data(), Triangles.size() * sizeof(Triangle));

				L0TrianglesBuffer.GetInterface()->Unmap(0u, nullptr);

				pL0TrianglesBuffer = nullptr;
			}

			if (InlineRootConstants.CurrentSampleIndex <= InlineRootConstants.MaxSampleIndex) {
				// Render-passes 1 and 2, batched: Generate the Intersection Maps, then accumulate them into the Accumulation Frame.
				RP1CommandAllocator.GetInterface()->Reset();

				RP1GraphicsCommandList.GetInterface()->Reset(RP1CommandAllocator.GetInterface(), RP1PipelineState.GetInterface());

				RP1GraphicsCommandList.GetInterface()->ResourceBarrier(1u, &L0SpheresBufferCopyDestToCopySource);

				RP1GraphicsCommandList.GetInterface()->ResourceBarrier(1u, &L1SpheresBufferUnorderedAccessToCopyDest);

				RP1GraphicsCommandList.GetInterface()->CopyResource(L1SpheresBuffer.GetInterface(), L0SpheresBuffer.GetInterface());

				RP1GraphicsCommandList.GetInterface()->ResourceBarrier(1u, &L0SpheresBufferCopySourceToCopyDest);

				RP1GraphicsCommandList.GetInterface()->ResourceBarrier(1u, &L1SpheresBufferCopyDestToUnorderedAccess);

				RP1GraphicsCommandList.GetInterface()->ResourceBarrier(1u, &L0RectanglesBufferCopyDestToCopySource);

				RP1GraphicsCommandList.GetInterface()->ResourceBarrier(1u, &L1RectanglesBufferUnorderedAccessToCopyDest);

				RP1GraphicsCommandList.GetInterface()->CopyResource(L1RectanglesBuffer.GetInterface(), L0RectanglesBuffer.GetInterface());

				RP1GraphicsCommandList.GetInterface()->ResourceBarrier(1u, &L0RectanglesBufferCopySourceToCopyDest);

				RP1GraphicsCommandList.GetInterface()->ResourceBarrier(1u, &L1RectanglesBufferCopyDestToUnorderedAccess);

				RP1GraphicsCommandList.GetInterface()->ResourceBarrier(1u, &L0TrianglesBufferCopyDestToCopySource);

				RP1GraphicsCommandList.GetInterface()->ResourceBarrier(1u, &L1TrianglesBufferUnorderedAccessToCopyDest);

				RP1GraphicsCommandList.GetInterface()->CopyResource(L1TrianglesBuffer.GetInterface(), L0TrianglesBuffer.GetInterface());

				RP1GraphicsCommandList.GetInterface()->ResourceBarrier(1u, &L0TrianglesBufferCopySourceToCopyDest);

				RP1GraphicsCommandList.GetInterface()->ResourceBarrier(1u, &L1TrianglesBufferCopyDestToUnorderedAccess);

				RP1GraphicsCommandList.GetInterface()->SetComputeRootSignature(GlobalRootSignature.GetInterface());

				RP1GraphicsCommandList.GetInterface()->SetDescriptorHeaps(1u, &pGlobalDescriptorHeap);

				RP1GraphicsCommandList.GetInterface()->SetComputeRootDescriptorTable(
					0u, GlobalDescriptorHeap.GetInterface()->GetGPUDescriptorHandleForHeapStart());

				const uint SubmissionEndSampleIndex{ std::min(InlineRootConstants.CurrentSampleIndex + SamplesPerSubmission, InlineRootConstants.MaxSampleIndex + 1u) };

				for (; InlineRootConstants.CurrentSampleIndex < SubmissionEndSampleIndex; InlineRootConstants.CurrentSampleIndex++) {
					RP1GraphicsCommandList.GetInterface()->SetComputeRoot32BitConstants(1u, RootConstantCount, ( void* )&InlineRootConstants, 0u);

					RP1GraphicsCommandList.GetInterface()->SetPipelineState(RP1PipelineState.GetInterface());

					RP1GraphicsCommandList.GetInterface()->Dispatch(GridDimensionsByGroup.x, GridDimensionsByGroup.y, GridDimensionsByGroup.z);

					RP1GraphicsCommandList.GetInterface()->ResourceBarrier(1u, &GlobalUAVBarrier);

					RP1GraphicsCommandList.GetInterface()->SetPipelineState(RP2PipelineState.GetInterface());

					RP1GraphicsCommandList.GetInterface()->Dispatch(GridDimensionsByGroup.x, GridDimensionsByGroup.y, GridDimensionsByGroup.z);

					RP1GraphicsCommandList.GetInterface()->ResourceBarrier(1u, &GlobalUAVBarrier);
				}

				RP1GraphicsCommandList.GetInterface()->Close();

				CommandQueue.GetInterface()->ExecuteCommandLists(1u, &pRP1GraphicsCommandList);

				Fence.FlushCommandQueue(CommandQueue.GetInterface());
			}

			if (InlineRootConstants.CurrentSampleIndex > InlineRootConstants.MaxSampleIndex) {
				// Render-pass 3: Generate and store the Final Frame
				RP3CommandAllocator.GetInterface()->Reset();

				RP3GraphicsCommandList.GetInterface()->Reset(RP3CommandAllocator.GetInterface(), RP3PipelineState.GetInterface());

				RP3GraphicsCommandList.GetInterface()->SetComputeRootSignature(GlobalRootSignature.GetInterface());

				RP3GraphicsCommandList.GetInterface()->SetDescriptorHeaps(1u, &pGlobalDescriptorHeap);

				RP3GraphicsCommandList.GetInterface()->SetComputeRootDescriptorTable(
					0u, GlobalDescriptorHeap.GetInterface()->GetGPUDescriptorHandleForHeapStart());

				RP3GraphicsCommandList.GetInterface()->SetComputeRoot32BitConstants(1u, RootConstantCount, ( void* )&InlineRootConstants, 0u);

				RP3GraphicsCommandList.GetInterface()->SetPipelineState(RP3PipelineState.GetInterface());

				RP3GraphicsCommandList.GetInterface()->Dispatch(GridDimensionsByGroup.x, GridDimensionsByGroup.y, GridDimensionsByGroup.z);

				RP3GraphicsCommandList.GetInterface()->ResourceBarrier(1u, &L1FinalFrame2DTextureUnorderedAccessToCopySource);

				RP3GraphicsCommandList.GetInterface()->CopyTextureRegion(
					&L0FinalFrameBufferTextureCopyLocation, 0u, 0u, 0u, &L1FinalFrame2DTextureTextureCopyLocation, nullptr);

				RP3GraphicsCommandList.GetInterface()->ResourceBarrier(1u, &L1FinalFrame2DTextureCopySourceToUnorderedAccess);

				RP3GraphicsCommandList.GetInterface()->ResourceBarrier(1u, &L0FinalFrameBufferCopyDestToCopySource);

				RP3GraphicsCommandList.GetInterface()->ResourceBarrier(1u, &L0FinalFrameBufferCopySourceToCopyDest);

				RP3GraphicsCommandList.GetInterface()->Close();

				CommandQueue.GetInterface()->ExecuteCommandLists(1u, &pRP3GraphicsCommandList);

				Fence.FlushCommandQueue(CommandQueue.GetInterface());

				void* pL0FinalFrameBuffer{ nullptr };

				L0FinalFrameBuffer.GetInterface()->Map(0u, nullptr, &pL0FinalFrameBuffer);

				memcpy_s(HostFinalFrameBuffers[CurrentRenderIndex]->GetPointerToBufferStart(),
					HostFinalFrameBuffers[CurrentRenderIndex]->GetBufferSizeInBytes(), pL0FinalFrameBuffer, L0FinalFrameBufferConfig.BufferWidth);

				L0FinalFrameBuffer.GetInterface()->Unmap(0u, nullptr);

				pL0FinalFrameBuffer = nullptr;

				InlineRootConstants.CurrentSampleIndex = 0u;

				CurrentRenderIndex++;

				if (CurrentRenderIndex > MaxRenderIndex) {
					CurrentlyRendering = false;
					CurrentlyPresenting = true;
					HasFinishedRendering = true;
					RenderDuration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - RenderStartTime);
				}
			}
		}

		// Present Logic.
		if (CurrentlyPresenting == true) {
			void* pL0FinalFrameBuffer{ nullptr };

			L0FinalFrameBuffer.GetInterface()->Map(0u, nullptr, &pL0FinalFrameBuffer);

			memcpy_s(pL0FinalFrameBuffer, L0FinalFrameBufferConfig.BufferWidth, HostFinalFrameBuffers[CurrentPresentIndex]->GetPointerToBufferStart(),
				HostFinalFrameBuffers[CurrentPresentIndex]->GetBufferSizeInBytes());

			L0FinalFrameBuffer.GetInterface()->Unmap(0u, nullptr);

			pL0FinalFrameBuffer = nullptr;

			PresentCommandAllocator.GetInterface()->Reset();

			PresentGraphicsCommandList.GetInterface()->Reset(PresentCommandAllocator.GetInterface(), nullptr);

			PresentGraphicsCommandList.GetInterface()->ResourceBarrier(1u, &L0FinalFrameBufferCopyDestToCopySource);

			PresentGraphicsCommandList.GetInterface()->ResourceBarrier(1u, &L1FinalFrame2DTextureUnorderedAccessToCopyDest);

			PresentGraphicsCommandList.GetInterface()->CopyTextureRegion(
				&L1FinalFrame2DTextureTextureCopyLocation, 0u, 0u, 0u, &L0FinalFrameBufferTextureCopyLocation, nullptr);

			PresentGraphicsCommandList.GetInterface()->ResourceBarrier(1u, &L0FinalFrameBufferCopySourceToCopyDest);

			PresentGraphicsCommandList.GetInterface()->ResourceBarrier(1u, &L1FinalFrame2DTextureCopyDestToCopySource);

			CurrentBackBufferIndex = SwapChain.GetInterface()->GetCurrentBackBufferIndex();

			PresentGraphicsCommandList.GetInterface()->ResourceBarrier(1u, &SwapChainBackBufferPresentToCopyDestBarriers[CurrentBackBufferIndex]);

			PresentGraphicsCommandList.GetInterface()->CopyResource(SwapChainBackBuffers[CurrentBackBufferIndex], L1FinalFrame2DTexture.GetInterface());

			PresentGraphicsCommandList.GetInterface()->ResourceBarrier(1u, &L1FinalFrame2DTextureCopySourceToUnorderedAccess);

			PresentGraphicsCommandList.GetInterface()->ResourceBarrier(1u, &SwapChainBackBufferCopyDestToPresentBarriers[CurrentBackBufferIndex]);

			PresentGraphicsCommandList.GetInterface()->Close();

			CommandQueue.GetInterface()->ExecuteCommandLists(1u, &pPresentGraphicsCommandList);

			SwapChain.GetInterface()->Present(1u, NULL);

			Fence.FlushCommandQueue(CommandQueue.GetInterface());

			CurrentPresentIndex++;

			if (CurrentPresentIndex > MaxPresentIndex) {
				CurrentPresentIndex = 0;
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
	DirectXStuff::ReportDXGIDebugInfo();

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

	for (__int64 i{ SwapChainBackBufferCount - 1 }; i >= 0; i--) {
		if (SwapChainBackBuffers[i] != nullptr) {
			SwapChainBackBuffers[i]->Release();
			SwapChainBackBuffers[i] = nullptr;
		}
	}

	for (__int64 i{ FinalFrameCount - 1 }; i >= 0; i--) {
		if (HostFinalFrameBuffers[i] != nullptr) {
			delete HostFinalFrameBuffers[i];
			HostFinalFrameBuffers[i] = nullptr;
		}
	}

	for (__int64 i{ FinalFrameCount - 1 }; i >= 0; i--) {
		if (HostFinalFrameBufferConfig[i] != nullptr) {
			delete HostFinalFrameBufferConfig[i];
			HostFinalFrameBufferConfig[i] = nullptr;
		}
	}

	CoUninitialize();

	// Exit.
	int SuccessExitCode{ 128 };
	return SuccessExitCode;
}
