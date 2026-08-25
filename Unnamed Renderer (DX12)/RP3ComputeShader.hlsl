// RP3ComputeShader.hlsl - Consume the Accumulation Frame to produce a Final Frame.
// Apr 2020
// Chris M.
// https://github.com/RealTimeChris

#include "RTCommon.hlsli"


[numthreads(128, 8, 1)] void ComputeMain(uint3 GridThreadId
										 : SV_DispatchThreadID) {
	FinalFrame[GridThreadId.xy].x = sqrt(AccumulationFrame[GridThreadId.xy].x / ( float )RootConstants.SamplesPerPixel);
	FinalFrame[GridThreadId.xy].y = sqrt(AccumulationFrame[GridThreadId.xy].y / ( float )RootConstants.SamplesPerPixel);
	FinalFrame[GridThreadId.xy].z = sqrt(AccumulationFrame[GridThreadId.xy].z / ( float )RootConstants.SamplesPerPixel);

	AccumulationFrame[GridThreadId.xy].x = 0.0f;
	AccumulationFrame[GridThreadId.xy].y = 0.0f;
	AccumulationFrame[GridThreadId.xy].z = 0.0f;
}
