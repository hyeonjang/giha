#ifdef GIHA_SLANG

#include <giha/slang/slang.h>
#include <slang.h>
#include <slang-com-ptr.h>

#include <filesystem>
#include <fstream>
#include <cstdarg>
#include <array>
#include <vector>
#include <iterator>

// utils to process demangling the class name: I want to only remain name of class
std::string diagonize(ISlangBlob* blob) {
    if(!blob) {
        return "No diagnostic blob";
    }
    return (const char*)blob->getBufferPointer();
}

namespace giha {

namespace {
std::filesystem::path project_root() {
    static const std::filesystem::path root =
        std::filesystem::path(__FILE__).parent_path().parent_path();
    return root;
}

std::filesystem::path resolveModulePath(const char* moduleName) {
    using std::filesystem::exists;
    using std::filesystem::path;
    using std::filesystem::canonical;

    path candidate(moduleName);
    if (exists(candidate)) { return canonical(candidate); }

    path relative = project_root() / candidate;
    if (exists(relative)) { return canonical(relative); }

    throw std::runtime_error(
        std::string("Failed to locate Slang module: ") + moduleName);
}

std::string readTextFile(const std::filesystem::path& filePath) {
    std::ifstream file(filePath, std::ios::binary);
    CHECK(file.good(), "Failed to open module file: %s", filePath.string().c_str());

    std::string data((std::istreambuf_iterator<char>(file)),
                     std::istreambuf_iterator<char>());
    return data;
}
} // namespace

inline std::vector<slang::SpecializationArg> getSpecializationArgs(slang::IModule* _module, std::initializer_list<const char*> specializationArgsName) {
    std::vector<slang::SpecializationArg> specializationArgs;
    specializationArgs.reserve(specializationArgs.size());

    for (const auto& name : specializationArgsName) {
        auto typeReflection = _module->getLayout()->findTypeByName(name);
        CHECK(typeReflection != nullptr, "Failed to get type reflection %s\n", name);

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

void buildAndAppendEntryPoints(
    slang::ISession* session, 
    const SlangModuleDesc desc, 
    std::vector<slang::IComponentType*>& componentTypes
) {
    Slang::ComPtr<slang::IBlob> diagnosticBlob;
    slang::IModule* _module = session->loadModule(
        desc.name,
        diagnosticBlob.writeRef()
    );

    if (!_module) {
        auto resolvedPath = resolveModulePath(desc.name);
        auto source = readTextFile(resolvedPath);

        diagnosticBlob.setNull();
        const std::string moduleName = resolvedPath.stem().string();
        _module = session->loadModuleFromSourceString(
            moduleName.c_str(),
            resolvedPath.string().c_str(),
            source.c_str(),
            diagnosticBlob.writeRef()
        );
    }

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
        buildAndAppendEntryPoints(session, desc, componentTypes);
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
