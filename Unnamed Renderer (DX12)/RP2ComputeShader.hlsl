// RP2ComputeShader.hlsl - Generate the Accumulation Frame.
// Apr 2020
// Chris M.
// https://github.com/RealTimeChris

#include "RTCommon.hlsli"


[numthreads(128, 8, 1)] void ComputeMain(uint3 GridThreadId
										 : SV_DispatchThreadID) {
	PathPayload CurrentPayload;

	for (int CurrentRecursionDepth = { ( int )RootConstants.MaxRecursionDepth - 1 }; CurrentRecursionDepth >= 0; CurrentRecursionDepth--) {
		uint3 IntersectionMapIndex = { GridThreadId.x, GridThreadId.y, CurrentRecursionDepth };

		IntersectionRecord HitRecord;
		HitRecord.WStDistance = IntersectionMap01[IntersectionMapIndex].w;
		HitRecord.WSIntersectionPoint = IntersectionMap01[IntersectionMapIndex].xyz;
		HitRecord.WSIncomingPathDirection = IntersectionMap02[IntersectionMapIndex].xyz;
		HitRecord.PrimitiveId = IntersectionMap03[IntersectionMapIndex].w;
		HitRecord.ObjectId = IntersectionMap03[IntersectionMapIndex].x;
		HitRecord.MaterialId = IntersectionMap03[IntersectionMapIndex].y;
		HitRecord.CurrentRecursionDepth = IntersectionMap03[IntersectionMapIndex].z;

		// Update the current Path's Payload.
		switch (HitRecord.MaterialId) {
			// Miss/Sky
			case 0:
				UpdatePayloadFromSkyIntersection(HitRecord, CurrentPayload);

				break;
			// Surface Normal Map
			case 1:
				UpdatePayloadFromSurfaceNormalIntersection(HitRecord, CurrentPayload);

				break;
			// Diffuse
			case 2:
				UpdatePayloadFromDiffuseIntersection(HitRecord, CurrentPayload);

				break;
			// Dielectric
			case 3:
				UpdatePayloadFromDielectricIntersection(HitRecord, CurrentPayload);

				break;
			// Metallic
			case 4:
				UpdatePayloadFromMetallicIntersection(HitRecord, CurrentPayload);

				break;
			// Diffuse Light
			case 5:
				UpdatePayloadFromDiffuseLightIntersection(HitRecord, CurrentPayload);

				break;
		}
	}

	AccumulationFrame[GridThreadId.xy].x += CurrentPayload.r;
	AccumulationFrame[GridThreadId.xy].y += CurrentPayload.g;
	AccumulationFrame[GridThreadId.xy].z += CurrentPayload.b;
}
