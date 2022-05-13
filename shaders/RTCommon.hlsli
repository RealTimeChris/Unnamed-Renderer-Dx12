// RTCommon.hlsli - Shared types, resources, and Path/Payload update functions used by all three ray-tracing compute passes.
// Apr 2020
// Chris M.
// https://github.com/RealTimeChris
#pragma once


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
struct sphere {
	float3 wsOriginStart;
	float3 wsOriginEnd;
	float3 wsOrigin;// World-Space origin of the primitive.
	float wsRadiusStart;
	float wsRadiusEnd;
	float wsRadius;// World-Space radius of the primitive.
	float3 colorStart;
	float3 colorEnd;
	float3 color;// Color/Light-Attenuation of the primitive.
	float materialScalarStart;
	float materialScalarEnd;
	float materialScalar;// Used for Metallic Fuzziness or Dielectric Refractive Index.
	uint primitiveId;// Used for identifying the type of primitive.
	uint objectId;// Unique identifier for the given sphere.
	uint materialId;// Material idenfitier for properly selecting intersection functions.
};


// Represents a Procedural Rectangle.
struct rectangle {
	float3 q1Start;
	float3 q1End;
	float3 q1;// First "Corner" of the rectangle as a plane.
	float3 q2Start;
	float3 q2End;
	float3 q2;// Second "Corner" of the rectangle as a plane.
	float3 q3Start;
	float3 q3End;
	float3 q3;
	float3 q4Start;
	float3 q4End;
	float3 q4;
	float3 colorStart;
	float3 colorEnd;
	float3 color;// Color/Light-Attenuation of the primitive.
	float materialScalarStart;
	float materialScalarEnd;
	float materialScalar;// Used for Metallic Fuzziness or Dielectric Refractive Index.
	uint primitiveId;// Used for identifying the type of primitive.
	uint objectId;// Unique identifier for the given rectangle.
	uint materialId;// Material idenfitier for properly selecting intersection functions.
};


// Represents a Procedural Triangle primitive.
struct triangle_primitive {
	float3 v1Start;// First vertex of the triangle.
	float3 v1End;
	float3 v1;
	float3 v2Start;// Second vertex of the triangle.
	float3 v2End;
	float3 v2;
	float3 v3Start;// Third vertex of the triangle.
	float3 v3End;
	float3 v3;
	float3 colorStart;
	float3 colorEnd;
	float3 color;
	float materialScalarStart;
	float materialScalarEnd;
	float materialScalar;
	uint primitiveId;
	uint objectId;
	uint materialId;
};


// Represents a Path, besides the tMin/tMax values.
struct path {
	float3 wsOrigin;
	float3 wsDirection;
};


// Represents the Light Energy being carried by a given Path.
// 0.0f <= (r, g, b) <= +1.0f
struct path_payload {
	float r, g, b;
};


// Represents an Intersection between a Path and a Scene Object.
struct intersection_record {
	float wStDistance;// World-space distance between path origin and intersection point.
	float3 wsIntersectionPoint;// World-space coordinates of the intersection.
	float3 wsIncomingPathDirection;// World-space direction of the intersecting path.
	uint primitiveId;// Identifier for which type of primitive it is.
	uint objectId;// Unique identifier of the intersected primitive.
	uint materialId;// Material identifier of the intersected primitive.
	uint currentRecursionDepth;// Current path-depth.
};


// 32-bit Root Constants to be passed from Host to Device/Shader via Root Signature(s).
struct inline_root_constants {
	uint3 tsGridDimensions;// Total number of threads per grid, along each of the 3 dimensions.
	float padding00;
	float3 wsCameraLookFrom;// World-Space coordinates of the camera's position.
	float padding01;
	float3 wsCameraLookAt;// World-Space coordinates the camera is aimed at.
	float padding02;
	float3 wsCameraUp;// World-Space "up" direction for the camera.
	float padding03;
	float2 wsViewPortDimensions;// World-Space dimensions of the camera's view port.
	float wsViewPortZCoord;// Unused with lookfrom/lookat camera; kept for layout compatibility.
	float padding04;
	uint maxRecursionDepth;// Maximum depth/number of paths that can be cast into the scene.
	uint samplesPerPixel;// Samples Per Pixel.
	uint maxSampleIndex;// Max Sample Index, with Zero-Indexing.
	uint currentSampleIndex;// Current Sample Index.
	float pathMinDistance;// Minimum distance along a path that an intersection can occur.
	float pathMaxDistance;// Maximum distance along a path that an intersection can occur.
	float2 padding05;
	float3 skyTopColor;// Top sky color.
	float padding06;
	float3 skyBottomColor;// Bottom sky color.
	float padding07;
	uint sphereCount;// Quantity of procedural spheres in the scene.
	uint rectangleCount;// Quantity of procedural rectangles in the scene.
	uint triangleCount;// Quantity of procedural triangles in the scene.
	float globalTickInRadians;// Current cyclical tick value for global system-state.
	float lensRadius;// Radius of the camera's lens aperture; 0.0f yields a pinhole camera (no depth-of-field).
	float focusDistance;// World-Space distance from the camera to the plane of perfect focus.
};


// Global Pipeline Resources.
RWStructuredBuffer<sphere> spheres : register(u0);
RWStructuredBuffer<rectangle> rectangles : register(u1);
RWStructuredBuffer<triangle_primitive> triangles : register(u2);
RWTexture3D<float4> intersectionMap01 : register(u4);
RWTexture3D<float4> intersectionMap02 : register(u5);
RWTexture3D<uint4> intersectionMap03 : register(u6);
RWTexture2D<float4> accumulationFrame : register(u7);
RWTexture2D<unorm float4> finalFrame : register(u8);
ConstantBuffer<inline_root_constants> rootConstants : register(b0);

static const float pi = 3.14159265358979323846f;

static uint randomState;

uint pcg_hash(uint input) {
	uint state = input * 747796405u + 2891336453u;
	uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
	return (word >> 22u) ^ word;
}

void seed_random(in uint2 gridThreadId) {
	randomState = pcg_hash(gridThreadId.x + pcg_hash(gridThreadId.y + pcg_hash(rootConstants.currentSampleIndex + pcg_hash(asuint(rootConstants.globalTickInRadians)))));
}

float random_unit_float() {
	randomState = pcg_hash(randomState);
	return ( float )(randomState >> 8u) * (1.0f / 16777216.0f);
}

float random_signed_float() {
	return random_unit_float() * 2.0f - 1.0f;
}


// Acquires a random offset value for random distributed multi-sampling.
void get_random_offset_into_pixel(in uint2 gridThreadId, inout float2 randomOffset) {
	randomOffset.x = random_unit_float();
	randomOffset.y = random_unit_float();
}


// Calculates normalized thread-space coordinates with randomized pixel-offset enabled.
// 0.0f <= (x,y) <= +1.0f
void get_normalized_ts_coords(in uint2 gridThreadId, in float2 randomOffset, inout float2 normalizedTsCoord) {
	float2 threadId = ( float2 )gridThreadId + randomOffset;
	float2 threadDims = ( float2 )rootConstants.tsGridDimensions;

	normalizedTsCoord.x = threadId.x / threadDims.x;
	normalizedTsCoord.y = (threadDims.y - threadId.y) / threadDims.y;
}


// Generates an IntersectionRecord, representing an intersection between a given Path and either a Sphere or the Sky.
void create_intersection_record(in path currentPath, inout intersection_record hitRecord) {
	float closestHitDistance = rootConstants.pathMaxDistance;

	for (int currentSphereIndex = { 0 }; currentSphereIndex < ( int )rootConstants.sphereCount; currentSphereIndex++) {
		sphere currentSphere = spheres[currentSphereIndex];

		float a = dot(currentPath.wsDirection, currentPath.wsDirection);
		float b = 2.0f * dot(currentPath.wsDirection, (currentPath.wsOrigin - currentSphere.wsOrigin));
		float c = dot((currentPath.wsOrigin - currentSphere.wsOrigin), (currentPath.wsOrigin - currentSphere.wsOrigin)) -
			(currentSphere.wsRadius * currentSphere.wsRadius);
		float discriminant = (b * b) - 4.0f * a * c;

		if (discriminant >= 0.0f) {
			float xPos, xNeg;

			xPos = ((-1.0f * b) + sqrt(discriminant)) / (2.0f * a);
			xNeg = ((-1.0f * b) - sqrt(discriminant)) / (2.0f * a);

			if ((xPos <= xNeg) && (xPos < closestHitDistance) && (xPos > rootConstants.pathMinDistance)) {
				closestHitDistance = xPos;

				hitRecord.wStDistance = xPos;
				hitRecord.wsIntersectionPoint = currentPath.wsOrigin + (xPos * currentPath.wsDirection);
				hitRecord.wsIncomingPathDirection = currentPath.wsDirection;
				hitRecord.primitiveId = currentSphere.primitiveId;
				hitRecord.objectId = currentSphere.objectId;
				hitRecord.materialId = currentSphere.materialId;
			} else if ((xNeg < xPos) && (xNeg < closestHitDistance) && (xNeg > rootConstants.pathMinDistance)) {
				closestHitDistance = xNeg;

				hitRecord.wStDistance = xNeg;
				hitRecord.wsIntersectionPoint = currentPath.wsOrigin + (xNeg * currentPath.wsDirection);
				hitRecord.wsIncomingPathDirection = currentPath.wsDirection;
				hitRecord.primitiveId = currentSphere.primitiveId;
				hitRecord.objectId = currentSphere.objectId;
				hitRecord.materialId = currentSphere.materialId;
			}
		}
	}

	for (int currentRectangleIndex = { 0 }; currentRectangleIndex < ( int )rootConstants.rectangleCount; currentRectangleIndex++) {
		rectangle currentRectangle = rectangles[currentRectangleIndex];

		float3 planeNormal = normalize(cross((currentRectangle.q2 - currentRectangle.q1), (currentRectangle.q3 - currentRectangle.q1)));

		float3 r2 = currentPath.wsDirection + currentPath.wsOrigin;

		float3 dR = currentPath.wsOrigin - r2;

		float3 ds21 = currentRectangle.q2 - currentRectangle.q1;
		float3 ds31 = currentRectangle.q3 - currentRectangle.q1;

		float ndotdR = dot(planeNormal, dR);

		float t = dot(planeNormal, (currentPath.wsOrigin - currentRectangle.q1)) / ndotdR;

		float3 m = currentPath.wsOrigin + (currentPath.wsDirection * t);
		float3 dMS1 = m - currentRectangle.q1;

		float u = dot(dMS1, ds21);
		float v = dot(dMS1, ds31);

		if (abs(ndotdR) > 1e-8f && t < closestHitDistance && t >= rootConstants.pathMinDistance && u >= 0.0f && u <= dot(ds21, ds21) && v >= 0.0f &&
			v <= dot(ds31, ds31)) {
			closestHitDistance = t;

			hitRecord.wStDistance = t;
			hitRecord.wsIntersectionPoint = m;
			hitRecord.wsIncomingPathDirection = currentPath.wsDirection;
			hitRecord.primitiveId = currentRectangle.primitiveId;
			hitRecord.objectId = currentRectangle.objectId;
			hitRecord.materialId = currentRectangle.materialId;
		}
	}

	for (int currentTriangleIndex = { 0 }; currentTriangleIndex < ( int )rootConstants.triangleCount; currentTriangleIndex++) {
		triangle_primitive currentTriangle = triangles[currentTriangleIndex];

		float3 u, v;

		u = currentTriangle.v2 - currentTriangle.v1;
		v = currentTriangle.v3 - currentTriangle.v1;

		float3 surfaceNormal = normalize(cross(u, v));

		float3 dR = currentPath.wsDirection;

		float ndotdR = dot(surfaceNormal, dR);

		float d = dot(surfaceNormal, currentTriangle.v1);

		float t = (-dot(surfaceNormal, currentPath.wsOrigin) + d) / dot(surfaceNormal, currentPath.wsDirection);

		float3 wsIntersectionPoint = currentPath.wsOrigin + t * currentPath.wsDirection;

		float3 edge01, edge02, edge03;
		edge01 = currentTriangle.v2 - currentTriangle.v1;
		edge02 = currentTriangle.v3 - currentTriangle.v2;
		edge03 = currentTriangle.v1 - currentTriangle.v3;

		float3 c1, c2, c3;
		c1 = wsIntersectionPoint - currentTriangle.v1;
		c2 = wsIntersectionPoint - currentTriangle.v2;
		c3 = wsIntersectionPoint - currentTriangle.v3;

		float nDotCrossProduct1, nDotCrossProduct2, nDotCrossProduct3;
		nDotCrossProduct1 = dot(surfaceNormal, cross(edge01, c1));
		nDotCrossProduct2 = dot(surfaceNormal, cross(edge02, c2));
		nDotCrossProduct3 = dot(surfaceNormal, cross(edge03, c3));

		if (abs(ndotdR) > 1e-8f && t > 0.0f && t < closestHitDistance && t >= rootConstants.pathMinDistance && nDotCrossProduct1 >= 0.0f &&
			nDotCrossProduct2 >= 0.0f && nDotCrossProduct3 >= 0.0f) {
			closestHitDistance = t;

			hitRecord.wStDistance = t;
			hitRecord.wsIntersectionPoint = currentPath.wsOrigin + (t * currentPath.wsDirection);
			hitRecord.wsIncomingPathDirection = currentPath.wsDirection;
			hitRecord.primitiveId = currentTriangle.primitiveId;
			hitRecord.objectId = currentTriangle.objectId;
			hitRecord.materialId = currentTriangle.materialId;
		}
	}

	if (closestHitDistance == rootConstants.pathMaxDistance) {
		hitRecord.wStDistance = rootConstants.pathMaxDistance;
		hitRecord.wsIntersectionPoint = currentPath.wsOrigin + (closestHitDistance * currentPath.wsDirection);
		hitRecord.wsIncomingPathDirection = currentPath.wsDirection;
		hitRecord.objectId = 0;
		hitRecord.materialId = 0;
	}
}


// Updates the current Path's values, as a result of an intersection with the Sky.
void update_path_from_sky_intersection(in intersection_record hitRecord, inout path currentPath) {
	currentPath.wsOrigin = currentPath.wsOrigin;

	currentPath.wsDirection = currentPath.wsDirection;
}


// Updates the current Path's values, as a result of an intersection with a Normal-Mapped Sphere.
void update_path_from_surface_normal_intersection(in intersection_record hitRecord, inout path currentPath) {
	float3 surfaceNormal;

	if (hitRecord.primitiveId == 0) {
		surfaceNormal = normalize(hitRecord.wsIntersectionPoint - spheres[hitRecord.objectId].wsOrigin);
	} else if (hitRecord.primitiveId == 1) {
		surfaceNormal = normalize(cross(
			(rectangles[hitRecord.objectId].q2 - rectangles[hitRecord.objectId].q1), (rectangles[hitRecord.objectId].q3 - rectangles[hitRecord.objectId].q1)));
	} else if (hitRecord.primitiveId == 2) {
		float3 u, v;

		u = triangles[hitRecord.objectId].v2 - triangles[hitRecord.objectId].v1;
		v = triangles[hitRecord.objectId].v3 - triangles[hitRecord.objectId].v1;

		surfaceNormal = normalize(cross(u, v));
	}

	currentPath.wsOrigin = hitRecord.wsIntersectionPoint;

	currentPath.wsDirection = surfaceNormal;
}


// Updates the current Path's values, as a result of an intersection with a Diffuse Sphere.
void update_path_from_diffuse_intersection(in uint2 gridThreadId, in intersection_record hitRecord, inout path currentPath) {
	float3 randomDirection = { random_signed_float(), random_signed_float(), random_signed_float() };

	float3 surfaceNormal;

	if (hitRecord.primitiveId == 0) {
		surfaceNormal = normalize(hitRecord.wsIntersectionPoint - spheres[hitRecord.objectId].wsOrigin);
	} else if (hitRecord.primitiveId == 1) {
		surfaceNormal = normalize(cross(
			(rectangles[hitRecord.objectId].q2 - rectangles[hitRecord.objectId].q1), (rectangles[hitRecord.objectId].q3 - rectangles[hitRecord.objectId].q1)));
	} else if (hitRecord.primitiveId == 2) {
		float3 u, v;

		u = triangles[hitRecord.objectId].v2 - triangles[hitRecord.objectId].v1;
		v = triangles[hitRecord.objectId].v3 - triangles[hitRecord.objectId].v1;

		surfaceNormal = normalize(cross(u, v));
	}

	randomDirection = normalize(randomDirection);
	randomDirection = normalize(surfaceNormal + randomDirection);

	currentPath.wsOrigin = hitRecord.wsIntersectionPoint;

	currentPath.wsDirection = randomDirection;
}


// Christophe Schlick's approximation for calculating probability of refraction.
void schlick(in float cosThetaA, in float n1, in float n2, inout float refractionProbability) {
	float r0 = (n1 - n2) / (n1 + n2);
	r0 = r0 * r0;
	refractionProbability = r0 + (1.0f - r0) * pow((1.0f - cosThetaA), 5.0f);
}


// Updates the current Path's values, as a result of an intersection with a Dielectric Sphere.
void update_path_from_dielectric_intersection(in uint2 gridThreadId, in intersection_record hitRecord, inout path currentPath) {
	float n1, n2;
	float3 surfaceNormal;
	float refractiveIndex;

	if (hitRecord.primitiveId == 0) {
		refractiveIndex = spheres[hitRecord.objectId].materialScalar;
		surfaceNormal = normalize(hitRecord.wsIntersectionPoint - spheres[hitRecord.objectId].wsOrigin);
	} else if (hitRecord.primitiveId == 1) {
		refractiveIndex = rectangles[hitRecord.objectId].materialScalar;
		surfaceNormal = normalize(cross(
			(rectangles[hitRecord.objectId].q2 - rectangles[hitRecord.objectId].q1), (rectangles[hitRecord.objectId].q3 - rectangles[hitRecord.objectId].q1)));
	} else {
		refractiveIndex = triangles[hitRecord.objectId].materialScalar;
		surfaceNormal = normalize(cross(
			(triangles[hitRecord.objectId].v2 - triangles[hitRecord.objectId].v1), (triangles[hitRecord.objectId].v3 - triangles[hitRecord.objectId].v1)));
	}

	if (dot(hitRecord.wsIncomingPathDirection, surfaceNormal) < 0.0f) {
		n1 = 1.0f;
		n2 = refractiveIndex;
	} else {
		n1 = refractiveIndex;
		n2 = 1.0f;
		surfaceNormal = -surfaceNormal;
	}

	float cosThetaA = dot(-hitRecord.wsIncomingPathDirection, surfaceNormal);
	float sinThetaA = sqrt(saturate(1.0f - cosThetaA * cosThetaA));

	float refractionRatio = n1 / n2;
	bool cannotRefract = (refractionRatio * sinThetaA) > 1.0f;

	float refractionProbability;
	schlick(cosThetaA, n1, n2, refractionProbability);

	float refractionChance = random_unit_float();

	if (cannotRefract || refractionChance < refractionProbability) {
		float3 reflectedDirection = normalize(hitRecord.wsIncomingPathDirection - 2.0f * dot(hitRecord.wsIncomingPathDirection, surfaceNormal) * surfaceNormal);

		currentPath.wsDirection = reflectedDirection;
		currentPath.wsOrigin = hitRecord.wsIntersectionPoint;
	} else {
		float3 refractedPerp = refractionRatio * (hitRecord.wsIncomingPathDirection + cosThetaA * surfaceNormal);
		float3 refractedParallel = -sqrt(saturate(1.0f - dot(refractedPerp, refractedPerp))) * surfaceNormal;

		currentPath.wsDirection = normalize(refractedPerp + refractedParallel);
		currentPath.wsOrigin = hitRecord.wsIntersectionPoint;
	}
}


// Updates the current Path's values, as a result of an intersection with a Metallic Sphere.
void update_path_from_metallic_intersection(in uint2 gridThreadId, in intersection_record hitRecord, inout path currentPath) {
	float3 randomDirection = { random_signed_float(), random_signed_float(), random_signed_float() };

	float3 fuzzVector;

	float3 surfaceNormal;

	if (hitRecord.primitiveId == 0) {
		fuzzVector = normalize(randomDirection) * spheres[hitRecord.objectId].materialScalar;

		surfaceNormal = normalize(hitRecord.wsIntersectionPoint - spheres[hitRecord.objectId].wsOrigin);
	} else if (hitRecord.primitiveId == 1) {
		fuzzVector = normalize(randomDirection) * rectangles[hitRecord.objectId].materialScalar;

		surfaceNormal = normalize(cross(
			(rectangles[hitRecord.objectId].q2 - rectangles[hitRecord.objectId].q1), (rectangles[hitRecord.objectId].q3 - rectangles[hitRecord.objectId].q1)));
	} else if (hitRecord.primitiveId == 2) {
		fuzzVector = normalize(randomDirection) * triangles[hitRecord.objectId].materialScalar;

		float3 u, v;

		u = triangles[hitRecord.objectId].v2 - triangles[hitRecord.objectId].v1;
		v = triangles[hitRecord.objectId].v3 - triangles[hitRecord.objectId].v1;

		surfaceNormal = normalize(cross(u, v));
	}

	float3 reflectedDirection = normalize(hitRecord.wsIncomingPathDirection - 2.0f * dot(hitRecord.wsIncomingPathDirection, surfaceNormal) * surfaceNormal);

	currentPath.wsDirection = normalize(reflectedDirection + fuzzVector);

	currentPath.wsOrigin = hitRecord.wsIntersectionPoint;
}


// Updates the current Path's values, as a result of an intersection with a Diffuse Light Sphere.
void update_path_from_diffuse_light_intersection(in uint2 gridThreadId, in intersection_record hitRecord, inout path currentPath) {
	float3 randomDirection = { random_signed_float(), random_signed_float(), random_signed_float() };

	float3 surfaceNormal;

	if (hitRecord.primitiveId == 0) {
		surfaceNormal = normalize(hitRecord.wsIntersectionPoint - spheres[hitRecord.objectId].wsOrigin);
	} else if (hitRecord.primitiveId == 1) {
		surfaceNormal = normalize(cross(
			(rectangles[hitRecord.objectId].q2 - rectangles[hitRecord.objectId].q1), (rectangles[hitRecord.objectId].q3 - rectangles[hitRecord.objectId].q1)));
	} else if (hitRecord.primitiveId == 2) {
		float3 u, v;

		u = triangles[hitRecord.objectId].v2 - triangles[hitRecord.objectId].v1;
		v = triangles[hitRecord.objectId].v3 - triangles[hitRecord.objectId].v1;

		surfaceNormal = normalize(cross(u, v));
	}

	randomDirection = normalize(randomDirection);
	randomDirection = normalize(surfaceNormal + randomDirection);

	currentPath.wsOrigin = hitRecord.wsIntersectionPoint;

	currentPath.wsDirection = randomDirection;
}


// Updates a Path's Payload, given an intersection with the "Sky".
void update_payload_from_sky_intersection(in intersection_record hitRecord, inout path_payload payload) {
	float t = hitRecord.wsIncomingPathDirection.y * 0.50f + 0.50f;

	payload.r = ((1.0f - t) * rootConstants.skyBottomColor.x) + (t * rootConstants.skyTopColor.x);
	payload.g = ((1.0f - t) * rootConstants.skyBottomColor.y) + (t * rootConstants.skyTopColor.y);
	payload.b = ((1.0f - t) * rootConstants.skyBottomColor.z) + (t * rootConstants.skyTopColor.z);
}


// Updates a Path's Payload, given an intersection with a Normal-Mapped Sphere.
void update_payload_from_surface_normal_intersection(in intersection_record hitRecord, inout path_payload payload) {
	float3 surfaceNormal;

	if (hitRecord.primitiveId == 0) {
		surfaceNormal = normalize(hitRecord.wsIntersectionPoint - spheres[hitRecord.objectId].wsOrigin);
	} else if (hitRecord.primitiveId == 1) {
		surfaceNormal = normalize(cross(
			(rectangles[hitRecord.objectId].q2 - rectangles[hitRecord.objectId].q1), (rectangles[hitRecord.objectId].q3 - rectangles[hitRecord.objectId].q1)));
	} else if (hitRecord.primitiveId == 2) {
		float3 u, v;

		u = triangles[hitRecord.objectId].v2 - triangles[hitRecord.objectId].v1;
		v = triangles[hitRecord.objectId].v3 - triangles[hitRecord.objectId].v1;

		surfaceNormal = normalize(cross(u, v));
	}

	payload.r = surfaceNormal.x * 0.50f + 0.50f;
	payload.g = surfaceNormal.y * 0.50f + 0.50f;
	payload.b = surfaceNormal.z * 0.50f + 0.50f;
}


// Updates a Path's Payload, given an intersection with a Diffuse Sphere.
void update_payload_from_diffuse_intersection(in intersection_record hitRecord, inout path_payload payload) {
	if (hitRecord.primitiveId == 0) {
		if (hitRecord.currentRecursionDepth < (rootConstants.maxRecursionDepth - 1)) {
			payload.r = spheres[hitRecord.objectId].color.x * payload.r;
			payload.g = spheres[hitRecord.objectId].color.y * payload.g;
			payload.b = spheres[hitRecord.objectId].color.z * payload.b;
		} else if (hitRecord.currentRecursionDepth == (rootConstants.maxRecursionDepth - 1)) {
			payload.r = 0.0f;
			payload.g = 0.0f;
			payload.b = 0.0f;
		}
	} else if (hitRecord.primitiveId == 1) {
		if (hitRecord.currentRecursionDepth < (rootConstants.maxRecursionDepth - 1)) {
			payload.r = rectangles[hitRecord.objectId].color.x * payload.r;
			payload.g = rectangles[hitRecord.objectId].color.y * payload.g;
			payload.b = rectangles[hitRecord.objectId].color.z * payload.b;
		} else if (hitRecord.currentRecursionDepth == (rootConstants.maxRecursionDepth - 1)) {
			payload.r = 0.0f;
			payload.g = 0.0f;
			payload.b = 0.0f;
		}
	} else if (hitRecord.primitiveId == 2) {
		if (hitRecord.currentRecursionDepth < (rootConstants.maxRecursionDepth - 1)) {
			payload.r = triangles[hitRecord.objectId].color.x * payload.r;
			payload.g = triangles[hitRecord.objectId].color.y * payload.g;
			payload.b = triangles[hitRecord.objectId].color.z * payload.b;
		} else if (hitRecord.currentRecursionDepth == (rootConstants.maxRecursionDepth - 1)) {
			payload.r = 0.0f;
			payload.g = 0.0f;
			payload.b = 0.0f;
		}
	}
}


// Updates a Path's Payload, given an intersection with a Dielectric Sphere.
void update_payload_from_dielectric_intersection(in intersection_record hitRecord, inout path_payload payload) {
	if (hitRecord.primitiveId == 0) {
		if (hitRecord.currentRecursionDepth < (rootConstants.maxRecursionDepth - 1)) {
			payload.r = spheres[hitRecord.objectId].color.x * payload.r;
			payload.g = spheres[hitRecord.objectId].color.y * payload.g;
			payload.b = spheres[hitRecord.objectId].color.z * payload.b;
		} else if (hitRecord.currentRecursionDepth == (rootConstants.maxRecursionDepth - 1)) {
			payload.r = 0.0f;
			payload.g = 0.0f;
			payload.b = 0.0f;
		}
	} else if (hitRecord.primitiveId == 1) {
		if (hitRecord.currentRecursionDepth < (rootConstants.maxRecursionDepth - 1)) {
			payload.r = rectangles[hitRecord.objectId].color.x * payload.r;
			payload.g = rectangles[hitRecord.objectId].color.y * payload.g;
			payload.b = rectangles[hitRecord.objectId].color.z * payload.b;
		} else if (hitRecord.currentRecursionDepth == (rootConstants.maxRecursionDepth - 1)) {
			payload.r = 0.0f;
			payload.g = 0.0f;
			payload.b = 0.0f;
		}
	} else if (hitRecord.primitiveId == 2) {
		if (hitRecord.currentRecursionDepth < (rootConstants.maxRecursionDepth - 1)) {
			payload.r = triangles[hitRecord.objectId].color.x * payload.r;
			payload.g = triangles[hitRecord.objectId].color.y * payload.g;
			payload.b = triangles[hitRecord.objectId].color.z * payload.b;
		} else if (hitRecord.currentRecursionDepth == (rootConstants.maxRecursionDepth - 1)) {
			payload.r = 0.0f;
			payload.g = 0.0f;
			payload.b = 0.0f;
		}
	}
}


// Updates a Path's Payload, given an intersection with a Metallic Sphere.
void update_payload_from_metallic_intersection(in intersection_record hitRecord, inout path_payload payload) {
	if (hitRecord.primitiveId == 0) {
		if (hitRecord.currentRecursionDepth < (rootConstants.maxRecursionDepth - 1)) {
			payload.r = spheres[hitRecord.objectId].color.x * payload.r;
			payload.g = spheres[hitRecord.objectId].color.y * payload.g;
			payload.b = spheres[hitRecord.objectId].color.z * payload.b;
		} else if (hitRecord.currentRecursionDepth == (rootConstants.maxRecursionDepth - 1)) {
			payload.r = 0.0f;
			payload.g = 0.0f;
			payload.b = 0.0f;
		}
	} else if (hitRecord.primitiveId == 1) {
		if (hitRecord.currentRecursionDepth < (rootConstants.maxRecursionDepth - 1)) {
			payload.r = rectangles[hitRecord.objectId].color.x * payload.r;
			payload.g = rectangles[hitRecord.objectId].color.y * payload.g;
			payload.b = rectangles[hitRecord.objectId].color.z * payload.b;
		} else if (hitRecord.currentRecursionDepth == (rootConstants.maxRecursionDepth - 1)) {
			payload.r = 0.0f;
			payload.g = 0.0f;
			payload.b = 0.0f;
		}
	} else if (hitRecord.primitiveId == 2) {
		if (hitRecord.currentRecursionDepth < (rootConstants.maxRecursionDepth - 1)) {
			payload.r = triangles[hitRecord.objectId].color.x * payload.r;
			payload.g = triangles[hitRecord.objectId].color.y * payload.g;
			payload.b = triangles[hitRecord.objectId].color.z * payload.b;
		} else if (hitRecord.currentRecursionDepth == (rootConstants.maxRecursionDepth - 1)) {
			payload.r = 0.0f;
			payload.g = 0.0f;
			payload.b = 0.0f;
		}
	}
}


// Updates a Path's Payload, given an intersection with a Diffuse Light Sphere.
void update_payload_from_diffuse_light_intersection(in intersection_record hitRecord, inout path_payload payload) {
	if (hitRecord.primitiveId == 0) {
		if (hitRecord.currentRecursionDepth < (rootConstants.maxRecursionDepth - 1)) {
			payload.r = spheres[hitRecord.objectId].color.x;
			payload.g = spheres[hitRecord.objectId].color.y;
			payload.b = spheres[hitRecord.objectId].color.z;
		} else if (hitRecord.currentRecursionDepth == (rootConstants.maxRecursionDepth - 1)) {
			payload.r = 0.0f;
			payload.g = 0.0f;
			payload.b = 0.0f;
		}
	} else if (hitRecord.primitiveId == 1) {
		if (hitRecord.currentRecursionDepth < (rootConstants.maxRecursionDepth - 1)) {
			payload.r = rectangles[hitRecord.objectId].color.x;
			payload.g = rectangles[hitRecord.objectId].color.y;
			payload.b = rectangles[hitRecord.objectId].color.z;
		} else if (hitRecord.currentRecursionDepth == (rootConstants.maxRecursionDepth - 1)) {
			payload.r = 0.0f;
			payload.g = 0.0f;
			payload.b = 0.0f;
		}
	} else if (hitRecord.primitiveId == 2) {
		if (hitRecord.currentRecursionDepth < (rootConstants.maxRecursionDepth - 1)) {
			payload.r = triangles[hitRecord.objectId].color.x;
			payload.g = triangles[hitRecord.objectId].color.y;
			payload.b = triangles[hitRecord.objectId].color.z;
		} else if (hitRecord.currentRecursionDepth == (rootConstants.maxRecursionDepth - 1)) {
			payload.r = 0.0f;
			payload.g = 0.0f;
			payload.b = 0.0f;
		}
	}
}
