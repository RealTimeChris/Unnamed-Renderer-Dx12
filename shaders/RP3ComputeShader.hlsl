// RP3ComputeShader.hlsl - Consume the Accumulation Frame to produce a Final Frame.
// Apr 2020
// Chris M.
// https://github.com/RealTimeChris

#include "RTCommon.hlsli"


[numthreads(128, 8, 1)] void compute_main(uint3 gridThreadId
										 : SV_DispatchThreadID) {
	float3 compressedMean = accumulationFrame[gridThreadId.xy].xyz / ( float )rootConstants.samplesPerPixel;
	float3 resolvedColor = compressedMean / max(1.0f - compressedMean, 1e-4f);

	finalFrame[gridThreadId.xy].x = sqrt(resolvedColor.x);
	finalFrame[gridThreadId.xy].y = sqrt(resolvedColor.y);
	finalFrame[gridThreadId.xy].z = sqrt(resolvedColor.z);

	accumulationFrame[gridThreadId.xy].x = 0.0f;
	accumulationFrame[gridThreadId.xy].y = 0.0f;
	accumulationFrame[gridThreadId.xy].z = 0.0f;
}
