#ifdef GIHA_SLANG

#include <giha/slang/slang.h>
#include <slang.h>
#include <slang-com-ptr.h>

#include <filesystem>
#include <cstdarg>
#include <array>
#include <vector>

// utils to process demangling the class name: I want to only remain name of class
std::string diagonize(ISlangBlob* blob) {
    if(!blob) {
        return "No diagnostic blob";
    }
    return (const char*)blob->getBufferPointer();
}

namespace giha {

inline std::vector<slang::SpecializationArg> getSpecializationArgs(slang::IModule* _module, std::initializer_list<const char*> specializationArgsName) {
    std::vector<slang::SpecializationArg> specializationArgs;
    specializationArgs.reserve(specializationArgs.size());

    for (const auto& name : specializationArgsName) {
        auto typeReflection = _module->getLayout()->findTypeByName(name);
        CHECK(typeReflection != nullptr, "Failed to get type reflection");

        specializationArgs.push_back(slang::SpecializationArg::fromType(typeReflection));
    }

    return specializationArgs;
}

inline std::vector<slang::IEntryPoint*> getEntryPoints(slang::IModule* _module, std::initializer_list<const char*> entryPointNames) {
    std::vector<slang::IEntryPoint*> entryPoints;
    entryPoints.reserve(entryPointNames.size());  // Preallocate vector size

    // Fold expression to unpack arguments and find entry points
    for (const auto& name : entryPointNames) {
        slang::IEntryPoint* entryPoint = nullptr;
        CHECK_SLANG(_module->findEntryPointByName(name, &entryPoint),
                    "Failed to find entry point: " + std::string(name));
        entryPoints.push_back(entryPoint);
    }
    return entryPoints;
}

void buildEntryPoints(
    slang::ISession* session, 
    const SlangModuleDesc desc, 
    std::vector<slang::IComponentType*>& componentTypes
) {
    
    LOG_DEBUG("Loading module %s", desc.name);

    Slang::ComPtr<slang::IBlob> diagnosticBlob;
    slang::IModule* _module = session->loadModule(
        desc.name,
        diagnosticBlob.writeRef()
    );

    CHECK(_module, diagonize(diagnosticBlob).c_str());
    componentTypes.push_back(_module);

    componentTypes.reserve(desc.entryPointDescs.size());
    for (const auto& entryPointDesc : desc.entryPointDescs) {

        // speicalize
        slang::IEntryPoint* entryPoint = nullptr;
        CHECK_SLANG(
            _module->findEntryPointByName(entryPointDesc.name, &entryPoint), 
            "Failed to find EntryPoint: " + std::string(entryPointDesc.name)
        );

        auto specializationArgs = getSpecializationArgs(_module, entryPointDesc.specializationArgs);
        slang::IComponentType* specialized = nullptr;
        CHECK_SLANG(
            entryPoint->specialize(
                specializationArgs.data(), specializationArgs.size(),
                &specialized, diagnosticBlob.writeRef()
            ),
            diagonize(diagnosticBlob).c_str()
        );
        componentTypes.push_back(specialized);
    }
}

void kernel(
    slang::ISession* session,
    SlangModuleDesc desc,
    slang::IComponentType** _kernel
) {
    return kernel(session, {desc}, _kernel);
}

void kernel(
    slang::ISession* session,
    std::initializer_list<SlangModuleDesc> descs,
    slang::IComponentType** kernel
) {

    std::vector<slang::IComponentType*> componentTypes;
    for (const auto& desc : descs) {
        buildEntryPoints(session, desc, componentTypes);
    }

    Slang::ComPtr<slang::IBlob> diagnosticBlob;
    Slang::ComPtr<slang::IComponentType> composition;
    CHECK_SLANG(
        session->createCompositeComponentType(
            componentTypes.data(), componentTypes.size(),
            composition.writeRef(), diagnosticBlob.writeRef()
        ),
        diagonize(diagnosticBlob).c_str()
    );

    CHECK_SLANG(
        composition->link(kernel, diagnosticBlob.writeRef()),
        diagonize(diagnosticBlob).c_str()
    );
}
} // namespace giha
#endif // GIHA_SLANG