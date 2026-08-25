// RP1ComputeShader.hlsl - Generate the Intersection Maps.
// Apr 2020
// Chris M.
// https://github.com/RealTimeChris

#include "RTCommon.hlsli"


// Calculates the camera's orthonormal basis vectors (Right, Up, Back).
void GetWSCameraBasis(inout float3 U, inout float3 V, inout float3 W) {
	W = normalize(RootConstants.WSCameraLookFrom - RootConstants.WSCameraLookAt);
	U = normalize(cross(RootConstants.WSCameraUp, W));
	V = cross(W, U);
}


// Acquires a random point on the camera's lens aperture, mapped from a square onto a disk via concentric mapping.
void GetRandomLensSample(in uint2 GridThreadId, inout float2 RandomLensSample) {
	uint3 ChaosTexelsIndex03 = { GridThreadId.x, GridThreadId.y, 3 };
	uint3 ChaosTexelsIndex04 = { GridThreadId.x, GridThreadId.y, 4 };

	float2 SquareSample = { ChaosTexels[ChaosTexelsIndex03], ChaosTexels[ChaosTexelsIndex04] };

	if (SquareSample.x == 0.0f && SquareSample.y == 0.0f) {
		RandomLensSample = float2(0.0f, 0.0f);
		return;
	}

	float Radius;
	float Theta;

	if (abs(SquareSample.x) > abs(SquareSample.y)) {
		Radius = SquareSample.x;
		Theta = (PI / 4.0f) * (SquareSample.y / SquareSample.x);
	} else {
		Radius = SquareSample.y;
		Theta = (PI / 2.0f) - (PI / 4.0f) * (SquareSample.x / SquareSample.y);
	}

	RandomLensSample = Radius * float2(cos(Theta), sin(Theta));
}


// Offsets the path's origin to a random point on the lens, to produce depth-of-field blurring.
void GetWSCamPathOrigin(in float2 RandomLensSample, in float3 U, in float3 V, inout float3 WSCamPathOrigin) {
	float3 WSLensOffset = RootConstants.LensRadius * (RandomLensSample.x * U + RandomLensSample.y * V);

	WSCamPathOrigin = RootConstants.WSCameraLookFrom + WSLensOffset;
}

// Aims the path from its (possibly lens-offset) origin through the corresponding point on the focus plane.
void GetWSCamPathDirection(
	in float2 NormalizedTSCoords, in float3 WSCamPathOrigin, in float3 U, in float3 V, in float3 W, inout float3 WSCamPathDirection) {
	float3 HorizontalExtent = U * RootConstants.WSViewPortDimensions.x * RootConstants.FocusDistance;
	float3 VerticalExtent = V * RootConstants.WSViewPortDimensions.y * RootConstants.FocusDistance;

	float3 LowerLeftCorner =
		RootConstants.WSCameraLookFrom - (HorizontalExtent * 0.5f) - (VerticalExtent * 0.5f) - (RootConstants.FocusDistance * W);

	float3 WSFocusPlanePoint = LowerLeftCorner + (NormalizedTSCoords.x * HorizontalExtent) + (NormalizedTSCoords.y * VerticalExtent);

	WSCamPathDirection = normalize(WSFocusPlanePoint - WSCamPathOrigin);
}


[numthreads(128, 8, 1)] void ComputeMain(uint3 GridThreadId
										 : SV_DispatchThreadID) {
	float2 RandomPixelOffset;
	GetRandomOffsetIntoPixel(GridThreadId.xy, RandomPixelOffset);

	float2 NormalizedTSCoords;
	GetNormalizedTSCoords(GridThreadId.xy, RandomPixelOffset, NormalizedTSCoords);

	float3 U, V, W;
	GetWSCameraBasis(U, V, W);

	float2 RandomLensSample;
	GetRandomLensSample(GridThreadId.xy, RandomLensSample);

	float3 WSCamPathOrigin;
	GetWSCamPathOrigin(RandomLensSample, U, V, WSCamPathOrigin);

	float3 WSCamPathDirection;
	GetWSCamPathDirection(NormalizedTSCoords, WSCamPathOrigin, U, V, W, WSCamPathDirection);

	Path CurrentPath;
	CurrentPath.WSOrigin = WSCamPathOrigin;
	CurrentPath.WSDirection = WSCamPathDirection;

	for (int CurrentRecursionDepth = { 0 }; CurrentRecursionDepth < ( int )RootConstants.MaxRecursionDepth; CurrentRecursionDepth++) {
		IntersectionRecord HitRecord;
		HitRecord.CurrentRecursionDepth = CurrentRecursionDepth;
		CreateIntersectionRecord(CurrentPath, HitRecord);

		uint3 IntersectionMapIndex = { GridThreadId.x, GridThreadId.y, CurrentRecursionDepth };

		IntersectionMap01[IntersectionMapIndex].w = HitRecord.WStDistance;
		IntersectionMap01[IntersectionMapIndex].xyz = HitRecord.WSIntersectionPoint;

		IntersectionMap02[IntersectionMapIndex].w = 0.0f;// Unused.
		IntersectionMap02[IntersectionMapIndex].xyz = HitRecord.WSIncomingPathDirection;

		IntersectionMap03[IntersectionMapIndex].w = HitRecord.PrimitiveId;
		IntersectionMap03[IntersectionMapIndex].x = HitRecord.ObjectId;
		IntersectionMap03[IntersectionMapIndex].y = HitRecord.MaterialId;
		IntersectionMap03[IntersectionMapIndex].z = HitRecord.CurrentRecursionDepth;

		// Update the current Path's direction, depending on which kind of material it hit.
		switch (HitRecord.MaterialId) {
			// Miss/Sky
			case 0:
				UpdatePathFromSkyIntersection(HitRecord, CurrentPath);

				break;
			// Surface Normal Map
			case 1:
				UpdatePathFromSurfaceNormalIntersection(HitRecord, CurrentPath);

				break;
			// Diffuse
			case 2:
				UpdatePathFromDiffuseIntersection(GridThreadId.xy, HitRecord, CurrentPath);

				break;
			// Dielectric
			case 3:
				UpdatePathFromDielectricIntersection(GridThreadId.xy, HitRecord, CurrentPath);

				break;
			// Metallic
			case 4:
				UpdatePathFromMetallicIntersection(GridThreadId.xy, HitRecord, CurrentPath);

				break;
			// Diffuse Light
			case 5:
				UpdatePathFromDiffuseLightIntersection(GridThreadId.xy, HitRecord, CurrentPath);

				break;
		}
	}
}
