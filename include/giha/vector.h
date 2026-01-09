#pragma once

#include <cstdint>

template <typename T, size_t N>
struct tvec {
    T data[N];

    T& operator[](size_t i) { return data[i]; }
    const T& operator[](size_t i) const { return data[i]; }

    bool operator==(const tvec<T, N>& other) const {
        for (int i = 0; i < N; i++) {
            if (data[i] != other.data[i]) return false;
        }
        return true;
    }
};

#define DECLARE_TVEC(N)                \
template <typename T>                  \
struct tvec##N : public tvec<T, N> {};

#define DECLARE_TVEC_ALIAS(alias, type, N) \
using alias = tvec##N<type>;

DECLARE_TVEC(2);
DECLARE_TVEC_ALIAS(f32vec2, float, 2);
DECLARE_TVEC_ALIAS(f64vec2, double, 2);
DECLARE_TVEC_ALIAS(i32vec2, std::int32_t, 2);
DECLARE_TVEC_ALIAS(i64vec2, std::int64_t, 2);
DECLARE_TVEC_ALIAS(u32vec2, std::uint32_t, 2);
DECLARE_TVEC_ALIAS(u64vec2, std::uint64_t, 2);
DECLARE_TVEC(3);
DECLARE_TVEC_ALIAS(f32vec3, float, 3);
DECLARE_TVEC_ALIAS(f64vec3, double, 3);
DECLARE_TVEC_ALIAS(i32vec3, std::int32_t, 3);
DECLARE_TVEC_ALIAS(i64vec3, std::int64_t, 3);
DECLARE_TVEC_ALIAS(u32vec3, std::uint32_t, 3);
DECLARE_TVEC_ALIAS(u64vec3, std::uint64_t, 3);
DECLARE_TVEC(4);
DECLARE_TVEC_ALIAS(f32vec4, float, 4);
DECLARE_TVEC_ALIAS(f64vec4, double, 4);
DECLARE_TVEC_ALIAS(i32vec4, std::int32_t, 4);
DECLARE_TVEC_ALIAS(i64vec4, std::int64_t, 4);
DECLARE_TVEC_ALIAS(u32vec4, std::uint32_t, 4);
DECLARE_TVEC_ALIAS(u64vec4, std::uint64_t, 4);

template <typename T>
tvec2<T> minmax(const tvec2<T>& in) {
    return in[0] > in[1] ? tvec2<T> {in[1], in[0]} : tvec2<T> { in[0], in[1] };
}

template <typename T, size_t N>
struct VectorHash {

    size_t operator()(const tvec<T, N>& arr) const noexcept {
        size_t seed = std::hash<T>{}(arr[0]);
        for (int i = 1; i < N; i++) {
            mix(seed, std::hash<T>{}(arr[1]));
        }
        return seed;
    }

private:
    inline void mix(size_t& s, size_t value) const {
        s ^= value + 0x9e3779b9 + (s << 6) + (s >> 2);
    };
};
