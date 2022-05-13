// CornellBoxScene.hpp (Header Only)
// Oct 2026
// Chris M.
// https://github.com/RealTimeChris

#pragma once

#include "Scene.hpp"

namespace scene_stuff {

	URDX12_INLINE scene build_scene() {
		scene cornellBox{};
		cornellBox.wsCameraLookFrom = { 0.0f, 3.0f, 9.5f };
		cornellBox.wsCameraLookAt	= { 0.0f, 3.0f, 0.0f };
		cornellBox.wsCameraUp		= { 0.0f, 1.0f, 0.0f };
		cornellBox.vFoVInDegrees	= 50.0f;
		cornellBox.aperture			= 0.0f;
		cornellBox.focusDistance	= 9.5f;
		cornellBox.skyTopColor		= { 0.0f, 0.0f, 0.0f };
		cornellBox.skyBottomColor	= { 0.0f, 0.0f, 0.0f };
		cornellBox.name				= L"CornellBox";

		std::vector<sphere>& spheres{ cornellBox.spheres };
		unsigned __int32 nextObjectId{ 0u };

		// Glass sphere resting on the Cornell Box floor.
		sphere glassSphere{};
		glassSphere.wsOriginStart		= { 1.0f, 1.1f, 0.6f };
		glassSphere.wsOriginEnd			= glassSphere.wsOriginStart;
		glassSphere.wsRadiusStart		= 1.1f;
		glassSphere.wsRadiusEnd			= 1.1f;
		glassSphere.colorStart			= { 1.0f, 1.0f, 1.0f };
		glassSphere.colorEnd			= glassSphere.colorStart;
		glassSphere.materialScalarStart = 1.5f;
		glassSphere.materialScalarEnd	= glassSphere.materialScalarStart;
		glassSphere.primitiveId			= 0u;
		glassSphere.objectId			= nextObjectId++;
		glassSphere.materialId			= 3u;

		spheres.emplace_back(glassSphere);

		sphere redLightSphere{};
		redLightSphere.wsOriginStart	   = { 1.4f, 2.9f, -0.8f };
		redLightSphere.wsOriginEnd		   = redLightSphere.wsOriginStart;
		redLightSphere.wsRadiusStart	   = 0.5f;
		redLightSphere.wsRadiusEnd		   = 0.5f;
		redLightSphere.colorStart		   = { 4.0f, 0.0f, 0.0f };
		redLightSphere.colorEnd			   = redLightSphere.colorStart;
		redLightSphere.materialScalarStart = 0.0f;
		redLightSphere.materialScalarEnd   = redLightSphere.materialScalarStart;
		redLightSphere.primitiveId		   = 0u;
		redLightSphere.objectId			   = nextObjectId++;
		redLightSphere.materialId		   = 5u;

		spheres.emplace_back(redLightSphere);

		// Array of Rectangles for the scene, to be provided for Shader use as a Structured Constant Buffer.
		// Cornell Box: floor, ceiling, back/left/right walls, one ceiling light, and a 5-sided tall box.
		std::vector<rectangle>& rectangles{ cornellBox.rectangles };
		rectangles.resize(11u);
		unsigned __int32 rectangleIndex{ 0u };

		constexpr float3 wallColor{ 0.73f, 0.73f, 0.73f };
		constexpr float3 greenWallColor{ 0.12f, 0.45f, 0.15f };
		constexpr float3 redWallColor{ 0.65f, 0.05f, 0.05f };
		constexpr float3 lightColor{ 15.0f, 15.0f, 15.0f };

		// Tall box footprint, yawed ~18 degrees around its center so its right-hand face
		// angles toward the glass sphere instead of sitting flat, so it can catch a reflection.
		constexpr float boxYMax{ 3.3f };
		constexpr float boxFLx{ -2.20806f };
		constexpr float boxFLz{ -0.68637f };
		constexpr float boxFRx{ -0.68637f };
		constexpr float boxFRz{ -0.19194f };
		constexpr float boxBLx{ -1.71363f };
		constexpr float boxBLz{ -2.20806f };
		constexpr float boxBRx{ -0.19194f };
		constexpr float boxBRz{ -1.71363f };

		// Floor.
		rectangles[0].q1Start			  = { -3.0f, 0.0f, -3.0f };
		rectangles[0].q2Start			  = { -3.0f, 0.0f, +3.0f };
		rectangles[0].q3Start			  = { +3.0f, 0.0f, -3.0f };
		rectangles[0].q4Start			  = { +3.0f, 0.0f, +3.0f };
		rectangles[0].q1End				  = rectangles[0].q1Start;
		rectangles[0].q2End				  = rectangles[0].q2Start;
		rectangles[0].q3End				  = rectangles[0].q3Start;
		rectangles[0].q4End				  = rectangles[0].q4Start;
		rectangles[0].colorStart		  = wallColor;
		rectangles[0].colorEnd			  = wallColor;
		rectangles[0].materialScalarStart = 0.0f;
		rectangles[0].materialScalarEnd	  = 0.0f;
		rectangles[0].primitiveId		  = 1u;
		rectangles[0].objectId			  = rectangleIndex;
		rectangles[0].materialId		  = 2u;

		rectangleIndex++;

		// Ceiling.
		rectangles[1].q1Start			  = { -3.0f, 6.0f, -3.0f };
		rectangles[1].q2Start			  = { +3.0f, 6.0f, -3.0f };
		rectangles[1].q3Start			  = { -3.0f, 6.0f, +3.0f };
		rectangles[1].q4Start			  = { +3.0f, 6.0f, +3.0f };
		rectangles[1].q1End				  = rectangles[1].q1Start;
		rectangles[1].q2End				  = rectangles[1].q2Start;
		rectangles[1].q3End				  = rectangles[1].q3Start;
		rectangles[1].q4End				  = rectangles[1].q4Start;
		rectangles[1].colorStart		  = wallColor;
		rectangles[1].colorEnd			  = wallColor;
		rectangles[1].materialScalarStart = 0.0f;
		rectangles[1].materialScalarEnd	  = 0.0f;
		rectangles[1].primitiveId		  = 1u;
		rectangles[1].objectId			  = rectangleIndex;
		rectangles[1].materialId		  = 2u;

		rectangleIndex++;

		// Back wall.
		rectangles[2].q1Start			  = { -3.0f, 0.0f, -3.0f };
		rectangles[2].q2Start			  = { +3.0f, 0.0f, -3.0f };
		rectangles[2].q3Start			  = { -3.0f, 6.0f, -3.0f };
		rectangles[2].q4Start			  = { +3.0f, 6.0f, -3.0f };
		rectangles[2].q1End				  = rectangles[2].q1Start;
		rectangles[2].q2End				  = rectangles[2].q2Start;
		rectangles[2].q3End				  = rectangles[2].q3Start;
		rectangles[2].q4End				  = rectangles[2].q4Start;
		rectangles[2].colorStart		  = wallColor;
		rectangles[2].colorEnd			  = wallColor;
		rectangles[2].materialScalarStart = 0.0f;
		rectangles[2].materialScalarEnd	  = 0.0f;
		rectangles[2].primitiveId		  = 1u;
		rectangles[2].objectId			  = rectangleIndex;
		rectangles[2].materialId		  = 2u;

		rectangleIndex++;

		// Left wall (green).
		rectangles[3].q1Start			  = { -3.0f, 0.0f, -3.0f };
		rectangles[3].q2Start			  = { -3.0f, 6.0f, -3.0f };
		rectangles[3].q3Start			  = { -3.0f, 0.0f, +3.0f };
		rectangles[3].q4Start			  = { -3.0f, 6.0f, +3.0f };
		rectangles[3].q1End				  = rectangles[3].q1Start;
		rectangles[3].q2End				  = rectangles[3].q2Start;
		rectangles[3].q3End				  = rectangles[3].q3Start;
		rectangles[3].q4End				  = rectangles[3].q4Start;
		rectangles[3].colorStart		  = greenWallColor;
		rectangles[3].colorEnd			  = greenWallColor;
		rectangles[3].materialScalarStart = 0.0f;
		rectangles[3].materialScalarEnd	  = 0.0f;
		rectangles[3].primitiveId		  = 1u;
		rectangles[3].objectId			  = rectangleIndex;
		rectangles[3].materialId		  = 2u;

		rectangleIndex++;

		// Right wall (red).
		rectangles[4].q1Start			  = { +3.0f, 0.0f, -3.0f };
		rectangles[4].q2Start			  = { +3.0f, 0.0f, +3.0f };
		rectangles[4].q3Start			  = { +3.0f, 6.0f, -3.0f };
		rectangles[4].q4Start			  = { +3.0f, 6.0f, +3.0f };
		rectangles[4].q1End				  = rectangles[4].q1Start;
		rectangles[4].q2End				  = rectangles[4].q2Start;
		rectangles[4].q3End				  = rectangles[4].q3Start;
		rectangles[4].q4End				  = rectangles[4].q4Start;
		rectangles[4].colorStart		  = redWallColor;
		rectangles[4].colorEnd			  = redWallColor;
		rectangles[4].materialScalarStart = 0.0f;
		rectangles[4].materialScalarEnd	  = 0.0f;
		rectangles[4].primitiveId		  = 1u;
		rectangles[4].objectId			  = rectangleIndex;
		rectangles[4].materialId		  = 2u;

		rectangleIndex++;

		// Ceiling light.
		rectangles[5].q1Start			  = { -1.5f, 5.98f, -0.6f };
		rectangles[5].q2Start			  = { +1.5f, 5.98f, -0.6f };
		rectangles[5].q3Start			  = { -1.5f, 5.98f, +0.6f };
		rectangles[5].q4Start			  = { +1.5f, 5.98f, +0.6f };
		rectangles[5].q1End				  = rectangles[5].q1Start;
		rectangles[5].q2End				  = rectangles[5].q2Start;
		rectangles[5].q3End				  = rectangles[5].q3Start;
		rectangles[5].q4End				  = rectangles[5].q4Start;
		rectangles[5].colorStart		  = lightColor;
		rectangles[5].colorEnd			  = lightColor;
		rectangles[5].materialScalarStart = 0.0f;
		rectangles[5].materialScalarEnd	  = 0.0f;
		rectangles[5].primitiveId		  = 1u;
		rectangles[5].objectId			  = rectangleIndex;
		rectangles[5].materialId		  = 5u;

		rectangleIndex++;

		// Tall box: top face.
		rectangles[6].q1Start			  = { boxBLx, boxYMax, boxBLz };
		rectangles[6].q2Start			  = { boxFLx, boxYMax, boxFLz };
		rectangles[6].q3Start			  = { boxBRx, boxYMax, boxBRz };
		rectangles[6].q4Start			  = { boxFRx, boxYMax, boxFRz };
		rectangles[6].q1End				  = rectangles[6].q1Start;
		rectangles[6].q2End				  = rectangles[6].q2Start;
		rectangles[6].q3End				  = rectangles[6].q3Start;
		rectangles[6].q4End				  = rectangles[6].q4Start;
		rectangles[6].colorStart		  = wallColor;
		rectangles[6].colorEnd			  = wallColor;
		rectangles[6].materialScalarStart = 0.0f;
		rectangles[6].materialScalarEnd	  = 0.0f;
		rectangles[6].primitiveId		  = 1u;
		rectangles[6].objectId			  = rectangleIndex;
		rectangles[6].materialId		  = 4u;

		rectangleIndex++;

		// Tall box: front face.
		rectangles[7].q1Start			  = { boxFLx, 0.0f, boxFLz };
		rectangles[7].q2Start			  = { boxFRx, 0.0f, boxFRz };
		rectangles[7].q3Start			  = { boxFLx, boxYMax, boxFLz };
		rectangles[7].q4Start			  = { boxFRx, boxYMax, boxFRz };
		rectangles[7].q1End				  = rectangles[7].q1Start;
		rectangles[7].q2End				  = rectangles[7].q2Start;
		rectangles[7].q3End				  = rectangles[7].q3Start;
		rectangles[7].q4End				  = rectangles[7].q4Start;
		rectangles[7].colorStart		  = wallColor;
		rectangles[7].colorEnd			  = wallColor;
		rectangles[7].materialScalarStart = 0.0f;
		rectangles[7].materialScalarEnd	  = 0.0f;
		rectangles[7].primitiveId		  = 1u;
		rectangles[7].objectId			  = rectangleIndex;
		rectangles[7].materialId		  = 4u;

		rectangleIndex++;

		// Tall box: back face.
		rectangles[8].q1Start			  = { boxBLx, 0.0f, boxBLz };
		rectangles[8].q2Start			  = { boxBLx, boxYMax, boxBLz };
		rectangles[8].q3Start			  = { boxBRx, 0.0f, boxBRz };
		rectangles[8].q4Start			  = { boxBRx, boxYMax, boxBRz };
		rectangles[8].q1End				  = rectangles[8].q1Start;
		rectangles[8].q2End				  = rectangles[8].q2Start;
		rectangles[8].q3End				  = rectangles[8].q3Start;
		rectangles[8].q4End				  = rectangles[8].q4Start;
		rectangles[8].colorStart		  = wallColor;
		rectangles[8].colorEnd			  = wallColor;
		rectangles[8].materialScalarStart = 0.0f;
		rectangles[8].materialScalarEnd	  = 0.0f;
		rectangles[8].primitiveId		  = 1u;
		rectangles[8].objectId			  = rectangleIndex;
		rectangles[8].materialId		  = 4u;

		rectangleIndex++;

		// Tall box: left face.
		rectangles[9].q1Start			  = { boxBLx, 0.0f, boxBLz };
		rectangles[9].q2Start			  = { boxFLx, 0.0f, boxFLz };
		rectangles[9].q3Start			  = { boxBLx, boxYMax, boxBLz };
		rectangles[9].q4Start			  = { boxFLx, boxYMax, boxFLz };
		rectangles[9].q1End				  = rectangles[9].q1Start;
		rectangles[9].q2End				  = rectangles[9].q2Start;
		rectangles[9].q3End				  = rectangles[9].q3Start;
		rectangles[9].q4End				  = rectangles[9].q4Start;
		rectangles[9].colorStart		  = wallColor;
		rectangles[9].colorEnd			  = wallColor;
		rectangles[9].materialScalarStart = 0.0f;
		rectangles[9].materialScalarEnd	  = 0.0f;
		rectangles[9].primitiveId		  = 1u;
		rectangles[9].objectId			  = rectangleIndex;
		rectangles[9].materialId		  = 4u;

		rectangleIndex++;

		// Tall box: right face.
		rectangles[10].q1Start			   = { boxBRx, 0.0f, boxBRz };
		rectangles[10].q2Start			   = { boxBRx, boxYMax, boxBRz };
		rectangles[10].q3Start			   = { boxFRx, 0.0f, boxFRz };
		rectangles[10].q4Start			   = { boxFRx, boxYMax, boxFRz };
		rectangles[10].q1End			   = rectangles[10].q1Start;
		rectangles[10].q2End			   = rectangles[10].q2Start;
		rectangles[10].q3End			   = rectangles[10].q3Start;
		rectangles[10].q4End			   = rectangles[10].q4Start;
		rectangles[10].colorStart		   = wallColor;
		rectangles[10].colorEnd			   = wallColor;
		rectangles[10].materialScalarStart = 0.0f;
		rectangles[10].materialScalarEnd   = 0.0f;
		rectangles[10].primitiveId		   = 1u;
		rectangles[10].objectId			   = rectangleIndex;
		rectangles[10].materialId		   = 4u;

		rectangleIndex++;

		// Triangle procedural primitives. None are needed for the Cornell Box scene; a single
		// out-of-scene placeholder is kept since the GPU-side buffer must be non-zero-sized.
		std::vector<triangle_primitive>& triangles{ cornellBox.triangles };
		triangles.resize(1u);
		unsigned __int32 triangleIndex{ 0u };

		triangles[0].v1Start			 = { 1000.0f, 1000.0f, 1000.0f };
		triangles[0].v1End				 = triangles[0].v1Start;
		triangles[0].v2Start			 = { 1001.0f, 1000.0f, 1000.0f };
		triangles[0].v2End				 = triangles[0].v2Start;
		triangles[0].v3Start			 = { 1000.0f, 1001.0f, 1000.0f };
		triangles[0].v3End				 = triangles[0].v3Start;
		triangles[0].colorStart			 = { 0.0f, 0.0f, 0.0f };
		triangles[0].colorEnd			 = { 0.0f, 0.0f, 0.0f };
		triangles[0].materialScalarStart = 0.0f;
		triangles[0].materialScalarEnd	 = 0.0f;
		triangles[0].primitiveId		 = 2u;
		triangles[0].objectId			 = triangleIndex;
		triangles[0].materialId			 = 2u;

		triangleIndex++;

		return cornellBox;
	}

}
