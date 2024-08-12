#pragma once
#include "runtime/common.h"
#include "runtime/gfx/common.h"

// TODO: Should we depend on gfx?
#include "runtime/gfx/common.h"

namespace gpu {

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

// An index into a shader parameter table (aka shader signature/root signature)
using ShaderBindIndex = uint16_t;

} // namespace gpu





namespace gpu {

class Resource: public RefCount {
public:

};

class Buffer: public Resource {
public:

};

class Texture: public Resource {
public:

};

class Shader: public Resource {
public:

};

} // namespace gpu



namespace gpu {

class DrawPipeline {
public:

    void SubmitDrawPass(std::unique_ptr<DrawPass> pass);
};

// High level api for rendering
// Wraps d3d12 apis and manages memory
// TODO: Should we expose Descriptors and DescriptorHeaps to the user?
class DrawContext {
public:
    std::unique_ptr<DrawContext> Create();

    // Validate that all state has be set, shader is set, shader arguments are
    // bound
    bool IsReadyToDraw();

    RefCountedPtr<Buffer> CreateBuffer(
        size_t sizeBytes,
        ResourceFlags flags = ResourceFlags::Default);

    void UploadResourceData(const Resource* res, std::span<const uint8_t> data);

    // Sets the clip rect overriding the previous one
    void SetClipRect(const gfx::Rect& rect);

    // Set a single resource as input to the shader
    void SetShaderArgument(ShaderBindIndex bindIdx, const Resource* res);

    void DrawIndexed(uint32_t indexCount,
                     uint32_t indexOffset = 0,
                     uint32_t vertexOffset = 0);

    void DrawIndexedInstanced(uint32_t indexCountPerInstance,
                              uint32_t instanceCount,
                              uint32_t indexOffset = 0,
                              uint32_t vertexOffset = 0,
                              uint32_t instanceOffset = 0);

private:
};

// A unit of a drawing pipeline
// Connects high level drawing primitives like text or rects and a draw context
// Could be a UI pass to draw ui shapes or a postprocess pass to adjust gamma
class DrawPass {
public:
    virtual ~DrawPass() = default;
    virtual void PreDraw(gpu::DrawContext& context);
    virtual void Draw(gpu::DrawContext& context);
};


} // namespace gpu