// CornellBoxScene.hpp (Header Only)
// Oct 2026
// Chris M.
// https://github.com/RealTimeChris

#pragma once

#include "Scene.hpp"

namespace SceneStuff {

	Scene BuildScene() {
		Scene CornellBox{};
		CornellBox.WSCameraLookFrom = { 0.0f, 3.0f, 9.5f };
		CornellBox.WSCameraLookAt = { 0.0f, 3.0f, 0.0f };
		CornellBox.WSCameraUp = { 0.0f, 1.0f, 0.0f };
		CornellBox.VFoVInDegrees = 50.0f;
		CornellBox.Aperture = 0.0f;
		CornellBox.FocusDistance = 9.5f;
		CornellBox.SkyTopColor = { 0.0f, 0.0f, 0.0f };
		CornellBox.SkyBottomColor = { 0.0f, 0.0f, 0.0f };
		CornellBox.Name = L"CornellBox";

		std::vector<Sphere>& Spheres{ CornellBox.Spheres };
		unsigned __int32 NextObjectId{ 0u };

		// Glass sphere resting on the Cornell Box floor.
		Sphere GlassSphere{};
		GlassSphere.WSOriginStart = { 1.0f, 1.1f, 0.6f };
		GlassSphere.WSOriginEnd = GlassSphere.WSOriginStart;
		GlassSphere.WSRadiusStart = 1.1f;
		GlassSphere.WSRadiusEnd = 1.1f;
		GlassSphere.ColorStart = { 1.0f, 1.0f, 1.0f };
		GlassSphere.ColorEnd = GlassSphere.ColorStart;
		GlassSphere.MaterialScalarStart = 1.5f;
		GlassSphere.MaterialScalarEnd = GlassSphere.MaterialScalarStart;
		GlassSphere.PrimitiveId = 0u;
		GlassSphere.ObjectId = NextObjectId++;
		GlassSphere.MaterialId = 3u;

		Spheres.push_back(GlassSphere);


		// Array of Rectangles for the scene, to be provided for Shader use as a Structured Constant Buffer.
		// Cornell Box: floor, ceiling, back/left/right walls, one ceiling light, and a 5-sided tall box.
		std::vector<Rectangle>& Rectangles{ CornellBox.Rectangles };
		Rectangles.resize(11u);
		unsigned __int32 RectangleIndex{ 0u };

		constexpr float3 WallColor{ 0.73f, 0.73f, 0.73f };
		constexpr float3 GreenWallColor{ 0.12f, 0.45f, 0.15f };
		constexpr float3 RedWallColor{ 0.65f, 0.05f, 0.05f };
		constexpr float3 LightColor{ 15.0f, 15.0f, 15.0f };

		// Tall box footprint, yawed ~18 degrees around its center so its right-hand face
		// angles toward the glass sphere instead of sitting flat, so it can catch a reflection.
		constexpr float BoxYMax{ 3.3f };
		constexpr float BoxFLx{ -2.20806f };
		constexpr float BoxFLz{ -0.68637f };
		constexpr float BoxFRx{ -0.68637f };
		constexpr float BoxFRz{ -0.19194f };
		constexpr float BoxBLx{ -1.71363f };
		constexpr float BoxBLz{ -2.20806f };
		constexpr float BoxBRx{ -0.19194f };
		constexpr float BoxBRz{ -1.71363f };

		// Floor.
		Rectangles[0].Q1Start = { -3.0f, 0.0f, -3.0f };
		Rectangles[0].Q2Start = { -3.0f, 0.0f, +3.0f };
		Rectangles[0].Q3Start = { +3.0f, 0.0f, -3.0f };
		Rectangles[0].Q4Start = { +3.0f, 0.0f, +3.0f };
		Rectangles[0].Q1End = Rectangles[0].Q1Start;
		Rectangles[0].Q2End = Rectangles[0].Q2Start;
		Rectangles[0].Q3End = Rectangles[0].Q3Start;
		Rectangles[0].Q4End = Rectangles[0].Q4Start;
		Rectangles[0].ColorStart = WallColor;
		Rectangles[0].ColorEnd = WallColor;
		Rectangles[0].MaterialScalarStart = 0.0f;
		Rectangles[0].MaterialScalarEnd = 0.0f;
		Rectangles[0].PrimitiveId = 1u;
		Rectangles[0].ObjectId = RectangleIndex;
		Rectangles[0].MaterialId = 2u;

		RectangleIndex++;

		// Ceiling.
		Rectangles[1].Q1Start = { -3.0f, 6.0f, -3.0f };
		Rectangles[1].Q2Start = { +3.0f, 6.0f, -3.0f };
		Rectangles[1].Q3Start = { -3.0f, 6.0f, +3.0f };
		Rectangles[1].Q4Start = { +3.0f, 6.0f, +3.0f };
		Rectangles[1].Q1End = Rectangles[1].Q1Start;
		Rectangles[1].Q2End = Rectangles[1].Q2Start;
		Rectangles[1].Q3End = Rectangles[1].Q3Start;
		Rectangles[1].Q4End = Rectangles[1].Q4Start;
		Rectangles[1].ColorStart = WallColor;
		Rectangles[1].ColorEnd = WallColor;
		Rectangles[1].MaterialScalarStart = 0.0f;
		Rectangles[1].MaterialScalarEnd = 0.0f;
		Rectangles[1].PrimitiveId = 1u;
		Rectangles[1].ObjectId = RectangleIndex;
		Rectangles[1].MaterialId = 2u;

		RectangleIndex++;

		// Back wall.
		Rectangles[2].Q1Start = { -3.0f, 0.0f, -3.0f };
		Rectangles[2].Q2Start = { +3.0f, 0.0f, -3.0f };
		Rectangles[2].Q3Start = { -3.0f, 6.0f, -3.0f };
		Rectangles[2].Q4Start = { +3.0f, 6.0f, -3.0f };
		Rectangles[2].Q1End = Rectangles[2].Q1Start;
		Rectangles[2].Q2End = Rectangles[2].Q2Start;
		Rectangles[2].Q3End = Rectangles[2].Q3Start;
		Rectangles[2].Q4End = Rectangles[2].Q4Start;
		Rectangles[2].ColorStart = WallColor;
		Rectangles[2].ColorEnd = WallColor;
		Rectangles[2].MaterialScalarStart = 0.0f;
		Rectangles[2].MaterialScalarEnd = 0.0f;
		Rectangles[2].PrimitiveId = 1u;
		Rectangles[2].ObjectId = RectangleIndex;
		Rectangles[2].MaterialId = 2u;

		RectangleIndex++;

		// Left wall (green).
		Rectangles[3].Q1Start = { -3.0f, 0.0f, -3.0f };
		Rectangles[3].Q2Start = { -3.0f, 6.0f, -3.0f };
		Rectangles[3].Q3Start = { -3.0f, 0.0f, +3.0f };
		Rectangles[3].Q4Start = { -3.0f, 6.0f, +3.0f };
		Rectangles[3].Q1End = Rectangles[3].Q1Start;
		Rectangles[3].Q2End = Rectangles[3].Q2Start;
		Rectangles[3].Q3End = Rectangles[3].Q3Start;
		Rectangles[3].Q4End = Rectangles[3].Q4Start;
		Rectangles[3].ColorStart = GreenWallColor;
		Rectangles[3].ColorEnd = GreenWallColor;
		Rectangles[3].MaterialScalarStart = 0.0f;
		Rectangles[3].MaterialScalarEnd = 0.0f;
		Rectangles[3].PrimitiveId = 1u;
		Rectangles[3].ObjectId = RectangleIndex;
		Rectangles[3].MaterialId = 2u;

		RectangleIndex++;

		// Right wall (red).
		Rectangles[4].Q1Start = { +3.0f, 0.0f, -3.0f };
		Rectangles[4].Q2Start = { +3.0f, 0.0f, +3.0f };
		Rectangles[4].Q3Start = { +3.0f, 6.0f, -3.0f };
		Rectangles[4].Q4Start = { +3.0f, 6.0f, +3.0f };
		Rectangles[4].Q1End = Rectangles[4].Q1Start;
		Rectangles[4].Q2End = Rectangles[4].Q2Start;
		Rectangles[4].Q3End = Rectangles[4].Q3Start;
		Rectangles[4].Q4End = Rectangles[4].Q4Start;
		Rectangles[4].ColorStart = RedWallColor;
		Rectangles[4].ColorEnd = RedWallColor;
		Rectangles[4].MaterialScalarStart = 0.0f;
		Rectangles[4].MaterialScalarEnd = 0.0f;
		Rectangles[4].PrimitiveId = 1u;
		Rectangles[4].ObjectId = RectangleIndex;
		Rectangles[4].MaterialId = 2u;

		RectangleIndex++;

		// Ceiling light.
		Rectangles[5].Q1Start = { -1.5f, 5.98f, -0.6f };
		Rectangles[5].Q2Start = { +1.5f, 5.98f, -0.6f };
		Rectangles[5].Q3Start = { -1.5f, 5.98f, +0.6f };
		Rectangles[5].Q4Start = { +1.5f, 5.98f, +0.6f };
		Rectangles[5].Q1End = Rectangles[5].Q1Start;
		Rectangles[5].Q2End = Rectangles[5].Q2Start;
		Rectangles[5].Q3End = Rectangles[5].Q3Start;
		Rectangles[5].Q4End = Rectangles[5].Q4Start;
		Rectangles[5].ColorStart = LightColor;
		Rectangles[5].ColorEnd = LightColor;
		Rectangles[5].MaterialScalarStart = 0.0f;
		Rectangles[5].MaterialScalarEnd = 0.0f;
		Rectangles[5].PrimitiveId = 1u;
		Rectangles[5].ObjectId = RectangleIndex;
		Rectangles[5].MaterialId = 5u;

		RectangleIndex++;

		// Tall box: top face.
		Rectangles[6].Q1Start = { BoxBLx, BoxYMax, BoxBLz };
		Rectangles[6].Q2Start = { BoxFLx, BoxYMax, BoxFLz };
		Rectangles[6].Q3Start = { BoxBRx, BoxYMax, BoxBRz };
		Rectangles[6].Q4Start = { BoxFRx, BoxYMax, BoxFRz };
		Rectangles[6].Q1End = Rectangles[6].Q1Start;
		Rectangles[6].Q2End = Rectangles[6].Q2Start;
		Rectangles[6].Q3End = Rectangles[6].Q3Start;
		Rectangles[6].Q4End = Rectangles[6].Q4Start;
		Rectangles[6].ColorStart = WallColor;
		Rectangles[6].ColorEnd = WallColor;
		Rectangles[6].MaterialScalarStart = 0.0f;
		Rectangles[6].MaterialScalarEnd = 0.0f;
		Rectangles[6].PrimitiveId = 1u;
		Rectangles[6].ObjectId = RectangleIndex;
		Rectangles[6].MaterialId = 4u;

		RectangleIndex++;

		// Tall box: front face.
		Rectangles[7].Q1Start = { BoxFLx, 0.0f, BoxFLz };
		Rectangles[7].Q2Start = { BoxFRx, 0.0f, BoxFRz };
		Rectangles[7].Q3Start = { BoxFLx, BoxYMax, BoxFLz };
		Rectangles[7].Q4Start = { BoxFRx, BoxYMax, BoxFRz };
		Rectangles[7].Q1End = Rectangles[7].Q1Start;
		Rectangles[7].Q2End = Rectangles[7].Q2Start;
		Rectangles[7].Q3End = Rectangles[7].Q3Start;
		Rectangles[7].Q4End = Rectangles[7].Q4Start;
		Rectangles[7].ColorStart = WallColor;
		Rectangles[7].ColorEnd = WallColor;
		Rectangles[7].MaterialScalarStart = 0.0f;
		Rectangles[7].MaterialScalarEnd = 0.0f;
		Rectangles[7].PrimitiveId = 1u;
		Rectangles[7].ObjectId = RectangleIndex;
		Rectangles[7].MaterialId = 4u;

		RectangleIndex++;

		// Tall box: back face.
		Rectangles[8].Q1Start = { BoxBLx, 0.0f, BoxBLz };
		Rectangles[8].Q2Start = { BoxBLx, BoxYMax, BoxBLz };
		Rectangles[8].Q3Start = { BoxBRx, 0.0f, BoxBRz };
		Rectangles[8].Q4Start = { BoxBRx, BoxYMax, BoxBRz };
		Rectangles[8].Q1End = Rectangles[8].Q1Start;
		Rectangles[8].Q2End = Rectangles[8].Q2Start;
		Rectangles[8].Q3End = Rectangles[8].Q3Start;
		Rectangles[8].Q4End = Rectangles[8].Q4Start;
		Rectangles[8].ColorStart = WallColor;
		Rectangles[8].ColorEnd = WallColor;
		Rectangles[8].MaterialScalarStart = 0.0f;
		Rectangles[8].MaterialScalarEnd = 0.0f;
		Rectangles[8].PrimitiveId = 1u;
		Rectangles[8].ObjectId = RectangleIndex;
		Rectangles[8].MaterialId = 4u;

		RectangleIndex++;

		// Tall box: left face.
		Rectangles[9].Q1Start = { BoxBLx, 0.0f, BoxBLz };
		Rectangles[9].Q2Start = { BoxFLx, 0.0f, BoxFLz };
		Rectangles[9].Q3Start = { BoxBLx, BoxYMax, BoxBLz };
		Rectangles[9].Q4Start = { BoxFLx, BoxYMax, BoxFLz };
		Rectangles[9].Q1End = Rectangles[9].Q1Start;
		Rectangles[9].Q2End = Rectangles[9].Q2Start;
		Rectangles[9].Q3End = Rectangles[9].Q3Start;
		Rectangles[9].Q4End = Rectangles[9].Q4Start;
		Rectangles[9].ColorStart = WallColor;
		Rectangles[9].ColorEnd = WallColor;
		Rectangles[9].MaterialScalarStart = 0.0f;
		Rectangles[9].MaterialScalarEnd = 0.0f;
		Rectangles[9].PrimitiveId = 1u;
		Rectangles[9].ObjectId = RectangleIndex;
		Rectangles[9].MaterialId = 4u;

		RectangleIndex++;

		// Tall box: right face.
		Rectangles[10].Q1Start = { BoxBRx, 0.0f, BoxBRz };
		Rectangles[10].Q2Start = { BoxBRx, BoxYMax, BoxBRz };
		Rectangles[10].Q3Start = { BoxFRx, 0.0f, BoxFRz };
		Rectangles[10].Q4Start = { BoxFRx, BoxYMax, BoxFRz };
		Rectangles[10].Q1End = Rectangles[10].Q1Start;
		Rectangles[10].Q2End = Rectangles[10].Q2Start;
		Rectangles[10].Q3End = Rectangles[10].Q3Start;
		Rectangles[10].Q4End = Rectangles[10].Q4Start;
		Rectangles[10].ColorStart = WallColor;
		Rectangles[10].ColorEnd = WallColor;
		Rectangles[10].MaterialScalarStart = 0.0f;
		Rectangles[10].MaterialScalarEnd = 0.0f;
		Rectangles[10].PrimitiveId = 1u;
		Rectangles[10].ObjectId = RectangleIndex;
		Rectangles[10].MaterialId = 4u;

		RectangleIndex++;


		// Triangle procedural primitives. None are needed for the Cornell Box scene; a single
		// out-of-scene placeholder is kept since the GPU-side buffer must be non-zero-sized.
		std::vector<Triangle>& Triangles{ CornellBox.Triangles };
		Triangles.resize(1u);
		unsigned __int32 TriangleIndex{ 0u };

		Triangles[0].V1Start = { 1000.0f, 1000.0f, 1000.0f };
		Triangles[0].V1End = Triangles[0].V1Start;
		Triangles[0].V2Start = { 1001.0f, 1000.0f, 1000.0f };
		Triangles[0].V2End = Triangles[0].V2Start;
		Triangles[0].V3Start = { 1000.0f, 1001.0f, 1000.0f };
		Triangles[0].V3End = Triangles[0].V3Start;
		Triangles[0].ColorStart = { 0.0f, 0.0f, 0.0f };
		Triangles[0].ColorEnd = { 0.0f, 0.0f, 0.0f };
		Triangles[0].MaterialScalarStart = 0.0f;
		Triangles[0].MaterialScalarEnd = 0.0f;
		Triangles[0].PrimitiveId = 2u;
		Triangles[0].ObjectId = TriangleIndex;
		Triangles[0].MaterialId = 2u;

		TriangleIndex++;

		return CornellBox;
	}

}
