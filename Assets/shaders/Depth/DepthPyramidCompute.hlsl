
[[vk::binding(0, 0)]]
Texture2D depthTexture : register(t0, space0);
[[vk::binding(1, 0)]]
SamplerState depthSampler : register(s0);

[[vk::binding(2, 0)]]
RWTexture2D<float> oDepthReduced : register(u1);

struct DepthReduceData
{
	float2 srcSize;
	float2 dstSize;
};

[[vk::push_constant]]
DepthReduceData pcs;

[numthreads(32, 32, 1)]
void main_cs(uint3 GlobalInvocationID : SV_DispatchThreadID)
{
	uint2 SamplePoint = uint2(GlobalInvocationID.xy) * 2;
			
	// Sampler is set up to do min reduction, so this computes the minimum depth of a 2x2 texel quad
	float depth = depthTexture.SampleLevel(depthSampler, (SamplePoint + float2(1,1)) / pcs.srcSize, 0 ).r;
	oDepthReduced[int2(GlobalInvocationID.xy)].x = depth;
}