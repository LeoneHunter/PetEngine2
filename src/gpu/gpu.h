#pragma once
#include "gpu/common.h"
#include "gpu/shader/shader_dsl.h"
#include "gpu/shader/shader_dsl_api.h"

#include "base/string_utils.h"
#include "base/util.h"

#include <span>

namespace gfx {
class Rect;
}

namespace gpu {

class Device;
class Buffer;
class Texture;
class PipelineState;

class Object : public RefCountedBase {
public:
    virtual ~Object() = default;

    void SetLabel(const std::string& label) { label_ = label; }

    const std::string& GetLabel() const { return label_; }

private:
    std::string label_;
};

// Shader code with meta data
struct ShaderCode {
    // Input and output parameter of a shader
    struct Varying {
        std::string id;
        DataType type;
        Semantic semantic;
    };

    // HLSL register or GLSL location
    struct Location {
        std::string type;
        uint32_t index;
    };

    // Shader constants
    struct Uniform {
        std::string id;
        DataType type;
        BindIndex binding;
        Location location;
    };

    ShaderType type;
    std::vector<Varying> inputs;
    std::vector<Varying> outputs;
    std::vector<Uniform> uniforms;
    // Identifier of the entry function (main)
    std::string mainId;
    // Short version tag: ps_5_0, vs_6_1, gl_140
    std::string version;

    std::string code;
};

// Generates shader code for the backend (hlsl)
class ShaderCodeGenerator {
public:
    virtual ~ShaderCodeGenerator() = default;

    virtual std::unique_ptr<ShaderCode> Generate(ShaderType type,
                                                 std::string_view main,
                                                 ShaderDSLContext* ctx) = 0;
};

class Shader {
public:
    std::vector<uint8_t> bytecode;
};

// A general purpose GPU resource
// Basically a heap pointer into the GPU visible memory
class Buffer : public Object {};

// TODO: Add usage flags: storage, render target, uav, etc.
// TODO: Add mips
struct TextureDesc {
    TextureDimension dimension;
    TextureFormat format;
    uint32_t width;
    uint32_t height;
};

// Same as Buffer but has different layout and additional mip-maps levels
class Texture : public Object {
public:
    virtual uint32_t GetWidth() const = 0;
    virtual uint32_t GetHeight() const = 0;
};


// Deferred rendering context
// Commands are unqued into a command list and then
//   submitted to the gpu via Device::Submit(Context)
class CommandContext : public Object {
public:
    virtual Device* GetParentDevice() = 0;

    virtual void WriteBuffer(Buffer* buf, std::span<const uint8_t> data) = 0;

    virtual void SetPipelineState(PipelineState* ps) = 0;
    virtual void SetClipRect(const gfx::Rect& rect) = 0;
    virtual void SetRenderTarget(Texture* rt) = 0;

    virtual void SetVertexBuffer(Buffer* buf, uint32_t sizeBytes) = 0;

    virtual void SetIndexBuffer(Buffer* buf,
                                IndexFormat format,
                                uint32_t sizeBytes) = 0;

    virtual void SetUniform(BindIndex bindIndex, Buffer* buf) = 0;
    virtual void SetUniform(BindIndex bindIndex, Texture* tex) = 0;

    virtual void DrawIndexed(uint32_t numIndices,
                             uint32_t indexOffset,
                             uint32_t vertexOffset) = 0;
};

// Immutable set of pipeline parameters
// A new object should be created for each unique combination of parameters
// - Shaders
// - Render targets
// - Rasterizer parameters
// - Blend parameters
// - Input layout
struct PipelineStateDesc {
    struct ShaderInfo {
        ShaderType type;
        std::span<const uint8_t> data;
        std::span<ShaderCode::Uniform> uniforms;
        std::span<ShaderCode::Varying> inputs;
        std::span<ShaderCode::Varying> outputs;
    };

    ShaderInfo vertexShader;
    ShaderInfo pixelShader;
    InputLayout layout;
    std::vector<TextureFormat> renderTargets;

public:
    template <std::convertible_to<InputLayout::Element>... T>
    PipelineStateDesc& SetInputLayout(T&&... elem) {
        (layout.Push(std::forward<T>(elem)), ...);
        return *this;
    }

    PipelineStateDesc& SetVertexShader(std::span<const uint8_t> data,
                                       std::span<ShaderCode::Uniform> uniforms,
                                       std::span<ShaderCode::Varying> inputs,
                                       std::span<ShaderCode::Varying> outputs) {
        //
        vertexShader = {ShaderType::Vertex, data, uniforms, inputs, outputs};
        return *this;
    }

    PipelineStateDesc& SetPixelShader(std::span<const uint8_t> data,
                                      std::span<ShaderCode::Uniform> uniforms,
                                      std::span<ShaderCode::Varying> inputs,
                                      std::span<ShaderCode::Varying> outputs) {
        //
        pixelShader = {ShaderType::Pixel, data, uniforms, inputs, outputs};
        return *this;
    }

    PipelineStateDesc& AddRenderTarget(TextureFormat format) {
        renderTargets.push_back(format);
        return *this;
    }
};

// Grouped rendering context state paramters
class PipelineState : public Object {};

// Manages back buffers (final render output)
class SwapChain : public Object {
public:
    virtual RefCountedPtr<Texture> GetCurrentBackBufferRenderTarget() = 0;
    virtual TextureFormat GetFormat() const = 0;
    virtual void Present() = 0;
};

// Main GPU interface. Owns all other objects and resources.
class Device : public RefCountedBase {
public:
    // TODO: Refactor
    // Defined in d3d12/device.cpp
    static ExpectedRef<Device> CreateD3D12(uint32_t index, bool debug);

    virtual ~Device() = default;

    virtual std::unique_ptr<ShaderCodeGenerator>
    CreateShaderCodeGenerator() = 0;

    virtual ShaderCompileResult CompileShader(const std::string& main,
                                              const std::string& code,
                                              ShaderType type,
                                              bool debugBuild) = 0;

    virtual ExpectedRef<PipelineState> CreatePipelineState(
        const PipelineStateDesc& desc) = 0;

    virtual ExpectedRef<Buffer> CreateBuffer(uint32_t size) = 0;
    virtual ExpectedRef<Texture> CreateTexture(const TextureDesc& desc) = 0;

    // Create a context ready for commands recording
    virtual RefCountedPtr<CommandContext> CreateCommandContext() = 0;

    virtual RefCountedPtr<SwapChain> CreateSwapChainForWindow(
        void* window,
        uint32_t width,
        uint32_t height,
        uint32_t numFrames) = 0;

    virtual void Submit(RefCountedPtr<CommandContext> cmdCtx) = 0;

    // Wait until all GPU commands are finished
    virtual void WaitUntilIdle() = 0;
};


};  // namespace gpu