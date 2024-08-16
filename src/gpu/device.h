#pragma once
#include "base/common.h"

namespace d3d12 {
class Device;
}  // namespace d3d12

// Lowest level interface to the GPU
class GPUDevice {
public:
    enum class CreateFlags {
        None = 0x0,
        PreferHighestVRAM = 0x1,
        PreferDedicated = 0x2,
        PreferIntegrated = 0x4,
        WithDebugLayer = 0x8,
        // Return error if no adapters found
        // matching all requested flags
        Strict = 0x10,
    };

    // Description of the device
    struct Description {
        std::string descriptionString;
        uint32_t vendorID;
        uint32_t deviceID;
        uint32_t revision;
        size_t dedicatedMemory;
        size_t systemMemory;
        size_t shaderSystemMemory;
    };

    // Create interface to the GPU
    static std::unique_ptr<GPUDevice> Create(CreateFlags flags);

private:
    Description description_;
    std::unique_ptr<d3d12::Device> device_;
};