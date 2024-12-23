


cbuffer ConstantInput : register(b0)
{
    uint numElements;
}

RWStructuredBuffer<uint> keys : register(u1);
RWStructuredBuffer<uint> values : register(u2);

[RootSignature("RootFlags(0), CBV(b0, visibility=SHADER_VISIBILITY_ALL), UAV(u1), UAV(u2)")]
[numthreads(THREAD_GROUP_SIZE_X, 1, 1)]
void main(uint3 inThreadId : SV_DispatchThreadID)
{
    uint threadId = inThreadId.x;

    if (threadId >= numElements)
    {
        return;
    }
    
    values[threadId] = threadId;
}
