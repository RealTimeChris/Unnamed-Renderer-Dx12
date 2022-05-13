// RP1ComputeShader.hlsl - Generate the Intersection Maps.
// Apr 2020
// Chris M.
// https://github.com/RealTimeChris

#include "RTCommon.hlsli"


// Calculates the camera's orthonormal basis vectors (Right, Up, Back).
void get_ws_camera_basis(inout float3 u, inout float3 v, inout float3 w) {
	w = normalize(rootConstants.wsCameraLookFrom - rootConstants.wsCameraLookAt);
	u = normalize(cross(rootConstants.wsCameraUp, w));
	v = cross(w, u);
}


// Acquires a random point on the camera's lens aperture, mapped from a square onto a disk via concentric mapping.
void get_random_lens_sample(in uint2 gridThreadId, inout float2 randomLensSample) {
	float2 squareSample = { random_signed_float(), random_signed_float() };

	if (squareSample.x == 0.0f && squareSample.y == 0.0f) {
		randomLensSample = float2(0.0f, 0.0f);
		return;
	}

	float radius;
	float theta;

	if (abs(squareSample.x) > abs(squareSample.y)) {
		radius = squareSample.x;
		theta = (pi / 4.0f) * (squareSample.y / squareSample.x);
	} else {
		radius = squareSample.y;
		theta = (pi / 2.0f) - (pi / 4.0f) * (squareSample.x / squareSample.y);
	}

	randomLensSample = radius * float2(cos(theta), sin(theta));
}


// Offsets the path's origin to a random point on the lens, to produce depth-of-field blurring.
void get_ws_cam_path_origin(in float2 randomLensSample, in float3 u, in float3 v, inout float3 wsCamPathOrigin) {
	float3 wsLensOffset = rootConstants.lensRadius * (randomLensSample.x * u + randomLensSample.y * v);

	wsCamPathOrigin = rootConstants.wsCameraLookFrom + wsLensOffset;
}

// Aims the path from its (possibly lens-offset) origin through the corresponding point on the focus plane.
void get_ws_cam_path_direction(
	in float2 normalizedTsCoords, in float3 wsCamPathOrigin, in float3 u, in float3 v, in float3 w, inout float3 wsCamPathDirection) {
	float3 horizontalExtent = u * rootConstants.wsViewPortDimensions.x * rootConstants.focusDistance;
	float3 verticalExtent = v * rootConstants.wsViewPortDimensions.y * rootConstants.focusDistance;

	float3 lowerLeftCorner =
		rootConstants.wsCameraLookFrom - (horizontalExtent * 0.5f) - (verticalExtent * 0.5f) - (rootConstants.focusDistance * w);

	float3 wsFocusPlanePoint = lowerLeftCorner + (normalizedTsCoords.x * horizontalExtent) + (normalizedTsCoords.y * verticalExtent);

	wsCamPathDirection = normalize(wsFocusPlanePoint - wsCamPathOrigin);
}


[numthreads(128, 8, 1)] void compute_main(uint3 gridThreadId
										 : SV_DispatchThreadID) {
	seed_random(gridThreadId.xy);

	float2 randomPixelOffset;
	get_random_offset_into_pixel(gridThreadId.xy, randomPixelOffset);

	float2 normalizedTsCoords;
	get_normalized_ts_coords(gridThreadId.xy, randomPixelOffset, normalizedTsCoords);

	float3 u, v, w;
	get_ws_camera_basis(u, v, w);

	float2 randomLensSample;
	get_random_lens_sample(gridThreadId.xy, randomLensSample);

	float3 wsCamPathOrigin;
	get_ws_cam_path_origin(randomLensSample, u, v, wsCamPathOrigin);

	float3 wsCamPathDirection;
	get_ws_cam_path_direction(normalizedTsCoords, wsCamPathOrigin, u, v, w, wsCamPathDirection);

	path currentPath;
	currentPath.wsOrigin = wsCamPathOrigin;
	currentPath.wsDirection = wsCamPathDirection;

	for (int currentRecursionDepth = { 0 }; currentRecursionDepth < ( int )rootConstants.maxRecursionDepth; currentRecursionDepth++) {
		intersection_record hitRecord;
		hitRecord.currentRecursionDepth = currentRecursionDepth;
		create_intersection_record(currentPath, hitRecord);

		uint3 intersectionMapIndex = { gridThreadId.x, gridThreadId.y, currentRecursionDepth };

		intersectionMap01[intersectionMapIndex].w = hitRecord.wStDistance;
		intersectionMap01[intersectionMapIndex].xyz = hitRecord.wsIntersectionPoint;

		intersectionMap02[intersectionMapIndex].w = 0.0f;// Unused.
		intersectionMap02[intersectionMapIndex].xyz = hitRecord.wsIncomingPathDirection;

		intersectionMap03[intersectionMapIndex].w = hitRecord.primitiveId;
		intersectionMap03[intersectionMapIndex].x = hitRecord.objectId;
		intersectionMap03[intersectionMapIndex].y = hitRecord.materialId;
		intersectionMap03[intersectionMapIndex].z = hitRecord.currentRecursionDepth;

		// Update the current Path's direction, depending on which kind of material it hit.
		switch (hitRecord.materialId) {
			// Miss/Sky
			case 0:
				update_path_from_sky_intersection(hitRecord, currentPath);

				break;
			// Surface Normal Map
			case 1:
				update_path_from_surface_normal_intersection(hitRecord, currentPath);

				break;
			// Diffuse
			case 2:
				update_path_from_diffuse_intersection(gridThreadId.xy, hitRecord, currentPath);

				break;
			// Dielectric
			case 3:
				update_path_from_dielectric_intersection(gridThreadId.xy, hitRecord, currentPath);

				break;
			// Metallic
			case 4:
				update_path_from_metallic_intersection(gridThreadId.xy, hitRecord, currentPath);

				break;
			// Diffuse Light
			case 5:
				update_path_from_diffuse_light_intersection(gridThreadId.xy, hitRecord, currentPath);

				break;
		}
	}
}
