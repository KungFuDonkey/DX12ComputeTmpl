
#define WAVE_THREADS (32)
#define RADIX_BITS (8)
#define NUM_BINS (256)
#define RADIX_BITMASK (0xFF)
#define ITEMS_PER_THREAD (15)
#define ITEMS_PER_WAVE (ITEMS_PER_THREAD * WAVE_THREADS)
#define TILE_THREADS (256)
#define TILE_WAVES (TILE_THREADS / WAVE_THREADS)
#define WAVE_HISTOGRAM_SIZE (TILE_WAVES * NUM_BINS)
#define ITEMS_PER_TILE (ITEMS_PER_THREAD * TILE_THREADS)
#define TILE_SCHEDULE_COUNTERS_OFFSET (256 * 4)

#define FLAG_NOT_READY 0
#define FLAG_REDUCTION 1
#define FLAG_INCLUSIVE 2
#define FLAG_MASK      3


cbuffer ConstantInput : register(b0)
{
    uint numElements;
    uint sortPass;
}

RWStructuredBuffer<uint> keys : register(u1);
RWStructuredBuffer<uint> values : register(u2);
RWStructuredBuffer<uint> keysOut : register(u3);
RWStructuredBuffer<uint> valuesOut : register(u4);
RWStructuredBuffer<uint> globalHistogram : register(u5);
RWStructuredBuffer<uint> passHistogram : register(u6);
RWStructuredBuffer<uint> debug0 : register(u7);
RWStructuredBuffer<uint> debug1: register(u8);
RWStructuredBuffer<uint> debug2 : register(u9);
RWStructuredBuffer<uint> debug3 : register(u10);

groupshared uint SharedMem[8192];

[RootSignature("RootFlags(0), CBV(b0, visibility=SHADER_VISIBILITY_ALL), UAV(u1), UAV(u2), UAV(u3), UAV(u4), UAV(u5), UAV(u6), UAV(u7), UAV(u8), UAV(u9), UAV(u10)")]
[numthreads(WAVE_THREADS, 1, 1)]
void InitGlobalHistogram(
    uint3 inThreadId : SV_DispatchThreadID,
    uint3 inGroupThreadId : SV_GroupThreadID
)
{
    uint threadId = inThreadId.x;
    uint waveId = inGroupThreadId.x;

    for (uint i = waveId; i < NUM_BINS * 4; i += WAVE_THREADS)
    {
        SharedMem[i] = 0;
    }

    if (threadId >= numElements)
    {
        return;
    }

    uint key = keys[threadId];

    for (uint i = 0; i < 4; i++)
    {
        uint bin = (key >> (i * RADIX_BITS)) & RADIX_BITMASK;
        InterlockedAdd(SharedMem[bin + NUM_BINS * i], 1);
    }

    GroupMemoryBarrierWithGroupSync();
    
    for (uint i = waveId; i < NUM_BINS * 4; i += WAVE_THREADS)
    {
        uint val = SharedMem[i];

        if (val > 0)
        {
            InterlockedAdd(globalHistogram[i], val);
        }
    }
}

[RootSignature("RootFlags(0), CBV(b0, visibility=SHADER_VISIBILITY_ALL), UAV(u1), UAV(u2), UAV(u3), UAV(u4), UAV(u5), UAV(u6), UAV(u7), UAV(u8), UAV(u9), UAV(u10)")]
[numthreads(WAVE_THREADS, 1, 1)]
void ScanGlobalHistogram(
    uint3 inGroupThreadId : SV_GroupThreadID,
    uint3 inGroupId : SV_GroupID)
{
    uint threadId = inGroupThreadId.x;
    uint histogram = inGroupId.x;

    uint histogramOffset = histogram * NUM_BINS;
    uint prevScanValue = 0;
    
    const uint numThreadGroups = ((numElements - 1) / 3840) + 1;
    const uint numPasses = 4;
    const uint passOffset = histogram * 256 * numThreadGroups;
    
    for (uint i = threadId; i < NUM_BINS; i += WAVE_THREADS)
    {
        uint binValue = globalHistogram[histogramOffset + i];
        uint scanValue = prevScanValue + WavePrefixSum(binValue);
        prevScanValue = WaveReadLaneAt(scanValue + binValue, WAVE_THREADS - 1);
        passHistogram[passOffset + i] = (scanValue << 2U) | FLAG_INCLUSIVE;
    }
}

[RootSignature("RootFlags(0), CBV(b0, visibility=SHADER_VISIBILITY_ALL), UAV(u1), UAV(u2), UAV(u3), UAV(u4), UAV(u5), UAV(u6), UAV(u7), UAV(u8), UAV(u9), UAV(u10)")]
[numthreads(TILE_THREADS, 1, 1)]
void SortStep(
    uint3 inThreadId : SV_DispatchThreadID,
    uint3 inGroupThreadId : SV_GroupThreadID)
{
    uint localId = inGroupThreadId.x;
    uint groupWaveId = localId / WAVE_THREADS;
    uint laneId = WaveGetLaneIndex();
    uint laneMaskLT = (1U << laneId) - 1U;
    uint threadLaneMask = (1U << laneId);
    const uint waveHistogramOffset = groupWaveId * NUM_BINS + NUM_BINS;

    const uint localSortOffset = NUM_BINS;
    const uint radixShift = sortPass * 8;
    
    const uint numThreadGroups = ((numElements - 1) / 3840) + 1;
    const uint numPasses = 4;
    const uint passOffset = sortPass * 256 * numThreadGroups;

    // clear shared memory histogram part and obtain tile id
    {
        uint sharedMemValue = 0;
        if (localId == 0)
        {
            // obtains tile id from the pass offset
            InterlockedAdd(globalHistogram[TILE_SCHEDULE_COUNTERS_OFFSET + sortPass], 1, sharedMemValue);
        }
        SharedMem[localId] = sharedMemValue;
        
        GroupMemoryBarrierWithGroupSync();
    }

    uint tileId = SharedMem[0];
    GroupMemoryBarrierWithGroupSync();

    for (uint i = localId; i < WAVE_HISTOGRAM_SIZE; i += TILE_THREADS)
    {
        SharedMem[i] = 0;
    }
    GroupMemoryBarrierWithGroupSync();

    // bounds for the tile
    uint tileStart = tileId * ITEMS_PER_TILE;
    uint tileEnd = tileStart + ITEMS_PER_TILE;

    // for not perfect arrays
    // todo: should be <, its here for local sort now, change in the future
    if (tileEnd <= numElements)
    {
        uint threadKeys[ITEMS_PER_THREAD];

        // read keys for this wave within the group
        for (uint i = 0, offset = tileStart + ITEMS_PER_WAVE * groupWaveId + laneId; i < ITEMS_PER_THREAD; i++, offset += WAVE_THREADS)
        {
            threadKeys[i] = keys[offset];
        }

        // offsets can be 16 bit
        uint offsets[ITEMS_PER_THREAD];

        // WLMS
        for (uint i = 0; i < ITEMS_PER_THREAD; i++)
        {
            // complex way to count the number of same values in a wave
            // works by masking out threads that do not have the same bit set
            // and then counting the bits

            // bin for the current key
            const uint bin = (threadKeys[i] >> radixShift) & RADIX_BITMASK;

            // initial mask -> all on
            uint sameMask = 0xFFFFFFFFU;
            for (uint bit = 0; bit < RADIX_BITS; bit++)
            {
                // check if the bit is on in the current bin
                const bool isBitOn = (bin >> bit) & 1;

                // WaveActiveBallot returns a mask of all threads that have true for isBitOn
                // We XOR this with the opposite value of the current bit to create a mask of
                // threads that have exactly the same bin value
                sameMask &= ((isBitOn ? 0U : 0xFFFFFFFFU) ^ WaveActiveBallot(isBitOn).x);
            }

            // This will be 0 for the thread with the lowest LaneID, it will have on bits for other threads
            const uint lowerMask = sameMask & laneMaskLT;
            const uint bits = countbits(lowerMask);
            
            uint preIncrementValue = 0;
            
            uint baseThread = lowerMask == 0 ? laneId : firstbithigh(lowerMask);
            
            if (bits == 0)
            {
                // Atomically increment the histogram for this warp in shared memory
                // Atomic might not be necessary?
                InterlockedAdd(SharedMem[waveHistogramOffset + bin], countbits(sameMask), preIncrementValue);
            }

            // Read warp histogram offset from thread that increased the atomic and add
            // internal index to get the offset for this thread
            offsets[i] = WaveReadLaneAt(preIncrementValue, baseThread) + bits;
        }

        GroupMemoryBarrierWithGroupSync();

        // Reduce warp histograms
        uint reduction = SharedMem[NUM_BINS + localId];
        for (uint i = 1; i < TILE_WAVES; i++)
        {
            // increment reduction and write previous value to warp histogram
            const uint index = NUM_BINS + localId + i * NUM_BINS;
            reduction += SharedMem[index];
            SharedMem[index] = reduction - SharedMem[index];
        }

        // write state flag with reduction for other groups
        // Why is this an atomic add, could be an exchange if you store (reduction << 2U | FLAG_REDUCTION), or maybe even not an atomic at all?
        InterlockedAdd(passHistogram[localId + tileId * NUM_BINS], (reduction << 2U) | FLAG_REDUCTION);

        // local histogram creation
        SharedMem[localId] = WavePrefixSum(reduction);
        
        GroupMemoryBarrierWithGroupSync();
        
        // make wave prefix sum circular, so total is in the first thread for that wave
        if (laneId == 31)
        {
            SharedMem[localId - 31] = reduction + SharedMem[localId];
        }
        
        GroupMemoryBarrierWithGroupSync();

        // reduce scan over the first indices of a wave scan
        if (localId < TILE_WAVES)
        {
            SharedMem[localId * WAVE_THREADS] = WavePrefixSum(SharedMem[localId * WAVE_THREADS]);
        }
        
        GroupMemoryBarrierWithGroupSync();
        
        // Apply counts to other indices within the scan
        if (groupWaveId > 0 && laneId > 0)
        {
            SharedMem[localId] += SharedMem[groupWaveId * WAVE_THREADS];
        }
        
        GroupMemoryBarrierWithGroupSync();        

        // create local offsets
        if (groupWaveId)
        {
            // Combines previous wave histograms and local histogram and thread offset to find final position
            for (uint i = 0; i < ITEMS_PER_THREAD; i++)
            {
                const uint bin = (threadKeys[i] >> radixShift) & RADIX_BITMASK;
                const uint localOffset = SharedMem[bin];
                const uint waveOffset = SharedMem[waveHistogramOffset + bin];
                offsets[i] += localOffset + waveOffset;
            }
        }
        else
        {
            // first wave has no previous histogram, so only use local histogram
            for (uint i = 0; i < ITEMS_PER_THREAD; i++)
            {
                const uint bin = (threadKeys[i] >> radixShift) & RADIX_BITMASK;
                const uint localOffset = SharedMem[bin];
                offsets[i] += localOffset;
            }
        }
        
        
        GroupMemoryBarrierWithGroupSync();
        

        // scatter keys into shared memory (overrides wave histograms)
        for (uint i = 0; i < ITEMS_PER_THREAD; i++)
        {
            SharedMem[localSortOffset + offsets[i]] = threadKeys[i];
            SharedMem[localSortOffset + offsets[i] + 3840] = offsets[i];
        }

        GroupMemoryBarrierWithGroupSync();
        
        // Lookback for global index
        uint globalReduction = 0;
        for (uint globalTileId = tileId; globalTileId >= 0; )
        {
            const uint flagPayload = passHistogram[passOffset + localId + globalTileId * NUM_BINS];
            const uint flag = flagPayload & FLAG_MASK;
            const uint flagValue = flagPayload >> 2U;
            
            // check if previous thread group was done
            if ((flagPayload & FLAG_MASK) == FLAG_INCLUSIVE)
            {
                globalReduction += flagValue;
                
                // update count and move flag to FLAG_INCLUSIVE 
                InterlockedAdd(passHistogram[passOffset + localId + (tileId + 1) * NUM_BINS], 1 | (globalReduction << 2));
                
                // update local histogram with global reduction
                SharedMem[localId] = globalReduction - SharedMem[localId];
                
                // reached the end of the scan
                break;
            }
            
            if ((flagPayload & FLAG_MASK) == FLAG_REDUCTION)
            {
                globalReduction += flagValue;
                
                // continue scan on previous
                globalTileId--;
            }
        }
        
        GroupMemoryBarrierWithGroupSync();
        
        // scatter into global memory
        for (uint i = localId; i < ITEMS_PER_TILE; i += WAVE_THREADS)
        {
            uint key = SharedMem[localSortOffset + i];
            keysOut[SharedMem[(key >> radixShift) & RADIX_BITMASK] + i] = key;
        }

    }
    else if (tileStart < numElements)
    {
        // Update bounds
        tileEnd = numElements;
        
        // Lookback if there is more than one group
        if (tileId)
        {
            uint globalReduction = 0;
            for (uint globalTileId = tileId; globalTileId >= 0;)
            {
                const uint flagPayload = passHistogram[passOffset + localId + globalTileId * NUM_BINS];
                const uint flag = flagPayload & FLAG_MASK;
                const uint flagValue = flagPayload >> 2U;
            
                // check if previous thread group was done
                if ((flagPayload & FLAG_MASK) == FLAG_INCLUSIVE)
                {
                    globalReduction += flagValue;
                
                    // update count and move flag to FLAG_INCLUSIVE 
                    InterlockedAdd(passHistogram[passOffset + localId + (tileId + 1) * NUM_BINS], 1 | (globalReduction << 2));
                
                    // update local histogram with global reduction
                    SharedMem[localId] = globalReduction - SharedMem[localId];
                
                    // reached the end of the scan
                    break;
                }
            
                if ((flagPayload & FLAG_MASK) == FLAG_REDUCTION)
                {
                    globalReduction += flagValue;
                
                    // continue scan on previous
                    globalTileId--;
                }
            }
        }
        else
        {
            SharedMem[localId] = passHistogram[localId] >> 2;
        }
        
        GroupMemoryBarrierWithGroupSync();

        for (uint i = localId + tileStart; i < tileEnd; i += WAVE_THREADS)
        {
            
        }

    }

}