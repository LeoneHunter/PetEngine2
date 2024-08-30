#include "device.h"
#include "gpu/d3d12/buffer.h"
#include "gpu/d3d12/context.h"
#include "gpu/d3d12/pipeline_state.h"
#include "gpu/d3d12/swap_chain.h"
#include "gpu/d3d12/texture.h"

#include "gpu/d3d12/slab_allocator.h"
#include "gpu/shader/hlsl_codegen.h"

#include <expected>

#include <d3dcompiler.h>


namespace gpu::d3d12 {

struct DeviceCreateResult {
    RefCountedPtr<ID3D12Device> device;
    AdapterProps props;
};

// Tries to create a device with specified index from provided factory
std::expected<DeviceCreateResult, GenericErrorCode> TryCreateDevice(
    IDXGIFactory6* factory,
    int index) {
    //
    RefCountedPtr<IDXGIAdapter1> adapter;
    RefCountedPtr<ID3D12Device> device;

    if (factory->EnumAdapters1(index, &adapter) == DXGI_ERROR_NOT_FOUND) {
        return std::unexpected(GenericErrorCode::NotFound);
    }

    DXGI_ADAPTER_DESC1 desc{};
    adapter->GetDesc1(&desc);
    // Skip software adapters
    if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
        return std::unexpected(GenericErrorCode::NotFound);
    }
    // Try to creata a device
    auto hr = D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0,
                                IID_PPV_ARGS(&device));
    if (FAILED(hr)) {
        return std::unexpected(GenericErrorCode::InternalError);
    }

    // Can be used to determine integrated adapters
    uint64_t nonLocalMemory = 0;
    {
        DXGI_QUERY_VIDEO_MEMORY_INFO nonLocalInfo{};
        RefCountedPtr<IDXGIAdapter3> adapter3;

        hr = adapter->QueryInterface(IID_PPV_ARGS(&adapter3));
        if (SUCCEEDED(hr)) {
            const auto nodeIndex = 0;
            hr = adapter3->QueryVideoMemoryInfo(
                nodeIndex, DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL, &nonLocalInfo);
            if (SUCCEEDED(hr)) {
                nonLocalMemory = nonLocalInfo.Budget;
            }
        }
    }

    // Get feature level
    D3D_FEATURE_LEVEL featureLevelMax = D3D_FEATURE_LEVEL_12_0;
    {
        D3D_FEATURE_LEVEL featuresToTest[] = {D3D_FEATURE_LEVEL_12_0,
                                              D3D_FEATURE_LEVEL_12_1,
                                              D3D_FEATURE_LEVEL_12_2};
        D3D12_FEATURE_DATA_FEATURE_LEVELS featureLevels{};
        featureLevels.pFeatureLevelsRequested = featuresToTest;
        featureLevels.NumFeatureLevels = (UINT)std::size(featuresToTest);
        hr = device->CheckFeatureSupport(D3D12_FEATURE_FEATURE_LEVELS,
                                         &featureLevels, sizeof(featureLevels));
        if (SUCCEEDED(hr)) {
            featureLevelMax = featureLevels.MaxSupportedFeatureLevel;
        }
    }

    // Get shader model
    D3D_SHADER_MODEL shaderModelMax = D3D_SHADER_MODEL_5_1;
    {
        D3D_SHADER_MODEL shaderModelsToTest[] = {
            D3D_SHADER_MODEL_6_7, D3D_SHADER_MODEL_6_6, D3D_SHADER_MODEL_6_5,
            D3D_SHADER_MODEL_6_4, D3D_SHADER_MODEL_6_3, D3D_SHADER_MODEL_6_2,
            D3D_SHADER_MODEL_6_1, D3D_SHADER_MODEL_6_0,
        };
        D3D12_FEATURE_DATA_SHADER_MODEL shaderModelFeature{};

        for (D3D_SHADER_MODEL shaderModel : shaderModelsToTest) {
            shaderModelFeature.HighestShaderModel = shaderModel;
            hr = device->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL,
                                             &shaderModelFeature,
                                             sizeof(shaderModelFeature));
            if (SUCCEEDED(hr)) {
                shaderModelMax = shaderModelFeature.HighestShaderModel;
                break;
            }
        }
    }

    // Check resource tiers
    D3D12_FEATURE_DATA_D3D12_OPTIONS featureOptions{};
    device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &featureOptions,
                                sizeof(featureOptions));

    return DeviceCreateResult{
        .device = std::move(device),
        .props = {.adapterIndex = index,
                  .adapterDesc = desc,
                  .nonLocalMemory = nonLocalMemory,
                  .featureLevelMax = featureLevelMax,
                  .shaderModelMax = shaderModelMax,
                  .resourceBindingTier = featureOptions.ResourceBindingTier,
                  .resourceHeapTier = featureOptions.ResourceHeapTier}};
}

// static
std::vector<AdapterProps> DeviceFactory::EnumerateAdapters() {
    RefCountedPtr<IDXGIFactory6> dxgiFactory;
    const DWORD dxgiFactoryFlags = 0;
    auto hr = CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&dxgiFactory));
    DASSERT_HR(hr);

    std::vector<AdapterProps> adaptersInfo;
    std::expected<DeviceCreateResult, GenericErrorCode> result;
    int i = 0;

    for (;;) {
        result = TryCreateDevice(dxgiFactory.Get(), i);
        if (!result) {
            continue;
        }
        adaptersInfo.push_back(result->props);
    }
    return adaptersInfo;
}

// static
gpu::ExpectedRef<DeviceD3D12> DeviceFactory::CreateDevice(
    int adapterIndex,
    bool enableDebugLayer) {

    // DXGI debug layer
    if (enableDebugLayer) {
        RefCountedPtr<ID3D12Debug> debug;
        auto hr = D3D12GetDebugInterface(IID_PPV_ARGS(&debug));
        DASSERT_HR(hr);
        debug->EnableDebugLayer();

        // Enable GPU validation
        RefCountedPtr<ID3D12Debug1> debug1;
        hr = debug->QueryInterface(IID_PPV_ARGS(&debug1));
        if (SUCCEEDED(hr)) {
            debug1->SetEnableGPUBasedValidation(true);
        }

#ifndef NDEBUG
        // Set DXGI debug messages filtering
        RefCountedPtr<IDXGIInfoQueue> infoQueue;
        hr = DXGIGetDebugInterface1(0, IID_PPV_ARGS(&infoQueue));
        DASSERT_HR(hr);

        infoQueue->SetBreakOnSeverity(
            DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, true);
        infoQueue->SetBreakOnSeverity(
            DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, true);
#endif
    }

    RefCountedPtr<IDXGIFactory6> dxgiFactory;
    AdapterProps props;
    RefCountedPtr<ID3D12Device> device;
    {
        const DWORD flags = 0;
        auto hr = CreateDXGIFactory2(flags, IID_PPV_ARGS(&dxgiFactory));
        DASSERT_HR(hr);

        std::expected<DeviceCreateResult, GenericErrorCode> result;
        result = TryCreateDevice(dxgiFactory.Get(), adapterIndex);
        DASSERT_F(result, "Cannot create a device with index {}: {}",
                  adapterIndex, result.error());
        if (!result) {
            return std::unexpected(GenericErrorCode::IndexNotFound);
        }
        props = result->props;
        device = std::move(result->device);
    }

    // Set main messages filtering
    if (enableDebugLayer) {
        RefCountedPtr<ID3D12InfoQueue> infoQueue;
        auto hr = device->QueryInterface(IID_PPV_ARGS(&infoQueue));
        DASSERT_HR(hr);

        D3D12_INFO_QUEUE_FILTER filter{};
        // Disable info messages
        // D3D12_MESSAGE_SEVERITY severities[] = {
        //     D3D12_MESSAGE_SEVERITY_INFO
        // };
        // filter.DenyList.NumSeverities = (UINT)std::size(severities);
        // filter.DenyList.pSeverityList = severities;

        // From https://github.com/microsoft/DirectX-Graphics-Samples
        D3D12_MESSAGE_ID ids[] = {
            // This occurs when there are uninitialized descriptors in a
            // descriptor table, even when a
            // shader does not access the missing descriptors.
            D3D12_MESSAGE_ID_INVALID_DESCRIPTOR_HANDLE,

            // Triggered when a shader does not export all color components of a
            // render target, such as
            // when only writing RGB to an R10G10B10A2 buffer, ignoring alpha.
            D3D12_MESSAGE_ID_CREATEGRAPHICSPIPELINESTATE_PS_OUTPUT_RT_OUTPUT_MISMATCH,

            // This occurs when a descriptor table is unbound even when a shader
            // does not access the missing
            // descriptors.  This is common with a root signature shared between
            // disparate shaders that
            // don't all need the same types of resources.
            D3D12_MESSAGE_ID_COMMAND_LIST_DESCRIPTOR_TABLE_NOT_SET,

            // RESOURCE_BARRIER_DUPLICATE_SUBRESOURCE_TRANSITIONS
            (D3D12_MESSAGE_ID)1008,
        };
        filter.DenyList.NumIDs = (UINT)std::size(ids);
        filter.DenyList.pIDList = ids;

        infoQueue->PushStorageFilter(&filter);
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
    }
    return RefCountedPtr<DeviceD3D12>(new DeviceD3D12(std::move(device)));
}

DeviceD3D12::DeviceD3D12(RefCountedPtr<ID3D12Device> device) : device_(device) {
    CreateAllocators();
    const auto desc = D3D12_COMMAND_QUEUE_DESC{
        .Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
        .Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
    };
    auto hr = device_->CreateCommandQueue(&desc, IID_PPV_ARGS(&cmdQueue_));
    DASSERT_HR(hr);
    hr = device->CreateFence(0, D3D12_FENCE_FLAG_SHARED, IID_PPV_ARGS(&fence_));
    DASSERT_HR(hr);
}

void DeviceD3D12::CreateAllocators() {
    // Default (shader visible) heap for resources
    heapAlloc_ = HeapAllocator::Create(this, D3D12_HEAP_TYPE_DEFAULT);
    defaultAlloc_ = SlabAllocator::Create(heapAlloc_.get(), nullptr);
}

ExpectedRef<PipelineState> DeviceD3D12::CreatePipelineState(
    const PipelineStateDesc& desc) {
    DASSERT(!desc.vertexShader.data.empty());
    DASSERT(!desc.pixelShader.data.empty());
    return MakeRefCounted<PipelineStateD3D12>(this, desc);
}

RefCountedPtr<CommandContext> DeviceD3D12::CreateCommandContext() {
    currentFrameIndex_ = currentSwapChain_->GetCurrentFrameIndex();
    RefCountedPtr<CommandContext> cmdCtx;
    // Look in cache for completed ones
    for (auto it = pendingContexts_.begin(); it != pendingContexts_.end();
         ++it) {
        PendingContext& ctx = *it;
        if (fence_->GetCompletedValue() >= ctx.fenceValue) {
            cmdCtx = ctx.commandContext;
            CastCommandContext(cmdCtx.Get())->Reset();
            pendingContexts_.erase(it);
            break;
        }
    }
    if (!cmdCtx) {
        cmdCtx = MakeRefCounted<ContextD3D12>(this);
    }
    return cmdCtx;
}

ExpectedRef<Buffer> DeviceD3D12::CreateBuffer(uint32_t size) {
    // DASSERT(flags == ResourceFlags::Default);
    // Create vertex, index, constant buffers
    // If |size| is less than |kMinAlignment| suballocate manually
    // Use a temp buffer per frame number (2 or 3)
    // TODO: Do suballocation for size < kMinSlotSize (64 KiB)
    DASSERT(size > 0);
    const auto allocSize = std::max(size, kResourceMinAlignment);
    // D3D12 validation layer emits error during cbv creation for size < 256
    size = std::max(size,
                    (uint32_t)D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

    // Dedicated allocation for size > kMaxSlotSize (4 MiB)
    const bool useDedicated = size > kResourceMaxAlignment;
    RefCountedPtr<ID3D12Resource> res;
    ResourceHeapAllocation addr;

    if (useDedicated) {
        const auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
        const auto desc = CD3DX12_RESOURCE_DESC::Buffer(size);
        auto hr = device_->CreateCommittedResource(
            &heapProps, D3D12_HEAP_FLAG_NONE, &desc,
            D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&res));
        if (FAILED(hr)) {
            return std::unexpected(GenericErrorCode::OutOfMemory);
        }

    } else {
        const auto result = defaultAlloc_->Allocate(allocSize);
        if (!result) {
            return std::unexpected(result.error());
        }
        addr = *result;
        const auto desc = CD3DX12_RESOURCE_DESC::Buffer(size);
        const auto hr = device_->CreatePlacedResource(
            addr.heap, addr.offset, &desc, D3D12_RESOURCE_STATE_COMMON, nullptr,
            IID_PPV_ARGS(&res));
        if (FAILED(hr)) {
            return std::unexpected(GenericErrorCode::OutOfMemory);
        }
    }
    return MakeRefCounted<BufferD3D12>(this, addr, res, size);
}

ExpectedRef<Texture> d3d12::DeviceD3D12::CreateTexture(
    const TextureDesc& desc) {

    DASSERT_M(false, "Unimplemented");
    return ExpectedRef<Texture>();
}

RefCountedPtr<SwapChain> DeviceD3D12::CreateSwapChainForWindow(
    void* window,
    uint32_t width,
    uint32_t height,
    uint32_t numFrames) {
    // TODO: Stash old swap chain and wait untill all operations are finished
    DASSERT(!currentSwapChain_);
    DASSERT(window);
    currentSwapChain_ = MakeRefCounted<SwapChainD3D12>(
        this, cmdQueue_.Get(), window, width, height, numFrames);
    currentFrameIndex_ = currentSwapChain_->GetCurrentFrameIndex();
    return currentSwapChain_;
}

void d3d12::DeviceD3D12::Submit(RefCountedPtr<CommandContext> cmdCtx) {
    currentFrameIndex_ = currentSwapChain_->GetCurrentFrameIndex();
    ContextD3D12* contextD3D12 = CastCommandContext(cmdCtx.Get());
    contextD3D12->Flush(cmdQueue_.Get());

    lastFenceValue_ += 1;
    auto hr = cmdQueue_->Signal(fence_.Get(), lastFenceValue_);
    DASSERT_HR(hr);
    pendingContexts_.push_back(PendingContext{
        .fenceValue = lastFenceValue_,
        .commandContext = GetRef(contextD3D12),
    });
}

void d3d12::DeviceD3D12::WaitUntilIdle() {
    if (fence_->GetCompletedValue() >= lastFenceValue_) {
        return;
    }
    // clang-format off
    HANDLE event = ::CreateEvent(
        NULL,   // default security attributes
        TRUE,   // manual-reset event
        FALSE,  // initial state is nonsignaled
        TEXT("FenceComplete")  // object name
    );
    // clang-format on
    fence_->SetEventOnCompletion(lastFenceValue_, event);
    ::WaitForSingleObject(event, INFINITE);
    ::CloseHandle(event);
}

void DeviceD3D12::FreeResource(ResourceHeapAllocation addr, ID3D12Resource* res) {
    res->Release();
    if(addr.heap) {
        defaultAlloc_->Free(addr);
    }
}

std::unique_ptr<ShaderCodeGenerator> DeviceD3D12::CreateShaderCodeGenerator() {
    return std::unique_ptr<ShaderCodeGenerator>(new gpu::HLSLCodeGenerator());
}

ShaderCompileResult DeviceD3D12::CompileShader(const std::string& main,
                                               const std::string& code,
                                               ShaderType type,
                                               bool debugBuild) {
    std::string targetString;
    switch (type) {
        case ShaderType::Vertex: targetString = "vs_5_1"; break;
        case ShaderType::Pixel: targetString = "ps_5_1"; break;
        default: DASSERT_M(false, "Unknown shader type");
    }
    ShaderCompileResult result;

    UINT compileFlags = 0;
    if (debugBuild) {
        compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
    }
    RefCountedPtr<ID3DBlob> errors;
    RefCountedPtr<ID3DBlob> bytecode;
    HRESULT hr = D3DCompile(code.c_str(), code.size(), "", nullptr, nullptr,
                            main.c_str(), targetString.c_str(), compileFlags, 0,
                            &bytecode, &errors);
    if (FAILED(hr)) {
        result.msg = std::string((char*)errors->GetBufferPointer(),
                                 errors->GetBufferSize());
        result.hasErrors = true;
    } else {
        result.bytecode.resize(bytecode->GetBufferSize());
        memcpy(result.bytecode.data(), bytecode->GetBufferPointer(),
               result.bytecode.size());
        result.hasErrors = false;
    }
    return result;
}

}  // namespace gpu::d3d12


// Declared in gpu/gpu.h
gpu::ExpectedRef<gpu::Device> gpu::Device::CreateD3D12(uint32_t index,
                                                       bool debug) {
    return d3d12::DeviceFactory::CreateDevice(index, debug);
}
