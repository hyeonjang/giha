//
// reference to https://github.com/MircoWerner/VkRadixSort
// 
import giha;

#define RADIX_SORT_BINS 256

#define ITERATIONS 4

//
uniform uint gNumBatchPerWorkGroup;

groupshared uint[RADIX_SORT_BINS] sharedHistogram;

// launcher
[shader("compute")]
[numthreads(WORKGROUP_SIZE, 1, 1)]
void BuildHistogram<Key, DigitSource>
(
    StructuredBuffer<Key> elements,
    uniform RWStructuredBuffer<uint> histogram,
    uniform uint passIndex,
    uint3 workGroupId: SV_GroupID,
    uint localLinearId: SV_GroupIndex,
)
    where DigitSource : IRadixDigitSource<Key>
{
    let numElements = elements.getCount();
    let workGroupSize = WorkgroupSize();

    if (localLinearId < RADIX_SORT_BINS)
    {
        sharedHistogram[localLinearId] = 0U;
    }
    GroupMemoryBarrierWithGroupSync();

    // distribution control
    for (uint batchId = 0; batchId < gNumBatchPerWorkGroup; batchId++)
    {
        const uint id = workGroupId.x * workGroupSize.x * gNumBatchPerWorkGroup + workGroupSize.x * batchId + localLinearId;
        if (id < numElements)
        {
            const uint bin = DigitSource::extractDigit(elements[id], passIndex);
            InterlockedAdd(sharedHistogram[bin], 1U);
        }
    }
    GroupMemoryBarrierWithGroupSync();

    if (localLinearId < RADIX_SORT_BINS)
    {
        histogram[workGroupId.x * RADIX_SORT_BINS + localLinearId] = sharedHistogram[localLinearId];
    }
}

groupshared uint[RADIX_SORT_BINS / SUBGROUP_SIZE] sums;
groupshared uint[RADIX_SORT_BINS] sharedOffsets;

// todo remove
struct BinFlags {
    uint flags[WORKGROUP_SIZE / 32];
};
groupshared BinFlags[RADIX_SORT_BINS] bin_flags;

[shader("compute")]
[numthreads(WORKGROUP_SIZE, 1, 1)]
void SortByScatter<Key: IInteger, DigitSource>
(
    StructuredBuffer<Key> elements,
    RWStructuredBuffer<uint> histogram,
    uniform uint passIndex,
    RWStructuredBuffer<Key> elementsOut,
    uint3 workGroupId: SV_GroupID,
    uint3 groupThreadId: SV_GroupThreadID,
    uint localLinearId: SV_GroupIndex
) where DigitSource : IRadixDigitSource<Key>
{
    let numElements = elements.getCount();
    let workGroupSize = WorkgroupSize();
    uint laneIndex = WaveGetLaneIndex();
    uint laneCount = WaveGetLaneCount(); // waveSize
    uint waveIndex = localLinearId / laneCount;

    uint workGroupBinOffset = 0;
    uint histogramPrefixSum = 0;

    if (localLinearId < RADIX_SORT_BINS)
    {
        uint histogramCount = 0;
        for (uint j = 0; j < WorkgroupCount().x; j++)
        {
            const uint t = histogram[RADIX_SORT_BINS * j + localLinearId];
            if (j == workGroupId.x)
            {
                workGroupBinOffset = histogramCount;
            }
            histogramCount += t;
        }

        uint sum = WaveActiveSum(histogramCount);
        histogramPrefixSum = WavePrefixSum(histogramCount);

        if (WaveIsFirstLane())
        {
            sums[waveIndex] = sum;
        }
    }
    GroupMemoryBarrierWithGroupSync();

    if (localLinearId == 0)
    {
        uint prefix = 0;
        for (uint i = 0; i < RADIX_SORT_BINS / SUBGROUP_SIZE; ++i)
        {
            uint waveTotal = sums[i];
            sums[i] = prefix;
            prefix += waveTotal;
        }
    }
    GroupMemoryBarrierWithGroupSync();

    const uint flags_bin = localLinearId / 32;
    const uint flags_bit = 1 << (localLinearId % 32);

    //
    if (localLinearId < RADIX_SORT_BINS)
    {
        const uint waveOffset = sums[waveIndex];
        const uint globalHistogram = waveOffset + histogramPrefixSum;
        sharedOffsets[localLinearId] = globalHistogram + workGroupBinOffset;
    }

    // scatter
    for (uint batchId = 0; batchId < gNumBatchPerWorkGroup; batchId++) {
        const uint id = workGroupSize.x * gNumBatchPerWorkGroup * workGroupId.x + workGroupSize.x * batchId + localLinearId;

        if (localLinearId < RADIX_SORT_BINS) {
            for (int i = 0; i < workGroupSize.x / 32; i++) {
                bin_flags[localLinearId].flags[i] = 0U;
            }
        }
        GroupMemoryBarrierWithGroupSync();

        uint bin = 0;
        uint binOffset = 0;
        if (id < numElements) {

            bin = DigitSource::extractDigit(elements[id], passIndex);
            binOffset = sharedOffsets[bin];
            InterlockedAdd(bin_flags[bin].flags[flags_bin], flags_bit);
        }
        GroupMemoryBarrierWithGroupSync();

        if (id < numElements) {

            uint prefix = 0;
            uint count = 0;
            uint full_count = 0;
            for (uint i = 0; i < workGroupSize.x / 32; i++) {
                const uint bits = bin_flags[bin].flags[i];
                full_count = countbits(bits);
                const uint partial_count = countbits(bits & (flags_bit - 1));
                prefix += (i < flags_bin) ? full_count : 0U;
                prefix += (i == flags_bin) ? partial_count : 0U;
                count += full_count;
            }

            elementsOut[binOffset + prefix] = elements[id];
        }
        GroupMemoryBarrierWithGroupSync();
    }
}
