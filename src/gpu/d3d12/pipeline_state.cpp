#include "pipeline_state.h"

// D3D12 wrappers and headers
#include "common.h"
#include "descriptor_heap.h"
#include "slab_allocator.h"

namespace gpu::d3d12 {

class PipelineStateD3D12::CommonRootSignature : public RefCountedBase {
public:
    constexpr static int kNumParameters = 4;
    // SP - 14
    constexpr static int kNumCBVs = 8;
    // SP - 128
    constexpr static int kNumSRVs = 8;
    // SP - 16
    constexpr static int kNumUAVs = 8;
    // SP - 16
    constexpr static int kNumSamplers = 8;
    // SP - 0
    constexpr static int kNumStaticSamplers = 4;

    constexpr static int kCBVBaseRegister = 0;
    constexpr static int kSRVBaseRegister = 0;
    constexpr static int kUAVBaseRegister = 0;
    constexpr static int kSamplerBaseRegister = 0;
    constexpr static int kNodeMask = 1;

    // -------------------------------
    // Param    Type        Registers
    // -------------------------------
    // 0        Table       b0-b8
    // 1        Table       t0-t8
    // 2        Table       u0-u8
    // 3        Table       s0-s8
    //          SSampler    s0-s8
    // -------------------------------
    // TODO: Add UAVs and shader visibility
    CommonRootSignature(DeviceD3D12* device) {
        /*====================================================================*/
        // Declare parameters
        /*====================================================================*/
        CD3DX12_ROOT_PARAMETER1 params[kNumParameters];
        // 8 uniforms in a slot 0 with registers b0 - b8
        const auto cbvRange = CD3DX12_DESCRIPTOR_RANGE1(
            D3D12_DESCRIPTOR_RANGE_TYPE_CBV, kNumCBVs, kCBVBaseRegister);
        params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        params[0].DescriptorTable.NumDescriptorRanges = 1;
        params[0].DescriptorTable.pDescriptorRanges = &cbvRange;

        // 8 textures in a slot 1 with registers t0 - t8
        const auto srvRange = CD3DX12_DESCRIPTOR_RANGE1(
            D3D12_DESCRIPTOR_RANGE_TYPE_SRV, kNumSRVs, kSRVBaseRegister);
        params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        params[1].DescriptorTable.NumDescriptorRanges = 1;
        params[1].DescriptorTable.pDescriptorRanges = &srvRange;

        // 8 uavs in a slot 2 with registers u0 - u8
        const auto uavRange = CD3DX12_DESCRIPTOR_RANGE1(
            D3D12_DESCRIPTOR_RANGE_TYPE_UAV, kNumUAVs, kUAVBaseRegister);
        params[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        params[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        params[2].DescriptorTable.NumDescriptorRanges = 1;
        params[2].DescriptorTable.pDescriptorRanges = &uavRange;

        // 8 samplers in a slot 3 with registers s0 - s8
        const auto samplerRange =
            CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER,
                                      kNumSamplers, kSamplerBaseRegister);
        params[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        params[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        params[3].DescriptorTable.NumDescriptorRanges = 1;
        params[3].DescriptorTable.pDescriptorRanges = &samplerRange;

        // 4 tiled samplers s9 - s12
        CD3DX12_STATIC_SAMPLER_DESC samplers[kNumStaticSamplers];
        samplers[0].Init(9, D3D12_FILTER_ANISOTROPIC);
        samplers[1].Init(10, D3D12_FILTER_ANISOTROPIC);
        samplers[2].Init(11, D3D12_FILTER_ANISOTROPIC);
        samplers[3].Init(12, D3D12_FILTER_ANISOTROPIC);

        /*====================================================================*/
        // Init RS
        /*====================================================================*/
        const auto flags = 
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
        const auto desc = CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC(
            kNumParameters, params, kNumStaticSamplers, samplers, flags);

        RefCountedPtr<ID3DBlob> blob;
        RefCountedPtr<ID3DBlob> errors;
        const auto hr =
            D3D12SerializeVersionedRootSignature(&desc, &blob, &errors);
        DASSERT_F(SUCCEEDED(hr), "D3D12: Cannot serialize root signature. {}",
                  std::string_view((char*)errors->GetBufferPointer(),
                                   errors->GetBufferSize()));

        DASSERT_HR(device->GetNative()->CreateRootSignature(
            kNodeMask, blob->GetBufferPointer(), blob->GetBufferSize(),
            IID_PPV_ARGS(&rs_)));
        device_ = device;
    }

    ID3D12RootSignature* GetNative() { return rs_.Get(); }

    BindingLayout GetBindingLayout() const {
        return BindingLayout()
            .PushRange(DescriptorType::CBV, kNumCBVs)
            .PushRange(DescriptorType::SRV, kNumSRVs)
            .PushRange(DescriptorType::UAV, kNumUAVs)
            .PushRange(DescriptorType::Sampler, kNumSamplers);
    }

    PipelineStateD3D12::BindSlot GetBindSlot(const std::string& type,
                                             uint32_t idx) {
        RegisterType regType;
        if (type == "b") {
            regType = RegisterType::CBV;
        } else if (type == "u") {
            regType = RegisterType::UAV;
        } else if (type == "t") {
            regType = RegisterType::SRV;
        } else {
            UNREACHABLE();
        }
        return GetBindSlot(regType, idx);
    }

    // Returns bind slot {param, offset} for the register {type, idx}
    // I.e. {b, 4} -> {0, 4}, {u, 3} -> {2, 3}
    PipelineStateD3D12::BindSlot GetBindSlot(RegisterType type, uint32_t idx) {
        switch (type) {
            case RegisterType::CBV: {
                DASSERT(idx < kNumCBVs);
                return {0, idx};
            }
            case RegisterType::SRV: {
                DASSERT(idx < kNumSRVs);
                return {1, idx};
            }
            case RegisterType::UAV: {
                DASSERT(idx < kNumUAVs);
                return {2, idx};
            }
        }
        UNREACHABLE();
    }

private:
    RefCountedPtr<Device> device_;
    RefCountedPtr<ID3D12RootSignature> rs_;
};
// Shared root signature used by all instances
RefCountedPtr<PipelineStateD3D12::CommonRootSignature> commonRs;


PipelineStateD3D12::PipelineStateD3D12(DeviceD3D12* device,
                                       const PipelineStateDesc& desc)
    : device_(device), inputLayout_(desc.layout) {
    // - Create a common RS for all shaders
    if (!commonRs) {
        commonRs = new CommonRootSignature(device);
    }
    rootSig_ = commonRs;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
    psoDesc.NodeMask = 1;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.pRootSignature = commonRs->GetNative();
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.NumRenderTargets = 1;
    psoDesc.SampleDesc.Count = 1;
    psoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

    // Copy render targets
    for (int i = 0;
         i < std::size(psoDesc.RTVFormats) && i < std::size(desc.renderTargets);
         ++i) {
        psoDesc.RTVFormats[i] = DXGIFormat(desc.renderTargets[i]);
    }

    // Create the input layout
    std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayout;
    for (InputLayout::Element elem : desc.layout) {
        inputLayout.push_back(D3D12_INPUT_ELEMENT_DESC{
            .SemanticName = SemanticString(elem.semanticName),
            .Format = DXGIFormat(elem.format),
            .AlignedByteOffset = elem.alignedByteOffset,
            .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
        });
    }
    psoDesc.InputLayout = {inputLayout.data(), (UINT)inputLayout.size()};

    const auto& vertexShader = desc.vertexShader.data;
    psoDesc.VS = {vertexShader.data(), vertexShader.size()};

    const auto& pixelShader = desc.pixelShader.data;
    psoDesc.PS = {pixelShader.data(), pixelShader.size()};

    psoDesc.BlendState = CD3DX12_BLEND_DESC(CD3DX12_DEFAULT());
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(CD3DX12_DEFAULT());
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(CD3DX12_DEFAULT());

    const auto err = device_->GetNative()->CreateGraphicsPipelineState(
        &psoDesc, IID_PPV_ARGS(&pso_));
    DASSERT_HR(err);

    // Collect uniforms
    // TODO: Remap bind indices
    for (const ShaderCode::Uniform& uniform : desc.vertexShader.uniforms) {
        uniforms_.push_back(uniform);
    }
}

PipelineStateD3D12::~PipelineStateD3D12() {
    rootSig_ = nullptr;
    // Delete if we are the last user
    if (commonRs->GetRefCount() == 1) {
        commonRs = nullptr;
    }
}

BindingLayout PipelineStateD3D12::GetBindingLayout() {
    return commonRs->GetBindingLayout();
}

PipelineStateD3D12::BindSlot PipelineStateD3D12::GetBindSlot(
    BindIndex bindIdx) {
    // Find the register
    for (const Uniform& uniform : uniforms_) {
        if (uniform.binding == bindIdx) {
            return commonRs->GetBindSlot(uniform.location.type,
                                         uniform.location.index);
        }
    }
    DASSERT_F(false, "Invalid bind index");
}

ID3D12RootSignature* PipelineStateD3D12::GetNativeRS() {
    return rootSig_->GetNative();
}

uint32_t PipelineStateD3D12::GetVertexBufferStride() {
    return inputLayout_.GetStride();
}

}  // namespace gpu::d3d12