#pragma once
#include "gpu/d3d12/common.h"
#include "gpu/gpu.h"

namespace gpu::d3d12 {

class DeviceD3D12;

class BufferD3D12 final : public gpu::Buffer {
public:
    BufferD3D12(DeviceD3D12* device,
                ResourceHeapAllocation addr,
                RefCountedPtr<ID3D12Resource> res,
                uint32_t size);

    ~BufferD3D12();

    ResourceHeapAllocation GetAllocationAddress() const { return addr_; }
    uint64_t GetVirtualAddress() { return res_->GetGPUVirtualAddress(); }
    uint32_t GetSize() const { return size_; }
    ID3D12Resource* GetNative() { return res_.Get(); }
    D3D12_RESOURCE_STATES GetState() const { return state_; }

    void UpdateState(ID3D12GraphicsCommandList* cmdList,
                         D3D12_RESOURCE_STATES state);

    void CreateCBV(Descriptor slot);

private:
    uint32_t size_;
    ResourceHeapAllocation addr_;
    RefCountedPtr<ID3D12Resource> res_;
    RefCountedPtr<DeviceD3D12> device_;
    D3D12_RESOURCE_STATES state_ = D3D12_RESOURCE_STATE_COMMON;
};

inline BufferD3D12* CastBuffer(Buffer* buf) {
    return static_cast<BufferD3D12*>(buf);
}

}  // namespace gpu::d3d12