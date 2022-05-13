// Scene.hpp (Header Only)
// Oct 2026
// Chris M.
// https://github.com/RealTimeChris

#pragma once

#include "DirectXStuff.hpp"
#include <vector>

namespace SceneStuff {

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

	struct Scene {
		float3 WSCameraLookFrom;
		float3 WSCameraLookAt;
		float3 WSCameraUp;
		float VFoVInDegrees;
		float Aperture;
		float FocusDistance;
		float3 SkyTopColor;
		float3 SkyBottomColor;
		const wchar_t* Name;
		std::vector<Sphere> Spheres;
		std::vector<Rectangle> Rectangles;
		std::vector<Triangle> Triangles;
	};

	Rectangle CreatePlaceholderRectangle(uint ObjectId) {
		Rectangle Placeholder{};
		Placeholder.Q1Start = { 1000.0f, 1000.0f, 1000.0f };
		Placeholder.Q2Start = { 1001.0f, 1000.0f, 1000.0f };
		Placeholder.Q3Start = { 1000.0f, 1001.0f, 1000.0f };
		Placeholder.Q4Start = { 1001.0f, 1001.0f, 1000.0f };
		Placeholder.Q1End = Placeholder.Q1Start;
		Placeholder.Q2End = Placeholder.Q2Start;
		Placeholder.Q3End = Placeholder.Q3Start;
		Placeholder.Q4End = Placeholder.Q4Start;
		Placeholder.PrimitiveId = 1u;
		Placeholder.ObjectId = ObjectId;
		Placeholder.MaterialId = 2u;
		return Placeholder;
	}

	Triangle CreatePlaceholderTriangle(uint ObjectId) {
		Triangle Placeholder{};
		Placeholder.V1Start = { 1000.0f, 1000.0f, 1000.0f };
		Placeholder.V1End = Placeholder.V1Start;
		Placeholder.V2Start = { 1001.0f, 1000.0f, 1000.0f };
		Placeholder.V2End = Placeholder.V2Start;
		Placeholder.V3Start = { 1000.0f, 1001.0f, 1000.0f };
		Placeholder.V3End = Placeholder.V3Start;
		Placeholder.PrimitiveId = 2u;
		Placeholder.ObjectId = ObjectId;
		Placeholder.MaterialId = 2u;
		return Placeholder;
	}

	Scene BuildScene();

}
