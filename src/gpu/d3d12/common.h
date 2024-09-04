#pragma once
#include "base/common.h"
#include "gpu/common.h"

#include <span>

// We are using https://github.com/microsoft/DirectX-Headers
// These includes extended apis CD3DX12
#define D3DX12_NO_STATE_OBJECT_HELPERS
#define D3DX12_NO_CHECK_FEATURE_SUPPORT_CLASS

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <directx/d3dx12.h>
#include <dxgi.h>
#include <dxgi1_6.h>
#include <wrl.h>

#if defined(IS_DEBUG_BUILD)
#include <dxgidebug.h>
#endif


#if defined(IS_DEBUG_BUILD)
#define DASSERT_HR(hresult)                                                \
    do {                                                                   \
        if (FAILED(hresult)) {                                             \
            LOGF(Fatal, "HRESULT failed. Code {:#08X}", (int32_t)hresult); \
            __debugbreak();                                                \
            std::abort();                                                  \
        }                                                                  \
    } while (0)
#else
#define DASSERT_HR(hresult)
#endif

using Microsoft::WRL::ComPtr;

namespace gpu::d3d12 {

// 64 KiB
// D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT
constexpr uint32_t kResourceMinAlignment = 1 << 16;

// 4 MiB
// D3D12_DEFAULT_MSAA_RESOURCE_PLACEMENT_ALIGNMENT
constexpr uint32_t kResourceMaxAlignment = 1 << 22;

// HLSL register types
enum class RegisterType { CBV, SRV, UAV };

constexpr std::string RegisterPrefixFromType(RegisterType reg) {
    switch (reg) {
        case RegisterType::CBV: return "b";
        case RegisterType::SRV: return "t";
        case RegisterType::UAV: return "u";
        default: return "";
    }
}

enum class DescriptorType { CBV, SRV, UAV, RTV, Sampler };

constexpr uint8_t GetFormatBytes(DXGI_FORMAT format) {
    switch (format) {
        case DXGI_FORMAT_R32G32B32A32_TYPELESS:
        case DXGI_FORMAT_R32G32B32A32_FLOAT:
        case DXGI_FORMAT_R32G32B32A32_UINT:
        case DXGI_FORMAT_R32G32B32A32_SINT: return 16;

        case DXGI_FORMAT_R32G32B32_TYPELESS:
        case DXGI_FORMAT_R32G32B32_FLOAT:
        case DXGI_FORMAT_R32G32B32_UINT:
        case DXGI_FORMAT_R32G32B32_SINT: return 12;

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
        case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT: return 8;

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
        case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB: return 4;

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
        case DXGI_FORMAT_B4G4R4A4_UNORM: return 2;

        case DXGI_FORMAT_R8_TYPELESS:
        case DXGI_FORMAT_R8_UNORM:
        case DXGI_FORMAT_R8_UINT:
        case DXGI_FORMAT_R8_SNORM:
        case DXGI_FORMAT_R8_SINT:
        case DXGI_FORMAT_A8_UNORM:
        case DXGI_FORMAT_P8: return 1;

        default: return 0;
    }
}

constexpr DXGI_FORMAT DXGIFormat(IndexFormat format) {
    switch (format) {
        case IndexFormat::Uint16: return DXGI_FORMAT_R16_UINT;
        case IndexFormat::Uint32: return DXGI_FORMAT_R32_UINT;
        default: return DXGI_FORMAT_UNKNOWN;
    }
}

constexpr DXGI_FORMAT DXGIFormat(TextureFormat format) {
    // clang-format off
    switch(format) {
        case TextureFormat::Undefined: return DXGI_FORMAT_UNKNOWN;
        case TextureFormat::R8Unorm: return DXGI_FORMAT_R8_UNORM;
        case TextureFormat::R8Snorm: return DXGI_FORMAT_R8_SNORM;
        case TextureFormat::R8Uint: return DXGI_FORMAT_R8_UINT;
        case TextureFormat::R8Sint: return DXGI_FORMAT_R8_SINT;
        case TextureFormat::R16Uint: return DXGI_FORMAT_R16_UINT;
        case TextureFormat::R16Sint: return DXGI_FORMAT_R16_SINT;
        case TextureFormat::R16Float: return DXGI_FORMAT_R16_FLOAT;
        case TextureFormat::RG8Unorm: return DXGI_FORMAT_R8G8_UNORM;
        case TextureFormat::RG8Snorm: return DXGI_FORMAT_R8G8_SNORM;
        case TextureFormat::RG8Uint: return DXGI_FORMAT_R8G8_UINT;
        case TextureFormat::RG8Sint: return DXGI_FORMAT_R8G8_SINT;
        case TextureFormat::R32Float: return DXGI_FORMAT_R32_FLOAT;
        case TextureFormat::R32Uint: return DXGI_FORMAT_R32_UINT;
        case TextureFormat::R32Sint: return DXGI_FORMAT_R32_SINT;
        case TextureFormat::RG16Uint: return DXGI_FORMAT_R16G16_UINT;
        case TextureFormat::RG16Sint: return DXGI_FORMAT_R16G16_SINT;
        case TextureFormat::RG16Float: return DXGI_FORMAT_R16G16_FLOAT;
        case TextureFormat::RGBA8Unorm: return DXGI_FORMAT_R8G8B8A8_UNORM;
        case TextureFormat::RGBA8UnormSrgb: return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        case TextureFormat::RGBA8Snorm: return DXGI_FORMAT_R8G8B8A8_SNORM;
        case TextureFormat::RGBA8Uint: return DXGI_FORMAT_R8G8B8A8_UINT;
        case TextureFormat::RGBA8Sint: return DXGI_FORMAT_R8G8B8A8_SINT;
        case TextureFormat::BGRA8Unorm: return DXGI_FORMAT_B8G8R8A8_UNORM;
        case TextureFormat::BGRA8UnormSrgb: return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
        case TextureFormat::RGB10A2Uint: return DXGI_FORMAT_R10G10B10A2_UINT;
        case TextureFormat::RGB10A2Unorm: return DXGI_FORMAT_R10G10B10A2_UNORM;
        case TextureFormat::RG11B10Ufloat: return DXGI_FORMAT_R11G11B10_FLOAT;
        case TextureFormat::RGB9E5Ufloat: return DXGI_FORMAT_R9G9B9E5_SHAREDEXP;
        case TextureFormat::RG32Float: return DXGI_FORMAT_R32G32_FLOAT;
        case TextureFormat::RG32Uint: return DXGI_FORMAT_R32G32_UINT;
        case TextureFormat::RG32Sint: return DXGI_FORMAT_R32G32_SINT;
        case TextureFormat::RGBA16Uint: return DXGI_FORMAT_R16G16B16A16_UINT;
        case TextureFormat::RGBA16Sint: return DXGI_FORMAT_R16G16B16A16_SINT;
        case TextureFormat::RGBA16Float: return DXGI_FORMAT_R16G16B16A16_FLOAT;
        case TextureFormat::RGBA32Float: return DXGI_FORMAT_R32G32B32A32_FLOAT;
        case TextureFormat::RGBA32Uint: return DXGI_FORMAT_R32G32B32A32_UINT;
        case TextureFormat::RGBA32Sint: return DXGI_FORMAT_R32G32B32A32_SINT;
        default: return DXGI_FORMAT_UNKNOWN;
    };
    // clang-format on
}

constexpr DXGI_FORMAT DXGIFormat(VertexFormat format) {
    // clang-format off
    switch(format) {
        case VertexFormat::Undefined: return DXGI_FORMAT_UNKNOWN;
        case VertexFormat::Uint8x2: return DXGI_FORMAT_R8G8_UINT;
        case VertexFormat::Uint8x4: return DXGI_FORMAT_R8G8B8A8_UINT;
        case VertexFormat::Sint8x2: return DXGI_FORMAT_R8G8_SINT;
        case VertexFormat::Sint8x4: return DXGI_FORMAT_R8G8B8A8_SINT;
        case VertexFormat::Unorm8x2: return DXGI_FORMAT_R8G8_UNORM;
        case VertexFormat::Unorm8x4: return DXGI_FORMAT_R8G8B8A8_UNORM;
        case VertexFormat::Snorm8x2: return DXGI_FORMAT_R8G8_SNORM;
        case VertexFormat::Snorm8x4: return DXGI_FORMAT_R8G8B8A8_SNORM;
        case VertexFormat::Uint16x2: return DXGI_FORMAT_R16G16_UINT;
        case VertexFormat::Uint16x4: return DXGI_FORMAT_R16G16B16A16_UINT;
        case VertexFormat::Sint16x2: return DXGI_FORMAT_R16G16_SINT;
        case VertexFormat::Sint16x4: return DXGI_FORMAT_R16G16B16A16_SINT;
        case VertexFormat::Unorm16x2: return DXGI_FORMAT_R16G16_UNORM;
        case VertexFormat::Unorm16x4: return DXGI_FORMAT_R16G16B16A16_UNORM;
        case VertexFormat::Snorm16x2: return DXGI_FORMAT_R16G16_SNORM;
        case VertexFormat::Snorm16x4: return DXGI_FORMAT_R16G16B16A16_SNORM;
        case VertexFormat::Float16x2: return DXGI_FORMAT_R16G16_FLOAT;
        case VertexFormat::Float16x4: return DXGI_FORMAT_R16G16B16A16_FLOAT;
        case VertexFormat::Float32: return DXGI_FORMAT_R32_FLOAT;
        case VertexFormat::Float32x2: return DXGI_FORMAT_R32G32_FLOAT;
        case VertexFormat::Float32x3: return DXGI_FORMAT_R32G32B32_FLOAT;
        case VertexFormat::Float32x4: return DXGI_FORMAT_R32G32B32A32_FLOAT;
        case VertexFormat::Uint32: return DXGI_FORMAT_R32_UINT;
        case VertexFormat::Uint32x2: return DXGI_FORMAT_R32G32_UINT;
        case VertexFormat::Uint32x3: return DXGI_FORMAT_R32G32B32_UINT;
        case VertexFormat::Uint32x4: return DXGI_FORMAT_R32G32B32A32_UINT;
        case VertexFormat::Sint32: return DXGI_FORMAT_R32_SINT;
        case VertexFormat::Sint32x2: return DXGI_FORMAT_R32G32_SINT;
        case VertexFormat::Sint32x3: return DXGI_FORMAT_R32G32B32_SINT;
        default: return DXGI_FORMAT_UNKNOWN;
    }
    // clang-format on
}

constexpr D3D12_SRV_DIMENSION SrvDimension(TextureDimension dim) {
    switch (dim) {
        case TextureDimension::Dim1D: return D3D12_SRV_DIMENSION_TEXTURE1D;
        case TextureDimension::Dim2D: return D3D12_SRV_DIMENSION_TEXTURE2D;
        case TextureDimension::Dim3D: return D3D12_SRV_DIMENSION_TEXTURE3D;
        default: return D3D12_SRV_DIMENSION_UNKNOWN;
    }
}

constexpr D3D12_RESOURCE_DIMENSION ResourceDimension(
    TextureDimension dimension) {
    switch(dimension) {
        case TextureDimension::Dim1D: return D3D12_RESOURCE_DIMENSION_TEXTURE1D;
        case TextureDimension::Dim2D: return D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        case TextureDimension::Dim3D: return D3D12_RESOURCE_DIMENSION_TEXTURE3D;
        default: return D3D12_RESOURCE_DIMENSION_UNKNOWN;
    }
}

constexpr const char* SemanticString(Semantic semantic) {
    switch (semantic) {
        case Semantic::Position: return "POSITION";
        case Semantic::Texcoord: return "TEXCOORD";
        case Semantic::Color: return "COLOR";
        default: return "";
    }
}

// Result of the allocation
// A heap and an offset into it in bytes
// Can be used then: heap->CreatePlacedResource(offset)
// If heap == nullptr, dedicated allocation
struct ResourceHeapAllocation {
    ID3D12Heap* heap = nullptr;
    uint32_t offset = 0;
};

// Passed to Allocator to track all allocations
// Could be used for validation, debug or statistics
// TODO: Maybe move to api.h
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


struct Descriptor {
    D3D12_DESCRIPTOR_HEAP_TYPE type;
    CD3DX12_CPU_DESCRIPTOR_HANDLE cpu;
    CD3DX12_GPU_DESCRIPTOR_HANDLE gpu;

    Descriptor() = default;

    Descriptor(D3D12_DESCRIPTOR_HEAP_TYPE type,
               uint32_t offset,
               D3D12_CPU_DESCRIPTOR_HANDLE cpuBase,
               D3D12_GPU_DESCRIPTOR_HANDLE gpuBase,
               uint32_t incrementSize)
        : type(type) {
        cpu.InitOffsetted(cpuBase, (int32_t)offset, incrementSize);
        gpu.InitOffsetted(gpuBase, (int32_t)offset, incrementSize);
    }
};
using DescriptorRange = std::vector<Descriptor>;


}  // namespace gpu::d3d12
