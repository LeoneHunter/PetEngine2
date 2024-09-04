#include "context.h"

#include "buffer.h"
#include "device.h"
#include "pipeline_state.h"
#include "slab_allocator.h"
#include "texture.h"

#include "base/bench.h"
#include "gfx/common.h"

namespace gpu::d3d12 {

// CPU visible video memory
class ContextD3D12::UploadHeap {
public:
    UploadHeap(ID3D12Device* device, uint32_t size)
        : size_(size), usedBytes_(0) {
        const auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        const auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(size);
        auto hr = device->CreateCommittedResource(
            &heapProps, D3D12_HEAP_FLAG_NONE, &bufferDesc,
            D3D12_RESOURCE_STATE_COPY_SOURCE, nullptr, IID_PPV_ARGS(&res_));
        DASSERT_HR(hr);
        // TODO: Should we clear the buffer?
    }

    // Returns offset in bytes
    uint32_t AppendData(std::span<const uint8_t> data) {
        DASSERT(data.size() <= std::numeric_limits<uint32_t>::max());
        uint32_t offset = usedBytes_;
        usedBytes_ += (uint32_t)data.size();
        DASSERT(usedBytes_ <= size_);

        uint8_t* mapped = nullptr;
        const auto readRange = CD3DX12_RANGE(0, 0);
        const auto hr =
            res_->Map(0, &readRange, reinterpret_cast<void**>(&mapped));
        DASSERT_HR(hr);
        memcpy(mapped + offset, data.data(), data.size());
        res_->Unmap(0, nullptr);
        return offset;
    }

    ID3D12Resource* GetResource() { return res_.Get(); }

    void Clear() { usedBytes_ = 0; }

private:
    const uint32_t size_;
    uint32_t usedBytes_;
    RefCountedPtr<ID3D12Resource> res_;
};

// Arena allocator for descriptors
class ContextD3D12::DescriptorArenaAlloc {
public:
    DescriptorArenaAlloc(uint32_t capacity,
                         D3D12_DESCRIPTOR_HEAP_TYPE type,
                         ID3D12Device* device)
        : capacity_(capacity), type_(type), offset_(0) {
        // RTV and DSV descriptor heaps are not shader visible
        const bool isShaderVisible =
            type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV ||
            type == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
        const auto desc = D3D12_DESCRIPTOR_HEAP_DESC{
            .Type = type,
            .NumDescriptors = capacity_,
            .Flags = isShaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
                                     : D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
        };
        const auto hr =
            device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap_));
        DASSERT_HR(hr);
        incrementSize_ = device->GetDescriptorHandleIncrementSize(type);
        cpuBase_ = heap_->GetCPUDescriptorHandleForHeapStart();
        if (isShaderVisible) {
            gpuBase_ = heap_->GetGPUDescriptorHandleForHeapStart();
        }
    }

    Descriptor AllocateSingle() {
        DASSERT(offset_ <= capacity_);
        return Descriptor(type_, offset_++, cpuBase_, gpuBase_, incrementSize_);
    }

    DescriptorRange AllocateRange(uint32_t count) {
        DASSERT(offset_ + count <= capacity_);
        DescriptorRange range;
        for (uint32_t i = 0; i < count; ++i) {
            range.push_back(AllocateSingle());
        }
        return range;
    }

    ID3D12DescriptorHeap* GetNative() { return heap_.Get(); }

    // NOTE: GPU can still use descriptors so should be synchronized
    void Clear() { offset_ = 0; }

private:
    const uint32_t capacity_;
    const D3D12_DESCRIPTOR_HEAP_TYPE type_;
    D3D12_CPU_DESCRIPTOR_HANDLE cpuBase_;
    D3D12_GPU_DESCRIPTOR_HANDLE gpuBase_;
    RefCountedPtr<ID3D12DescriptorHeap> heap_;
    uint32_t incrementSize_;
    uint32_t offset_;
};

class ContextD3D12::BindingLayout {
public:
    void PushRange(DescriptorType type,
                   uint32_t paramIdx,
                   const DescriptorRange& range) {
        bindings_.push_back(Elem{type, paramIdx, range});
    }

    Descriptor GetSlot(uint32_t paramIdx, uint32_t rangeIdx) {
        for (const Elem& range : bindings_) {
            if (range.paramIdx == paramIdx) {
                DASSERT(rangeIdx < range.descriptors.size());
                return range.descriptors[rangeIdx];
            }
        }
        return {};
    }

    struct Elem {
        DescriptorType type;
        uint32_t paramIdx;
        DescriptorRange descriptors;
    };

private:
    std::vector<Elem> bindings_;
};

ContextD3D12::ContextD3D12(DeviceD3D12* device) {
    auto timer = bench::ScopedTimer([](bench::Timer& tm) {
        LOG_INFO("GPU: ContextD3D12 has been initialized in {}ms", tm.Millis());
    });

    device_ = device;
    ID3D12Device* nativeDevice = device->GetNative();
    uploadHeap_.reset(new UploadHeap(nativeDevice, kUploadHeapSize));
    auto hr = nativeDevice->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&cmdAlloc_));
    DASSERT_HR(hr);
    hr = nativeDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
                                         cmdAlloc_.Get(), NULL,
                                         IID_PPV_ARGS(&cmdList_));
    DASSERT_HR(hr);
    descriptorArenaCbvSrvUav_.reset(new DescriptorArenaAlloc(
        kCbvSrvUavDescriptorHeapSize, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        nativeDevice));
    descriptorArenaRtv_.reset(new DescriptorArenaAlloc(
        kRtvDescriptorHeapSize, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, nativeDevice));
    descriptorArenaSampler_.reset(new DescriptorArenaAlloc(
        kSamplerDescriptorHeapSize, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,
        nativeDevice));
    SetDefaultState();
    state_ = State::Recording;
}

ContextD3D12::~ContextD3D12() {
    DASSERT_F(state_ != State::Pending,
              "Destructing context that is being used by GPU");
}

Device* ContextD3D12::GetParentDevice() {
    return device_.Get();
}

void ContextD3D12::WriteBuffer(Buffer* buf, std::span<const uint8_t> data) {
    DASSERT(state_ == State::Recording);
    BufferD3D12* bufferD3D12 = CastBuffer(buf);
    DASSERT(data.size() <= bufferD3D12->GetSize());
    const uint32_t size = data.size();
    const uint32_t offset = uploadHeap_->AppendData(data);

    bufferD3D12->UpdateState(cmdList_.Get(), D3D12_RESOURCE_STATE_COPY_DEST);

    ID3D12Device* nativeDevice = device_->GetNative();
    cmdList_->CopyBufferRegion(bufferD3D12->GetNative(), 0,
                               uploadHeap_->GetResource(), offset, size);
    // TODO: Clear upload buffer later
}

void ContextD3D12::WriteTexture(Texture* tex, std::span<const uint8_t> data) {
    // TODO: Implement texture upload
    DASSERT(false && "UNIMPLEMENTED");
}

void ContextD3D12::SetRenderTarget(Texture* texure) {
    DASSERT(state_ == State::Recording);
    // Create rtv for back buffer render target
    TextureD3D12* textureD3D12 = CastTexture(texure);
    renderTarget_ = texure;
    renderTargetDescriptor_ = descriptorArenaRtv_->AllocateSingle();
    device_->GetNative()->CreateRenderTargetView(
        textureD3D12->GetNative(), nullptr, renderTargetDescriptor_.cpu);
    // Set back buffer RTV
    cmdList_->OMSetRenderTargets(1, &renderTargetDescriptor_.cpu, false,
                                 nullptr);
    // Set default viewport and scissor rect from render target
    const auto viewport = D3D12_VIEWPORT{
        .TopLeftX = 0.f,
        .TopLeftY = 0.f,
        .Width = (float)renderTarget_->GetWidth(),
        .Height = (float)renderTarget_->GetHeight(),
        .MinDepth = 0.f,
        .MaxDepth = 1.f,
    };
    cmdList_->RSSetViewports(1, &viewport);
    const auto scissorRect = D3D12_RECT{
        .left = 0,
        .top = 0,
        .right = (LONG)renderTarget_->GetWidth(),
        .bottom = (LONG)renderTarget_->GetHeight(),
    };
    cmdList_->RSSetScissorRects(1, &scissorRect);
    // Transition and clear
    textureD3D12->UpdateState(cmdList_.Get(),
                              D3D12_RESOURCE_STATE_RENDER_TARGET);
    const float color[4] = {1.f, 1.f, 1.f, 1.f};
    cmdList_->ClearRenderTargetView(renderTargetDescriptor_.cpu, color, 0,
                                    nullptr);
}

void ContextD3D12::SetPipelineState(PipelineState* ps) {
    DASSERT(state_ == State::Recording);
    currentPSO_ = CastPipelineState(ps);
    currentBindings_.reset(new BindingLayout());
    // TODO: Add state management
    cmdList_->SetPipelineState(currentPSO_->GetNative());
    cmdList_->SetGraphicsRootSignature(currentPSO_->GetNativeRS());
    // Allocate descriptor ranges for the pso
    // TODO: Maybe move to PSO. I.e. ps->AllocateRanges(alloc);
    const d3d12::BindingLayout layout = currentPSO_->GetBindingLayout();
    for (const d3d12::BindingLayout::DescriptorRange& range : layout.ranges) {
        DASSERT(range.type != DescriptorType::RTV);
        DescriptorRange table =
            GetDescriptorHeapAlloc(range.type)->AllocateRange(range.count);
        currentBindings_->PushRange(range.type, range.parameterIndex, table);
        // Set binding descriptor tables
        cmdList_->SetGraphicsRootDescriptorTable(range.parameterIndex,
                                                 table.front().gpu);
    }
}

void ContextD3D12::SetClipRect(const gfx::Rect& rect) {
    DASSERT(state_ == State::Recording);
    const auto desc = D3D12_RECT{
        .left = (LONG)rect.Left(),
        .top = (LONG)rect.Top(),
        .right = (LONG)rect.Right(),
        .bottom = (LONG)rect.Bottom(),
    };
    cmdList_->RSSetScissorRects(1, &desc);
}

void ContextD3D12::SetVertexBuffer(Buffer* buf, uint32_t sizeBytes) {
    DASSERT(state_ == State::Recording);
    DASSERT(currentPSO_);
    const auto bufferStride = currentPSO_->GetVertexBufferStride();
    BufferD3D12* bufferD3D12 = CastBuffer(buf);
    DASSERT(sizeBytes > 0 && sizeBytes <= bufferD3D12->GetSize());
    bufferD3D12->UpdateState(cmdList_.Get(),
                             D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
    const auto desc = D3D12_VERTEX_BUFFER_VIEW{
        .BufferLocation = bufferD3D12->GetVirtualAddress(),
        .SizeInBytes = sizeBytes,
        .StrideInBytes = bufferStride,
    };
    cmdList_->IASetVertexBuffers(0, 1, &desc);
}

void ContextD3D12::SetIndexBuffer(Buffer* buf,
                                  IndexFormat format,
                                  uint32_t size) {
    DASSERT(state_ == State::Recording);
    DASSERT(currentPSO_);
    BufferD3D12* bufferD3D12 = CastBuffer(buf);
    DASSERT(size > 0 && size <= bufferD3D12->GetSize());
    bufferD3D12->UpdateState(cmdList_.Get(), D3D12_RESOURCE_STATE_INDEX_BUFFER);
    const auto desc = D3D12_INDEX_BUFFER_VIEW{
        .BufferLocation = bufferD3D12->GetVirtualAddress(),
        .SizeInBytes = size,
        .Format = DXGIFormat(format),
    };
    cmdList_->IASetIndexBuffer(&desc);
}

// Binding mapping chain
// bindIndex (32) -> register (b0) -> root signature parameter (1) ->
//   descriptor range (index 4)
void ContextD3D12::SetUniform(BindIndex bindIndex, Buffer* buf) {
    DASSERT(state_ == State::Recording);
    DASSERT(currentPSO_);
    DASSERT(currentBindings_);
    BufferD3D12* bufferD3D12 = CastBuffer(buf);
    bufferD3D12->UpdateState(cmdList_.Get(), D3D12_RESOURCE_STATE_GENERIC_READ);
    // TODO: Get the descriptor type from the buffer usage flags
    // For now just assume CBV for buffers, SRV for textures
    const auto [paramIdx, rangeIdx] = currentPSO_->GetBindSlot(bindIndex);
    const Descriptor slot = currentBindings_->GetSlot(paramIdx, rangeIdx);
    bufferD3D12->CreateCBV(slot);
}

void ContextD3D12::SetUniform(BindIndex bindIndex, Texture* tex) {
    DASSERT(state_ == State::Recording);
    DASSERT(currentPSO_);
    DASSERT(currentBindings_);
    TextureD3D12* textureD3D12 = CastTexture(tex);
    textureD3D12->UpdateState(cmdList_.Get(),
                              D3D12_RESOURCE_STATE_GENERIC_READ);
    const auto [paramIdx, rangeIdx] = currentPSO_->GetBindSlot(bindIndex);
    const Descriptor slot = currentBindings_->GetSlot(paramIdx, rangeIdx);
    textureD3D12->CreateSRV(slot);
}

void ContextD3D12::DrawIndexed(uint32_t numIndices,
                               uint32_t indexOffset,
                               uint32_t vertexOffset) {
    DASSERT(state_ == State::Recording);
    cmdList_->DrawIndexedInstanced(numIndices, 1, indexOffset, vertexOffset, 0);
}

void ContextD3D12::SetDefaultState() {
    cmdList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    const auto heaps = std::array{descriptorArenaCbvSrvUav_->GetNative(),
                                  descriptorArenaSampler_->GetNative()};
    cmdList_->SetDescriptorHeaps(heaps.size(), heaps.data());
    // Setup blend factor
    const float blend_factor[4] = {0.f, 0.f, 0.f, 0.f};
    cmdList_->OMSetBlendFactor(blend_factor);
}

ContextD3D12::DescriptorArenaAlloc* ContextD3D12::GetDescriptorHeapAlloc(
    DescriptorType type) {
    // clang-format off
    switch(type) {
        case DescriptorType::CBV: 
        case DescriptorType::SRV: 
        case DescriptorType::UAV: 
            return descriptorArenaCbvSrvUav_.get();
        case DescriptorType::RTV:
            return descriptorArenaRtv_.get();
        case DescriptorType::Sampler:
            return descriptorArenaSampler_.get();
        default: return nullptr;
    }
    // clang-format on
}

void ContextD3D12::Flush(ID3D12CommandQueue* cmdQueue) {
    DASSERT(state_ == State::Recording);
    auto timer = bench::ScopedTimer([](bench::Timer& tm) {
        LOG_INFO("GPU: ContextD3D12 has been flushed in {}ms", tm.Millis());
    });

    TextureD3D12* textureD3D12 = CastTexture(renderTarget_.Get());
    textureD3D12->UpdateState(cmdList_.Get(), D3D12_RESOURCE_STATE_PRESENT);
    cmdList_->Close();
    ID3D12CommandList* lists[]{cmdList_.Get()};
    cmdQueue->ExecuteCommandLists(std::size(lists), lists);
    state_ = State::Pending;
}

// Pre-render setup before reuse
void ContextD3D12::Reset() {
    DASSERT(state_ == State::Pending);
    cmdAlloc_->Reset();
    cmdList_->Reset(cmdAlloc_.Get(), nullptr);
    uploadHeap_->Clear();
    descriptorArenaCbvSrvUav_->Clear();
    descriptorArenaRtv_->Clear();
    descriptorArenaSampler_->Clear();
    renderTarget_ = nullptr;
    SetDefaultState();
    renderTargetDescriptor_ = Descriptor();
    state_ = State::Recording;
}

}  // namespace gpu::d3d12