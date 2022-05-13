// RP3ComputeShader.hlsl - Consume the Accumulation Frame to produce a Final Frame.
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
	float3 WSCameraFocalOrigin;// World-Space coordinates of the camera's focal point.
	float Padding01;
	float2 WSViewPortDimensions;// World-Space dimensions of the camera's view port.
	float WSViewPortZCoord;// Set this based on desired Vertical Field of View.
	float Padding02;
	uint MaxRecursionDepth;// Maximum depth/number of paths that can be cast into the scene.
	uint SamplesPerPixel;// Samples Per Pixel.
	uint MaxSampleIndex;// Max Sample Index, with Zero-Indexing.
	uint CurrentSampleIndex;// Current Sample Index.
	float PathMinDistance;// Minimum distance along a path that an intersection can occur.
	float PathMaxDistance;// Maximum distance along a path that an intersection can occur.
	float2 Padding03;
	float3 SkyTopColor;// Top sky color.
	float Padding04;
	float3 SkyBottomColor;// Bottom sky color.
	float Padding05;
	uint SphereCount;// Quantity of procedural spheres in the scene.
	uint RectangleCount;// Quantity of procedural rectangles in the scene.
	uint TriangleCount;// Quantity of procedural triangles in the scene.
	float GlobalTickInRadians;// Current cyclical tick value for global system-state.
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


// Calculates the world-space coordinates of current thread's camera path origin.
void GetWSCamPathOrigin(in float2 NormalizedTSCoords, inout float3 WSCamPathOrigin) {
	WSCamPathOrigin.x = NormalizedTSCoords.x * RootConstants.WSViewPortDimensions.x - (RootConstants.WSViewPortDimensions.x / 2.0f);
	WSCamPathOrigin.y = NormalizedTSCoords.y * RootConstants.WSViewPortDimensions.y - (RootConstants.WSViewPortDimensions.y / 2.0f);
	WSCamPathOrigin.z = RootConstants.WSViewPortZCoord;
}


// Calculates the world-space components of the current thread's camera path direction.
void GetWSCamPathDirection(in float3 WSCamPathOrigin, inout float3 WSCamPathDirection) {
	WSCamPathDirection = normalize(WSCamPathOrigin - RootConstants.WSCameraFocalOrigin);
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
void Schlick(in float ThetaA, in float n2, inout float RefractionProbability) {
	float CosThetaA = cos(ThetaA);

	float R0 = (1.0f - n2) / (1.0f + n2);
	R0 = R0 * R0;
	R0 = R0 + (1.0f - R0) * pow((1.0f - CosThetaA), 5.0f);

	RefractionProbability = R0;
}


// Updates the current Path's values, as a result of an intersection with a Dielectric Sphere.
void UpdatePathFromDielectricIntersection(in uint2 GridThreadId, in IntersectionRecord HitRecord, inout Path CurrentPath) {
	// Are we entering or leaving a sphere?
	float Distance = length(CurrentPath.WSOrigin - Spheres[HitRecord.ObjectId].WSOrigin);

	float n1, n2;
	float3 SurfaceNormal;

	if (HitRecord.PrimitiveId == 0) {
		if (Distance > Spheres[HitRecord.ObjectId].WSRadius) {
			n1 = 1.0f;
			n2 = Spheres[HitRecord.ObjectId].MaterialScalar;

			SurfaceNormal = normalize(HitRecord.WSIntersectionPoint - Spheres[HitRecord.ObjectId].WSOrigin);
		} else if (Distance <= Spheres[HitRecord.ObjectId].WSRadius) {
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

	float ThetaA = acos(dot(HitRecord.WSIncomingPathDirection, SurfaceNormal));
	float ThetaB = asin(sin(ThetaA) * (n1 / n2));

	float3 C = SurfaceNormal * cos(ThetaA);
	float3 M = (HitRecord.WSIncomingPathDirection + C) / sin(ThetaA);
	float3 A = M * sin(ThetaB);
	float3 B = -SurfaceNormal * cos(ThetaB);

	float3 RefractedDirection = normalize(A + B);

	float RefractionProbability;
	Schlick(ThetaA, n2, RefractionProbability);

	float3 ChaosTexelsIndex = { GridThreadId.x, GridThreadId.y, 0 };
	float RefractionChance = ChaosTexels[ChaosTexelsIndex] * 0.50f + 0.50f;

	if (RefractionChance > RefractionProbability || (isnan(RefractedDirection.x) || isnan(RefractedDirection.y) || isnan(RefractedDirection.z))) {
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
	FinalFrame[GridThreadId.xy].x = sqrt(AccumulationFrame[GridThreadId.xy].x / ( float )RootConstants.SamplesPerPixel);
	FinalFrame[GridThreadId.xy].y = sqrt(AccumulationFrame[GridThreadId.xy].y / ( float )RootConstants.SamplesPerPixel);
	FinalFrame[GridThreadId.xy].z = sqrt(AccumulationFrame[GridThreadId.xy].z / ( float )RootConstants.SamplesPerPixel);

	AccumulationFrame[GridThreadId.xy].x = 0.0f;
	AccumulationFrame[GridThreadId.xy].y = 0.0f;
	AccumulationFrame[GridThreadId.xy].z = 0.0f;
}
