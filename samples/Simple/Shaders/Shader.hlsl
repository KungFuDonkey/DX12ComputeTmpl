
// Inputs:
//	THREAD_GROUP_SIZE_X
//	THREAD_GROUP_SIZE_Y
//	THREAD_GROUP_SIZE_Z
//	THREAD_GROUP_SIZE
//	DISPATCH_SIZE_X
//	DISPATCH_SIZE_Y
//	DISPATCH_SIZE_Z
//	DISPATCH_SIZE

#if __RESHARPER__
#define THREAD_GROUP_SIZE_X 8
#define THREAD_GROUP_SIZE_Y 8
#define THREAD_GROUP_SIZE_Z 1
#define THREAD_GROUP_SIZE THREAD_GROUP_SIZE_X * THREAD_GROUP_SIZE_Y * THREAD_GROUP_SIZE_Z

#define DISPATCH_SIZE_X 4
#define DISPATCH_SIZE_Y 4
#define DISPATCH_SIZE_Z 1
#define DISPATCH_SIZE DISPATCH_SIZE_X * DISPATCH_SIZE_Y * DISPATCH_SIZE_Z
#endif

cbuffer ConstantInput : register(b0)
{
	float divValue;
}

RWStructuredBuffer<float4> uav : register(u1);

[RootSignature("RootFlags(0), CBV(b0, visibility=SHADER_VISIBILITY_ALL), UAV(u1)")]
[numthreads(THREAD_GROUP_SIZE_X, THREAD_GROUP_SIZE_Y, THREAD_GROUP_SIZE_Z)]
void main(
	uint3 inGroupID : SV_GroupID,
	uint inGroupIndex : SV_GroupIndex)
{
	uint dispatchThreadId = inGroupIndex + (inGroupID.x + inGroupID.y * DISPATCH_SIZE_X) * THREAD_GROUP_SIZE;

	float4 value = uav[dispatchThreadId];

	value = float4(value.x / divValue, value.x * 2.0 / divValue, value.x * 4.0 / divValue, value.x * 8.0 / divValue);
	uav[dispatchThreadId] = value;
}
