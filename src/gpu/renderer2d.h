#pragma once
#include "runtime/common.h"
#include "runtime/gfx/common.h"
#include "d3d12/device_dx12.h"
#include "d3d12/slab_allocator_dx12_v2.h"
#include "d3d12/descriptor_heap_dx12.h"

// Number of independent frames that can be queued
constexpr uint32_t kNumFramesQueued = 2;

enum class BufferFormat {
    Unknown,
    Float16,
    Float32,
    Float64,
    Int32,
    Uint32,
};
enum class TextureFormat {
    Unknown,
    Float4,
};
using Handle = void*;

struct InputLayoutElement {

    enum class Semantic {
        Position,
        Texcoord,
        Color,
    };

    enum class Format {
        Float32,
        Int32,
        Uint32,
    };

    uint32_t offset;
    Semantic semantic;
    Format format;
};

// Rendering frontend for colored and textured 2D shapes
// Provides OpenGL-like api
class Renderer2D {
public:

    void Init(void* windowHandle);
    void Shutdown();

    Handle CreateBuffer(BufferFormat format);
    Handle CreateTexture(TextureFormat format, uint32_t pitch);

    template<std::ranges::contiguous_range T>
    void UploadResourceData(Handle handle, const T& data);
    void UploadData(Handle handle, void* data, size_t size);

    void ReleaseResource(Handle handle);

    // Should be called to create a new frame context
    void BeginFrame();
    void EndFrame();
    // Swaps the back buffer and presents on screen
    void Present();

    void SetViewport(uint32_t w, uint32_t h);
    void SetBackground(const gfx::Color4f& color);
    void SetClipRect(const gfx::Rect& rect);
    void SetClipCircle(gfx::Point center, float radius);

    template<class... T>
        requires (std::same_as<T, InputLayoutElement> && ...)
    void SetInputLayout(const T&... layout);

    // Only triangle list is supported
    void SetVertexBuffer(Handle handle);
    void SetIndexBuffer(Handle handle);
    void SetTexture(Handle handle);
    void SetColor(const gfx::Color4f& color);

    void SetShaderColored();
    void SetShaderTextured();

    void Draw(uint32_t numIndices, uint32_t indexOffset, uint32_t vertexOffset);

private:
    // Stores all per frame data
    struct FrameContext;

private:
    std::unique_ptr<d3d12::Device> device_;
    std::unique_ptr<d3d12::PooledDescriptorAllocator> descriptorAlloc_;
    // TODO: Alloc for upload heaps
    std::unique_ptr<d3d12::SlabAllocator> uploadAlloc_;
    std::unique_ptr<d3d12::SlabAllocator> textureAlloc_;
    std::unique_ptr<d3d12::SlabAllocator> bufferAlloc_;

    RefCountedPtr<ID3D12RootSignature> rootSignature_;
    RefCountedPtr<ID3D12PipelineState> basicPipelineState_;
    RefCountedPtr<ID3D12PipelineState> texturePipelineState_;

    std::vector<FrameContext> frameContexts;
    uint32_t currentFrameIndex_ = 0;
};
