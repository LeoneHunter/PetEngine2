#pragma once
#include "common.h"
#include "gpu/gpu.h"

#include <span>

namespace gpu::d3d12 {

class DeviceD3D12;

// Shader binding layout
// Describes how parameters are passed to the shader in PSO
class BindingLayout {
public:
    struct DescriptorRange {
        // Root signature parameter index
        uint32_t parameterIndex;
        DescriptorType type;
        uint32_t count;
    };

    BindingLayout& PushRange(DescriptorType type, uint32_t count) {
        ranges.push_back(DescriptorRange{(uint32_t)ranges.size(), type, count});
        return *this;
    };

    std::vector<DescriptorRange> ranges;
};

class PipelineStateD3D12 final : public PipelineState {
public:
    PipelineStateD3D12(DeviceD3D12* device, const PipelineStateDesc& desc);
    ~PipelineStateD3D12();

    BindingLayout GetBindingLayout();

    // A slot inside a descriptor range
    struct BindSlot {
        uint32_t paramIdx;
        uint32_t rangeIdx;
    };
    BindSlot GetBindSlot(BindIndex bindIdx);

    ID3D12PipelineState* GetNative() { return pso_.Get(); }
    ID3D12RootSignature* GetNativeRS();

    uint32_t GetVertexBufferStride();

    class CommonRootSignature;
    using Uniform = ShaderCode::Uniform;

private:
    RefCountedPtr<CommonRootSignature> rootSig_;
    RefCountedPtr<ID3D12PipelineState> pso_;
    RefCountedPtr<DeviceD3D12> device_;
    // Combined shader uniforms
    std::vector<Uniform> uniforms_;
    InputLayout inputLayout_;
};

inline PipelineStateD3D12* CastPipelineState(gpu::PipelineState* ps) {
    return static_cast<PipelineStateD3D12*>(ps);
}

}  // namespace gpu::d3d12