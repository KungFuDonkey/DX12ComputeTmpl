
#if __RESHARPER__
#define THREAD_GROUP_SIZE_X 32
#endif

#define WAVE_TRHEADS 32

cbuffer ConstantInput : register(b0)
{
    uint numElements;
}

RWStructuredBuffer<uint> keys : register(u1);
RWStructuredBuffer<uint> values : register(u2);
RWStructuredBuffer<uint> globalHistogram : register(u3);

groupshared uint SharedMem[32768];

const uint numBins = 256;

[RootSignature("RootFlags(0), CBV(b0, visibility=SHADER_VISIBILITY_ALL), UAV(u1), UAV(u2), UAV(u3)")]
[numthreads(THREAD_GROUP_SIZE_X, 1, 1)]
void InitGlobalHistogram(uint3 inThreadId : SV_DispatchThreadID)
{
    uint threadId = inThreadId.x;

    if (threadId >= numElements)
    {
        return;
    }

    uint key = keys[threadId];

    for (uint i = 0; i < 4; i++)
    {
        uint bin = (key >> (i * 8)) & 0xFF;
        InterlockedAdd(SharedMem[bin + numBins * i], 1);
    }

    GroupMemoryBarrierWithGroupSync();

    for (uint i = threadId; i < numBins * 4; i += THREAD_GROUP_SIZE_X)
    {
        InterlockedAdd(globalHistogram[i], SharedMem[i]);
    }
}

[RootSignature("RootFlags(0), CBV(b0, visibility=SHADER_VISIBILITY_ALL), UAV(u1), UAV(u2), UAV(u3)")]
[numthreads(4, 1, 1)]
void ScanGlobalHistogram(
    uint3 inGroupThreadId : SV_GroupThreadID,
    uint3 inGroupId : SV_GroupID)
{
    uint threadId = inGroupThreadId.x;
    uint histogram = inGroupId.x;

    if (threadId >= numElements)
    {
        return;
    }

    uint histogramOffset = histogram * numBins;
    uint prevScanValue = 0U;
    for (uint i = threadId; i < numBins; i += THREAD_GROUP_SIZE_X)
    {
        uint binValue = globalHistogram[histogramOffset + i];
        uint scanValue = prevScanValue + WavePrefixSum(binValue);
        prevScanValue = WaveReadLaneAt(WAVE_TRHEADS - 1, scanValue + binValue);
        globalHistogram[histogramOffset + i] = scanValue;
    }
    
}
