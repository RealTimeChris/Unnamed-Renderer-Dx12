// NeonGalleryScene.hpp (Header Only)
// Oct 2026
// Chris M.
// https://github.com/RealTimeChris

#pragma once

#include "Scene.hpp"
#include <cmath>

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

	URDX12_INLINE float3 create_slab_corner(float3 center, float3 u, float3 v, float3 w, float a, float b, float c) {
		return { center.x + a * u.x + b * v.x + c * w.x, center.y + a * u.y + b * v.y + c * w.y, center.z + a * u.z + b * v.z + c * w.z };
	}

	URDX12_INLINE void create_glass_slab(std::vector<rectangle>& rectangles, float3 center, float3 halfExtents, float yawInRadians, float3 color, float refractiveIndex) {
		const float3 u{ halfExtents.x * cosf(yawInRadians), 0.0f, halfExtents.x * sinf(yawInRadians) };
		const float3 v{ 0.0f, halfExtents.y, 0.0f };
		const float3 w{ -halfExtents.z * sinf(yawInRadians), 0.0f, halfExtents.z * cosf(yawInRadians) };

		const float3 faces[6][3]{
			{ create_slab_corner(center, u, v, w, -1.0f, -1.0f, +1.0f), create_slab_corner(center, u, v, w, +1.0f, -1.0f, +1.0f),
				create_slab_corner(center, u, v, w, -1.0f, +1.0f, +1.0f) },
			{ create_slab_corner(center, u, v, w, -1.0f, -1.0f, -1.0f), create_slab_corner(center, u, v, w, -1.0f, +1.0f, -1.0f),
				create_slab_corner(center, u, v, w, +1.0f, -1.0f, -1.0f) },
			{ create_slab_corner(center, u, v, w, +1.0f, -1.0f, -1.0f), create_slab_corner(center, u, v, w, +1.0f, +1.0f, -1.0f),
				create_slab_corner(center, u, v, w, +1.0f, -1.0f, +1.0f) },
			{ create_slab_corner(center, u, v, w, -1.0f, -1.0f, -1.0f), create_slab_corner(center, u, v, w, -1.0f, -1.0f, +1.0f),
				create_slab_corner(center, u, v, w, -1.0f, +1.0f, -1.0f) },
			{ create_slab_corner(center, u, v, w, -1.0f, +1.0f, -1.0f), create_slab_corner(center, u, v, w, -1.0f, +1.0f, +1.0f),
				create_slab_corner(center, u, v, w, +1.0f, +1.0f, -1.0f) },
			{ create_slab_corner(center, u, v, w, -1.0f, -1.0f, -1.0f), create_slab_corner(center, u, v, w, +1.0f, -1.0f, -1.0f),
				create_slab_corner(center, u, v, w, -1.0f, -1.0f, +1.0f) },
		};

		for (const float3(&face)[3]: faces) {
			rectangles.emplace_back(create_static_rectangle(face[0], face[1], face[2], color, refractiveIndex, ( uint )rectangles.size(), 3u));
		}
	}

	URDX12_INLINE scene build_scene() {
		scene neonGallery{};
		neonGallery.wsCameraLookFrom = { 0.0f, 3.0f, 8.5f };
		neonGallery.wsCameraLookAt	 = { 0.0f, 3.4f, 0.0f };
		neonGallery.wsCameraUp		 = { 0.0f, 1.0f, 0.0f };
		neonGallery.vFoVInDegrees	 = 66.0f;
		neonGallery.aperture		 = 0.0f;
		neonGallery.focusDistance	 = 6.5f;
		neonGallery.skyTopColor		 = { 0.26f, 0.30f, 0.62f };
		neonGallery.skyBottomColor	 = { 0.42f, 0.46f, 0.75f };
		neonGallery.name			 = L"NeonGallery";

		const sphere groundSphere{ create_static_sphere({ 0.0f, -1000.0f, 0.0f }, 1000.0f, { 0.50f, 0.50f, 0.65f }, 0.0f, 0u, 2u) };
		const sphere clearGlassSphere{ create_static_sphere({ 0.0f, 2.3f, 2.0f }, 2.3f, { 1.0f, 0.35f, 0.35f }, 1.05f, 1u, 3u) };
		const sphere slateSphere{ create_static_sphere({ -7.6f, 3.4f, -3.4f }, 3.4f, { 0.55f, 0.65f, 0.80f }, 0.0f, 2u, 2u) };
		const sphere tiltedGlassSphere{ create_static_sphere({ -4.4f, 5.4f, 0.5f }, 2.2f, { 1.0f, 1.0f, 1.0f }, 1.5f, 3u, 3u) };
		const sphere backdropSphere{ create_static_sphere({ 0.0f, 4.5f, -14.0f }, 7.0f, { 2.5f, 0.9f, 1.8f }, 0.0f, 4u, 5u) };
		const sphere mirrorDomeSphere{ create_static_sphere({ 0.0f, 10.5f, -9.0f }, 4.5f, { 0.35f, 0.35f, 0.50f }, 0.0f, 5u, 4u) };
		const sphere chromeSphere{ create_static_sphere({ 6.8f, 8.2f, -2.0f }, 3.2f, { 0.75f, 0.78f, 0.90f }, 0.02f, 6u, 4u) };
		const sphere coralSphere{ create_static_sphere({ 7.2f, 6.0f, 1.4f }, 1.4f, { 1.0f, 0.55f, 0.45f }, 0.0f, 7u, 2u) };
		const sphere pinkSphere{ create_static_sphere({ 6.4f, 2.0f, -0.5f }, 1.5f, { 0.95f, 0.25f, 0.55f }, 0.0f, 8u, 2u) };
		const sphere smallPeachGlassSphere{ create_static_sphere({ -0.3f, 0.6f, -5.0f }, 0.6f, { 0.95f, 0.65f, 0.55f }, 1.5f, 9u, 3u) };
		const sphere smallChromeSphere{ create_static_sphere({ 1.4f, 0.9f, -6.0f }, 0.9f, { 0.70f, 0.75f, 0.80f }, 0.05f, 10u, 4u) };
		const sphere tinyMagentaSphere{ create_static_sphere({ 1.0f, 0.3f, -3.5f }, 0.3f, { 0.80f, 0.25f, 0.75f }, 0.0f, 11u, 2u) };
		const sphere cyanCoreSphere{ create_static_sphere({ 0.0f, 2.3f, 2.0f }, 0.8f, { 0.0f, 3.0f, 3.0f }, 0.0f, 12u, 5u) };

		std::vector<sphere>& spheres{ neonGallery.spheres };
		spheres.emplace_back(groundSphere);
		spheres.emplace_back(clearGlassSphere);
		spheres.emplace_back(slateSphere);
		spheres.emplace_back(tiltedGlassSphere);
		spheres.emplace_back(backdropSphere);
		spheres.emplace_back(mirrorDomeSphere);
		spheres.emplace_back(chromeSphere);
		spheres.emplace_back(coralSphere);
		spheres.emplace_back(pinkSphere);
		spheres.emplace_back(smallPeachGlassSphere);
		spheres.emplace_back(smallChromeSphere);
		spheres.emplace_back(tinyMagentaSphere);
		spheres.emplace_back(cyanCoreSphere);

		std::vector<rectangle>& rectangles{ neonGallery.rectangles };
		rectangles.emplace_back(
			create_static_rectangle({ -4.2f, 1.6f, 2.2f }, { -3.0f, 1.6f, 0.6f }, { -4.2f, 3.6f, 2.2f }, { 0.0f, 6.0f, 0.0f }, 0.0f, ( uint )rectangles.size(), 5u));
		rectangles.emplace_back(
			create_static_rectangle({ 3.0f, 1.0f, 0.6f }, { 4.2f, 1.0f, 2.4f }, { 3.0f, 4.2f, 0.6f }, { 0.0f, 6.0f, 6.0f }, 0.0f, ( uint )rectangles.size(), 5u));
		rectangles.emplace_back(
			create_static_rectangle({ 4.2f, 11.0f, -3.0f }, { 6.2f, 11.0f, -3.0f }, { 4.2f, 12.2f, -3.0f }, { 4.0f, 0.0f, 0.0f }, 0.0f, ( uint )rectangles.size(), 5u));
		rectangles.emplace_back(
			create_static_rectangle({ -0.6f, 3.6f, -8.0f }, { 0.6f, 3.6f, -8.0f }, { -0.6f, 4.3f, -8.0f }, { 4.0f, 0.0f, 0.0f }, 0.0f, ( uint )rectangles.size(), 5u));

		rectangles.emplace_back(
			create_static_rectangle({ -6.0f, 7.0f, 11.0f }, { 2.0f, 7.0f, 11.0f }, { -6.0f, 13.0f, 11.0f }, { 3.0f, 0.0f, 0.0f }, 0.0f, ( uint )rectangles.size(), 5u));

		create_glass_slab(rectangles, { -3.0f, 1.3f, 4.2f }, { 0.6f, 1.2f, 0.06f }, 0.52f, { 0.90f, 0.97f, 1.0f }, 1.33f);
		create_glass_slab(rectangles, { 2.8f, 1.3f, 4.4f }, { 0.6f, 1.2f, 0.06f }, -0.44f, { 0.92f, 1.0f, 0.92f }, 1.5f);
		create_glass_slab(rectangles, { 1.6f, 1.0f, 5.6f }, { 0.4f, 0.9f, 0.06f }, 0.79f, { 1.0f, 1.0f, 1.0f }, 2.42f);

		neonGallery.triangles.emplace_back(create_placeholder_triangle(0u));

		return neonGallery;
	}

}
