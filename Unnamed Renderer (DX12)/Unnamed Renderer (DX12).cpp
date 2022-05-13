// Unnamed Renderer (DX12).cpp - Hobby path-tracer.
// Apr 2020
// Chris M.
// https://github.com/RealTimeChris

#include "DirectXStuff.hpp"
#include "WinRTStuff.hpp"
#include <objbase.h>
#include <wincodec.h>
#pragma comment(lib, "windowscodecs.lib")

int __stdcall wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int) {
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

	// Represents a Procedural Sphere.
	struct Sphere {
		float3 WSOriginStart;
		float3 WSOriginEnd;
		float3 WSOrigin;// World-Space origin of the primitive.
		float WSRadiusStart;
		float WSRadiusEnd;
		float WSRadius;// World-Space radius of the primitive.
		float3 ColorStart;
		float3 ColorEnd;
		float3 Color;// Color/Light-Attenuation of the primitive.
		float MaterialScalarStart;
		float MaterialScalarEnd;
		float MaterialScalar;// Used for Metallic Fuzziness or Dielectric Refractive Index.
		uint PrimitiveId;// Used for identifying the type of primitive.
		uint ObjectId;// Unique identifier for the given sphere.
		uint MaterialId;// Material idenfitier for properly selecting intersection functions.
	};

	// Represents a Procedural Rectangle.
	struct Rectangle {
		float3 Q1Start;
		float3 Q1End;
		float3 Q1;// First "Corner" of the rectangle as a plane.
		float3 Q2Start;
		float3 Q2End;
		float3 Q2;// Second "Corner" of the rectangle as a plane.
		float3 Q3Start;
		float3 Q3End;
		float3 Q3;
		float3 Q4Start;
		float3 Q4End;
		float3 Q4;
		float3 ColorStart;
		float3 ColorEnd;
		float3 Color;// Color/Light-Attenuation of the primitive.
		float MaterialScalarStart;
		float MaterialScalarEnd;
		float MaterialScalar;// Used for Metallic Fuzziness or Dielectric Refractive Index.
		uint PrimitiveId;// Used for identifying the type of primitive.
		uint ObjectId;// Unique identifier for the given rectangle.
		uint MaterialId;// Material idenfitier for properly selecting intersection functions.
	};

	// Represents a Procedural Triangle primitive.
	struct Triangle {
		float3 V1Start;// First vertex of the triangle.
		float3 V1End;
		float3 V1;
		float3 V2Start;// Second vertex of the triangle.
		float3 V2End;
		float3 V2;
		float3 V3Start;// Third vertex of the triangle.
		float3 V3End;
		float3 V3;
		float3 ColorStart;
		float3 ColorEnd;
		float3 Color;
		float MaterialScalarStart;
		float MaterialScalarEnd;
		float MaterialScalar;
		uint PrimitiveId;
		uint ObjectId;
		uint MaterialId;
	};

	// View port dimesions, in pixels.
	const uint3 PSViewPortDimensions{ 1280u, 720u, 1u };
	const uint BytesPerFinalPixel{ 4u };

	// Values for defining and mapping the workload.
	const uint SecondsToRender{ 1u };
	const uint FramesPerSecond{ 2u };
	const uint FinalFrameCount{ FramesPerSecond * SecondsToRender };
	const uint SamplesPerPixel{ 100u };
	const uint3 TSGridDimensions{ PSViewPortDimensions.x, PSViewPortDimensions.y, PSViewPortDimensions.z };
	const uint3 TSGroupDimensions{ 128u, 8u, 1u };
	const uint3 GridDimensionsByGroup{ DirectXStuff::SetGroupCountPerGrid(TSGridDimensions, TSGroupDimensions) };

	// World-Space View Port Values.
	const float WSViewPortAspectRatio{ ( float )PSViewPortDimensions.x / ( float )PSViewPortDimensions.y };

	// Camera definition values.
	const float VFoVInDegrees{ 20.0f };
	const float VFoVInRadians{ (VFoVInDegrees / 180.0f) * ( float )M_PI };
	const float WSViewPortHeight{ 2.0f * ( float )tan(VFoVInRadians / 2.0f) };
	const float WSViewPortWidth{ WSViewPortHeight * WSViewPortAspectRatio };

	const float3 WSCameraLookFrom{ 13.0f, 2.0f, 3.0f };
	const float3 WSCameraLookAt{ 0.0f, 0.0f, 0.0f };
	const float3 WSCameraUp{ 0.0f, 1.0f, 0.0f };

	// Depth-of-field values. Aperture of 0.0f yields a pinhole camera (no blurring).
	const float Aperture{ 0.1f };
	const float LensRadius{ Aperture * 0.5f };
	const float FocusDistance{ 10.0f };

	// Maximum path-tracing recursion depth.
	const uint MaxRecursionDepth{ 30u };

	// Resource Values.
	const uint3 ChaosTexelsDimensions{ PSViewPortDimensions.x, PSViewPortDimensions.y, 5u };
	const uint ChaosTexelCount{ ChaosTexelsDimensions.x * ChaosTexelsDimensions.y * ChaosTexelsDimensions.z };
	const uint3 IntersectionMapDimensions{ PSViewPortDimensions.x, PSViewPortDimensions.y, MaxRecursionDepth };
	const uint2 AccumulationFrameDimensions{ PSViewPortDimensions.x, PSViewPortDimensions.y };
	const uint2 FinalFrameDimensions{ PSViewPortDimensions.x, PSViewPortDimensions.y };

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

	const uint RootConstantCount{ sizeof(InlineRootConstants) / sizeof(float) };

	InlineRootConstants InlineRootConstants{};

	InlineRootConstants.TSGridDimensions = TSGridDimensions;
	InlineRootConstants.WSCameraLookFrom = WSCameraLookFrom;
	InlineRootConstants.WSCameraLookAt = WSCameraLookAt;
	InlineRootConstants.WSCameraUp = WSCameraUp;
	InlineRootConstants.WSViewPortDimensions.x = WSViewPortWidth;
	InlineRootConstants.WSViewPortDimensions.y = WSViewPortHeight;
	InlineRootConstants.MaxRecursionDepth = MaxRecursionDepth;
	InlineRootConstants.SamplesPerPixel = SamplesPerPixel;
	InlineRootConstants.MaxSampleIndex = SamplesPerPixel - 1u;
	InlineRootConstants.CurrentSampleIndex = 0u;
	InlineRootConstants.PathMinDistance = 0.001f;
	InlineRootConstants.PathMaxDistance = 10'000.0f;
	InlineRootConstants.SkyTopColor = { 0.99f, 0.99f, 0.99f };
	InlineRootConstants.SkyBottomColor = { 0.07f, 0.14f, 0.93f };
	InlineRootConstants.SphereCount = 0u;
	InlineRootConstants.RectangleCount = 0u;
	InlineRootConstants.TriangleCount = 0u;
	InlineRootConstants.GlobalTickInRadians = 0.0f;
	InlineRootConstants.LensRadius = LensRadius;
	InlineRootConstants.FocusDistance = FocusDistance;

	/*
		PRIMITIVE ID:
			0 - Sphere
			1 - Rectangle
			2 - Triangle
	*/

	/*
		MATERIAL ID:
			0 - Miss/Sky
			1 - Surface Normal Map
			2 - Diffuse
			3 - Dielectric
			4 - Metallic
			5 - Diffuse Light
	*/

	// Array of Spheres for the scene, to be provided for Shader use as a Structured Constant Buffer.
	// Array of Spheres for the scene, to be provided for Shader use as a Structured Constant Buffer.
	std::vector<Sphere> Spheres{};
	unsigned __int32 NextObjectId{ 0u };

	auto RandomDouble = []() -> float {
		return ( float )rand() / (( float )RAND_MAX + 1.0f);
	};

	auto RandomDoubleRange = [&](float Min, float Max) -> float {
		return Min + (Max - Min) * RandomDouble();
	};

	auto RandomColor = [&]() -> float3 {
		return float3{ RandomDouble(), RandomDouble(), RandomDouble() };
	};

	auto RandomColorRange = [&](float Min, float Max) -> float3 {
		return float3{ RandomDoubleRange(Min, Max), RandomDoubleRange(Min, Max), RandomDoubleRange(Min, Max) };
	};

	// Ground sphere.
	Sphere GroundSphere{};
	GroundSphere.WSOriginStart = { 0.0f, -1000.0f, 0.0f };
	GroundSphere.WSOriginEnd = { 0.0f, -1000.0f, 0.0f };
	GroundSphere.WSRadiusStart = 1000.0f;
	GroundSphere.WSRadiusEnd = 1000.0f;
	GroundSphere.ColorStart = { 0.5f, 0.5f, 0.5f };
	GroundSphere.ColorEnd = { 0.5f, 0.5f, 0.5f };
	GroundSphere.MaterialScalarStart = 0.0f;
	GroundSphere.MaterialScalarEnd = 0.0f;
	GroundSphere.PrimitiveId = 0u;
	GroundSphere.ObjectId = NextObjectId++;
	GroundSphere.MaterialId = 2u;

	Spheres.push_back(GroundSphere);

	// Random small spheres.
	for (__int32 a{ -11 }; a < 11; a++) {
		for (__int32 b{ -11 }; b < 11; b++) {
			float ChooseMat{ RandomDouble() };

			float3 Center{ ( float )a + 0.9f * RandomDouble(), 0.2f, ( float )b + 0.9f * RandomDouble() };

			float3 AvoidPoint{ 4.0f, 0.2f, 0.0f };
			float3 Delta{ Center.x - AvoidPoint.x, Center.y - AvoidPoint.y, Center.z - AvoidPoint.z };
			float DistanceFromAvoidPoint{ sqrt(Delta.x * Delta.x + Delta.y * Delta.y + Delta.z * Delta.z) };

			if (DistanceFromAvoidPoint > 0.9f) {
				Sphere CurrentSphere{};
				CurrentSphere.WSOriginStart = Center;
				CurrentSphere.WSOriginEnd = Center;
				CurrentSphere.WSRadiusStart = 0.2f;
				CurrentSphere.WSRadiusEnd = 0.2f;
				CurrentSphere.PrimitiveId = 0u;
				CurrentSphere.ObjectId = NextObjectId++;

				if (ChooseMat < 0.2f) {
					// Diffuse.
					float3 Albedo01{ RandomColor() };
					float3 Albedo02{ RandomColor() };
					float3 Albedo{ Albedo01.x * Albedo02.x, Albedo01.y * Albedo02.y, Albedo01.z * Albedo02.z };

					CurrentSphere.ColorStart = Albedo;
					CurrentSphere.ColorEnd = Albedo;
					CurrentSphere.MaterialScalarStart = 0.0f;
					CurrentSphere.MaterialScalarEnd = 0.0f;
					CurrentSphere.MaterialId = 2u;
				} else if (ChooseMat < 0.55f) {
					// Metal.
					float3 Albedo{ RandomColorRange(0.5f, 1.0f) };
					float Fuzz{ RandomDoubleRange(0.0f, 0.5f) };

					CurrentSphere.ColorStart = Albedo;
					CurrentSphere.ColorEnd = Albedo;
					CurrentSphere.MaterialScalarStart = Fuzz;
					CurrentSphere.MaterialScalarEnd = Fuzz;
					CurrentSphere.MaterialId = 4u;
				} else {
					// Glass.
					CurrentSphere.ColorStart = { 1.0f, 1.0f, 1.0f };
					CurrentSphere.ColorEnd = { 1.0f, 1.0f, 1.0f };
					CurrentSphere.MaterialScalarStart = 1.5f;
					CurrentSphere.MaterialScalarEnd = 1.5f;
					CurrentSphere.MaterialId = 3u;
				}

				Spheres.push_back(CurrentSphere);
			}
		}
	}

	// Three feature spheres.
	Sphere GlassSphere{};
	GlassSphere.WSOriginStart = { 0.0f, 1.0f, 0.0f };
	GlassSphere.WSOriginEnd = { 0.0f, 1.0f, 0.0f };
	GlassSphere.WSRadiusStart = 1.0f;
	GlassSphere.WSRadiusEnd = 1.0f;
	GlassSphere.ColorStart = { 1.0f, 1.0f, 1.0f };
	GlassSphere.ColorEnd = { 1.0f, 1.0f, 1.0f };
	GlassSphere.MaterialScalarStart = 1.5f;
	GlassSphere.MaterialScalarEnd = 1.5f;
	GlassSphere.PrimitiveId = 0u;
	GlassSphere.ObjectId = NextObjectId++;
	GlassSphere.MaterialId = 3u;

	Spheres.push_back(GlassSphere);

	Sphere DiffuseSphere{};
	DiffuseSphere.WSOriginStart = { -4.0f, 1.0f, 0.0f };
	DiffuseSphere.WSOriginEnd = { -4.0f, 1.0f, 0.0f };
	DiffuseSphere.WSRadiusStart = 1.0f;
	DiffuseSphere.WSRadiusEnd = 1.0f;
	DiffuseSphere.ColorStart = { 0.4f, 0.2f, 0.1f };
	DiffuseSphere.ColorEnd = { 0.4f, 0.2f, 0.1f };
	DiffuseSphere.MaterialScalarStart = 0.0f;
	DiffuseSphere.MaterialScalarEnd = 0.0f;
	DiffuseSphere.PrimitiveId = 0u;
	DiffuseSphere.ObjectId = NextObjectId++;
	DiffuseSphere.MaterialId = 2u;

	Spheres.push_back(DiffuseSphere);

	Sphere MetalSphere{};
	MetalSphere.WSOriginStart = { 4.0f, 1.0f, 0.0f };
	MetalSphere.WSOriginEnd = { 4.0f, 1.0f, 0.0f };
	MetalSphere.WSRadiusStart = 1.0f;
	MetalSphere.WSRadiusEnd = 1.0f;
	MetalSphere.ColorStart = { 0.7f, 0.6f, 0.5f };
	MetalSphere.ColorEnd = { 0.7f, 0.6f, 0.5f };
	MetalSphere.MaterialScalarStart = 0.0f;
	MetalSphere.MaterialScalarEnd = 0.0f;
	MetalSphere.PrimitiveId = 0u;
	MetalSphere.ObjectId = NextObjectId++;
	MetalSphere.MaterialId = 4u;

	Spheres.push_back(MetalSphere);

	// Set the Sphere count in the Inline Root Constants.
	InlineRootConstants.SphereCount = ( uint )Spheres.size();

	// Array of Rectangles for the scene, to be provided for Shader use as a Structured Constant Buffer.
	Rectangle Rectangles[5]{};
	unsigned __int32 RectangleIndex{ 0u };

	Rectangles[0].Q1Start = { -8.0f, +2.0f, +5.0f };
	Rectangles[0].Q1End = { -8.0f, +4.0f, +9.0f };
	Rectangles[0].Q2Start = { -8.0f, +2.0f, +10.0f };
	Rectangles[0].Q2End = { -8.0f, +4.0f, +14.0f };
	Rectangles[0].Q3Start = { -8.0f, -2.0f, +5.0f };
	Rectangles[0].Q3End = { -8.0f, -4.0f, +10.0f };
	Rectangles[0].Q4Start = { -8.0f, -2.0f, +12.0f };
	Rectangles[0].Q4End = { -8.0f, -4.0f, +14.0f };
	Rectangles[0].ColorStart = { 0.0, 16.0, 0.0 };
	Rectangles[0].ColorEnd = { 0.0, 16.0, 0.0 };
	Rectangles[0].MaterialScalarStart = 0.0f;
	Rectangles[0].MaterialScalarEnd = 0.0f;
	Rectangles[0].PrimitiveId = 1u;
	Rectangles[0].ObjectId = RectangleIndex;
	Rectangles[0].MaterialId = 5u;

	RectangleIndex++;

	Rectangles[1].Q1Start = { +8.0f, +2.0f, +5.0f };
	Rectangles[1].Q1End = { +8.0f, +4.0f, +9.0f };
	Rectangles[1].Q2Start = { +8.0f, +2.0f, +10.0f };
	Rectangles[1].Q2End = { +8.0f, +4.0f, +14.0f };
	Rectangles[1].Q3Start = { +8.0f, -2.0f, +5.0f };
	Rectangles[1].Q3End = { +8.0f, -4.0f, +10.0f };
	Rectangles[1].Q4Start = { +8.0f, -2.0f, +12.0f };
	Rectangles[1].Q4End = { +8.0f, -4.0f, +14.0f };
	Rectangles[1].ColorStart = { 16.0, 0.0, 0.0 };
	Rectangles[1].ColorEnd = { 16.0, 0.0, 0.0 };
	Rectangles[1].MaterialScalarStart = 0.0f;
	Rectangles[1].MaterialScalarEnd = 0.0f;
	Rectangles[1].PrimitiveId = 1u;
	Rectangles[1].ObjectId = RectangleIndex;
	Rectangles[1].MaterialId = 5u;

	RectangleIndex++;

	Rectangles[2].Q1Start = { -8.0f, +0.0f, -8.0f };
	Rectangles[2].Q1End = { -8.0f, +0.0f, -3.0f };
	Rectangles[2].Q2Start = { +8.0f, +0.0f, -8.0f };
	Rectangles[2].Q2End = { +8.0f, +0.0f, -3.0f };
	Rectangles[2].Q3Start = { -8.0f, +8.0f, -8.0f };
	Rectangles[2].Q3End = { -8.0f, 8.0f, -3.0f };
	Rectangles[2].Q4Start = { +8.0f, 8.0f, -8.0f };
	Rectangles[2].Q4End = { +8.0f, 8.0f, -3.0f };
	Rectangles[2].ColorStart = { +0.0, +0.0, 16.0 };
	Rectangles[2].ColorEnd = { +0.0f, +0.0f, +16.0f };
	Rectangles[2].MaterialScalarStart = 0.0f;
	Rectangles[2].MaterialScalarEnd = 0.0f;
	Rectangles[2].PrimitiveId = 1u;
	Rectangles[2].ObjectId = RectangleIndex;
	Rectangles[2].MaterialId = 5u;

	RectangleIndex++;

	Rectangles[3].Q1Start = { -18.0f, -6.0f, -10.0f };
	Rectangles[3].Q1End = { -22.0f, -6.0f, -8.0f };
	Rectangles[3].Q2Start = { +18.0f, -6.0f, -10.0f };
	Rectangles[3].Q2End = { +22.0f, -6.0f, -8.0f };
	Rectangles[3].Q3Start = { -18.0f, +18.0f, -12.0f };
	Rectangles[3].Q3End = { -22.0f, +14.0f, -10.0f };
	Rectangles[3].Q4Start = { +18.0f, +18.0f, -12.0f };
	Rectangles[3].Q4End = { +22.0f, +14.0f, -10.0f };
	Rectangles[3].ColorStart = { +0.97f, +0.99f, +0.99f };
	Rectangles[3].ColorEnd = { +0.97f, +0.99f, +0.99f };
	Rectangles[3].MaterialScalarStart = 0.0f;
	Rectangles[3].MaterialScalarEnd = 0.0;
	Rectangles[3].PrimitiveId = 1u;
	Rectangles[3].ObjectId = RectangleIndex;
	Rectangles[3].MaterialId = 3u;

	RectangleIndex++;

	Rectangles[4].Q1Start = { 30.0f, -6.0f, 38.0f };
	Rectangles[4].Q1End = { 30.0f, -6.0f, 38.0f };
	Rectangles[4].Q2Start = { 30.0f, +16.0f, 38.0f };
	Rectangles[4].Q2End = { 30.0f, +16.0f, 38.0f };
	Rectangles[4].Q3Start = { 60.0f, -6.0f, 30.0f };
	Rectangles[4].Q3End = { 60.0f, -6.0f, 30.0f };
	Rectangles[4].Q4Start = { 60.0f, +16.0f, 30.0f };
	Rectangles[4].Q4End = { 60.0f, +16.0f, 30.0f };
	Rectangles[4].ColorStart = { +1.80f, +1.80f, +1.80f };
	Rectangles[4].ColorEnd = { 0.0f, +8.00f, +8.00f };
	Rectangles[4].MaterialScalarStart = 0.4f;
	Rectangles[4].MaterialScalarEnd = 0.4;
	Rectangles[4].PrimitiveId = 1u;
	Rectangles[4].ObjectId = RectangleIndex;
	Rectangles[4].MaterialId = 5u;

	RectangleIndex++;

	// Set the Rectangle count in the Inline Root Constants.
	InlineRootConstants.RectangleCount = sizeof(Rectangles) / sizeof(Rectangle);

	// Triangle procedural primitives.
	Triangle Triangles[5]{};
	unsigned __int32 TriangleIndex{ 0u };

	Triangles[0].V1Start = { -50.0f, -10.0f, +28.0f };
	Triangles[0].V1End = { -50.0f, -10.0f, +28.0f };
	Triangles[0].V2Start = { -20.0f, +36.0f, +34.0f };
	Triangles[0].V2End = { -20.0f, +45.0f, +34.0f };
	Triangles[0].V3Start = { -10.0f, -10.0f, +30.0f };
	Triangles[0].V3End = { -10.0f, -10.0f, +30.0f };
	Triangles[0].ColorStart = { +0.97f, +0.85f, +0.13f };
	Triangles[0].ColorEnd = { +0.65f, +0.85f, +0.65f };
	Triangles[0].MaterialScalarStart = 0.0f;
	Triangles[0].MaterialScalarEnd = 0.0f;
	Triangles[0].PrimitiveId = 2u;
	Triangles[0].ObjectId = TriangleIndex;
	Triangles[0].MaterialId = 4u;

	TriangleIndex++;

	Triangles[1].V1Start = { +6.0f, 15.0f, 65.0f };
	Triangles[1].V1End = { +6.0f, 15.0f, 65.0f };
	Triangles[1].V2Start = { 0.0f, 12.0f, 65.0f };
	Triangles[1].V2End = { 0.0f, 12.0f, 65.0f };
	Triangles[1].V3Start = { -6.0f, 150.0f, 65.0f };
	Triangles[1].V3End = { -6.0f, 15.0f, 65.0f };
	Triangles[1].ColorStart = { 0.15f, +0.43f, +1.0f };
	Triangles[1].ColorEnd = { +0.15f, +0.43f, +1.0f };
	Triangles[1].MaterialScalarStart = 0.0f;
	Triangles[1].MaterialScalarEnd = 0.0f;
	Triangles[1].PrimitiveId = 2u;
	Triangles[1].ObjectId = TriangleIndex;
	Triangles[1].MaterialId = 4u;

	TriangleIndex++;

	Triangles[2].V1Start = { +4.0f, -4.0f, +5.0f };
	Triangles[2].V1End = { +5.0f, -4.0f, +7.0f };
	Triangles[2].V2Start = { -4.0f, -4.0f, +1.0f };
	Triangles[2].V2End = { -1.0f, -4.0f, +3.0f };
	Triangles[2].V3Start = { +0.0f, -4.0f, +1.0f };
	Triangles[2].V3End = { +2.0f, -4.0f, +3.0f };
	Triangles[2].ColorStart = { +12.0f, +0.0f, +12.0f };
	Triangles[2].ColorEnd = { +12.0f, +0.0f, +12.0f };
	Triangles[2].MaterialScalarStart = 0.0f;
	Triangles[2].MaterialScalarEnd = 0.0f;
	Triangles[2].PrimitiveId = 2u;
	Triangles[2].ObjectId = TriangleIndex;
	Triangles[2].MaterialId = 5u;

	TriangleIndex++;

	Triangles[3].V1Start = { -34.0f, 5.0f, +40.0f };
	Triangles[3].V1End = { -28.0f, 5.0f, +44.0f };
	Triangles[3].V2Start = { -18.0f, +20.0f, +36.0f };
	Triangles[3].V2End = { -18.0f, +14.0f, +36.0f };
	Triangles[3].V3Start = { -8.0f, 5.0f, +36.0f };
	Triangles[3].V3End = { -14.0f, 5.0f, +32.0f };
	Triangles[3].ColorStart = { +12.0f, +12.0f, +12.0f };
	Triangles[3].ColorEnd = { +12.0f, +12.0f, +12.0f };
	Triangles[3].MaterialScalarStart = 0.0f;
	Triangles[3].MaterialScalarEnd = 0.0f;
	Triangles[3].PrimitiveId = 2u;
	Triangles[3].ObjectId = TriangleIndex;
	Triangles[3].MaterialId = 5u;

	TriangleIndex++;

	Triangles[4].V1Start = { +34.0f, 5.0f, +40.0f };
	Triangles[4].V1End = { +28.0f, 5.0f, +44.0f };
	Triangles[4].V2Start = { +18.0f, +20.0f, +36.0f };
	Triangles[4].V2End = { +18.0f, +14.0f, +36.0f };
	Triangles[4].V3Start = { +8.0f, 5.0f, +36.0f };
	Triangles[4].V3End = { +14.0f, 5.0f, +32.0f };
	Triangles[4].ColorStart = { +12.0f, +12.0f, +0.0f };
	Triangles[4].ColorEnd = { +12.0f, +12.0f, +0.0f };
	Triangles[4].MaterialScalarStart = 0.0f;
	Triangles[4].MaterialScalarEnd = 0.0f;
	Triangles[4].PrimitiveId = 2u;
	Triangles[4].ObjectId = TriangleIndex;
	Triangles[4].MaterialId = 5u;

	TriangleIndex++;

	// Set the Triangle count in the Inline Root Constants.
	InlineRootConstants.TriangleCount = sizeof(Triangles) / sizeof(Triangle);




	/*
		GLOBAL PIPELINE RESOURCES/BARRIERS/COPY LOCATIONS:
			-L0 Spheres Buffer, L1 Spheres Buffer
			-L0 Rectangles Buffer, L1 Rectangles Buffer
			-L0 Triangles Buffer, L1 Triangles Buffer
			-Host Chaos Texels Buffer, L0 Chaos Texels Buffer, L1 Chaos Texels 3DTexture
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
	L0SpheresBufferConfig.BufferWidth = Spheres.size() * sizeof(Sphere);

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
	L1SpheresBufferConfig.BufferWidth = Spheres.size() * sizeof(Sphere);

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
	L0RectanglesBufferConfig.BufferWidth = sizeof(Rectangles);

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
	L1RectanglesBufferConfig.BufferWidth = sizeof(Rectangles);

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
	L0TrianglesBufferConfig.BufferWidth = sizeof(Triangles);

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
	L1TrianglesBufferConfig.BufferWidth = sizeof(Triangles);

	DirectXStuff::Buffer L1TrianglesBuffer{ Device.GetInterface(), L1TrianglesBufferConfig, L"L1TrianglesBuffer" };

	D3D12_RESOURCE_BARRIER L1TrianglesBufferCopyDestToUnorderedAccess{};
	L1TrianglesBufferCopyDestToUnorderedAccess =
		DirectXStuff::CreateResourceTransitionBarrier(L1TrianglesBuffer.GetInterface(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	D3D12_RESOURCE_BARRIER L1TrianglesBufferUnorderedAccessToCopyDest{};
	L1TrianglesBufferUnorderedAccessToCopyDest =
		DirectXStuff::CreateResourceTransitionBarrier(L1TrianglesBuffer.GetInterface(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_DEST);

	// Chaos Texels Buffer, Host version.
	DirectXStuff::RandomFloatHostBufferConfig HostChaosTexelsBufferConfig{};
	HostChaosTexelsBufferConfig.RandomFloatCount = ChaosTexelCount;

	DirectXStuff::RandomFloatHostBuffer HostChaosTexelsBuffer{ HostChaosTexelsBufferConfig };

	// Chaos Texels Buffer, Upload version.
	DirectXStuff::BufferConfig L0ChaosTexelsBufferConfig{};
	L0ChaosTexelsBufferConfig.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_COMBINE;
	L0ChaosTexelsBufferConfig.MemoryPool = D3D12_MEMORY_POOL_L0;
	L0ChaosTexelsBufferConfig.InitialResourceState = D3D12_RESOURCE_STATE_COPY_DEST;
	L0ChaosTexelsBufferConfig.BufferFormat = DXGI_FORMAT_UNKNOWN;
	L0ChaosTexelsBufferConfig.BufferWidth = ChaosTexelCount * sizeof(float);

	DirectXStuff::Buffer L0ChaosTexelsBuffer{ Device.GetInterface(), L0ChaosTexelsBufferConfig, L"L0ChaosTexelsBuffer" };

	D3D12_RESOURCE_BARRIER L0ChaosTexelsBufferCopyDestToCopySource{};
	L0ChaosTexelsBufferCopyDestToCopySource =
		DirectXStuff::CreateResourceTransitionBarrier(L0ChaosTexelsBuffer.GetInterface(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COPY_SOURCE);

	D3D12_RESOURCE_BARRIER L0ChaosTexelsBufferCopySourceToCopyDest{};
	L0ChaosTexelsBufferCopySourceToCopyDest =
		DirectXStuff::CreateResourceTransitionBarrier(L0ChaosTexelsBuffer.GetInterface(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COPY_DEST);

	D3D12_TEXTURE_COPY_LOCATION L0ChaosTexelsBufferTextureCopyLocation{};
	L0ChaosTexelsBufferTextureCopyLocation.pResource = L0ChaosTexelsBuffer.GetInterface();
	L0ChaosTexelsBufferTextureCopyLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	L0ChaosTexelsBufferTextureCopyLocation.PlacedFootprint.Offset = 0u;
	L0ChaosTexelsBufferTextureCopyLocation.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R32_FLOAT;
	L0ChaosTexelsBufferTextureCopyLocation.PlacedFootprint.Footprint.Width = ChaosTexelsDimensions.x;
	L0ChaosTexelsBufferTextureCopyLocation.PlacedFootprint.Footprint.Height = ChaosTexelsDimensions.y;
	L0ChaosTexelsBufferTextureCopyLocation.PlacedFootprint.Footprint.Depth = ChaosTexelsDimensions.z;
	L0ChaosTexelsBufferTextureCopyLocation.PlacedFootprint.Footprint.RowPitch = ChaosTexelsDimensions.x * sizeof(float);

	// Chaos Texels 3DTexture, Device version.
	DirectXStuff::Texture3DConfig L1ChaosTexels3DTextureConfig{};
	L1ChaosTexels3DTextureConfig.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_NOT_AVAILABLE;
	L1ChaosTexels3DTextureConfig.MemoryPool = D3D12_MEMORY_POOL_L1;
	L1ChaosTexels3DTextureConfig.NodeMask = 0u;
	L1ChaosTexels3DTextureConfig.InitialResourceState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	L1ChaosTexels3DTextureConfig.TextureFormat = DXGI_FORMAT_R32_FLOAT;
	L1ChaosTexels3DTextureConfig.TextureWidth = ChaosTexelsDimensions.x;
	L1ChaosTexels3DTextureConfig.TextureHeight = ChaosTexelsDimensions.y;
	L1ChaosTexels3DTextureConfig.TextureDepth = ChaosTexelsDimensions.z;

	DirectXStuff::Texture3D L1ChaosTexels3DTexture{ Device.GetInterface(), L1ChaosTexels3DTextureConfig, L"L1ChaosTexels3DTexture" };

	D3D12_RESOURCE_BARRIER L1ChaosTexels3DTextureCopyDestToUnorderedAccess{};
	L1ChaosTexels3DTextureCopyDestToUnorderedAccess = DirectXStuff::CreateResourceTransitionBarrier(
		L1ChaosTexels3DTexture.GetInterface(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	D3D12_RESOURCE_BARRIER L1ChaosTexels3DTextureUnorderedAccessToCopyDest{};
	L1ChaosTexels3DTextureUnorderedAccessToCopyDest = DirectXStuff::CreateResourceTransitionBarrier(
		L1ChaosTexels3DTexture.GetInterface(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_DEST);

	D3D12_TEXTURE_COPY_LOCATION L1ChaosTexels3DTextureTextureCopyLocation{};
	L1ChaosTexels3DTextureTextureCopyLocation.pResource = L1ChaosTexels3DTexture.GetInterface();
	L1ChaosTexels3DTextureTextureCopyLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	L1ChaosTexels3DTextureTextureCopyLocation.SubresourceIndex = 0u;

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
			Resource 3 - L1ChaosTexels3DTexture
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

	D3D12_CPU_DESCRIPTOR_HANDLE L1ChaosTexels3DTextureUAVCPUHandle{};

	D3D12_UNORDERED_ACCESS_VIEW_DESC L1ChaosTexels3DTextureUAVDesc{};
	L1ChaosTexels3DTextureUAVDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
	L1ChaosTexels3DTextureUAVDesc.Format = L1ChaosTexels3DTextureConfig.TextureFormat;
	L1ChaosTexels3DTextureUAVDesc.Texture3D.FirstWSlice = 0u;
	L1ChaosTexels3DTextureUAVDesc.Texture3D.MipSlice = 0u;
	L1ChaosTexels3DTextureUAVDesc.Texture3D.WSize = L1ChaosTexels3DTextureConfig.TextureDepth;

	L1ChaosTexels3DTextureUAVCPUHandle.ptr =
		GlobalDescriptorHeap.GetInterface()->GetCPUDescriptorHandleForHeapStart().ptr + (CurrentDescriptorIndex * DescriptorHandleIncrementSize);

	Device.GetInterface()->CreateUnorderedAccessView(
		L1ChaosTexels3DTexture.GetInterface(), nullptr, &L1ChaosTexels3DTextureUAVDesc, L1ChaosTexels3DTextureUAVCPUHandle);

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

	DirectXStuff::CommandAllocator RP2CommandAllocator{ Device.GetInterface(), L"RP2CommandAllocator" };

	DirectXStuff::GraphicsCommandList RP2GraphicsCommandList{ Device.GetInterface(), RP2CommandAllocator.GetInterface(), L"RP2GraphicsCommandList" };




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
		( UINT )CurrentBackBufferIndex, __uuidof(ID3D12Resource), reinterpret_cast<void**>(&SwapChainBackBuffers[CurrentBackBufferIndex]));
	DirectXStuff::ResultCheck(Result, L"GetBuffer() failed.", L"SwapChain Prep Error");

	Result = SwapChainBackBuffers[CurrentBackBufferIndex]->SetName(L"SwapChainBackBuffer00");
	DirectXStuff::ResultCheck(Result, L"SetName() failed.", L"SwapChain Prep Error");

	Result = SwapChain.GetInterface()->Present(SwapChainSyncInterval, NULL);
	DirectXStuff::ResultCheck(Result, L"Present() failed.", L"SwapChain Prep Error");

	Fence.FlushCommandQueue(CommandQueue.GetInterface());

	CurrentBackBufferIndex = SwapChain.GetInterface()->GetCurrentBackBufferIndex();

	Result = SwapChain.GetInterface()->GetBuffer(
		( UINT )CurrentBackBufferIndex, __uuidof(ID3D12Resource), reinterpret_cast<void**>(&SwapChainBackBuffers[CurrentBackBufferIndex]));
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

	const __int32 MaxPresentIndex{ ( __int32 )FinalFrameCount - 1 };
	__int32 CurrentPresentIndex{ 0 };
	bool CurrentlyPresenting{ true };

	auto pGlobalDescriptorHeap = GlobalDescriptorHeap.GetInterface();
	auto pRP1GraphicsCommandList = RP1GraphicsCommandList.GetListForSubmission();
	auto pRP2GraphicsCommandList = RP2GraphicsCommandList.GetListForSubmission();
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
			reinterpret_cast<BYTE*>(pPixelData), &pWICBitmap);
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
			} else {
				SaveFinalFrameAsJpeg(HostFinalFrameBuffers[CurrentPresentIndex]->GetPointerToBufferStart(), L"RenderedFrame.jpg");
			}

			// Prevent this Left Mouse Button event from being re-handled on subsequent loop iterations
			// before the next real message overwrites MessageStruct.
			MessageStruct.message = 0u;
		}

		// Rendering Logic.
		if (CurrentlyRendering == true) {
			if (InlineRootConstants.CurrentSampleIndex == 0u) {
				// Set the Inline Root Constants, Scene Objects, and Chaos Texels:
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

				for (__int64 i{ 0 }; i < sizeof(Rectangles) / sizeof(Rectangle); i++) {
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

				for (__int64 i{ 0 }; i < sizeof(Triangles) / sizeof(Triangle); i++) {
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

				//memcpy_s(pL0RectanglesBuffer, L0RectanglesBufferConfig.BufferWidth, Rectangles, sizeof(Rectangles));

				L0RectanglesBuffer.GetInterface()->Unmap(0u, nullptr);

				pL0RectanglesBuffer = nullptr;

				void* pL0TrianglesBuffer{ nullptr };

				L0TrianglesBuffer.GetInterface()->Map(0u, nullptr, &pL0TrianglesBuffer);

				//memcpy_s(pL0TrianglesBuffer, L0TrianglesBufferConfig.BufferWidth, Triangles, sizeof(Triangles));

				L0TrianglesBuffer.GetInterface()->Unmap(0u, nullptr);

				pL0TrianglesBuffer = nullptr;
			}

			if (InlineRootConstants.CurrentSampleIndex <= InlineRootConstants.MaxSampleIndex) {
				// Update and copy the Chaos Texels.
				HostChaosTexelsBuffer.RefreshBufferContents();

				void* pL0ChaosTexelsBuffer{ nullptr };

				L0ChaosTexelsBuffer.GetInterface()->Map(0u, nullptr, &pL0ChaosTexelsBuffer);

				memcpy_s(pL0ChaosTexelsBuffer, L0ChaosTexelsBufferConfig.BufferWidth, HostChaosTexelsBuffer.GetPointerToBufferStart(),
					HostChaosTexelsBuffer.GetBufferSizeInBytes());

				L0ChaosTexelsBuffer.GetInterface()->Unmap(0u, nullptr);

				pL0ChaosTexelsBuffer = nullptr;


				// Render-pass 1: Generate the Intersection Map
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

				RP1GraphicsCommandList.GetInterface()->ResourceBarrier(1u, &L0ChaosTexelsBufferCopyDestToCopySource);

				RP1GraphicsCommandList.GetInterface()->ResourceBarrier(1u, &L1ChaosTexels3DTextureUnorderedAccessToCopyDest);

				RP1GraphicsCommandList.GetInterface()->CopyTextureRegion(
					&L1ChaosTexels3DTextureTextureCopyLocation, 0u, 0u, 0u, &L0ChaosTexelsBufferTextureCopyLocation, nullptr);

				RP1GraphicsCommandList.GetInterface()->ResourceBarrier(1u, &L0ChaosTexelsBufferCopySourceToCopyDest);

				RP1GraphicsCommandList.GetInterface()->ResourceBarrier(1u, &L1ChaosTexels3DTextureCopyDestToUnorderedAccess);

				RP1GraphicsCommandList.GetInterface()->SetComputeRootSignature(GlobalRootSignature.GetInterface());

				RP1GraphicsCommandList.GetInterface()->SetDescriptorHeaps(1u, &pGlobalDescriptorHeap);

				RP1GraphicsCommandList.GetInterface()->SetComputeRootDescriptorTable(
					0u, GlobalDescriptorHeap.GetInterface()->GetGPUDescriptorHandleForHeapStart());

				RP1GraphicsCommandList.GetInterface()->SetComputeRoot32BitConstants(1u, RootConstantCount, ( void* )&InlineRootConstants, 0u);

				RP1GraphicsCommandList.GetInterface()->SetPipelineState(RP1PipelineState.GetInterface());

				RP1GraphicsCommandList.GetInterface()->Dispatch(GridDimensionsByGroup.x, GridDimensionsByGroup.y, GridDimensionsByGroup.z);

				RP1GraphicsCommandList.GetInterface()->Close();

				CommandQueue.GetInterface()->ExecuteCommandLists(1u, &pRP1GraphicsCommandList);

				Fence.FlushCommandQueue(CommandQueue.GetInterface());



				// Render-pass 2: Generate the Accumulation Frame
				RP2CommandAllocator.GetInterface()->Reset();

				RP2GraphicsCommandList.GetInterface()->Reset(RP2CommandAllocator.GetInterface(), RP2PipelineState.GetInterface());

				RP2GraphicsCommandList.GetInterface()->SetComputeRootSignature(GlobalRootSignature.GetInterface());

				RP2GraphicsCommandList.GetInterface()->SetDescriptorHeaps(1u, &pGlobalDescriptorHeap);

				RP2GraphicsCommandList.GetInterface()->SetComputeRootDescriptorTable(
					0u, GlobalDescriptorHeap.GetInterface()->GetGPUDescriptorHandleForHeapStart());

				RP2GraphicsCommandList.GetInterface()->SetComputeRoot32BitConstants(1u, RootConstantCount, ( void* )&InlineRootConstants, 0u);

				RP2GraphicsCommandList.GetInterface()->SetPipelineState(RP2PipelineState.GetInterface());

				RP2GraphicsCommandList.GetInterface()->Dispatch(GridDimensionsByGroup.x, GridDimensionsByGroup.y, GridDimensionsByGroup.z);

				RP2GraphicsCommandList.GetInterface()->Close();

				CommandQueue.GetInterface()->ExecuteCommandLists(1u, &pRP2GraphicsCommandList);

				Fence.FlushCommandQueue(CommandQueue.GetInterface());

				InlineRootConstants.CurrentSampleIndex++;
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

	if (pRP2GraphicsCommandList != nullptr) {
		pRP2GraphicsCommandList = nullptr;
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
