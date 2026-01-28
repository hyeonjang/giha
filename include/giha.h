#ifndef _GIHA_H
#define _GIHA_H

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>

#ifdef _WIN32
#define __FILENAME__ (strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__)
#else 
#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#endif

#ifndef GIHA_LOG_LEVEL
#define GIHA_LOG_LEVEL 3
#endif

#define GIHA_LOG_LEVEL_SILENT 0
#define GIHA_LOG_LEVEL_ERROR  1
#define GIHA_LOG_LEVEL_INFO   2
#define GIHA_LOG_LEVEL_DEBUG  3

#define LOG(level, label, format, ...)                                        \
    do {                                                                      \
        if (GIHA_LOG_LEVEL >= (level)) {                                      \
            std::printf("[giha %s:%d][%s] " format "\n", __FILENAME__,        \
                        __LINE__, label, ##__VA_ARGS__);                      \
        }                                                                     \
    } while (0)

#define LOG_ERROR(format, ...) LOG(GIHA_LOG_LEVEL_ERROR, "error", format, ##__VA_ARGS__)
#define LOG_INFO(format, ...)  LOG(GIHA_LOG_LEVEL_INFO,  "info",  format, ##__VA_ARGS__)
#define LOG_DEBUG(format, ...) LOG(GIHA_LOG_LEVEL_DEBUG, "debug", format, ##__VA_ARGS__)

#define CHECK(condition, format, ...)                                        \
    do {                                                                     \
        if (!(condition)) {                                                  \
            char _giha_error_message[512];                                   \
            std::snprintf(_giha_error_message, sizeof(_giha_error_message),  \
                          format, ##__VA_ARGS__);                            \
            std::printf("[giha %s:%d] %s\n", __FILENAME__, __LINE__,         \
                        _giha_error_message);                                \
            throw std::runtime_error(_giha_error_message);                   \
        }                                                                    \
    } while (0)

namespace giha {

using u8  = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

using i8  = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;

using usize = std::size_t;
using isize = std::ptrdiff_t;

using f32 = float;
using f64 = double;

}

#endif // _GIHA_H
