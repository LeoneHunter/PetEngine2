#pragma once
#include "base/common.h"
#include "gpu/common.h"

// TODO: Should we depend on gfx?
#include "gfx/common.h"

namespace gpu {

// Optional flags GPU during resource creation (buffer, texture)
enum class ResourceFlags {
    // Resource will be placed in a default heap with default visibility
    Default = 0x0,
    // Resource lifetime is short (few frames)
    DynamicUsage = 0x1,
    // Resource will be placed in a CPU visible heap for writing
    UploadHeap = 0x2,
    // Resource will be placed in a CPU visible heap for reading
    ReadbackHeap = 0x4,
    // Resource will contain a constant data (aka d3d11 CBV)
    Constant = 0x8
};
DEFINE_ENUM_FLAGS_OPERATORS(ResourceFlags);

}  // namespace gpu



namespace gpu {

// TODO: Should we need a base class?
class Resource : public RefCountedBase {
public:
};

class Buffer : public Resource {
public:
};

class Texture : public Resource {
public:
};


// Stores rendering pipeline parameters
// - Shaders
// - Vertex buffer topology type
// - Root signature
class PipelineState : public RefCountedBase {
public:
    static std::unique_ptr<PipelineState> Create() { 
        // - Create a common RS for all shaders
        //   Allocate slots in the RS for different resources:
        //     - 4 slots for SRVs
        //     - 4 slots for CBVs
        return {}; 
    }

    void SetVertexShader(std::span<const uint8_t> data,
                         std::span<ShaderCode::Uniform> uniforms,
                         std::span<ShaderCode::Varying> inputs,
                         std::span<ShaderCode::Varying> outputs) {
        // - Create bindings to a common root signature
        // E.g. Take a uniform from the list and assign it to a slot in the 
        //   root signature: uniforms[0].bindIndex -> rs.textures[0]
        //   which binds a bindIndex to a slot 0 in the rs

        //
    }

    void SetPixelShader(std::span<const uint8_t> data,
                        std::span<ShaderCode::Uniform> uniforms,
                        std::span<ShaderCode::Varying> inputs,
                        std::span<ShaderCode::Varying> outputs) {
        //
    }
};



}  // namespace gpu



namespace gpu {



// High level api for rendering
// Wraps d3d12 apis and manages memory
// TODO: Should we expose Descriptors and DescriptorHeaps to the user?
class DrawContext {
public:
    std::unique_ptr<DrawContext> Create() { return {}; }

    // Validate that all state has be set, shader is set, shader arguments are
    // bound
    bool IsReadyToDraw() { return false; }

    // TODO: Handle alignment
    RefCountedPtr<Buffer> CreateBuffer(
        size_t sizeBytes,
        ResourceFlags flags = ResourceFlags::Default) {
        return {};
    }

    void UploadResourceData(const Resource* res,
                            std::span<const uint8_t> data) {}

    // Sets the clip rect overriding the previous one
    void SetClipRect(const gfx::Rect& rect) {}

    // Set a single resource as input to the shader
    void SetShaderArgument(BindIndex bindIdx, const Resource* res) {}

    void DrawIndexed(uint32_t indexCount,
                     uint32_t indexOffset = 0,
                     uint32_t vertexOffset = 0) {}

    void DrawIndexedInstanced(uint32_t indexCountPerInstance,
                              uint32_t instanceCount,
                              uint32_t indexOffset = 0,
                              uint32_t vertexOffset = 0,
                              uint32_t instanceOffset = 0) {}

private:
};

// A unit of a drawing pipeline
// Connects high level drawing primitives like text or rects and a draw context
// Could be a UI pass to draw ui shapes or a postprocess pass to adjust gamma
class DrawPass {
public:
    virtual ~DrawPass() = default;
    virtual void Init(gpu::DrawContext& context) {}
    virtual void Run(gpu::DrawContext& context) {}
};

class DrawPipeline {
public:
    void SubmitDrawPass(std::unique_ptr<DrawPass> pass) {}
};

}  // namespace gpu