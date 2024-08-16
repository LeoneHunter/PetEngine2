#include "common.h"

#include "base/util.h"
#include "task/future.h"

#include <span>


/*============================== RESOURCES ==================================*/
namespace rendering {

class Allocator;

class Device {
public:

    static std::unique_ptr<Device> Create();

};


// TODO: Maybe calculate some hash of the layout for fast comparisons
class Writer {
public:
    // For debug
    void SetName(const std::string& name);
    
    virtual void Write(const gfx::Mat44f& mat);
    virtual void Write(const Vec2f& mat);

    friend Writer& operator<<(Writer& writer, const gfx::Mat44f& val);
    friend Writer& operator<<(Writer& writer, float val);
    friend Writer& operator<<(Writer& writer, float2 val);
};

// Vertex buffer
// Index buffer
// Texture
// Constant buffer
class Resource {
public:
    void AddRef() {}
    void Release() {}
};


// Releases vram resource when goes out of scope
// NOTE: Resource is not released immediatly, only scheduled
class ScopedVramResource {
public:

    ScopedVramResource(int handle) 
        : handle_(handle)
    {}

    ~ScopedVramResource() {
        RemoveRef();
        handle_ = {};
    }

    void AddRef() {}

    void RemoveRef() {}

private:
    int handle_;
    // Owns the memory
    Allocator* allocator;
};


// A unique vram buffer
// Can be accessed only through a single handle
class NamedBuffer {
public:
    ~NamedBuffer() = default;
    virtual void Serialize(Writer& writer) = 0;

private:
    // Handle to vram buffer resource
    ScopedVramResource resource_;
};






// Aka ShaderArgument
// Can be passed to shader
struct Descriptor {
    int virtualAdress{};
};







enum class StructuredBufferFlags {
    None = 0x0,
    // Individual element can be passed to shader
    CreateDescriptorTable = 0x1,
};

// A composite buffer, contains structured data
// Similar to std::vector but for vram
// E.g. StructuredBuffer<MyStruct>
// Individual elements can be accesed through BufferViews
template<class T>
class StructuredBuffer: public Resource {
public:

    StructuredBuffer(Allocator*            allocator,
                     StructuredBufferFlags flags = StructuredBufferFlags::None);

    void Reserve(size_t bytes);
    void Assign(std::span<T> data);

    // Copies data into internal RAM storage until Flush() is called
    void PushBack(const T& data);

    // Copies data into internal RAM storage until Flush() is called
    template<class... Args>
    void EmplaceBack(Args&&... args);
    // Flushes all changes to VRAM
    Future<int> Flush();

    // Returns Descriptor to element at index 'index'
    // Which can be passed to a shader at rendering
    Descriptor DescriptorAt(size_t index);
};

// A type inside the StructuredBuffer should be 
// trivially copyable for StructuredBuffer::Assign()
// And should have ExtractLayout for type safety
template<class T>
concept IsBuffer = 
    std::is_standard_layout_v<T> &&
    std::is_trivially_copyable_v<T> &&
    requires (const T& t, rendering::Writer& writer) {
        t.Serialize(writer);
    };

// Helper for uploading C++ structs into a GPU for shader access
// Ensures the proper packing of the data
// Automatically pads the data when uploading
class UniformBufferFormatter {
public:
    constexpr static auto kShaderAlignment = 16;

    UniformBufferFormatter();

    // Allocates space for a buffer
    // Size of the space should be aligned by 16 bytes (float4 vector)
    void Reserve(size_t sizeBytes) {
        const size_t size = AlignUp(sizeof(T), kShaderAlignment);
    }

    void Write(const gfx::Mat44f& mat) const {
        if(rowBytesLeft_ < kShaderAlignment) {
            Pad();
        }
        const auto size = kShaderAlignment * 4;
        DASSERT(data_.size() - (ptr_ - data_.data()) > size);
        memcpy(ptr_, mat.Data(), size);
    }

    void Write(const Vec2f& vec) const {
        const auto size = sizeof(vec);
        if(rowBytesLeft_ < size) {
            Pad();
        }
        DASSERT(data_.size() - (ptr_ - data_.data()) > size);
        memcpy(ptr_, vec.Data(), size);
        rowBytesLeft_ -= size;
    }

    // Creates an upload command
    void Submit(Buffer* buffer, CommandList* list);

private:
    void Pad() {
        ptr_ += rowBytesLeft_;
        rowBytesLeft_ = kShaderAlignment;
    }

private:
    // Bytes left in a 16byte row of a buffer
    uint32_t rowBytesLeft_ = kShaderAlignment;
    uint8_t* ptr_;
    std::vector<uint8_t> data_;
};


// Stores the result of a render pass
class RenderTarget: public Resource {
public:

};

// An interface to the texture stored in VRAM
class Texture: public Resource {
public:

};

class Buffer: public Resource {
public:

};


} // namespace rendering





/*============================== SHADERS ==================================*/

namespace rendering {


// Basically like a function signature
// Describes how a shader accecces resources
class ShaderSignature {
public:

    struct Parameter { int index; };

    Parameter GetParameterByName(std::string_view name) const;


private:
    std::vector<std::unique_ptr<Resource>> arguments_;
};



// Contains info about compiled vertex shader 
class VertexShader {

};

// Contains info about compiled pixel shader 
class PixelShader {
public:

};

// A complete shader with signature and compiled code
class Shader: public Resource {
public:


private:
    ShaderSignature* signature_;
    uint64_t         hash_;
    std::string      debugName_;
};


} // namespace rendering







namespace rendering {


// VRAM located list of shader visible resources
// Stores resource descriptors internally
// E.g. matrices, textures, buffers
class ResourceTable: public Resource {
public:

    void PushBackTexture(Texture* texture);
    void PushBackMatrix44(const gfx::Mat44f& matrix);
    void PushBackVector3();
    void Pushu32(uint32_t entry);
    void PushF32(float entry);
    void PushBuffer(size_t size);
};



// Reusable object with configuration of the GPU pipeline
// Contains:
// - Shader
// - Topology type
// - Shader signature
// - Blend state
// - Rasterizer state
// - Depth stencil state
class PipelineState {
public:



private:
    Shader*          shader_;
    ShaderSignature* signature_;
    uint64_t              hash_;
    std::string      debugName_;
};



/**
 * Contains all state needed to render something
 * Basically there's one render pass per render target binding
 * E.g. rendering a shadow map onto a shadow map render target requires one pass
 * 
 * 1. Inputs:
 * - Buffers
 * - Textures
 * - Vertex and index buffer
 * 2. State:
 * - PSO
 * - Root signature 
 * - Draw arguments 
 * 3. Outputs:
 * - Render targets
 * - Buffers
 * 
 */
class RenderPass {
public:

    RenderPass(const std::string& name) {}

    // Bind resource to the shader output
    void SetRenderTarget(RenderTarget* renderTarget, uint32_t index);
    void SetSignature(ShaderSignature* signature);

};


// Frame independent flobal render state
class RenderState {
public:

};

// Stores all state needed for a frame rendering
class FrameContext {
public:


};



// Lowest level interface to the GPU
class GPUDevice {
public:

    // Create interface to the GPU
    static std::unique_ptr<GPUDevice> Create();

private:
};






enum class AllocationFlags {
    None = 0,
    StaticHeap = 0x1,
    DynamicHeap = 0x2,
};


// Responsible for resources allocation
// Could be several types of allocators:
// - Linear allocator
// - Frame allocator
// - Pool allocator
class Allocator {
public:

    static std::unique_ptr<Allocator> Create(GPUDevice* device);
    static Allocator& Default();

    std::shared_ptr<Texture> CreateTexture();
    std::shared_ptr<Buffer> CreateBuffer();

public:
    Allocator(const Allocator&) = delete;
    Allocator& operator=(const Allocator&) = delete;

    Allocator(Allocator&&) = delete;
    Allocator& operator=(Allocator&&) = delete;

};









// A native window swap chain
class SwapChain {
public:

};

// Executes render passes
class Renderer {
public:

    static std::unique_ptr<Renderer> Create(GPUDevice* device);

};






// Represents a context of main render or compute pass
// Commands like:
// - Draw: draw call
// - Dispatch: compute call
// - Execute indirect: indirect draw or dispatch call
// - Copy resource
// - Set rendering state
// - Set pipeline state
// - Set descriptor heap
// - Set render target
class CommandList {
public:

    void SetDescriptorHeap();

};

class ImmediateCommandList: public CommandList {
public:

};


class CommandContext {
public:

    ImmediateCommandList* CreateImmediateCommandList();

};


enum class StreamingStatus {
    Pending,
    Complete,
    Error,
    Cancelled,
};

// Returns a handle to the scheduled streaming request
// Could be queried or waited upon
// Internally stores a pointer only so can be easily moved
class StreamingRequest {
public:
    using Future = ::Future<StreamingStatus>;

    // Uniquely identifies the request so no copy
    StreamingRequest(const StreamingRequest&) = delete;
    StreamingRequest& operator=(const StreamingRequest&) = delete;

    Future GetFuture();
    // TODO: Decide whether we need cancelling
    void   Cancel();
    void   Wait();
    int    GetStatus();
    int    GetProgress();

private:
    Future result_;
};

// Streams resources to VRAM
// Uses special upload heap
// Probably should run on a separate thread or event loop
// And use a copy queue or several copy queues
class StreamingManager {
public:
    static StreamingManager& Default();

    // Upload commands will be placed into cmdList
    static RefCountedPtr<StreamingManager> Create(
            RefCountedPtr<CommandList> cmdList);

    // Schedules a resource to be loaded into vram
    // Async and batched
    // Should be called before starting to render otherwise
    //     the request might be completed only next frame
    void Schedule(RefCountedPtr<Resource> target, std::span<char> data);
    StreamingRequest ScheduleWithPromise(RefCountedPtr<Resource> target,
                                         std::span<char> data);

    // Flush all pending commands
    void Flush();
};





enum class HeapType {

};






// Frame specific data
class FrameContext {
public:



};


// Containts glabal state for the rendering
// Display swap chains, objects, allocators etc.
class GlobalContext {
public:


};




} // namespace rendering
