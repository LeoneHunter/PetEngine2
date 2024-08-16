#pragma once
#include "base/common.h"
#include "base/util.h"

#include <span>

// We are using https://github.com/microsoft/DirectX-Headers.git
// These includes extended apis
#define D3DX12_NO_STATE_OBJECT_HELPERS
#define D3DX12_NO_CHECK_FEATURE_SUPPORT_CLASS

#ifndef NOMINMAX
    #define NOMINMAX
#endif

#include <directx/d3dx12.h>
#include <dxgi.h>

#if defined(IS_DEBUG_BUILD)
    #define DASSERT_HR(hresult)                                                      \
        do {                                                                         \
            if(FAILED(hresult)) {                                                    \
                LOGF(Fatal, "HRESULT failed. Code {:#08X}", (int32_t)hresult);       \
                __debugbreak();                                                      \
                std::abort();                                                        \
            }                                                                        \
        } while(0)
#else
    #define DASSERT_HR(hresult)
#endif


namespace d3d12 {

enum class Error {
    Ok = 0,
    AdapterNotFound,
    AdapterSoftware,
    AdapterCannotCreate,
    CannotAllocate,
};

constexpr std::string to_string(Error err) {
    switch(err) {
        case Error::Ok: return "Ok";
        case Error::AdapterNotFound: return "AdapterNotFound";
        case Error::AdapterSoftware: return "AdaptareSoftware";
        case Error::AdapterCannotCreate: return "AdapterCannotCreate";
    }
    DASSERT(false);
}


// Resource types used by heaps
enum class HeapResourceType {
    Unknown,
    NonRtDsTexture,
    RtDsTexture,
    Buffer,
    _Count
};

constexpr std::string to_string(HeapResourceType type) {
    switch(type) {
        case HeapResourceType::Unknown: return "Unknown";
        case HeapResourceType::NonRtDsTexture: return "NotRtDsTexture";
        case HeapResourceType::RtDsTexture: return "RtDsTexture";
        case HeapResourceType::Buffer: return "Buffer";
    }
    UNREACHABLE();
}

constexpr uint8_t GetFormatBytes(DXGI_FORMAT format) {
    switch(format) {
    case DXGI_FORMAT_R32G32B32A32_TYPELESS:
    case DXGI_FORMAT_R32G32B32A32_FLOAT:
    case DXGI_FORMAT_R32G32B32A32_UINT:
    case DXGI_FORMAT_R32G32B32A32_SINT:
        return 16;

    case DXGI_FORMAT_R32G32B32_TYPELESS:
    case DXGI_FORMAT_R32G32B32_FLOAT:
    case DXGI_FORMAT_R32G32B32_UINT:
    case DXGI_FORMAT_R32G32B32_SINT:
        return 12;

    case DXGI_FORMAT_R16G16B16A16_TYPELESS:
    case DXGI_FORMAT_R16G16B16A16_FLOAT:
    case DXGI_FORMAT_R16G16B16A16_UNORM:
    case DXGI_FORMAT_R16G16B16A16_UINT:
    case DXGI_FORMAT_R16G16B16A16_SNORM:
    case DXGI_FORMAT_R16G16B16A16_SINT:
    case DXGI_FORMAT_R32G32_TYPELESS:
    case DXGI_FORMAT_R32G32_FLOAT:
    case DXGI_FORMAT_R32G32_UINT:
    case DXGI_FORMAT_R32G32_SINT:
    case DXGI_FORMAT_R32G8X24_TYPELESS:
    case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
    case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
    case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
        return 8;

    case DXGI_FORMAT_R10G10B10A2_TYPELESS:
    case DXGI_FORMAT_R10G10B10A2_UNORM:
    case DXGI_FORMAT_R10G10B10A2_UINT:
    case DXGI_FORMAT_R11G11B10_FLOAT:
    case DXGI_FORMAT_R8G8B8A8_TYPELESS:
    case DXGI_FORMAT_R8G8B8A8_UNORM:
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
    case DXGI_FORMAT_R8G8B8A8_UINT:
    case DXGI_FORMAT_R8G8B8A8_SNORM:
    case DXGI_FORMAT_R8G8B8A8_SINT:
    case DXGI_FORMAT_R16G16_TYPELESS:
    case DXGI_FORMAT_R16G16_FLOAT:
    case DXGI_FORMAT_R16G16_UNORM:
    case DXGI_FORMAT_R16G16_UINT:
    case DXGI_FORMAT_R16G16_SNORM:
    case DXGI_FORMAT_R16G16_SINT:
    case DXGI_FORMAT_R32_TYPELESS:
    case DXGI_FORMAT_D32_FLOAT:
    case DXGI_FORMAT_R32_FLOAT:
    case DXGI_FORMAT_R32_UINT:
    case DXGI_FORMAT_R32_SINT:
    case DXGI_FORMAT_R24G8_TYPELESS:
    case DXGI_FORMAT_D24_UNORM_S8_UINT:
    case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
    case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
    case DXGI_FORMAT_R9G9B9E5_SHAREDEXP:
    case DXGI_FORMAT_R8G8_B8G8_UNORM:
    case DXGI_FORMAT_G8R8_G8B8_UNORM:
    case DXGI_FORMAT_B8G8R8A8_UNORM:
    case DXGI_FORMAT_B8G8R8X8_UNORM:
    case DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM:
    case DXGI_FORMAT_B8G8R8A8_TYPELESS:
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
    case DXGI_FORMAT_B8G8R8X8_TYPELESS:
    case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
        return 4;

    case DXGI_FORMAT_R8G8_TYPELESS:
    case DXGI_FORMAT_R8G8_UNORM:
    case DXGI_FORMAT_R8G8_UINT:
    case DXGI_FORMAT_R8G8_SNORM:
    case DXGI_FORMAT_R8G8_SINT:
    case DXGI_FORMAT_R16_TYPELESS:
    case DXGI_FORMAT_R16_FLOAT:
    case DXGI_FORMAT_D16_UNORM:
    case DXGI_FORMAT_R16_UNORM:
    case DXGI_FORMAT_R16_UINT:
    case DXGI_FORMAT_R16_SNORM:
    case DXGI_FORMAT_R16_SINT:
    case DXGI_FORMAT_B5G6R5_UNORM:
    case DXGI_FORMAT_B5G5R5A1_UNORM:
    case DXGI_FORMAT_A8P8:
    case DXGI_FORMAT_B4G4R4A4_UNORM:
        return 2;

    case DXGI_FORMAT_R8_TYPELESS:
    case DXGI_FORMAT_R8_UNORM:
    case DXGI_FORMAT_R8_UINT:
    case DXGI_FORMAT_R8_SNORM:
    case DXGI_FORMAT_R8_SINT:
    case DXGI_FORMAT_A8_UNORM:
    case DXGI_FORMAT_P8:
        return 1;

    default:
        return 0;
    }
}


// Passed to Allocator to track all allocations
// Could be used for validation, debug or statistics
class AllocationTracker {
public:
    struct SpanID {
        uintptr_t heapAddress;
        uint32_t pageIndex;

        auto operator<=>(const SpanID&) const = default;
    };

    struct SpanInfo {
        uint32_t sizeClass;
        uint32_t numPages;
        uint16_t numSlots;
        uint16_t numFreeSlots;
    };

    struct SlotAllocInfo {
        uint32_t slotIndex;
    };

    struct HeapInfo {
        std::string name;
        uintptr_t address;
        uint32_t size;
    };

    virtual ~AllocationTracker() = default;

    virtual void DidCreateHeap(const HeapInfo& info) {}
    virtual void DidDestroyHeap(uintptr_t address) {}

    virtual void DidCommitSpan(const SpanID& id, const SpanInfo& info) {}
    virtual void DidDecommitSpan(const SpanID& id) {}

    virtual void DidAllocateSlot(const SpanID& id, const SlotAllocInfo& info) {}
    virtual void DidFreeSlot(const SpanID& id, const SlotAllocInfo& info) {}
};

} // namespace d3d12




