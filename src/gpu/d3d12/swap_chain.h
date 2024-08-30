#pragma once
#include "common.h"
#include "gpu/gpu.h"

#include <span>

namespace gpu::d3d12 {

class DeviceD3D12;
class ContextD3D12;

constexpr unsigned kMaxNumFrames = 3;

class SwapChainD3D12 final : public SwapChain {
public:
    SwapChainD3D12(DeviceD3D12* device,
                   ID3D12CommandQueue* cmdQueue,
                   void* window,
                   uint32_t width,
                   uint32_t height,
                   uint32_t numFrames);
    ~SwapChainD3D12();

    uint32_t GetCurrentFrameIndex() const { return currentFrameIdx_; }

public:
    TextureFormat GetFormat() const override { return format_; }
    RefCountedPtr<Texture> GetCurrentBackBufferRenderTarget() override;
    void Present() override;

private:
    RefCountedPtr<DeviceD3D12> device_;
    RefCountedPtr<ID3D12CommandQueue> cmdQueue_;
    RefCountedPtr<IDXGISwapChain3> swapChain_;
    uint32_t currentFrameIdx_;
    TextureFormat format_;
};

inline SwapChainD3D12* CastSwapChain(gpu::SwapChain* sc) {
    return static_cast<SwapChainD3D12*>(sc);
}

}  // namespace gpu::d3d12