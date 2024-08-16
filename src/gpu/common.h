#pragma once
#include "base/common.h"
#include "base/string_utils.h"

namespace gpu {

constexpr uint32_t kInvalidBindIndex{std::numeric_limits<uint32_t>::max()};

// Index into the root signature for shader
// Basically an index of a shader parameter
struct BindIndex {
    constexpr BindIndex(uint32_t val = kInvalidBindIndex): value(val) {}
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

enum class ShaderType {
    Vertex,
    Pixel,
    // Geometry
    // Compute
};

// Shader code with meta data
struct ShaderCode {
    // Input and output parameter of a shader
    struct Varying {
        std::string id;
        DataType type;
        Semantic semantic;
    };

    // Shader constants
    struct Uniform {
        std::string id;
        DataType type;
        BindIndex bindIndex;
    };

    ShaderType type;
    std::vector<Varying> inputs;
    std::vector<Varying> outputs;
    std::vector<Uniform> uniforms;
    std::string mainId;

    std::string code;
};


} // namespace gpu

DEFINE_CAST_FORMATTER(gpu::BindIndex, uint32_t);