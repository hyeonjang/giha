#pragma once

#include "slang-type.h"

#include <giha/slang/slang.h>

#include <slang.h>
#include <slang-com-ptr.h>
#include <slang-rhi.h>
#include <slang-rhi/shader-cursor.h>

#include <filesystem>
#include <string>
#include <unordered_map>
#include <utility>

// Utilities for constructing Slang sessions and lazily creating gfx devices for tests.
namespace giha {

class SlangKernel {
public:
    SlangKernel() = default;
    SlangKernel(std::string _name, Slang::ComPtr<slang::IComponentType> _kernel)
    : name(std::move(_name)), kernel(std::move(_kernel)) {}

public:
    std::string name;
    Slang::ComPtr<slang::IComponentType> kernel;
};

class SlangSession {
public:
    SlangSession() = default;
    SlangSession(slang::ISession* _session): session(_session) {}

    SlangKernel& addKernel(giha::SlangModuleDesc desc) {
        return addKernel({desc});
    }

    SlangKernel& addKernel(std::initializer_list<giha::SlangModuleDesc> desc) {

        ensure_session();

        // kernel name
        auto kernelName = makeKernelName(desc);
        if (kernels.find(kernelName) != kernels.end()) { return kernels[kernelName]; }

        //
        // create IComponentType by kernel
        //
        Slang::ComPtr<slang::IComponentType> kernel;
        giha::kernel(session.get(), desc, kernel.writeRef());
        kernels.emplace(kernelName, SlangKernel(kernelName, std::move(kernel)));
        return kernels[kernelName];
    }

private:
    std::string makeKernelName(std::initializer_list<SlangModuleDesc> modules) {
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

public:
    Slang::ComPtr<slang::ISession> session;
    std::unordered_map<std::string, SlangKernel> kernels;

private:
    static std::filesystem::path project_root() {
        static const std::filesystem::path root =
            std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
        return root;
    }

    std::string resolve_kernel_module(const std::string& module) const {
        auto candidate = project_root() / "module" / module;
        auto path_string = candidate.string();
        auto error_message = std::string("Slang kernel module not found: ") + path_string;
        CHECK(std::filesystem::exists(candidate), error_message.c_str());

        return path_string;
    }

    void ensure_session() const {
        CHECK(session.get() != nullptr, "Slang session is not initialized");
    }
};

class SlangManager {
public:
    static SlangManager& Instance() {
        static SlangManager instance;
        return instance;
    }

    SlangManager(const SlangManager&) = delete;
    SlangManager& operator=(const SlangManager&) = delete;

    Slang::ComPtr<slang::IGlobalSession> GlobalSession() {
        return globalSession;
    }

    SlangSession& create_session(const std::string& name, const slang::SessionDesc& sessionDesc) {

        slang::ISession* session = nullptr;
        CHECK_SLANG(
            globalSession->createSession(sessionDesc, &session),
            "Failed to create session"
        );

        sessions[name] = SlangSession(session);
        return sessions[name];
    }

    SlangSession& session(const char* name) {
        return sessions[name];
    }

    SlangSession& only_host_callable_session() {
        if (sessions.find("host_callable") == sessions.end()) {

            slang::SessionDesc sessionDesc = {};
            slang::TargetDesc targetDesc = {};
            targetDesc.format = SlangCompileTarget::SLANG_SHADER_HOST_CALLABLE;
            targetDesc.flags = SLANG_TARGET_FLAG_GENERATE_WHOLE_PROGRAM;
            sessionDesc.targets = &targetDesc;
            sessionDesc.targetCount = 1;

            return create_session("host_callable", sessionDesc);
        }
        return sessions["host_callable"];
    }

    SlangSession& only_spirv_session() {
        if (sessions.find("spriv") == sessions.end()) {

            slang::SessionDesc desc = {};
            slang::TargetDesc target_desc[1];
            {
                target_desc[0].format = SlangCompileTarget::SLANG_SPIRV;
                target_desc[0].profile = globalSession->findProfile("spriv_1_6");
                target_desc[0].flags = SLANG_TARGET_UNKNOWN;
            }

            desc.targets = &target_desc[0];
            desc.targetCount = 1;

            return create_session("spirv", desc);
        }
        return sessions["spriv"];
    }

private:
    SlangManager() {
        CHECK_SLANG(
            slang::createGlobalSession(globalSession.writeRef()),
            "Failed to create global session"
        );
    }

    Slang::ComPtr<slang::IGlobalSession> globalSession;
    std::unordered_map<std::string, SlangSession> sessions;
};

namespace slangrhi {
// 
class ComputeKernel {
public:
    ComputeKernel(rhi::IDevice* device, SlangKernel kernel) {

        Slang::ComPtr<slang::IBlob> diagnosticBlob;
        program = device->createShaderProgram(kernel.kernel, diagnosticBlob.writeRef());
        CHECK(program != nullptr, diagonize(diagnosticBlob).c_str());

        rhi::ComputePipelineDesc desc;
        desc.program = program.get();

        pipeline = device->createComputePipeline(desc);
    };

    Slang::ComPtr<rhi::IShaderProgram>   program;
    Slang::ComPtr<rhi::IComputePipeline> pipeline;
};

inline std::vector<ComputeKernel> makeComputeKernels(
    rhi::IDevice* device,
    SlangSession& session,
    std::initializer_list<SlangModuleDesc> kernelsDesc
) {
    std::vector<ComputeKernel> kernels;
    kernels.reserve(kernelsDesc.size());
    for (auto desc : kernelsDesc) {
        kernels.emplace_back(device, session.addKernel(desc));
    }
    return kernels;
}

class EntryBinder {
public:

    struct Buff { const char* path; Slang::ComPtr<rhi::IBuffer> buffer; };
    struct Data { const char* path; std::vector<u8> data; };

    EntryBinder() {}

    EntryBinder& bind(const char* path, Slang::ComPtr<rhi::IBuffer> buffer) {
        _buffs.push_back({ path, buffer });
        return *this;
    }

    template <typename T>
    EntryBinder& bind(const char* path, const T& value) {

        Data data;
        data.path = path;
        data.data.resize(sizeof(T));
        std::memcpy(data.data.data(), &value, sizeof(T));

        _datas.push_back(std::move(data));
        return *this;
    }

    void operator()(rhi::ShaderCursor cursor) const {
        for (auto& b : _buffs) cursor.getPath(b.path).setBinding(b.buffer);
        for (auto& d : _datas) cursor.getPath(d.path).setData(d.data.data(), d.data.size());
    }

// private:
    std::vector<Buff> _buffs;
    std::vector<Data> _datas;
};

class Dispatcher {
public:
    explicit Dispatcher(rhi::IDevice* device): _device(device) {}

    Dispatcher& setGlobals(EntryBinder bindGlobal) {
        _globalBinder = std::move(bindGlobal);
        _hasGlobalBinder = true;
        return *this;
    }

    void begin(rhi::QueueType queueType = rhi::QueueType::Graphics) {
        CHECK(!_recording, "Dispatcher already recording");
        _queue = _device->getQueue(queueType);
        CHECK(_queue != nullptr, "Failed to get queue");
        CHECK_SLANG(_queue->createCommandEncoder(_encoder.writeRef()), "Failed to create command encoder");
        _computePass = _encoder->beginComputePass();
        CHECK(_computePass != nullptr, "Failed to begin compute pass");
        _rootObject = nullptr;
        _recording = true;
    }

    void bindKernel(const ComputeKernel& kernel) {
        const EntryBinder* binder = _hasGlobalBinder ? &_globalBinder : nullptr;
        bindKernelImpl(&kernel, binder);
    }

    void bindKernel(const ComputeKernel& kernel, const EntryBinder& globals) {
        bindKernelImpl(&kernel, &globals);
    }

    void dispatchEntry(u32 entryIndex, EntryBinder bindEntry, usize gx, usize gy = 1, usize gz = 1) {
        CHECK(_recording, "Call begin() before dispatchEntry()");
        CHECK(_rootObject != nullptr, "Call bindKernel() before dispatchEntry()");
        rhi::ShaderCursor entry(_rootObject->getEntryPoint(entryIndex));
        bindEntry(entry);
        _computePass->dispatchCompute(gx, gy, gz);
    }

    void end() {
        CHECK(_recording, "Call begin() before end()");

        _computePass->end();

        auto commandBuffer = _encoder->finish();
        CHECK_SLANG(_queue->submit(commandBuffer), "Failed to submit command buffer");

        _encoder = nullptr;
        _computePass = nullptr;
        _rootObject = nullptr;
        _queue = nullptr;
        _recording = false;
    }

private:
    void bindKernelImpl(const ComputeKernel* kernel, const EntryBinder* globals) {
        CHECK(_recording, "Call begin() before bindKernel()");
        CHECK(kernel != nullptr, "Invalid kernel");
        _rootObject = _computePass->bindPipeline(kernel->pipeline.get());
        CHECK(_rootObject != nullptr, "Failed to bind compute pipeline");
        if (globals) {
            (*globals)(rhi::ShaderCursor(_rootObject));
        }
    }

    rhi::IDevice* _device = nullptr;
    rhi::ICommandQueue* _queue = nullptr;
    Slang::ComPtr<rhi::ICommandEncoder> _encoder;
    Slang::ComPtr<rhi::IComputePassEncoder> _computePass;
    Slang::ComPtr<rhi::IShaderObject> _rootObject;
    EntryBinder _globalBinder;
    bool _hasGlobalBinder = false;
    bool _recording = false;
};

} // namespace slangrhi
} // namespace giha
