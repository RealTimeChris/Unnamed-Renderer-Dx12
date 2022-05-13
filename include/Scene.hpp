// Scene.hpp (Header Only)
// Oct 2026
// Chris M.
// https://github.com/RealTimeChris

#pragma once

#include "DirectXStuff.hpp"
#include <vector>

namespace scene_stuff {

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

	// Represents a Procedural Sphere.
	struct sphere {
		float3 wsOriginStart;
		float3 wsOriginEnd;
		float3 wsOrigin;// World-Space origin of the primitive.
		float wsRadiusStart;
		float wsRadiusEnd;
		float wsRadius;// World-Space radius of the primitive.
		float3 colorStart;
		float3 colorEnd;
		float3 color;// Color/Light-Attenuation of the primitive.
		float materialScalarStart;
		float materialScalarEnd;
		float materialScalar;// Used for Metallic Fuzziness or Dielectric Refractive Index.
		uint primitiveId;// Used for identifying the type of primitive.
		uint objectId;// Unique identifier for the given sphere.
		uint materialId;// Material idenfitier for properly selecting intersection functions.
	};

	// Represents a Procedural Rectangle.
	struct rectangle {
		float3 q1Start;
		float3 q1End;
		float3 q1;// First "Corner" of the rectangle as a plane.
		float3 q2Start;
		float3 q2End;
		float3 q2;// Second "Corner" of the rectangle as a plane.
		float3 q3Start;
		float3 q3End;
		float3 q3;
		float3 q4Start;
		float3 q4End;
		float3 q4;
		float3 colorStart;
		float3 colorEnd;
		float3 color;// Color/Light-Attenuation of the primitive.
		float materialScalarStart;
		float materialScalarEnd;
		float materialScalar;// Used for Metallic Fuzziness or Dielectric Refractive Index.
		uint primitiveId;// Used for identifying the type of primitive.
		uint objectId;// Unique identifier for the given rectangle.
		uint materialId;// Material idenfitier for properly selecting intersection functions.
	};

	// Represents a Procedural Triangle primitive.
	struct triangle_primitive {
		float3 v1Start;// First vertex of the triangle.
		float3 v1End;
		float3 v1;
		float3 v2Start;// Second vertex of the triangle.
		float3 v2End;
		float3 v2;
		float3 v3Start;// Third vertex of the triangle.
		float3 v3End;
		float3 v3;
		float3 colorStart;
		float3 colorEnd;
		float3 color;
		float materialScalarStart;
		float materialScalarEnd;
		float materialScalar;
		uint primitiveId;
		uint objectId;
		uint materialId;
	};

	struct scene {
		float3 wsCameraLookFrom;
		float3 wsCameraLookAt;
		float3 wsCameraUp;
		float vFoVInDegrees;
		float aperture;
		float focusDistance;
		float3 skyTopColor;
		float3 skyBottomColor;
		const wchar_t* name;
		std::vector<sphere> spheres;
		std::vector<rectangle> rectangles;
		std::vector<triangle_primitive> triangles;
	};

	URDX12_INLINE rectangle create_placeholder_rectangle(uint objectId) {
		rectangle placeholder{};
		placeholder.q1Start		= { 1000.0f, 1000.0f, 1000.0f };
		placeholder.q2Start		= { 1001.0f, 1000.0f, 1000.0f };
		placeholder.q3Start		= { 1000.0f, 1001.0f, 1000.0f };
		placeholder.q4Start		= { 1001.0f, 1001.0f, 1000.0f };
		placeholder.q1End		= placeholder.q1Start;
		placeholder.q2End		= placeholder.q2Start;
		placeholder.q3End		= placeholder.q3Start;
		placeholder.q4End		= placeholder.q4Start;
		placeholder.primitiveId = 1u;
		placeholder.objectId	= objectId;
		placeholder.materialId	= 2u;
		return placeholder;
	}

	URDX12_INLINE triangle_primitive create_placeholder_triangle(uint objectId) {
		triangle_primitive placeholder{};
		placeholder.v1Start		= { 1000.0f, 1000.0f, 1000.0f };
		placeholder.v1End		= placeholder.v1Start;
		placeholder.v2Start		= { 1001.0f, 1000.0f, 1000.0f };
		placeholder.v2End		= placeholder.v2Start;
		placeholder.v3Start		= { 1000.0f, 1001.0f, 1000.0f };
		placeholder.v3End		= placeholder.v3Start;
		placeholder.primitiveId = 2u;
		placeholder.objectId	= objectId;
		placeholder.materialId	= 2u;
		return placeholder;
	}

	scene build_scene();

}
