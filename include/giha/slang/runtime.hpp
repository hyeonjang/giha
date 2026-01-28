#pragma once

#ifdef GIHA_SLANG

#include <giha/slang/slang.h>

#include <slang-com-ptr.h>
// #include <slang.h>

#include <initializer_list>
#include <string>
#include <unordered_map>

namespace giha {

class SlangKernelCache {
public:
    SlangKernelCache();
    explicit SlangKernelCache(slang::ISession* session);

    bool valid() const;
    slang::ISession* get() const;

    void reset(slang::ISession* session);

    Slang::ComPtr<slang::IComponentType> get(SlangModuleDesc desc);
    Slang::ComPtr<slang::IComponentType> get(std::initializer_list<SlangModuleDesc> desc);
    Slang::ComPtr<slang::IComponentType> get(slang::ISession* session, SlangModuleDesc desc);
    Slang::ComPtr<slang::IComponentType> get(slang::ISession* session, std::initializer_list<SlangModuleDesc> desc);
    
    std::vector<Slang::ComPtr<slang::IComponentType>> kernels(std::initializer_list<SlangModuleDesc> descs);
    std::vector<Slang::ComPtr<slang::IComponentType>> kernels(slang::ISession* session, std::initializer_list<SlangModuleDesc> descs);

    void clearKernelCache();

private:
    static std::string makeKernelName(std::initializer_list<SlangModuleDesc> modules);

    slang::ISession* defaultSession = nullptr;
    std::unordered_map<std::string, Slang::ComPtr<slang::IComponentType>> _kernels;
};
} // namespace giha

#endif // GIHA_SLANG
