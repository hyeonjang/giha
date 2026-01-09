implementing giha;

#define RADIX_BITS 8

// define radix sort digit
public interface IRadixDigitSource<Key>
{
    static uint getPassCount();
    static uint extractDigit(Key key, uint passIndex);
}

public struct ScalarRadixDigitSource<I : IInteger> : IRadixDigitSource<I>
{
    static const uint kPassCount = (sizeof(I) * 8) / RADIX_BITS;

    static uint getPassCount() { return kPassCount; }
    static uint extractDigit(I key, uint passIndex)
    {
        return bitfieldExtract(key.toUInt(), passIndex * RADIX_BITS, RADIX_BITS);
    }
}

public struct VectorRadixDigitSource<I : IInteger, int N> : IRadixDigitSource<vector<I, N>>
{
    static const uint kDigitsPerLane = (sizeof(I) * 8) / RADIX_BITS;
    static const uint kPassCount = N * kDigitsPerLane;

    static uint getPassCount() { return kPassCount; }
    static uint extractDigit(vector<I, N> key, uint passIndex)
    {
        uint lane = passIndex / kDigitsPerLane;
        uint lanePass = passIndex % kDigitsPerLane;
        uint shift = lanePass * RADIX_BITS;
        return bitfieldExtract(key[lane].toUInt(), shift, RADIX_BITS);
    }
}

typealias UIntRadixDigitSource = ScalarRadixDigitSource<uint>;