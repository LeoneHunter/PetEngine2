#include "buffer.h"
#include "device.h"

namespace gpu::d3d12 {

BufferD3D12::BufferD3D12(DeviceD3D12* device,
                         ResourceHeapAllocation addr,
                         RefCountedPtr<ID3D12Resource> res,
                         uint32_t size)
    : size_(size), addr_(addr), device_(device), res_(res) {}

BufferD3D12::~BufferD3D12() {
    device_->FreeResource(addr_, res_.Get());
}

void BufferD3D12::UpdateState(ID3D12GraphicsCommandList* cmdList,
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

void BufferD3D12::CreateCBV(Descriptor slot) {
    const auto desc = D3D12_CONSTANT_BUFFER_VIEW_DESC{
        .BufferLocation = GetVirtualAddress(),
        .SizeInBytes = GetSize(),
    };
    device_->GetNative()->CreateConstantBufferView(&desc, slot.cpu);
}

}  // namespace gpu::d3d12