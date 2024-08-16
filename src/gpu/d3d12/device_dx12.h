#pragma once
#include "common_dx12.h"

namespace d3d12 {

// Main interface to gpu
// All resources and classes are created through this class
class Device {
public:

    struct Properties {
        int                         adapterIndex;
        DXGI_ADAPTER_DESC1          adapterDesc;
        // Can be used to determine integrated adapters
        // For integrated it should be 0
        uint64_t                    nonLocalMemory;
        D3D_FEATURE_LEVEL           featureLevelMax;
        D3D_SHADER_MODEL            shaderModelMax;

        D3D12_RESOURCE_BINDING_TIER resourceBindingTier;
        D3D12_RESOURCE_HEAP_TIER    resourceHeapTier;
    };

    // Enumerates installed adapters aka GPUs
    static std::vector<Properties> EnumerateAdapters();

    static std::unique_ptr<Device> Create(int adapterIndex,
                                          bool enableDebugLayer);

    ID3D12Device* GetDevice() { return device1_.Get(); }

    const Properties& GetProperties() const { return props_; }

private:
    RefCountedPtr<ID3D12Device> device1_;
    Properties props_;
};


} // namespace d3d12
