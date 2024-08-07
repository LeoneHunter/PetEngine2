#include "v1.h"
#include "common.h"

#include "thirdparty/imgui/imgui.h"
#include "thirdparty/imgui/imgui_internal.h"

#include <stack>


namespace shader_types {
    using int_t = int32_t;
    using uint_t = uint32_t;
    using float_t = float;
    using float4_t = std::array<float, 4>;
    using int4_t = std::array<int32_t, 4>;
    using float4x4_t = std::array<float, 16>;
} // namespace shader_types





// Types used in shaders
enum class ShaderType {
    Unknown,
    Float,
    Float2,
    Mat44f,
};

template<class T>
constexpr auto ToShaderType() { 
    if constexpr(std::same_as<T, gfx::Mat44f>) { 
        return ShaderType::Mat44f; 
    } else if constexpr(std::same_as<T, float>) { 
        return ShaderType::Float; 
    } else if constexpr(std::same_as<T, float2>) { 
        return ShaderType::Float2; 
    } else { 
        return ShaderType::Unknown; 
    }
}

constexpr size_t SizeOfShaderType(ShaderType type) {
    switch(type) {
        case ShaderType::Float: return sizeof(float);
        case ShaderType::Float2: return sizeof(float2);
        case ShaderType::Mat44f: return sizeof(gfx::Mat44f);
    }
    return 0;
}

static_assert(SizeOfShaderType(ShaderType::Float) == 4, "HLSL float size should be 4 bytes");
static_assert(SizeOfShaderType(ShaderType::Float2) == 8, "HLSL float2 size should be 8 bytes");
static_assert(SizeOfShaderType(ShaderType::Mat44f) == 64, "HLSL float4x4 size should be 4*4*4 (64) bytes");



// Format of shader attachments like textures and buffers
// TODO: Add most useful formats from Vulkan and DX12
enum class ShaderFormat {
    Unknown,
    R32G32_Float,           // float2
    R32G32B32A32_Float,     // float4
    R8G8B8A8_Unorm,         // vec4<uint8_t> 0:255
};




// Describes the layout of vertex buffer
struct InputLayout {

    struct Element {
        std::string  semanticName;
        ShaderFormat format;
        uint32_t     alignedByteOffest;
    };

    // TODO: Consider change to static array
    std::vector<Element> layout;

    InputLayout& Push(const std::string& semanticName,
                      ShaderFormat       format,
                      uint32_t           alignedByteOffset) {
    }

    const Element& operator[](size_t index) const { 
        DASSERT(index < layout.size());
        return layout[index];
    }

    auto begin() const { return layout.begin(); }
    auto end() const { return layout.end(); }
};

void InputLayoutTest() {

    struct ImDrawVert {
        ImVec2  pos;
        ImVec2  uv;
        ImU32   col;
    };

    InputLayout l;
    l.Push("POSITION", ShaderFormat::R32G32_Float, offsetof(ImDrawVert, pos));
    l.Push("TEXCOORD", ShaderFormat::R32G32_Float, offsetof(ImDrawVert, pos));
    l.Push("COLOR", ShaderFormat::R32G32_Float, offsetof(ImDrawVert, pos));

    DASSERT(l[0].semanticName == "POSITION");
}








// Color blend mode used by pixel shaders to blend colors
enum class BlendMode {
    Mix,
    Add,
    Multiply
};

// Assembles a complete shader from parts
// Then needs to be compiled and stored into a PSO
class ShaderBuilder {
public:

    // Sets InputLayout format that shader will expect
    // TODO: Validate that layout has the position attribute
    ShaderBuilder& SetInputLayout(const InputLayout& inputLayout);
    // The pixel shader will use color from a cbuffer
    ShaderBuilder& AddFillColor(BlendMode blendMode);
    // The pixel shader will use colors from the vertex data
    ShaderBuilder& AddVertexColorAttribute(BlendMode blendMode);
    // The pixel shader will use a texture
    // The input layout must have the 'TEXCOORD' semantic
    ShaderBuilder& AddTexture(BlendMode blendMode);

    void Build();
};










// Per frame constants visible to shaders
// Should have exact layout as defined in a shader signature
class PerFrameData: public rendering::NamedBuffer {
public:

    gfx::Mat44f viewProjMatrix;
    float2      someVec2;
    float       someFactor;

    static constexpr std::array<ShaderType, 3> ReflectLayout() {
        return {
            ToShaderType<gfx::Mat44f>(),
            ToShaderType<float2>(),
            ToShaderType<float>(),
        };
    }

    static constexpr std::string_view DebugName() { return "PerFrameData"; }
};





class ForwardColorPass {
public:

    void StreamResources() {
       // - Store all static geometry into a single large buffer
       // - Store all descriptors into a single large heap
    }

    // For resource barriers
    void DeclareInputs(RenderPassContext& context) {
    }

    void DeclareOutputs(RenderPassContext& context) {
    }

    void Prepare(RenderPassContext& context) {
        context.SetViewport();
        context.SetScissorRect();
    }

    void Draw(RenderPassContext& context) {
        // - Set descriptor tables
        for(auto batch = 0; batch < 100; ++batch) {
            // - Set per batch PSO (shaders)
            // - Draw
        }
    }

    // Per pass data
    gfx::Mat44f                     modelViewMatrix;
    Rect                            viewport;
    rendering::RenderTarget*        renderTarget;

    PerFrameData*                   perFrameCBuffer;

    // Per draw call data
    std::vector<int>                materialList;
    std::vector<rendering::Shader*> shaderList;
    std::vector<char>               sortedGeometry;

};



// Allocates buffers and textures from default heaps
class DefaultResourceAllocator {
public:



};

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
using BindIndex = uint32_t;

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


// Context in which a render pass runs
// Abstracts renderer implementation and graphics api
class RenderPassContext {
public:

    // Inputs and outputs should be declared to synchronize 
    // resource usage and allow for multithreaded execution
    void DeclareInput(rendering::Resource* res);
    void DeclareOutput(rendering::Resource* res);

    void SetShader(rendering::Shader* shader);

    void SetViewport();
    void SetScissorRect();

    rendering::RenderTarget* GetCurrentSwapChainTarget();

    void SetConstant(const gfx::Mat44f& val, rendering::ShaderSignature::Parameter parameter);

    // Record the current context into a draw command
    void RecordDraw();





    RefCountedPtr<rendering::Buffer> CreateBuffer(BufferFormat format);
    Handle CreateTexture(TextureFormat format, uint32_t pitch);

    template<std::ranges::contiguous_range T>
    void UploadResourceData(RefCountedPtr<Buffer> buf, const T& data);

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



    // Connects the resource to the shader
    void SetParameter(RefCountedPtr<Texture> res, BindIndex index);

    // Passes the data directly to the shader (not via cbuffers)
    void SetDirectParameter(RefCountedPtr<Texture> res, BindIndex index);

    // Only triangle list is supported
    void SetVertexBuffer(Handle handle);
    void SetIndexBuffer(Handle handle);
    void SetParameter(Handle handle);
    void SetColor(const gfx::Color4f& color);

private:
    std::unique_ptr<d3d12::Device> device_;
    // Alloc for textures and buffers
    std::unique_ptr<DefaultResourceAllocator> resourceAlloc;
    // TODO: Alloc for upload heaps
    std::unique_ptr<d3d12::SlabAllocator> uploadAlloc;
};

using namespace rendering;





// A pass encapsulates all the data for a draw stage
// Could be a UI pass, Postprocess pass, 3D depth pass, 3D color pass, etc.
class ImGuiUIPass: public rendering::RenderPass {
public:

    // Creates a new pass with the provided draw lists
    ImGuiUIPass(std::vector<ImDrawList*> drawLists) {}

    // TODO: Probably not needed because we should begin streaming
    // as soon as we have the data
    void StreamResources() {
        // - Get an allocator
        // - Create a buffer
        // - Transfer draw data into the buffer
    }

    // For resource barriers
    void DeclareInputs(RenderPassContext& context) {
        // fontAtlas
        // renderTarget
        context.DeclareInput(fontAtlas);
    }

    void DeclareOutputs(RenderPassContext& context) {
        // Output render target
        renderTarget = context.GetCurrentSwapChainTarget();
        context.DeclareOutput(renderTarget);
    }

    void Prepare(RenderPassContext& context) {
        context.SetViewport();
        context.SetShader(colorShader);
        // Passes the constant directly into the shader parameter declared in
        // the signature
    }

    void Draw(RenderPassContext& context) {
        // NOTE: Should be finished before start drawing
        RefCountedPtr<VertexBuffer> buffer =
            VertexBuffer::Create(context.GetDevice());
        context.UploadResourceData(buffer, drawLists.front()->VtxBuffer);



        context.SetVertexBuffer(buffer);

        // Bind vertex data
        // context.SetVertexBuffer(vertexBuffer);
		int global_vtx_offset = 0;
		int global_idx_offset = 0;

        for(ImDrawList* cmd_list: drawLists) {
			for(int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++) {
				const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];				
				// Project scissor/clipping rectangles into framebuffer space
				ImVec2 clip_min(pcmd->ClipRect.x, pcmd->ClipRect.y);
				ImVec2 clip_max(pcmd->ClipRect.z, pcmd->ClipRect.w);
				if(clip_max.x <= clip_min.x || clip_max.y <= clip_min.y) {
					continue;
                }
                context.SetShader(texturedShader);
                context.SetParameter(
                    fontAtlas, texturedShader.GetBindIndexByName("texture"));
                context.SetDirectParameter(
                    modelViewMatrix,
                    texturedShader.GetBindIndexByName("modelViewMatrix"));
                context.RecordDraw();
			}
			global_idx_offset += cmd_list->IdxBuffer.Size;
			global_vtx_offset += cmd_list->VtxBuffer.Size;
		}
    }

private:
    // Per pass data
    rendering::Texture*      fontAtlas;
    gfx::Mat44f              modelViewMatrix;
    Rect                     viewport;
    // Output
    rendering::RenderTarget* renderTarget;

    // Shaders we will use
    rendering::Shader*       colorShader;
    rendering::Shader*       texturedShader;

    // Per draw call data
    std::vector<ImDrawList*> drawLists;
    rendering::Buffer*       vertexBuffer;
};

/*=================================== HELLO TRIANGLE =======================================*/

struct HelloTriangle: rendering::RenderPass {

    void Init(RenderPassContext& ctx) {
        // # Global context setup
        // - Create device
        // - Create allocators
        // - Create frame context (contains all the data needed to draw a frame)
        // # Shared objects
        // - Create shader signatures
        // - Create shaders
        // - Create default PSOs and add shaders to them
        // - Create shader parameter buffers (per frame cbuffers and per object
        // cbuffers) # Render pass
        // - Populate cbuffers
        // - Setup the render state
        // - Bind the PSO
        // - Draw

        // Draw the rect with parameters
        constexpr auto rounding = 5;  // pixels
        gfx::Rect rect = gfx::Rect::FromTLWH(100, 100, 400, 200);
        gfx::Color4f color = gfx::colors::kRed;

        // Create buffers
        std::vector<gfx::Point> vertices = {rect.TL(), rect.TR(), rect.BR(),
                                            rect.BL()};
        // 2 triangles
        std::vector<uint32_t> indices = {1, 3, 0, 1, 2, 3};

        // Upload buffers to vram
        vertexBuffer = ctx.CreateBuffer(BufferFormat::Float32);
        ctx.UploadResourceData(vertexBuffer, vertices);
        indexBuffer = ctx.CreateBuffer(BufferFormat::Uint32);
        ctx.UploadResourceData(indexBuffer, indices);
    }

    void Draw(RenderPassContext& ctx) {
        // Begin frame
        ctx.SetBackground(gfx::Color4f::FromHexCode("#104510"));
        ctx.SetVertexBuffer(vertexBuffer);
        ctx.SetIndexBuffer(indexBuffer);
        ctx.SetShader(texturedShader);
        ctx.SetArgument(texturedShader.kTextureArgument, textureView);
        ctx.Draw(4, 0, 0);

        effect.Apply(context, geometry, texture, color1, factor);
        blur.Apply(context, geometry, factor);
    }

private:
    RefCountedPtr<Buffer> vertexBuffer;
    RefCountedPtr<Buffer> indexBuffer;

    rendering::Shader* colorShader;
    rendering::Shader* texturedShader;
    rendering::TextureView textureView;
    rendering::BufferView bufferView;
};



// Helper to write named shader parameters into a buffer
class BufferWriter {
public:

    BufferWriter(const rendering::Buffer* buf);

    template<class T>
    BufferWriter& Write(const std::string& name, const T& param);

};


class ShaderBuilder {};

class ShaderBuilderD3D12: public ShaderBuilder {
public:

    ShaderBuilderD3D12& SetVertexCode(const std::string& code);
    ShaderBuilderD3D12& SetPixelCode(const std::string& code);
    ShaderBuilderD3D12& AddConstant(const std::string& bufferName,
                                    const std::string& constantName,
                                    const std::string& constantType);
    ShaderBuilderD3D12& AddOutput(const std::string& name, const std::string& type);

    void Validate();

    void Build();
};

// Represents a GPU "function" performing some calculations on pixels
// Could be: 
// - Texture fill
// - Color fill
// - Blur
// - Color correction
class PixelEffect {
public:
    // Writes input arguments into |buf|
    // The parameters should be allocated inside the buffer
    // NOTE: At execution time on GPU no checks are performed and parameters are accessed by
    // the byte offset inside the buffer
    void WriteConstants(BufferWriter& buf, int param1, float param2) {
        buf.Write("param1", param1);
        buf.Write("param2", param2);
    }

    // Or abstract the buffers and textures into "inputs"
    void BindInputs(int param1, float param2, TextureView texture);


    // Generates the shader code
    void Build(ShaderBuilderD3D12& builder);

    // Inputs could be allocated from an existing buffer or a dedicated buffer
    // exclusive to this effect could be used
    // Then inputs should be passed to the buffer via LayoutInputs()
    std::vector<std::string>& GetInputs() const;
    std::vector<std::string>& GetOutputs() const;

private:
    // List of parameters used in the shader
    // A shader compiler should connect this parameters to a buffer with
    // arguments
    std::vector<std::string> inputs;
    std::vector<std::string> outputs;
    // Actual shader code
    std::string shaderCode;
};


// Places a texture over geometry
class TextureEffect: public PixelEffect {
public:

    // Binds resources like textures 
    void BindResources(RenderPassContext& ctx, const TextureView& texture);

};

















// A type inside the StructuredBuffer should be 
// trivially copyable for StructuredBuffer::Assign()
// And should have ExtractLayout for type safety
template<class T>
concept BufferType = 
    std::is_standard_layout_v<T> &&
    std::is_trivially_copyable_v<T> &&
    requires (const T& t, rendering::Writer& writer) {
        t.ExtractLayout(writer);
    };

void BufferTest() {
    using namespace rendering;
    using namespace gfx;
    auto& allocator = Allocator::Default();
    auto& streamingManager = StreamingManager::Default();

    // No virtual functions
    // No user defined constructors and destructors
    // Trivially copyable
    struct PerModelData {
        Mat44f modelMat;
        float2 modelVec;

        void ExtractLayout(Writer& writer) const {
            writer.SetName("PerModelData");
            writer << modelMat << modelVec;
        }
    };
    static_assert(BufferType<PerModelData>);

    // Array of static per model data
    std::vector<PerModelData> perModelData;
    StructuredBuffer<PerModelData> buffer(
        &allocator,
        StructuredBufferFlags::CreateDescriptorTable);
    buffer.Reserve(perModelData.size());

    // Two variants
    {
        // Per element
        for(const PerModelData& data: perModelData) {
            buffer.PushBack(data);
        }
        // Complete copy
        buffer.Assign(perModelData);
    }

    // Flush modifications to VRAM
    buffer.Flush();

    // Get descriptor to 'index' element
    Descriptor descriptor = buffer.DescriptorAt(10);
    // context.BindShaderArgument(
    //    signature.GetArgumentByName("PerModelData"), 
    //    descriptor);
}





















