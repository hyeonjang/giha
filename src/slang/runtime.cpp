#include <giha/slang/runtime.hpp>

#ifdef GIHA_SLANG

#include <utility>

namespace giha {

SlangKernelCache::SlangKernelCache() = default;

SlangKernelCache::SlangKernelCache(slang::ISession* session) {
    reset(session);
}

bool SlangKernelCache::valid() const { return defaultSession != nullptr; }

slang::ISession* SlangKernelCache::get() const { return defaultSession; }

void SlangKernelCache::reset(slang::ISession* session) {
    defaultSession = session;
    clearKernelCache();
}

Slang::ComPtr<slang::IComponentType> SlangKernelCache::get(SlangModuleDesc desc) { return get({desc}); }
Slang::ComPtr<slang::IComponentType> SlangKernelCache::get(std::initializer_list<SlangModuleDesc> desc) { return get(nullptr, desc); }
Slang::ComPtr<slang::IComponentType> SlangKernelCache::get(slang::ISession* session, SlangModuleDesc desc) {
    return get(session, {desc});
}

Slang::ComPtr<slang::IComponentType> SlangKernelCache::get(
    slang::ISession* session,
    std::initializer_list<SlangModuleDesc> desc
) {
    slang::ISession* resolvedSession = session ? session : defaultSession;
    CHECK(resolvedSession != nullptr, "Slang session is not provided");

    auto name = makeKernelName(desc);
    auto it = _kernels.find(name);
    if (it != _kernels.end()) { return it->second; }

    Slang::ComPtr<slang::IComponentType> kernel;
    giha::kernel(resolvedSession, desc, kernel.writeRef());
    auto [inserted, _] = _kernels.emplace(name, std::move(kernel));
    return inserted->second;
}

std::vector<Slang::ComPtr<slang::IComponentType>> SlangKernelCache::kernels(
    std::initializer_list<SlangModuleDesc> descs
) {
    return kernels(nullptr, descs);
}

std::vector<Slang::ComPtr<slang::IComponentType>> SlangKernelCache::kernels(
    slang::ISession* session,
    std::initializer_list<SlangModuleDesc> descs
) {
    std::vector<Slang::ComPtr<slang::IComponentType>> result;
    result.reserve(descs.size());
    for (const auto& desc : descs) {
        result.push_back(get(session, desc));
    }
    return result;
}

void SlangKernelCache::clearKernelCache() { _kernels.clear(); }
std::string SlangKernelCache::makeKernelName(std::initializer_list<SlangModuleDesc> modules) {
    std::string name;
    bool firstModule = true;
    for (const auto& module : modules) {
        if (!firstModule) name += "|";
        firstModule = false;
        name += module.name ? module.name : "<null>";
        name += ":";

        bool firstEntry = true;
        for (const auto& entry : module.entryPointDescs) {
            if (!firstEntry) name += ",";
            firstEntry = false;
            name += entry.name ? entry.name : "<null>";

            if (entry.specializationArgs.begin() != entry.specializationArgs.end()) {
                name += "<";
                bool firstArg = true;
                for (const char* arg : entry.specializationArgs) {
                    if (!firstArg) name += "+";
                    firstArg = false;
                    name += arg ? arg : "<null>";
                }
                name += ">";
            }
        }
    }
    return name;
}

} // namespace giha

#endif // GIHA_SLANG
