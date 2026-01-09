#ifndef _GIHA_SLANG_H
#define _GIHA_SLANG_H

#ifdef GIHA_SLANG

#include <giha.h>
#include <stdexcept>

#define CHECK_SLANG(res, errorMessage) \
    if(res < 0) { \
        printf("[giha] %s %d %s", __FILENAME__, __LINE__, errorMessage); \
        throw std::runtime_error(errorMessage); \
    }

// 
namespace slang {
class ISession;
class IModule;
class IEntryPoint;
class IComponentType;
class SpecializationArg;
} // namespace slang
class ISlangBlob;

namespace Slang {
template <typename T> class ComPtr;
}
std::string diagonize(ISlangBlob* blob);

namespace giha {

struct SlangEntryPoint {
    const char* name;
    std::initializer_list<const char*> specializationArgs;
};

struct SlangModuleDesc {
    const char* name;
    std::initializer_list<SlangEntryPoint> entryPointDescs;
};

void kernel(
    slang::ISession* session,
    SlangModuleDesc desc,
    slang::IComponentType** kernel
);

void kernel(
    slang::ISession* session,
    std::initializer_list<SlangModuleDesc> pack,
    slang::IComponentType** kernel
);

} // namespace giha
// #endif
#endif // GIHA_SLANG
#endif // _GIHA_SLANG_H
