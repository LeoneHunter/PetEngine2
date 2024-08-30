#include "texture.h"
#include "device.h"

namespace gpu::d3d12 {

TextureD3D12::TextureD3D12(DeviceD3D12* device,
                           const TextureDesc& desc,
                           RefCountedPtr<ID3D12Resource> res) {
    device_ = device;
    desc_ = desc;
    res_ = res;
}

void TextureD3D12::UpdateState(ID3D12GraphicsCommandList* cmdList,
                               D3D12_RESOURCE_STATES state) {
    const auto newState = state;
    const auto lastState = GetState();
    if (lastState != newState) {
        const auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            GetNative(), lastState, newState);
        cmdList->ResourceBarrier(1, &barrier);
        state_ = newState;
    }
}

void TextureD3D12::CreateSRV(Descriptor slot) {
    DASSERT(false && "Unimplemented");
    auto desc = D3D12_SHADER_RESOURCE_VIEW_DESC{
        .Format = DXGIFormat(desc_.format),
        .ViewDimension = SrvDimension(desc_.dimension),
        .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
    };
    if(desc_.dimension == TextureDimension::Dim1D) {
        desc.Texture1D = D3D12_TEX1D_SRV{
            .MostDetailedMip = 0,
            .MipLevels = (UINT)-1,
        };
    } else if(desc_.dimension == TextureDimension::Dim2D) {

    } else if(desc_.dimension == TextureDimension::Dim3D) {

    }
    device_->GetNative()->CreateShaderResourceView(GetNative(), &desc,
                                                   slot.cpu);
}

TextureD3D12::~TextureD3D12() = default;

}  // namespace gpu::d3d12
