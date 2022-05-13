// RTIOWScene.hpp (Header Only)
// Oct 2026
// Chris M.
// https://github.com/RealTimeChris

#pragma once

#include "Scene.hpp"
#include <algorithm>
#include <random>

namespace scene_stuff {

	URDX12_INLINE sphere create_static_sphere(float3 wsOrigin, float wsRadius, float3 color, float materialScalar, uint objectId, uint materialId) {
		sphere newSphere{};
		newSphere.wsOriginStart		  = wsOrigin;
		newSphere.wsOriginEnd		  = wsOrigin;
		newSphere.wsRadiusStart		  = wsRadius;
		newSphere.wsRadiusEnd		  = wsRadius;
		newSphere.colorStart		  = color;
		newSphere.colorEnd			  = color;
		newSphere.materialScalarStart = materialScalar;
		newSphere.materialScalarEnd	  = materialScalar;
		newSphere.primitiveId		  = 0u;
		newSphere.objectId			  = objectId;
		newSphere.materialId		  = materialId;
		return newSphere;
	}

	URDX12_INLINE rectangle create_static_rectangle(float3 q1, float3 q2, float3 q3, float3 color, float materialScalar, uint objectId, uint materialId) {
		rectangle newRectangle{};
		newRectangle.q1Start			 = q1;
		newRectangle.q2Start			 = q2;
		newRectangle.q3Start			 = q3;
		newRectangle.q4Start			 = { q2.x + q3.x - q1.x, q2.y + q3.y - q1.y, q2.z + q3.z - q1.z };
		newRectangle.q1End				 = newRectangle.q1Start;
		newRectangle.q2End				 = newRectangle.q2Start;
		newRectangle.q3End				 = newRectangle.q3Start;
		newRectangle.q4End				 = newRectangle.q4Start;
		newRectangle.colorStart			 = color;
		newRectangle.colorEnd			 = color;
		newRectangle.materialScalarStart = materialScalar;
		newRectangle.materialScalarEnd	 = materialScalar;
		newRectangle.primitiveId		 = 1u;
		newRectangle.objectId			 = objectId;
		newRectangle.materialId			 = materialId;
		return newRectangle;
	}

	URDX12_INLINE scene build_scene() {
		scene rtiow{};
		rtiow.wsCameraLookFrom = { 13.0f, 2.0f, 3.0f };
		rtiow.wsCameraLookAt   = { 0.0f, 0.0f, 0.0f };
		rtiow.wsCameraUp	   = { 0.0f, 1.0f, 0.0f };
		rtiow.vFoVInDegrees	   = 20.0f;
		rtiow.aperture		   = 0.1f;
		rtiow.focusDistance	   = 10.0f;
		rtiow.skyTopColor	   = { 0.26f, 0.30f, 0.62f };
		rtiow.skyBottomColor   = { 0.42f, 0.46f, 0.75f };
		rtiow.name			   = L"RTIOW";

		std::vector<sphere>& spheres{ rtiow.spheres };
		std::mt19937 randomEngine{ 1337u };
		std::uniform_real_distribution<float> unitDistribution{ 0.0f, 1.0f };
		auto random = [&](float minimum = 0.0f, float maximum = 1.0f) {
			return minimum + (maximum - minimum) * unitDistribution(randomEngine);
		};

		spheres.emplace_back(create_static_sphere({ 0.0f, -1000.0f, 0.0f }, 1000.0f, { 0.5f, 0.5f, 0.5f }, 0.0f, ( uint )spheres.size(), 2u));

		for (__int32 a{ -11 }; a < 11; a++) {
			for (__int32 b{ -11 }; b < 11; b++) {
				float materialChoice{ random() };
				float3 wsCenter{ ( float )a + 0.9f * random(), 0.2f, ( float )b + 0.9f * random() };

				float offsetX{ wsCenter.x - 4.0f };
				float offsetY{ wsCenter.y - 0.2f };
				float offsetZ{ wsCenter.z };

				if (sqrtf(offsetX * offsetX + offsetY * offsetY + offsetZ * offsetZ) <= 0.9f) {
					continue;
				}

				if (materialChoice < 0.8f) {
					float3 albedo{ random() * random(), random() * random(), random() * random() };
					if (materialChoice < 0.08f) {
						float emissionScale{ 4.0f / std::max({ albedo.x, albedo.y, albedo.z, 0.05f }) };
						float3 emission{ albedo.x * emissionScale, albedo.y * emissionScale, albedo.z * emissionScale };
						spheres.emplace_back(create_static_sphere(wsCenter, 0.2f, emission, 0.0f, ( uint )spheres.size(), 5u));
					} else {
						spheres.emplace_back(create_static_sphere(wsCenter, 0.2f, albedo, 0.0f, ( uint )spheres.size(), 2u));
					}
				} else if (materialChoice < 0.95f) {
					float3 albedo{ random(0.5f, 1.0f), random(0.5f, 1.0f), random(0.5f, 1.0f) };
					spheres.emplace_back(create_static_sphere(wsCenter, 0.2f, albedo, random(0.0f, 0.5f), ( uint )spheres.size(), 4u));
				} else {
					spheres.emplace_back(create_static_sphere(wsCenter, 0.2f, { 1.0f, 1.0f, 1.0f }, 1.5f, ( uint )spheres.size(), 3u));
				}
			}
		}

		spheres.emplace_back(create_static_sphere({ 0.0f, 1.0f, 0.0f }, 1.0f, { 1.0f, 1.0f, 1.0f }, 1.5f, ( uint )spheres.size(), 3u));
		spheres.emplace_back(create_static_sphere({ -4.0f, 1.0f, 0.0f }, 1.0f, { 0.4f, 0.2f, 0.1f }, 0.0f, ( uint )spheres.size(), 2u));
		spheres.emplace_back(create_static_sphere({ 4.0f, 1.0f, 0.0f }, 1.0f, { 0.7f, 0.6f, 0.5f }, 0.0f, ( uint )spheres.size(), 4u));

		std::vector<rectangle>& rectangles{ rtiow.rectangles };
		rectangles.emplace_back(
			create_static_rectangle({ -6.0f, 0.0f, -3.0f }, { -6.0f, 0.0f, -1.5f }, { -6.0f, 1.5f, -3.0f }, { 4.0f, 0.0f, 0.0f }, 0.0f, ( uint )rectangles.size(), 5u));
		rectangles.emplace_back(
			create_static_rectangle({ -6.0f, 0.0f, 1.5f }, { -6.0f, 0.0f, 3.0f }, { -6.0f, 1.5f, 1.5f }, { 0.0f, 4.0f, 4.0f }, 0.0f, ( uint )rectangles.size(), 5u));
		rectangles.emplace_back(
			create_static_rectangle({ -2.5f, 0.0f, 2.0f }, { -2.5f, 0.0f, 2.8f }, { -2.5f, 0.8f, 2.0f }, { 0.0f, 4.0f, 0.0f }, 0.0f, ( uint )rectangles.size(), 5u));
		rectangles.emplace_back(
			create_static_rectangle({ 2.0f, 0.0f, -2.6f }, { 2.0f, 0.0f, -1.8f }, { 2.0f, 0.8f, -2.6f }, { 4.0f, 0.0f, 4.0f }, 0.0f, ( uint )rectangles.size(), 5u));
		rectangles.emplace_back(
			create_static_rectangle({ 6.0f, 0.0f, 1.0f }, { 6.0f, 0.0f, 1.6f }, { 6.0f, 0.6f, 1.0f }, { 4.0f, 4.0f, 0.0f }, 0.0f, ( uint )rectangles.size(), 5u));
		rtiow.triangles.emplace_back(create_placeholder_triangle(0u));

		return rtiow;
	}

}
