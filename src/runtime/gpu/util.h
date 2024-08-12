#pragma once
#include "device.h"
#include "shader_compiler.h"

#include <span>

namespace gpu {

// Format of shader attachments like textures and buffers
// TODO: Add most useful formats from Vulkan and DX12
enum class ShaderFormat {
    Unknown,
    R32G32_Float,        // float2
    R32G32B32A32_Float,  // float4
    R8G8B8A8_Unorm,      // vec4<uint8_t> 0:255
};

// Describes the layout of vertex buffer
class InputLayout {
public:
    struct Element {
        std::string semanticName;
        ShaderFormat format;
        uint32_t alignedByteOffest;
    };

    // TODO: Consider change to static array
    std::vector<Element> layout;

    InputLayout& Push(const std::string& semanticName,
                      ShaderFormat format,
                      uint32_t alignedByteOffset) {}

    const Element& operator[](size_t index) const {
        DASSERT(index < layout.size());
        return layout[index];
    }

    auto begin() const { return layout.begin(); }
    auto end() const { return layout.end(); }
};

// Helper class that unifies different types and formats of geometry:
// simple rects, separate vertex buffers, indexed meshes
// Input geometry is copied into internal buffers and merged
class GeometryBuffer {
public:
    template <class VertexType, std::integral IndexType>
    void AddMeshFromBuffers(std::span<const VertexType> vertexBuffer,
                            std::span<const IndexType> indexBuffer,
                            const std::vector<std::string>& vertexBufferLayout);

    InputLayout GetLayout();

private:
};

}  // namespace gpu
