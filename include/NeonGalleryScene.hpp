// NeonGalleryScene.hpp (Header Only)
// Oct 2026
// Chris M.
// https://github.com/RealTimeChris

#pragma once

#include "Scene.hpp"

namespace SceneStuff {

	Sphere CreateStaticSphere(float3 WSOrigin, float WSRadius, float3 Color, float MaterialScalar, uint ObjectId, uint MaterialId) {
		Sphere NewSphere{};
		NewSphere.WSOriginStart = WSOrigin;
		NewSphere.WSOriginEnd = WSOrigin;
		NewSphere.WSRadiusStart = WSRadius;
		NewSphere.WSRadiusEnd = WSRadius;
		NewSphere.ColorStart = Color;
		NewSphere.ColorEnd = Color;
		NewSphere.MaterialScalarStart = MaterialScalar;
		NewSphere.MaterialScalarEnd = MaterialScalar;
		NewSphere.PrimitiveId = 0u;
		NewSphere.ObjectId = ObjectId;
		NewSphere.MaterialId = MaterialId;
		return NewSphere;
	}

	Rectangle CreateStaticRectangle(float3 Q1, float3 Q2, float3 Q3, float3 Color, float MaterialScalar, uint ObjectId, uint MaterialId) {
		Rectangle NewRectangle{};
		NewRectangle.Q1Start = Q1;
		NewRectangle.Q2Start = Q2;
		NewRectangle.Q3Start = Q3;
		NewRectangle.Q4Start = { Q2.x + Q3.x - Q1.x, Q2.y + Q3.y - Q1.y, Q2.z + Q3.z - Q1.z };
		NewRectangle.Q1End = NewRectangle.Q1Start;
		NewRectangle.Q2End = NewRectangle.Q2Start;
		NewRectangle.Q3End = NewRectangle.Q3Start;
		NewRectangle.Q4End = NewRectangle.Q4Start;
		NewRectangle.ColorStart = Color;
		NewRectangle.ColorEnd = Color;
		NewRectangle.MaterialScalarStart = MaterialScalar;
		NewRectangle.MaterialScalarEnd = MaterialScalar;
		NewRectangle.PrimitiveId = 1u;
		NewRectangle.ObjectId = ObjectId;
		NewRectangle.MaterialId = MaterialId;
		return NewRectangle;
	}

	Triangle CreateStaticTriangle(float3 V1, float3 V2, float3 V3, float3 Color, float MaterialScalar, uint ObjectId, uint MaterialId) {
		Triangle NewTriangle{};
		NewTriangle.V1Start = V1;
		NewTriangle.V2Start = V2;
		NewTriangle.V3Start = V3;
		NewTriangle.V1End = V1;
		NewTriangle.V2End = V2;
		NewTriangle.V3End = V3;
		NewTriangle.ColorStart = Color;
		NewTriangle.ColorEnd = Color;
		NewTriangle.MaterialScalarStart = MaterialScalar;
		NewTriangle.MaterialScalarEnd = MaterialScalar;
		NewTriangle.PrimitiveId = 2u;
		NewTriangle.ObjectId = ObjectId;
		NewTriangle.MaterialId = MaterialId;
		return NewTriangle;
	}

	Scene BuildScene() {
		Scene NeonGallery{};
		NeonGallery.WSCameraLookFrom = { 0.0f, 3.0f, 8.5f };
		NeonGallery.WSCameraLookAt = { 0.0f, 3.4f, 0.0f };
		NeonGallery.WSCameraUp = { 0.0f, 1.0f, 0.0f };
		NeonGallery.VFoVInDegrees = 66.0f;
		NeonGallery.Aperture = 0.0f;
		NeonGallery.FocusDistance = 6.5f;
		NeonGallery.SkyTopColor = { 0.26f, 0.30f, 0.62f };
		NeonGallery.SkyBottomColor = { 0.42f, 0.46f, 0.75f };
		NeonGallery.Name = L"NeonGallery";

		const Sphere GroundSphere{ CreateStaticSphere({ 0.0f, -1000.0f, 0.0f }, 1000.0f, { 0.50f, 0.50f, 0.65f }, 0.0f, 0u, 2u) };
		const Sphere ClearGlassSphere{ CreateStaticSphere({ 0.0f, 2.3f, 2.0f }, 2.3f, { 1.0f, 0.5f, 0.3f }, 0.0f, 1u, 4u) };
		const Sphere SlateSphere{ CreateStaticSphere({ -7.6f, 3.4f, -3.4f }, 3.4f, { 0.55f, 0.65f, 0.80f }, 0.0f, 2u, 2u) };
		const Sphere TiltedGlassSphere{ CreateStaticSphere({ -4.4f, 5.4f, 0.5f }, 2.2f, { 0.80f, 0.85f, 1.0f }, 1.5f, 3u, 3u) };
		const Sphere BackdropSphere{ CreateStaticSphere({ 0.0f, 4.5f, -14.0f }, 7.0f, { 0.40f, 0.42f, 0.62f }, 0.0f, 4u, 2u) };
		const Sphere MirrorDomeSphere{ CreateStaticSphere({ 0.0f, 10.5f, -9.0f }, 4.5f, { 0.35f, 0.35f, 0.50f }, 0.0f, 5u, 4u) };
		const Sphere ChromeSphere{ CreateStaticSphere({ 6.8f, 8.2f, -2.0f }, 3.2f, { 0.75f, 0.78f, 0.90f }, 0.02f, 6u, 4u) };
		const Sphere CoralSphere{ CreateStaticSphere({ 7.2f, 6.0f, 1.4f }, 1.4f, { 1.0f, 0.55f, 0.45f }, 0.0f, 7u, 2u) };
		const Sphere PinkSphere{ CreateStaticSphere({ 6.4f, 2.0f, -0.5f }, 1.5f, { 0.95f, 0.25f, 0.55f }, 0.0f, 8u, 2u) };
		const Sphere SmallPeachGlassSphere{ CreateStaticSphere({ -0.3f, 0.6f, -5.0f }, 0.6f, { 0.95f, 0.65f, 0.55f }, 1.5f, 9u, 3u) };
		const Sphere SmallChromeSphere{ CreateStaticSphere({ 1.4f, 0.9f, -6.0f }, 0.9f, { 0.70f, 0.75f, 0.80f }, 0.05f, 10u, 4u) };
		const Sphere TinyMagentaSphere{ CreateStaticSphere({ 1.0f, 0.3f, -3.5f }, 0.3f, { 0.80f, 0.25f, 0.75f }, 0.0f, 11u, 2u) };

		std::vector<Sphere>& Spheres{ NeonGallery.Spheres };
		Spheres.emplace_back(GroundSphere);
		Spheres.emplace_back(ClearGlassSphere);
		Spheres.emplace_back(SlateSphere);
		Spheres.emplace_back(TiltedGlassSphere);
		Spheres.emplace_back(BackdropSphere);
		Spheres.emplace_back(MirrorDomeSphere);
		Spheres.emplace_back(ChromeSphere);
		Spheres.emplace_back(CoralSphere);
		Spheres.emplace_back(PinkSphere);
		Spheres.emplace_back(SmallPeachGlassSphere);
		Spheres.emplace_back(SmallChromeSphere);
		Spheres.emplace_back(TinyMagentaSphere);

		std::vector<Rectangle>& Rectangles{ NeonGallery.Rectangles };
		Rectangles.emplace_back(CreateStaticRectangle({ -4.2f, 1.6f, 2.2f }, { -3.0f, 1.6f, 0.6f }, { -4.2f, 3.6f, 2.2f }, { 0.0f, 6.0f, 0.0f }, 0.0f, ( uint )Rectangles.size(), 5u));
		Rectangles.emplace_back(CreateStaticRectangle({ 3.0f, 1.0f, 0.6f }, { 4.2f, 1.0f, 2.4f }, { 3.0f, 4.2f, 0.6f }, { 0.0f, 6.0f, 6.0f }, 0.0f, ( uint )Rectangles.size(), 5u));
		Rectangles.emplace_back(CreateStaticRectangle({ 4.2f, 11.0f, -3.0f }, { 6.2f, 11.0f, -3.0f }, { 4.2f, 12.2f, -3.0f }, { 4.0f, 0.0f, 0.0f }, 0.0f, ( uint )Rectangles.size(), 5u));
		Rectangles.emplace_back(CreateStaticRectangle({ -0.6f, 3.6f, -8.0f }, { 0.6f, 3.6f, -8.0f }, { -0.6f, 4.3f, -8.0f }, { 4.0f, 0.0f, 0.0f }, 0.0f, ( uint )Rectangles.size(), 5u));

		Rectangles.emplace_back(CreateStaticRectangle({ -6.0f, 7.0f, 11.0f }, { 2.0f, 7.0f, 11.0f }, { -6.0f, 13.0f, 11.0f }, { 3.0f, 0.0f, 0.0f }, 0.0f, ( uint )Rectangles.size(), 5u));

		std::vector<Triangle>& Triangles{ NeonGallery.Triangles };
		Triangles.emplace_back(CreateStaticTriangle({ -1.8f, 0.0f, 6.4f }, { 0.4f, 0.0f, 6.8f }, { 1.4f, 1.8f, 5.0f }, { 6.0f, 6.0f, 0.0f }, 0.0f, ( uint )Triangles.size(), 5u));
		Triangles.emplace_back(CreateStaticTriangle({ -3.4f, 0.0f, -3.0f }, { -2.6f, 0.0f, -3.3f }, { -3.2f, 6.5f, -3.6f }, { 3.0f, 3.0f, 3.0f }, 0.0f, ( uint )Triangles.size(), 5u));
		Triangles.emplace_back(CreateStaticTriangle({ -1.0f, 0.0f, -3.0f }, { 0.4f, 0.0f, -3.4f }, { -0.6f, 6.8f, -3.8f }, { 0.85f, 0.80f, 0.45f }, 0.0f, ( uint )Triangles.size(), 2u));
		Triangles.emplace_back(CreateStaticTriangle({ -6.0f, 0.0f, -4.5f }, { -2.4f, 0.0f, -5.5f }, { -2.4f, 12.0f, -6.0f }, { 0.20f, 0.22f, 0.15f }, 0.0f, ( uint )Triangles.size(), 2u));

		return NeonGallery;
	}

}
