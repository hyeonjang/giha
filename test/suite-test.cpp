#include "suite-test.hpp"



#include <slang-rhi.h>

extern const std::string gFilePath = __FILE__;
extern const std::string gTestPath = gFilePath.substr(0, gFilePath.rfind("\\"));

extern TestRegistry GeometrySuite;
extern TestRegistry UtilsSuite;

Slang::ComPtr<rhi::IDevice> gDevice;

int main() {

    // 
    rhi::DeviceDesc deviceDesc;
// #ifdef __VULKAN__
    deviceDesc.slang.targetProfile = "spirv_1_6";
    deviceDesc.deviceType = rhi::DeviceType::Vulkan;
    static const slang::PreprocessorMacroDesc defines[] = { 
        { "__SLANG_APPLE__", "0" },
        { "WORKGROUP_SIZE", STR(WORKGROUP_SIZE) },
        { "SUBGROUP_SIZE", STR(SUBGROUP_SIZE) }
        // {"Index", "int"},
        // {"Key", "int"}
    };
// #endif
    deviceDesc.slang.preprocessorMacros = defines;
    deviceDesc.slang.preprocessorMacroCount = 1;

    gDevice = rhi::getRHI()->createDevice(deviceDesc);

    // if (RUN_SUITE(GeometrySuite) != EXIT_SUCCESS) return EXIT_FAILURE;
    if (RUN_SUITE(UtilsSuite) != EXIT_SUCCESS) return EXIT_FAILURE;

    std::cout << "All suites passed.\n";
    return EXIT_SUCCESS;
}
