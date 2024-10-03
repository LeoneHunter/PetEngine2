#pragma once
#include "base/common.h"
#include "base/string_utils.h"
#include "base/util.h"

namespace gpu {

template <class T>
using ExpectedRef = std::expected<RefCountedPtr<T>, GenericErrorCode>;

// Helper macro that asserts that the ExpectedRef is successful
// Usage: RefCountedPtr<Device> device = TRY(Device::Create());
#define TRY(EXPR)                                                        \
    [&] {                                                                \
        auto result = EXPR;                                              \
        DASSERT_F(result, "'{}' failed with {}", #EXPR, result.error()); \
        return *result;                                                  \
    }();


constexpr uint32_t kInvalidBindIndex{std::numeric_limits<uint32_t>::max()};

// Index into the root signature for shader
// Basically an index of a shader parameter
struct BindIndex {
    constexpr BindIndex(uint32_t val = kInvalidBindIndex) : value(val) {}
    constexpr auto operator<=>(const BindIndex& rhs) const = default;
    constexpr explicit operator uint32_t() const { return value; };
    uint32_t value;
};

// Predefined bind index for shader inputs and outputs
// NOTE: Semantic and bind index are mutually exclusive
enum class Semantic {
    None,
    Position,
    Color,
    Texcoord,
};

constexpr std::string to_string(Semantic semantic) {
    switch (semantic) {
        case Semantic::Position: return "Position";
        case Semantic::Color: return "Color";
        case Semantic::Texcoord: return "Texcoord";
    }
    return "";
}


// Additional property that can be added to variables
enum class Attribute {
    None,
    // Shader or function input parameter
    Input,
    // Shader or function return parameter
    Output,
    // Shader constant input parameter
    Uniform,
};

// A type of a variable, parameter, field. (memory location)
// Basically a builder local id
enum class DataType : uint32_t {
    Unknown,
    Float,
    Float2,
    Float3,
    Float4,
    Float4x4,
    Uint,
    Int,
    Function,
    Class,
    Sampler,
    Texture2D,
};

enum class IndexFormat {
    Undefined,
    Uint16,
    Uint32,
};

// Format of the mesh vertex data
enum class VertexFormat {
    Undefined,
    Uint8x2,
    Uint8x4,
    Sint8x2,
    Sint8x4,
    Unorm8x2,
    Unorm8x4,
    Snorm8x2,
    Snorm8x4,
    Uint16x2,
    Uint16x4,
    Sint16x2,
    Sint16x4,
    Unorm16x2,
    Unorm16x4,
    Snorm16x2,
    Snorm16x4,
    Float16x2,
    Float16x4,
    Float32,
    Float32x2,
    Float32x3,
    Float32x4,
    Uint32,
    Uint32x2,
    Uint32x3,
    Uint32x4,
    Sint32,
    Sint32x2,
    Sint32x3,
    Sint32x4,
};

// VertexFormat bytes sizes
constexpr uint32_t GetFormatSize(VertexFormat format) {
    // clang-format off
    switch (format) {
        case VertexFormat::Uint8x2:
        case VertexFormat::Sint8x2:
        case VertexFormat::Unorm8x2:
        case VertexFormat::Snorm8x2: 
            return 2;
        case VertexFormat::Uint8x4:
        case VertexFormat::Sint8x4:
        case VertexFormat::Unorm8x4:
        case VertexFormat::Snorm8x4:
        case VertexFormat::Uint16x2:
        case VertexFormat::Sint16x2:
        case VertexFormat::Unorm16x2:
        case VertexFormat::Snorm16x2:
        case VertexFormat::Float16x2:
        case VertexFormat::Uint32:
        case VertexFormat::Sint32:
        case VertexFormat::Float32: 
            return 4;
        case VertexFormat::Uint16x4:
        case VertexFormat::Sint16x4:
        case VertexFormat::Unorm16x4:
        case VertexFormat::Snorm16x4:
        case VertexFormat::Float16x4:
        case VertexFormat::Float32x2:
        case VertexFormat::Uint32x2:
        case VertexFormat::Sint32x2: 
            return 8;
        case VertexFormat::Float32x3:
        case VertexFormat::Uint32x3:
        case VertexFormat::Sint32x3: 
            return 12;
        case VertexFormat::Float32x4:
        case VertexFormat::Uint32x4:
        case VertexFormat::Sint32x4: 
            return 16;
        default: 
            return 0;
    }
    // clang-format on
}

enum class TextureFormat {
    Undefined,
    R8Unorm,
    R8Snorm,
    R8Uint,
    R8Sint,
    R16Uint,
    R16Sint,
    R16Float,
    RG8Unorm,
    RG8Snorm,
    RG8Uint,
    RG8Sint,
    R32Float,
    R32Uint,
    R32Sint,
    RG16Uint,
    RG16Sint,
    RG16Float,
    RGBA8Unorm,
    RGBA8UnormSrgb,
    RGBA8Snorm,
    RGBA8Uint,
    RGBA8Sint,
    BGRA8Unorm,
    BGRA8UnormSrgb,
    RGB10A2Uint,
    RGB10A2Unorm,
    RG11B10Ufloat,
    RGB9E5Ufloat,
    RG32Float,
    RG32Uint,
    RG32Sint,
    RGBA16Uint,
    RGBA16Sint,
    RGBA16Float,
    RGBA32Float,
    RGBA32Uint,
    RGBA32Sint,
};

enum class TextureDimension {
    Dim1D,
    Dim2D,
    Dim3D,
};

enum class TextureUsage {
    Unknown,
    ReadOnly,      // Corresponds to hlsl SRV
    ReadWrite,     // Corresponds to hlsl UAV
    RenderTarget,  // PipelineState attachment (output)
};

enum class BufferUsage {
    Unknown,
    Upload,     // CPU writable
    Readback,   // CPU readable
    Uniform,    // In default heap, shader constant
    ReadWrite,  // In default heap, shader r/w
    Index,      // As pipeline index buffer input
    Vertex,     // As pipeline vertex buffer input
};

enum class ShaderUsage {
    Vertex,
    Pixel,
    Compute,
};

// Resource types used by heaps
enum class HeapResourceType {
    Unknown,
    NonRtDsTexture,
    RtDsTexture,
    Buffer,
    _Count
};

constexpr std::string to_string(HeapResourceType type) {
    switch (type) {
        case HeapResourceType::Unknown: return "Unknown";
        case HeapResourceType::NonRtDsTexture: return "NotRtDsTexture";
        case HeapResourceType::RtDsTexture: return "RtDsTexture";
        case HeapResourceType::Buffer: return "Buffer";
    }
    UNREACHABLE();
}

struct ShaderCompileResult {
    std::vector<uint8_t> bytecode;
    bool hasErrors = false;
    std::string msg;

    explicit operator bool() const { return !hasErrors; }
};

// Vertex shader input data layout
class InputLayout {
public:
    struct Element {
        Semantic semanticName;
        VertexFormat format;
        uint32_t alignedByteOffset;
    };

    InputLayout& Push(const Element& elem) {
        layout.push_back(elem);
        // Update current stride if offset is larger
        if (elem.alignedByteOffset && elem.alignedByteOffset > stride) {
            stride = elem.alignedByteOffset;
        }
        stride += GetFormatSize(elem.format);
        return *this;
    }

    InputLayout& Push(Semantic semantic,
                      VertexFormat format,
                      uint32_t alignedByteOffset) {
        Push(Element{
            .semanticName = semantic,
            .format = format,
            .alignedByteOffset = alignedByteOffset,
        });
        return *this;
    }

    uint32_t GetStride() const { return stride; }

    InputLayout() { layout.reserve(4); }

    auto begin() const { return layout.begin(); }
    auto end() const { return layout.end(); }

private:
    std::vector<Element> layout;
    uint32_t stride;
};



}  // namespace gpu

DEFINE_CAST_FORMATTER(gpu::BindIndex, uint32_t);