implementing giha;

public interface IUIntCodec<Code, UInt : IInteger>
{
    static UInt encode(Code point);
    static UInt encode(Code point, uint bitCount);
    static Code decode(UInt code);
    static Code decode(UInt code, uint bitCount);
}

public struct MortonCodec<I : IInteger, int N> : IUIntCodec<vector<I, N>, uint64_t>
{
    static const uint kBitsPerLane = uint(sizeof(I) * 8);

    static uint clampBitCount(uint bitCount)
    {
        return min(bitCount, kBitsPerLane);
    }

    static uint64_t encode(vector<I, N> point)
    {
        return encode(point, kBitsPerLane);
    }

    static uint64_t encode(vector<I, N> point, uint bitCount)
    {
        uint64_t code = 0;
        uint usedBits = clampBitCount(bitCount);
        for (uint bit = 0; bit < usedBits; ++bit)
        {
            uint axisShift = bit * N;
            [unroll] for (int axis = 0; axis < N; ++axis)
            {
                uint64_t axisValue = point[axis].toUInt();
                uint64_t bitValue = (axisValue >> bit) & 1ull;
                code |= bitValue << (axisShift + axis);
            }
        }
        return code;
    }

    static vector<I, N> decode(uint64_t code)
    {
        return decode(code, kBitsPerLane);
    }

    static vector<I, N> decode(uint64_t code, uint bitCount)
    {
        vector<I, N> point = vector<I, N>(0);
        uint usedBits = clampBitCount(bitCount);
        for (uint bit = 0; bit < usedBits; ++bit)
        {
            uint axisShift = bit * N;
            [unroll] for (int axis = 0; axis < N; ++axis)
            {
                uint64_t bitValue = (code >> (axisShift + axis)) & 1ull;
                if (bitValue != 0)
                {
                    point[axis] = point[axis] | I(1 << bit);
                }
            }
        }
        return point;
    }
}

public struct HilbertCurve<I : IInteger, int N> : IUIntCodec<vector<I, N>, uint64_t>
{
    static const uint kBitsPerLane = uint(sizeof(I) * 8);

    static uint clampBitCount(uint bitCount)
    {
        return min(bitCount, kBitsPerLane);
    }

    static uint64_t encode(vector<I, N> point)
    {
        return encode(point, kBitsPerLane);
    }

    static uint64_t encode(vector<I, N> point, uint bitCount)
    {
        uint64_t code = 0;
        uint usedBits = clampBitCount(bitCount);
        for (uint bit = 0; bit < usedBits; ++bit)
        {
            uint axisShift = bit * N;
            [unroll] for (int axis = 0; axis < N; ++axis)
            {
                uint64_t axisValue = point[axis].toUInt();
                uint64_t bitValue = (axisValue >> bit) & 1ull;
                code |= bitValue << (axisShift + axis);
            }
        }
        return code;
    }

    static vector<I, N> decode(uint64_t code)
    {
        return decode(code, kBitsPerLane);
    }

    static vector<I, N> decode(uint64_t code, uint bitCount)
    {
        vector<I, N> point = vector<I, N>(0);
        uint usedBits = clampBitCount(bitCount);
        for (uint bit = 0; bit < usedBits; ++bit)
        {
            uint axisShift = bit * N;
            [unroll] for (int axis = 0; axis < N; ++axis)
            {
                uint64_t bitValue = (code >> (axisShift + axis)) & 1ull;
                if (bitValue != 0)
                {
                    point[axis] = point[axis] | I(1 << bit);
                }
            }
        }
        return point;
    }
}

public typealias Morton2u = MortonCodec<uint, 2>;
public typealias Morton3u = MortonCodec<uint, 3>;
