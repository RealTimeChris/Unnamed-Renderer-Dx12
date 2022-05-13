// RTIOWScene.hpp (Header Only)
// Oct 2026
// Chris M.
// https://github.com/RealTimeChris

#pragma once

#include "Scene.hpp"
#include <random>

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

	Scene BuildScene() {
		Scene RTIOW{};
		RTIOW.WSCameraLookFrom = { 13.0f, 2.0f, 3.0f };
		RTIOW.WSCameraLookAt = { 0.0f, 0.0f, 0.0f };
		RTIOW.WSCameraUp = { 0.0f, 1.0f, 0.0f };
		RTIOW.VFoVInDegrees = 20.0f;
		RTIOW.Aperture = 0.1f;
		RTIOW.FocusDistance = 10.0f;
		RTIOW.SkyTopColor = { 0.5f, 0.7f, 1.0f };
		RTIOW.SkyBottomColor = { 1.0f, 1.0f, 1.0f };
		RTIOW.Name = L"RTIOW";

		std::vector<Sphere>& Spheres{ RTIOW.Spheres };
		std::mt19937 RandomEngine{ 1337u };
		std::uniform_real_distribution<float> UnitDistribution{ 0.0f, 1.0f };
		auto Random = [&](float Minimum = 0.0f, float Maximum = 1.0f) {
			return Minimum + (Maximum - Minimum) * UnitDistribution(RandomEngine);
		};

		Spheres.push_back(CreateStaticSphere({ 0.0f, -1000.0f, 0.0f }, 1000.0f, { 0.5f, 0.5f, 0.5f }, 0.0f, ( uint )Spheres.size(), 2u));

		for (__int32 a{ -11 }; a < 11; a++) {
			for (__int32 b{ -11 }; b < 11; b++) {
				float MaterialChoice{ Random() };
				float3 WSCenter{ ( float )a + 0.9f * Random(), 0.2f, ( float )b + 0.9f * Random() };

				float OffsetX{ WSCenter.x - 4.0f };
				float OffsetY{ WSCenter.y - 0.2f };
				float OffsetZ{ WSCenter.z };

				if (sqrtf(OffsetX * OffsetX + OffsetY * OffsetY + OffsetZ * OffsetZ) <= 0.9f) {
					continue;
				}

				if (MaterialChoice < 0.8f) {
					float3 Albedo{ Random() * Random(), Random() * Random(), Random() * Random() };
					Spheres.push_back(CreateStaticSphere(WSCenter, 0.2f, Albedo, 0.0f, ( uint )Spheres.size(), 2u));
				} else if (MaterialChoice < 0.95f) {
					float3 Albedo{ Random(0.5f, 1.0f), Random(0.5f, 1.0f), Random(0.5f, 1.0f) };
					Spheres.push_back(CreateStaticSphere(WSCenter, 0.2f, Albedo, Random(0.0f, 0.5f), ( uint )Spheres.size(), 4u));
				} else {
					Spheres.push_back(CreateStaticSphere(WSCenter, 0.2f, { 1.0f, 1.0f, 1.0f }, 1.5f, ( uint )Spheres.size(), 3u));
				}
			}
		}

		Spheres.push_back(CreateStaticSphere({ 0.0f, 1.0f, 0.0f }, 1.0f, { 1.0f, 1.0f, 1.0f }, 1.5f, ( uint )Spheres.size(), 3u));
		Spheres.push_back(CreateStaticSphere({ -4.0f, 1.0f, 0.0f }, 1.0f, { 0.4f, 0.2f, 0.1f }, 0.0f, ( uint )Spheres.size(), 2u));
		Spheres.push_back(CreateStaticSphere({ 4.0f, 1.0f, 0.0f }, 1.0f, { 0.7f, 0.6f, 0.5f }, 0.0f, ( uint )Spheres.size(), 4u));

		RTIOW.Rectangles.push_back(CreatePlaceholderRectangle(0u));
		RTIOW.Triangles.push_back(CreatePlaceholderTriangle(0u));

		return RTIOW;
	}

}
