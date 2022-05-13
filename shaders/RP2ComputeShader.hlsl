// RP2ComputeShader.hlsl - Generate the Accumulation Frame.
// Apr 2020
// Chris M.
// https://github.com/RealTimeChris

#include "RTCommon.hlsli"


[numthreads(128, 8, 1)] void compute_main(uint3 gridThreadId
										 : SV_DispatchThreadID) {
	path_payload currentPayload;

	for (int currentRecursionDepth = { ( int )rootConstants.maxRecursionDepth - 1 }; currentRecursionDepth >= 0; currentRecursionDepth--) {
		uint3 intersectionMapIndex = { gridThreadId.x, gridThreadId.y, currentRecursionDepth };

		intersection_record hitRecord;
		hitRecord.wStDistance = intersectionMap01[intersectionMapIndex].w;
		hitRecord.wsIntersectionPoint = intersectionMap01[intersectionMapIndex].xyz;
		hitRecord.wsIncomingPathDirection = intersectionMap02[intersectionMapIndex].xyz;
		hitRecord.primitiveId = intersectionMap03[intersectionMapIndex].w;
		hitRecord.objectId = intersectionMap03[intersectionMapIndex].x;
		hitRecord.materialId = intersectionMap03[intersectionMapIndex].y;
		hitRecord.currentRecursionDepth = intersectionMap03[intersectionMapIndex].z;

		// Update the current Path's Payload.
		switch (hitRecord.materialId) {
			// Miss/Sky
			case 0:
				update_payload_from_sky_intersection(hitRecord, currentPayload);

				break;
			// Surface Normal Map
			case 1:
				update_payload_from_surface_normal_intersection(hitRecord, currentPayload);

				break;
			// Diffuse
			case 2:
				update_payload_from_diffuse_intersection(hitRecord, currentPayload);

				break;
			// Dielectric
			case 3:
				update_payload_from_dielectric_intersection(hitRecord, currentPayload);

				break;
			// Metallic
			case 4:
				update_payload_from_metallic_intersection(hitRecord, currentPayload);

				break;
			// Diffuse Light
			case 5:
				update_payload_from_diffuse_light_intersection(hitRecord, currentPayload);

				break;
		}
	}

	accumulationFrame[gridThreadId.xy].x += currentPayload.r / (1.0f + currentPayload.r);
	accumulationFrame[gridThreadId.xy].y += currentPayload.g / (1.0f + currentPayload.g);
	accumulationFrame[gridThreadId.xy].z += currentPayload.b / (1.0f + currentPayload.b);
}
