#pragma once
#include "gpu/d3d12/common.h"
#include "gpu/gpu.h"

namespace gpu::d3d12 {

class SlabAllocator;
class HeapAllocator;
class DeviceD3D12;
class SwapChainD3D12;
class ContextD3D12;

// Main device properties and features
struct AdapterProps {
    int adapterIndex;
    DXGI_ADAPTER_DESC1 adapterDesc;
    // Can be used to determine integrated adapters
    // For integrated it should be 0
    uint64_t nonLocalMemory;
    D3D_FEATURE_LEVEL featureLevelMax;
    D3D_SHADER_MODEL shaderModelMax;

    D3D12_RESOURCE_BINDING_TIER resourceBindingTier;
    D3D12_RESOURCE_HEAP_TIER resourceHeapTier;
};

struct DeviceDescription {
    std::string descriptionString;
    uint32_t vendorID;
    uint32_t deviceID;
    uint32_t revision;
    size_t dedicatedMemory;
    size_t systemMemory;
    size_t shaderSystemMemory;
    AdapterProps adapterProps;
};

// Enumerates and creates devices objects
class DeviceFactory {
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

    // Enumerates installed adapters aka GPUs
    static std::vector<AdapterProps> EnumerateAdapters();

    static gpu::ExpectedRef<DeviceD3D12> CreateDevice(int adapterIndex,
                                                      bool enableDebugLayer);
};

// Main interface to gpu
// All resources and classes are created through this class
class DeviceD3D12 final : public Device {
public:
    std::unique_ptr<ShaderCodeGenerator> CreateShaderCodeGenerator() override;

    ExpectedRef<PipelineState> CreatePipelineState(
        const PipelineStateDesc& desc) override;

    RefCountedPtr<CommandContext> CreateCommandContext() override;

    ExpectedRef<Buffer> CreateBuffer(uint32_t size, BufferUsage usage) override;
    ExpectedRef<Texture> CreateTexture(const TextureDesc& desc) override;

    ShaderCompileResult CompileShader(const std::string& main,
                                      const std::string& code,
                                      ShaderUsage type,
                                      bool debugBuild) override;

    RefCountedPtr<SwapChain> CreateSwapChainForWindow(
        void* window,
        uint32_t width,
        uint32_t height,
        uint32_t numFrames) override;
    
    void Submit(RefCountedPtr<CommandContext> cmdCtx) override;

    void WaitUntilIdle() override;

public:
    ID3D12Device* GetNative() { return device_.Get(); }

    struct AllocationResult {
        RefCountedPtr<ID3D12Resource> res;
        ResourceHeapAllocation allocation;
    };
    AllocationResult AllocateResource(uint64_t sizeBytes,
                                      D3D12_HEAP_TYPE heapType);
    void FreeResource(ResourceHeapAllocation addr, ID3D12Resource* res);

private:
    DeviceD3D12(RefCountedPtr<ID3D12Device> device);
    friend DeviceFactory;

    void CreateAllocators();

private:
    std::unique_ptr<HeapAllocator> heapAlloc_;
    // Default (shader visible) memory allocator
    std::unique_ptr<SlabAllocator> defaultAlloc_;
    RefCountedPtr<ID3D12CommandQueue> cmdQueue_;
    RefCountedPtr<ID3D12Device> device_;
    // Notified from GPU when done executing CommandContext
    RefCountedPtr<ID3D12Fence> fence_;
    uint64_t lastFenceValue_ = 0;

    struct PendingContext {
        // Value of the fence when GPU is done
        uint64_t fenceValue;
        RefCountedPtr<ContextD3D12> commandContext;
    };
    std::vector<PendingContext> pendingContexts_;

    uint32_t currentFrameIndex_ = 0;
    // Client state
    RefCountedPtr<SwapChainD3D12> currentSwapChain_;
};

constexpr DeviceD3D12* CastDevice(Device* device) {
    return static_cast<DeviceD3D12*>(device);
}

}  // namespace gpu::d3d12
