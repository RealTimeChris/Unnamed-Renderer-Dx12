// RP1ComputeShader.hlsl - Generate the Intersection Maps.
// Apr 2020
// Chris M.
// https://github.com/RealTimeChris


/*
	COORDINATE SPACES:
		TS - Thread-Space
		PS - Pixel-Space
		WS - World-Space
*/


/*
	PRIMITIVE ID:
		0 - Sphere
		1 - Rectangle
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


// Represents a Path, besides the tMin/tMax values.
struct Path {
	float3 WSOrigin;
	float3 WSDirection;
};


// Represents the Light Energy being carried by a given Path.
// 0.0f <= (r, g, b) <= +1.0f
struct PathPayload {
	float r, g, b;
};


// Represents an Intersection between a Path and a Scene Object.
struct IntersectionRecord {
	float WStDistance;// World-space distance between path origin and intersection point.
	float3 WSIntersectionPoint;// World-space coordinates of the intersection.
	float3 WSIncomingPathDirection;// World-space direction of the intersecting path.
	uint PrimitiveId;// Identifier for which type of primitive it is.
	uint ObjectId;// Unique identifier of the intersected primitive.
	uint MaterialId;// Material identifier of the intersected primitive.
	uint CurrentRecursionDepth;// Current path-depth.
};


// 32-bit Root Constants to be passed from Host to Device/Shader via Root Signature(s).
struct InlineRootConstants {
	uint3 TSGridDimensions;// Total number of threads per grid, along each of the 3 dimensions.
	float Padding00;
	float3 WSCameraLookFrom;// World-Space coordinates of the camera's position.
	float Padding01;
	float3 WSCameraLookAt;// World-Space coordinates the camera is aimed at.
	float Padding02;
	float3 WSCameraUp;// World-Space "up" direction for the camera.
	float Padding03;
	float2 WSViewPortDimensions;// World-Space dimensions of the camera's view port.
	float WSViewPortZCoord;// Unused with lookfrom/lookat camera; kept for layout compatibility.
	float Padding04;
	uint MaxRecursionDepth;// Maximum depth/number of paths that can be cast into the scene.
	uint SamplesPerPixel;// Samples Per Pixel.
	uint MaxSampleIndex;// Max Sample Index, with Zero-Indexing.
	uint CurrentSampleIndex;// Current Sample Index.
	float PathMinDistance;// Minimum distance along a path that an intersection can occur.
	float PathMaxDistance;// Maximum distance along a path that an intersection can occur.
	float2 Padding05;
	float3 SkyTopColor;// Top sky color.
	float Padding06;
	float3 SkyBottomColor;// Bottom sky color.
	float Padding07;
	uint SphereCount;// Quantity of procedural spheres in the scene.
	uint RectangleCount;// Quantity of procedural rectangles in the scene.
	uint TriangleCount;// Quantity of procedural triangles in the scene.
	float GlobalTickInRadians;// Current cyclical tick value for global system-state.
	float LensRadius;// Radius of the camera's lens aperture; 0.0f yields a pinhole camera (no depth-of-field).
	float FocusDistance;// World-Space distance from the camera to the plane of perfect focus.
};


// Global Pipeline Resources.
RWStructuredBuffer<Sphere> Spheres : register(u0);
RWStructuredBuffer<Rectangle> Rectangles : register(u1);
RWStructuredBuffer<Triangle> Triangles : register(u2);
RWTexture3D<float> ChaosTexels : register(u3);
RWTexture3D<float4> IntersectionMap01 : register(u4);
RWTexture3D<float4> IntersectionMap02 : register(u5);
RWTexture3D<uint4> IntersectionMap03 : register(u6);
RWTexture2D<float4> AccumulationFrame : register(u7);
RWTexture2D<unorm float4> FinalFrame : register(u8);
ConstantBuffer<InlineRootConstants> RootConstants : register(b0);

static const float PI = 3.14159265358979323846f;


// Acquires a random offset value for random distributed multi-sampling.
void GetRandomOffsetIntoPixel(in uint2 GridThreadId, inout float2 RandomOffset) {
	uint3 ChaosTexelsIndex00 = { GridThreadId.x, GridThreadId.y, 0 };
	uint3 ChaosTexelsIndex01 = { GridThreadId.x, GridThreadId.y, 1 };

	RandomOffset.x = ChaosTexels[ChaosTexelsIndex00] * 0.50f + 0.50f;
	RandomOffset.y = ChaosTexels[ChaosTexelsIndex01] * 0.50f + 0.50f;
}


// Calculates normalized thread-space coordinates with randomized pixel-offset enabled.
// 0.0f <= (x,y) <= +1.0f
void GetNormalizedTSCoords(in uint2 GridThreadId, in float2 RandomOffset, inout float2 NormalizedTSCoord) {
	float2 ThreadId = ( float2 )GridThreadId + RandomOffset;
	float2 ThreadDims = ( float2 )RootConstants.TSGridDimensions;

	NormalizedTSCoord.x = ThreadId.x / ThreadDims.x;
	NormalizedTSCoord.y = (ThreadDims.y - ThreadId.y) / ThreadDims.y;
}


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


// Generates an IntersectionRecord, representing an intersection between a given Path and either a Sphere or the Sky.
void CreateIntersectionRecord(in Path CurrentPath, inout IntersectionRecord HitRecord) {
	float ClosestHitDistance = RootConstants.PathMaxDistance;

	for (int CurrentSphereIndex = { 0 }; CurrentSphereIndex < ( int )RootConstants.SphereCount; CurrentSphereIndex++) {
		Sphere CurrentSphere = Spheres[CurrentSphereIndex];

		float a = dot(CurrentPath.WSDirection, CurrentPath.WSDirection);
		float b = 2.0f * dot(CurrentPath.WSDirection, (CurrentPath.WSOrigin - CurrentSphere.WSOrigin));
		float c = dot((CurrentPath.WSOrigin - CurrentSphere.WSOrigin), (CurrentPath.WSOrigin - CurrentSphere.WSOrigin)) -
			(CurrentSphere.WSRadius * CurrentSphere.WSRadius);
		float Discriminant = (b * b) - 4.0f * a * c;

		if (Discriminant >= 0.0f) {
			float xPos, xNeg;

			xPos = ((-1.0f * b) + sqrt(Discriminant)) / (2.0f * a);
			xNeg = ((-1.0f * b) - sqrt(Discriminant)) / (2.0f * a);

			if ((xPos <= xNeg) && (xPos < ClosestHitDistance) && (xPos > RootConstants.PathMinDistance)) {
				ClosestHitDistance = xPos;

				HitRecord.WStDistance = xPos;
				HitRecord.WSIntersectionPoint = CurrentPath.WSOrigin + (xPos * CurrentPath.WSDirection);
				HitRecord.WSIncomingPathDirection = CurrentPath.WSDirection;
				HitRecord.PrimitiveId = CurrentSphere.PrimitiveId;
				HitRecord.ObjectId = CurrentSphere.ObjectId;
				HitRecord.MaterialId = CurrentSphere.MaterialId;
			} else if ((xNeg < xPos) && (xNeg < ClosestHitDistance) && (xNeg > RootConstants.PathMinDistance)) {
				ClosestHitDistance = xNeg;

				HitRecord.WStDistance = xNeg;
				HitRecord.WSIntersectionPoint = CurrentPath.WSOrigin + (xNeg * CurrentPath.WSDirection);
				HitRecord.WSIncomingPathDirection = CurrentPath.WSDirection;
				HitRecord.PrimitiveId = CurrentSphere.PrimitiveId;
				HitRecord.ObjectId = CurrentSphere.ObjectId;
				HitRecord.MaterialId = CurrentSphere.MaterialId;
			}
		}
	}

	for (int CurrentRectangleIndex = { 0 }; CurrentRectangleIndex < ( int )RootConstants.RectangleCount; CurrentRectangleIndex++) {
		Rectangle CurrentRectangle = Rectangles[CurrentRectangleIndex];

		float3 PlaneNormal = normalize(cross((CurrentRectangle.Q2 - CurrentRectangle.Q1), (CurrentRectangle.Q3 - CurrentRectangle.Q1)));

		float3 R2 = CurrentPath.WSDirection + CurrentPath.WSOrigin;

		float3 dR = CurrentPath.WSOrigin - R2;

		float3 DS21 = CurrentRectangle.Q2 - CurrentRectangle.Q1;
		float3 DS31 = CurrentRectangle.Q3 - CurrentRectangle.Q1;

		float ndotdR = dot(PlaneNormal, dR);

		float t = dot(PlaneNormal, (CurrentPath.WSOrigin - CurrentRectangle.Q1)) / ndotdR;

		float3 M = CurrentPath.WSOrigin + (CurrentPath.WSDirection * t);
		float3 dMS1 = M - CurrentRectangle.Q1;

		float u = dot(dMS1, DS21);
		float v = dot(dMS1, DS31);

		if (abs(ndotdR) > 1e-8f && t < ClosestHitDistance && t >= RootConstants.PathMinDistance && u >= 0.0f && u <= dot(DS21, DS21) && v >= 0.0f &&
			v <= dot(DS31, DS31)) {
			ClosestHitDistance = t;

			HitRecord.WStDistance = t;
			HitRecord.WSIntersectionPoint = M;
			HitRecord.WSIncomingPathDirection = CurrentPath.WSDirection;
			HitRecord.PrimitiveId = CurrentRectangle.PrimitiveId;
			HitRecord.ObjectId = CurrentRectangle.ObjectId;
			HitRecord.MaterialId = CurrentRectangle.MaterialId;
		}
	}

	for (int CurrentTriangleIndex = { 0 }; CurrentTriangleIndex < ( int )RootConstants.TriangleCount; CurrentTriangleIndex++) {
		Triangle CurrentTriangle = Triangles[CurrentTriangleIndex];

		float3 U, V;

		U = CurrentTriangle.V2 - CurrentTriangle.V1;
		V = CurrentTriangle.V3 - CurrentTriangle.V1;

		float3 SurfaceNormal = normalize(cross(U, V));

		float3 dR = CurrentPath.WSDirection;

		float ndotdR = dot(SurfaceNormal, dR);

		float D = dot(SurfaceNormal, CurrentTriangle.V1);

		float t = (-dot(SurfaceNormal, CurrentPath.WSOrigin) + D) / dot(SurfaceNormal, CurrentPath.WSDirection);

		float3 WSIntersectionPoint = CurrentPath.WSOrigin + t * CurrentPath.WSDirection;

		float3 Edge01, Edge02, Edge03;
		Edge01 = CurrentTriangle.V2 - CurrentTriangle.V1;
		Edge02 = CurrentTriangle.V3 - CurrentTriangle.V2;
		Edge03 = CurrentTriangle.V1 - CurrentTriangle.V3;

		float3 C1, C2, C3;
		C1 = WSIntersectionPoint - CurrentTriangle.V1;
		C2 = WSIntersectionPoint - CurrentTriangle.V2;
		C3 = WSIntersectionPoint - CurrentTriangle.V3;

		float NDotCrossProduct1, NDotCrossProduct2, NDotCrossProduct3;
		NDotCrossProduct1 = dot(SurfaceNormal, cross(Edge01, C1));
		NDotCrossProduct2 = dot(SurfaceNormal, cross(Edge02, C2));
		NDotCrossProduct3 = dot(SurfaceNormal, cross(Edge03, C3));

		if (abs(ndotdR) > 1e-8f && t > 0.0f && t < ClosestHitDistance && t >= RootConstants.PathMinDistance && NDotCrossProduct1 >= 0.0f &&
			NDotCrossProduct2 >= 0.0f && NDotCrossProduct3 >= 0.0f) {
			ClosestHitDistance = t;

			HitRecord.WStDistance = t;
			HitRecord.WSIntersectionPoint = CurrentPath.WSOrigin + (t * CurrentPath.WSDirection);
			HitRecord.WSIncomingPathDirection = CurrentPath.WSDirection;
			HitRecord.PrimitiveId = CurrentTriangle.PrimitiveId;
			HitRecord.ObjectId = CurrentTriangle.ObjectId;
			HitRecord.MaterialId = CurrentTriangle.MaterialId;
		}
	}

	if (ClosestHitDistance == RootConstants.PathMaxDistance) {
		HitRecord.WStDistance = RootConstants.PathMaxDistance;
		HitRecord.WSIntersectionPoint = CurrentPath.WSOrigin + (ClosestHitDistance * CurrentPath.WSDirection);
		HitRecord.WSIncomingPathDirection = CurrentPath.WSDirection;
		HitRecord.ObjectId = 0;
		HitRecord.MaterialId = 0;
	}
}


// Updates the current Path's values, as a result of an intersection with the Sky.
void UpdatePathFromSkyIntersection(in IntersectionRecord HitRecord, inout Path CurrentPath) {
	CurrentPath.WSOrigin = CurrentPath.WSOrigin;

	CurrentPath.WSDirection = CurrentPath.WSDirection;
}


// Updates the current Path's values, as a result of an intersection with a Normal-Mapped Sphere.
void UpdatePathFromSurfaceNormalIntersection(in IntersectionRecord HitRecord, inout Path CurrentPath) {
	float3 SurfaceNormal;

	if (HitRecord.PrimitiveId == 0) {
		SurfaceNormal = normalize(HitRecord.WSIntersectionPoint - Spheres[HitRecord.ObjectId].WSOrigin);
	} else if (HitRecord.PrimitiveId == 1) {
		SurfaceNormal = normalize(cross(
			(Rectangles[HitRecord.ObjectId].Q2 - Rectangles[HitRecord.ObjectId].Q1), (Rectangles[HitRecord.ObjectId].Q3 - Rectangles[HitRecord.ObjectId].Q1)));
	} else if (HitRecord.PrimitiveId == 2) {
		float3 U, V;

		U = Triangles[HitRecord.ObjectId].V2 - Triangles[HitRecord.ObjectId].V1;
		V = Triangles[HitRecord.ObjectId].V3 - Triangles[HitRecord.ObjectId].V1;

		SurfaceNormal = normalize(cross(U, V));
	}

	CurrentPath.WSOrigin = HitRecord.WSIntersectionPoint;

	CurrentPath.WSDirection = SurfaceNormal;
}


// Updates the current Path's values, as a result of an intersection with a Diffuse Sphere.
void UpdatePathFromDiffuseIntersection(in uint2 GridThreadId, in IntersectionRecord HitRecord, inout Path CurrentPath) {
	uint3 ChaosTexelsIndex00 = { GridThreadId.x, GridThreadId.y, 0 };
	uint3 ChaosTexelsIndex01 = { GridThreadId.x, GridThreadId.y, 1 };
	uint3 ChaosTexelsIndex02 = { GridThreadId.x, GridThreadId.y, 2 };

	float3 RandomDirection = { ChaosTexels[ChaosTexelsIndex00], ChaosTexels[ChaosTexelsIndex01], ChaosTexels[ChaosTexelsIndex02] };

	float3 SurfaceNormal;

	if (HitRecord.PrimitiveId == 0) {
		SurfaceNormal = normalize(HitRecord.WSIntersectionPoint - Spheres[HitRecord.ObjectId].WSOrigin);
	} else if (HitRecord.PrimitiveId == 1) {
		SurfaceNormal = normalize(cross(
			(Rectangles[HitRecord.ObjectId].Q2 - Rectangles[HitRecord.ObjectId].Q1), (Rectangles[HitRecord.ObjectId].Q3 - Rectangles[HitRecord.ObjectId].Q1)));
	} else if (HitRecord.PrimitiveId == 2) {
		float3 U, V;

		U = Triangles[HitRecord.ObjectId].V2 - Triangles[HitRecord.ObjectId].V1;
		V = Triangles[HitRecord.ObjectId].V3 - Triangles[HitRecord.ObjectId].V1;

		SurfaceNormal = normalize(cross(U, V));
	}

	RandomDirection = normalize(RandomDirection);
	RandomDirection = normalize(SurfaceNormal + RandomDirection);

	CurrentPath.WSOrigin = HitRecord.WSIntersectionPoint;

	CurrentPath.WSDirection = RandomDirection;
}


// Christophe Schlick's approximation for calculating probability of refraction.
void Schlick(in float CosThetaA, in float n1, in float n2, inout float RefractionProbability) {
	float R0 = (n1 - n2) / (n1 + n2);

	R0 = R0 * R0;
	R0 = R0 + (1.0f - R0) * pow((1.0f - CosThetaA), 5.0f);

	RefractionProbability = R0;
}


// Updates the current Path's values, as a result of an intersection with a Dielectric Sphere.
void UpdatePathFromDielectricIntersection(in uint2 GridThreadId, in IntersectionRecord HitRecord, inout Path CurrentPath) {
	float n1, n2;
	float3 SurfaceNormal = normalize(HitRecord.WSIntersectionPoint - Spheres[HitRecord.ObjectId].WSOrigin);
	bool OutwardNormal = dot(HitRecord.WSIncomingPathDirection, SurfaceNormal) < 0.0f;

	if (HitRecord.PrimitiveId == 0) {
		if (OutwardNormal) {
			n1 = 1.0f;
			n2 = Spheres[HitRecord.ObjectId].MaterialScalar;

			SurfaceNormal = normalize(HitRecord.WSIntersectionPoint - Spheres[HitRecord.ObjectId].WSOrigin);
		} else {
			n1 = Spheres[HitRecord.ObjectId].MaterialScalar;
			n2 = 1.0f;

			SurfaceNormal = -normalize(HitRecord.WSIntersectionPoint - Spheres[HitRecord.ObjectId].WSOrigin);
		}
	} else if (HitRecord.PrimitiveId == 1) {
		n1 = 1.0f;
		n2 = Rectangles[HitRecord.ObjectId].MaterialScalar;

		SurfaceNormal = normalize(cross(
			(Rectangles[HitRecord.ObjectId].Q2 - Rectangles[HitRecord.ObjectId].Q1), (Rectangles[HitRecord.ObjectId].Q3 - Rectangles[HitRecord.ObjectId].Q1)));
	} else if (HitRecord.PrimitiveId == 2) {
		n1 = 1.0f;
		n2 = Triangles[HitRecord.ObjectId].MaterialScalar;

		float3 U, V;

		U = Triangles[HitRecord.ObjectId].V2 - Triangles[HitRecord.ObjectId].V1;
		V = Triangles[HitRecord.ObjectId].V3 - Triangles[HitRecord.ObjectId].V1;

		SurfaceNormal = normalize(cross(U, V));
	}

	float CosThetaA = dot(-HitRecord.WSIncomingPathDirection, SurfaceNormal);
	float ThetaA = acos(CosThetaA);
	float ThetaB;
	float SinThetaA = sqrt(1.0f - CosThetaA * CosThetaA);

	float RefractionRatio;
	bool CannotRefract;
	float RefractionProbability;

	RefractionRatio = n1 / n2;
	CannotRefract = RefractionRatio * SinThetaA > 1.0f;
	ThetaB = asin(sin(ThetaA) * (n1 / n2));
	Schlick(CosThetaA, n1, n2, RefractionProbability);

	float3 ChaosTexelsIndex = { GridThreadId.x, GridThreadId.y, 0 };
	float RefractionChance = ChaosTexels[ChaosTexelsIndex] * 0.50f + 0.50f * 100.0f;

	float3 C = SurfaceNormal * CosThetaA;
	float3 M = (HitRecord.WSIncomingPathDirection + C) / SinThetaA;
	float3 A = M * sin(ThetaB);
	float3 B = -SurfaceNormal * cos(ThetaB);

	float3 RefractedDirection = normalize(A + B);

	if (RefractionChance < RefractionProbability || (CannotRefract == true) ||
		(isnan(RefractedDirection.x) || isnan(RefractedDirection.y) || isnan(RefractedDirection.z))) {
		float3 ReflectedDirection = normalize(HitRecord.WSIncomingPathDirection - 2.0f * dot(HitRecord.WSIncomingPathDirection, SurfaceNormal) * SurfaceNormal);

		CurrentPath.WSDirection = ReflectedDirection;
		CurrentPath.WSOrigin = HitRecord.WSIntersectionPoint;
	} else {
		CurrentPath.WSDirection = RefractedDirection;
		CurrentPath.WSOrigin = HitRecord.WSIntersectionPoint;
	}
}


// Updates the current Path's values, as a result of an intersection with a Metallic Sphere.
void UpdatePathFromMetallicIntersection(in uint2 GridThreadId, in IntersectionRecord HitRecord, inout Path CurrentPath) {
	uint3 ChaosTexelsIndex00 = { GridThreadId.x, GridThreadId.y, 0 };
	uint3 ChaosTexelsIndex01 = { GridThreadId.x, GridThreadId.y, 1 };
	uint3 ChaosTexelsIndex02 = { GridThreadId.x, GridThreadId.y, 2 };

	float3 RandomDirection = { ChaosTexels[ChaosTexelsIndex00], ChaosTexels[ChaosTexelsIndex01], ChaosTexels[ChaosTexelsIndex02] };

	float3 FuzzVector;

	float3 SurfaceNormal;

	if (HitRecord.PrimitiveId == 0) {
		FuzzVector = normalize(RandomDirection) * Spheres[HitRecord.ObjectId].MaterialScalar;

		SurfaceNormal = normalize(HitRecord.WSIntersectionPoint - Spheres[HitRecord.ObjectId].WSOrigin);
	} else if (HitRecord.PrimitiveId == 1) {
		FuzzVector = normalize(RandomDirection) * Rectangles[HitRecord.ObjectId].MaterialScalar;

		SurfaceNormal = normalize(cross(
			(Rectangles[HitRecord.ObjectId].Q2 - Rectangles[HitRecord.ObjectId].Q1), (Rectangles[HitRecord.ObjectId].Q3 - Rectangles[HitRecord.ObjectId].Q1)));
	} else if (HitRecord.PrimitiveId == 2) {
		FuzzVector = normalize(RandomDirection) * Triangles[HitRecord.ObjectId].MaterialScalar;

		float3 U, V;

		U = Triangles[HitRecord.ObjectId].V2 - Triangles[HitRecord.ObjectId].V1;
		V = Triangles[HitRecord.ObjectId].V3 - Triangles[HitRecord.ObjectId].V1;

		SurfaceNormal = normalize(cross(U, V));
	}

	float3 ReflectedDirection = normalize(HitRecord.WSIncomingPathDirection - 2.0f * dot(HitRecord.WSIncomingPathDirection, SurfaceNormal) * SurfaceNormal);

	CurrentPath.WSDirection = normalize(ReflectedDirection + FuzzVector);

	CurrentPath.WSOrigin = HitRecord.WSIntersectionPoint;
}


// Updates the current Path's values, as a result of an intersection with a Diffuse Light Sphere.
void UpdatePathFromDiffuseLightIntersection(in uint2 GridThreadId, in IntersectionRecord HitRecord, inout Path CurrentPath) {
	uint3 ChaosTexelsIndex00 = { GridThreadId.x, GridThreadId.y, 0 };
	uint3 ChaosTexelsIndex01 = { GridThreadId.x, GridThreadId.y, 1 };
	uint3 ChaosTexelsIndex02 = { GridThreadId.x, GridThreadId.y, 2 };

	float3 RandomDirection = { ChaosTexels[ChaosTexelsIndex00], ChaosTexels[ChaosTexelsIndex01], ChaosTexels[ChaosTexelsIndex02] };

	float3 SurfaceNormal;

	if (HitRecord.PrimitiveId == 0) {
		SurfaceNormal = normalize(HitRecord.WSIntersectionPoint - Spheres[HitRecord.ObjectId].WSOrigin);
	} else if (HitRecord.PrimitiveId == 1) {
		SurfaceNormal = normalize(cross(
			(Rectangles[HitRecord.ObjectId].Q2 - Rectangles[HitRecord.ObjectId].Q1), (Rectangles[HitRecord.ObjectId].Q3 - Rectangles[HitRecord.ObjectId].Q1)));
	} else if (HitRecord.PrimitiveId == 2) {
		float3 U, V;

		U = Triangles[HitRecord.ObjectId].V2 - Triangles[HitRecord.ObjectId].V1;
		V = Triangles[HitRecord.ObjectId].V3 - Triangles[HitRecord.ObjectId].V1;

		SurfaceNormal = normalize(cross(U, V));
	}

	RandomDirection = normalize(RandomDirection);
	RandomDirection = normalize(SurfaceNormal + RandomDirection);

	CurrentPath.WSOrigin = HitRecord.WSIntersectionPoint;

	CurrentPath.WSDirection = RandomDirection;
}


// Updates a Path's Payload, given an intersection with the "Sky".
void UpdatePayloadFromSkyIntersection(in IntersectionRecord HitRecord, inout PathPayload Payload) {
	float t = HitRecord.WSIncomingPathDirection.y * 0.50f + 0.50f;

	Payload.r = ((1.0f - t) * RootConstants.SkyBottomColor.x) + (t * RootConstants.SkyTopColor.x);
	Payload.g = ((1.0f - t) * RootConstants.SkyBottomColor.y) + (t * RootConstants.SkyTopColor.y);
	Payload.b = ((1.0f - t) * RootConstants.SkyBottomColor.z) + (t * RootConstants.SkyTopColor.z);
}


// Updates a Path's Payload, given an intersection with a Normal-Mapped Sphere.
void UpdatePayloadFromSurfaceNormalIntersection(in IntersectionRecord HitRecord, inout PathPayload Payload) {
	float3 SurfaceNormal;

	if (HitRecord.PrimitiveId == 0) {
		SurfaceNormal = normalize(HitRecord.WSIntersectionPoint - Spheres[HitRecord.ObjectId].WSOrigin);
	} else if (HitRecord.PrimitiveId == 1) {
		SurfaceNormal = normalize(cross(
			(Rectangles[HitRecord.ObjectId].Q2 - Rectangles[HitRecord.ObjectId].Q1), (Rectangles[HitRecord.ObjectId].Q3 - Rectangles[HitRecord.ObjectId].Q1)));
	} else if (HitRecord.PrimitiveId == 2) {
		float3 U, V;

		U = Triangles[HitRecord.ObjectId].V2 - Triangles[HitRecord.ObjectId].V1;
		V = Triangles[HitRecord.ObjectId].V3 - Triangles[HitRecord.ObjectId].V1;

		SurfaceNormal = normalize(cross(U, V));
	}

	Payload.r = SurfaceNormal.x * 0.50f + 0.50f;
	Payload.g = SurfaceNormal.y * 0.50f + 0.50f;
	Payload.b = SurfaceNormal.z * 0.50f + 0.50f;
}


// Updates a Path's Payload, given an intersection with a Diffuse Sphere.
void UpdatePayloadFromDiffuseIntersection(in IntersectionRecord HitRecord, inout PathPayload Payload) {
	if (HitRecord.PrimitiveId == 0) {
		if (HitRecord.CurrentRecursionDepth < (RootConstants.MaxRecursionDepth - 1)) {
			Payload.r = Spheres[HitRecord.ObjectId].Color.x * Payload.r;
			Payload.g = Spheres[HitRecord.ObjectId].Color.y * Payload.g;
			Payload.b = Spheres[HitRecord.ObjectId].Color.z * Payload.b;
		} else if (HitRecord.CurrentRecursionDepth == (RootConstants.MaxRecursionDepth - 1)) {
			Payload.r = 0.0f;
			Payload.g = 0.0f;
			Payload.b = 0.0f;
		}
	} else if (HitRecord.PrimitiveId == 1) {
		if (HitRecord.CurrentRecursionDepth < (RootConstants.MaxRecursionDepth - 1)) {
			Payload.r = Rectangles[HitRecord.ObjectId].Color.x * Payload.r;
			Payload.g = Rectangles[HitRecord.ObjectId].Color.y * Payload.g;
			Payload.b = Rectangles[HitRecord.ObjectId].Color.z * Payload.b;
		} else if (HitRecord.CurrentRecursionDepth == (RootConstants.MaxRecursionDepth - 1)) {
			Payload.r = 0.0f;
			Payload.g = 0.0f;
			Payload.b = 0.0f;
		}
	} else if (HitRecord.PrimitiveId == 2) {
		if (HitRecord.CurrentRecursionDepth < (RootConstants.MaxRecursionDepth - 1)) {
			Payload.r = Triangles[HitRecord.ObjectId].Color.x * Payload.r;
			Payload.g = Triangles[HitRecord.ObjectId].Color.y * Payload.g;
			Payload.b = Triangles[HitRecord.ObjectId].Color.z * Payload.b;
		} else if (HitRecord.CurrentRecursionDepth == (RootConstants.MaxRecursionDepth - 1)) {
			Payload.r = 0.0f;
			Payload.g = 0.0f;
			Payload.b = 0.0f;
		}
	}
}


// Updates a Path's Payload, given an intersection with a Dielectric Sphere.
void UpdatePayloadFromDielectricIntersection(in IntersectionRecord HitRecord, inout PathPayload Payload) {
	if (HitRecord.PrimitiveId == 0) {
		if (HitRecord.CurrentRecursionDepth < (RootConstants.MaxRecursionDepth - 1)) {
			Payload.r = Spheres[HitRecord.ObjectId].Color.x * Payload.r;
			Payload.g = Spheres[HitRecord.ObjectId].Color.y * Payload.g;
			Payload.b = Spheres[HitRecord.ObjectId].Color.z * Payload.b;
		} else if (HitRecord.CurrentRecursionDepth == (RootConstants.MaxRecursionDepth - 1)) {
			Payload.r = 0.0f;
			Payload.g = 0.0f;
			Payload.b = 0.0f;
		}
	} else if (HitRecord.PrimitiveId == 1) {
		if (HitRecord.CurrentRecursionDepth < (RootConstants.MaxRecursionDepth - 1)) {
			Payload.r = Rectangles[HitRecord.ObjectId].Color.x * Payload.r;
			Payload.g = Rectangles[HitRecord.ObjectId].Color.y * Payload.g;
			Payload.b = Rectangles[HitRecord.ObjectId].Color.z * Payload.b;
		} else if (HitRecord.CurrentRecursionDepth == (RootConstants.MaxRecursionDepth - 1)) {
			Payload.r = 0.0f;
			Payload.g = 0.0f;
			Payload.b = 0.0f;
		}
	} else if (HitRecord.PrimitiveId == 2) {
		if (HitRecord.CurrentRecursionDepth < (RootConstants.MaxRecursionDepth - 1)) {
			Payload.r = Triangles[HitRecord.ObjectId].Color.x * Payload.r;
			Payload.g = Triangles[HitRecord.ObjectId].Color.y * Payload.g;
			Payload.b = Triangles[HitRecord.ObjectId].Color.z * Payload.b;
		} else if (HitRecord.CurrentRecursionDepth == (RootConstants.MaxRecursionDepth - 1)) {
			Payload.r = 0.0f;
			Payload.g = 0.0f;
			Payload.b = 0.0f;
		}
	}
}


// Updates a Path's Payload, given an intersection with a Metallic Sphere.
void UpdatePayloadFromMetallicIntersection(in IntersectionRecord HitRecord, inout PathPayload Payload) {
	if (HitRecord.PrimitiveId == 0) {
		if (HitRecord.CurrentRecursionDepth < (RootConstants.MaxRecursionDepth - 1)) {
			Payload.r = Spheres[HitRecord.ObjectId].Color.x * Payload.r;
			Payload.g = Spheres[HitRecord.ObjectId].Color.y * Payload.g;
			Payload.b = Spheres[HitRecord.ObjectId].Color.z * Payload.b;
		} else if (HitRecord.CurrentRecursionDepth == (RootConstants.MaxRecursionDepth - 1)) {
			Payload.r = 0.0f;
			Payload.g = 0.0f;
			Payload.b = 0.0f;
		}
	} else if (HitRecord.PrimitiveId == 1) {
		if (HitRecord.CurrentRecursionDepth < (RootConstants.MaxRecursionDepth - 1)) {
			Payload.r = Rectangles[HitRecord.ObjectId].Color.x * Payload.r;
			Payload.g = Rectangles[HitRecord.ObjectId].Color.y * Payload.g;
			Payload.b = Rectangles[HitRecord.ObjectId].Color.z * Payload.b;
		} else if (HitRecord.CurrentRecursionDepth == (RootConstants.MaxRecursionDepth - 1)) {
			Payload.r = 0.0f;
			Payload.g = 0.0f;
			Payload.b = 0.0f;
		}
	} else if (HitRecord.PrimitiveId == 2) {
		if (HitRecord.CurrentRecursionDepth < (RootConstants.MaxRecursionDepth - 1)) {
			Payload.r = Triangles[HitRecord.ObjectId].Color.x * Payload.r;
			Payload.g = Triangles[HitRecord.ObjectId].Color.y * Payload.g;
			Payload.b = Triangles[HitRecord.ObjectId].Color.z * Payload.b;
		} else if (HitRecord.CurrentRecursionDepth == (RootConstants.MaxRecursionDepth - 1)) {
			Payload.r = 0.0f;
			Payload.g = 0.0f;
			Payload.b = 0.0f;
		}
	}
}


// Updates a Path's Payload, given an intersection with a Diffuse Light Sphere.
void UpdatePayloadFromDiffuseLightIntersection(in IntersectionRecord HitRecord, inout PathPayload Payload) {
	if (HitRecord.PrimitiveId == 0) {
		if (HitRecord.CurrentRecursionDepth < (RootConstants.MaxRecursionDepth - 1)) {
			Payload.r = Spheres[HitRecord.ObjectId].Color.x;
			Payload.g = Spheres[HitRecord.ObjectId].Color.y;
			Payload.b = Spheres[HitRecord.ObjectId].Color.z;
		} else if (HitRecord.CurrentRecursionDepth == (RootConstants.MaxRecursionDepth - 1)) {
			Payload.r = 0.0f;
			Payload.g = 0.0f;
			Payload.b = 0.0f;
		}
	} else if (HitRecord.PrimitiveId == 1) {
		if (HitRecord.CurrentRecursionDepth < (RootConstants.MaxRecursionDepth - 1)) {
			Payload.r = Rectangles[HitRecord.ObjectId].Color.x;
			Payload.g = Rectangles[HitRecord.ObjectId].Color.y;
			Payload.b = Rectangles[HitRecord.ObjectId].Color.z;
		} else if (HitRecord.CurrentRecursionDepth == (RootConstants.MaxRecursionDepth - 1)) {
			Payload.r = 0.0f;
			Payload.g = 0.0f;
			Payload.b = 0.0f;
		}
	} else if (HitRecord.PrimitiveId == 2) {
		if (HitRecord.CurrentRecursionDepth < (RootConstants.MaxRecursionDepth - 1)) {
			Payload.r = Triangles[HitRecord.ObjectId].Color.x;
			Payload.g = Triangles[HitRecord.ObjectId].Color.y;
			Payload.b = Triangles[HitRecord.ObjectId].Color.z;
		} else if (HitRecord.CurrentRecursionDepth == (RootConstants.MaxRecursionDepth - 1)) {
			Payload.r = 0.0f;
			Payload.g = 0.0f;
			Payload.b = 0.0f;
		}
	}
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
