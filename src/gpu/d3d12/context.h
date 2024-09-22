#pragma once
#include "gpu/d3d12/common.h"
#include "gpu/gpu.h"

namespace gfx {
class Rect;
}

namespace gpu::d3d12 {

class DeviceD3D12;
class BufferD3D12;
class TextureD3D12;
class PipelineStateD3D12;

// TODO: 
// - Batch barriers
// - Use separate thread for work submission to gpu
// - Use multiple render targets
class ContextD3D12 final: public CommandContext {
public:
    // 4 MiB
    constexpr static int kUploadHeapSize = kResourceMaxAlignment;
    constexpr static int kCbvSrvUavDescriptorHeapSize = 256;
    constexpr static int kRtvDescriptorHeapSize = 256;
    constexpr static int kSamplerDescriptorHeapSize = 64;

    enum class State {
        // Ready to record commands
        Recording,
        // Used by GPU
        Pending,
    };

public:
    ContextD3D12(DeviceD3D12* device);
    ~ContextD3D12();

    void Flush(ID3D12CommandQueue* cmdQueue);
    void Reset();

public:
    Device* GetParentDevice() override;

    void WriteBuffer(Buffer* buf, std::span<const uint8_t> data) override;
    void WriteTexture(Texture* tex, std::span<const uint8_t> data) override;

    void SetRenderTarget(Texture* tex) override;

    void SetPipelineState(PipelineState* ps) override;
    void SetClipRect(const gfx::Rect& rect) override;

    void SetVertexBuffer(Buffer* buf, uint32_t sizeBytes) override;

    void SetIndexBuffer(Buffer* buf,
                        IndexFormat format,
                        uint32_t sizeBytes) override;

    void SetUniform(BindIndex bindIndex, Buffer* buf) override;
    void SetUniform(BindIndex bindIndex, Texture* tex) override;

    void DrawIndexed(uint32_t numIndices,
                     uint32_t indexOffset,
                     uint32_t vertexOffset) override;

private:
    void SetDefaultState();

    class DescriptorArenaAlloc;
    DescriptorArenaAlloc* GetDescriptorHeapAlloc(DescriptorType type);

private:
    class UploadHeap;
    class BindingLayout;
    // Rendering context
    RefCountedPtr<DeviceD3D12> device_;
    RefCountedPtr<ID3D12CommandAllocator> cmdAlloc_;
    RefCountedPtr<ID3D12GraphicsCommandList> cmdList_;
    std::unique_ptr<UploadHeap> uploadHeap_;
    // Only CPU visible
    std::unique_ptr<DescriptorArenaAlloc> descriptorArenaRtv_;
    // CPU and GPU visible
    std::unique_ptr<DescriptorArenaAlloc> descriptorArenaCbvSrvUav_;
    std::unique_ptr<DescriptorArenaAlloc> descriptorArenaSampler_;
    RefCountedPtr<Texture> renderTarget_;
    Descriptor renderTargetDescriptor_;
    State state_ = State::Recording;
    // Client state
    RefCountedPtr<PipelineStateD3D12> currentPSO_;
    std::unique_ptr<BindingLayout> currentBindings_;
};

constexpr ContextD3D12* CastCommandContext(CommandContext* ctx) {
    return static_cast<ContextD3D12*>(ctx);
}

}  // namespace gpu::d3d12