#pragma once
#include "gpu/gpu.h"
#include "gpu/d3d12/common.h"

namespace gpu::d3d12 {

class DeviceD3D12;

class TextureD3D12 final : public gpu::Texture {
public:
    TextureD3D12(DeviceD3D12* device,
                 const TextureDesc& desc,
                 RefCountedPtr<ID3D12Resource> res);
    ~TextureD3D12();

    D3D12_RESOURCE_STATES GetState() const { return state_; }

    void UpdateState(ID3D12GraphicsCommandList* cmdList,
                         D3D12_RESOURCE_STATES state);

    ID3D12Resource* GetNative() { return res_.Get(); }

    void CreateSRV(Descriptor slot);

public:
    uint32_t GetWidth() const override { return desc_.width; }
    uint32_t GetHeight() const override { return desc_.height; }

private:
    RefCountedPtr<DeviceD3D12> device_;
    RefCountedPtr<ID3D12Resource> res_;
    TextureDesc desc_;
    D3D12_RESOURCE_STATES state_ = D3D12_RESOURCE_STATE_COMMON;
};

constexpr TextureD3D12* CastTexture(gpu::Texture* tex) {
    return static_cast<TextureD3D12*>(tex);
}

}  // namespace gpu::d3d12