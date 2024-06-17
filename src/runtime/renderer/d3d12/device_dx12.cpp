#include "device_dx12.h"

#include <expected>

#include <dxgi1_6.h>

#ifndef NDEBUG
    #include <dxgidebug.h>
#endif

namespace dx12 {

struct DeviceCreateResult {
    RefCountedPtr<ID3D12Device> device;
    Device::Properties          props;
};

// Tries to create a device with specified index from provided factory
std::expected<DeviceCreateResult, Error> TryCreateDevice(IDXGIFactory6* factory,
                                                         int index) {
    RefCountedPtr<IDXGIAdapter1> adapter;
    RefCountedPtr<ID3D12Device> device;

    if(factory->EnumAdapters1(index, &adapter) == DXGI_ERROR_NOT_FOUND) {
        return std::unexpected(Error::AdapterNotFound);
    }

    DXGI_ADAPTER_DESC1 desc{};
    adapter->GetDesc1(&desc);
    // Skip software adapters
    if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
        return std::unexpected(Error::AdapterSoftware);
    }            
    // Try to creata a device
    auto hr = D3D12CreateDevice(
            adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device));
    if(FAILED(hr)) {
        return std::unexpected(Error::AdapterCannotCreate);
    }

    // Can be used to determine integrated adapters
    uint64_t nonLocalMemory = 0;
    {
        DXGI_QUERY_VIDEO_MEMORY_INFO nonLocalInfo{};
        RefCountedPtr<IDXGIAdapter3> adapter3;

        hr = adapter->QueryInterface(IID_PPV_ARGS(&adapter3));
        if(SUCCEEDED(hr)) {
            const auto nodeIndex = 0;
            hr = adapter3->QueryVideoMemoryInfo(
                    nodeIndex, DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL, &nonLocalInfo);
            if(SUCCEEDED(hr)) {
                nonLocalMemory = nonLocalInfo.Budget;
            }
        }
    }

    // Get feature level
    D3D_FEATURE_LEVEL featureLevelMax = D3D_FEATURE_LEVEL_12_0;
    {
        D3D_FEATURE_LEVEL featuresToTest[] = {
            D3D_FEATURE_LEVEL_12_0,
            D3D_FEATURE_LEVEL_12_1,
            D3D_FEATURE_LEVEL_12_2
        };
        D3D12_FEATURE_DATA_FEATURE_LEVELS featureLevels{};
        featureLevels.pFeatureLevelsRequested = featuresToTest;
        featureLevels.NumFeatureLevels = (UINT)std::size(featuresToTest);
        hr = device->CheckFeatureSupport(
                D3D12_FEATURE_FEATURE_LEVELS, &featureLevels, sizeof(featureLevels));
        if(SUCCEEDED(hr)) {
            featureLevelMax = featureLevels.MaxSupportedFeatureLevel;
        }
    }

    // Get shader model
    D3D_SHADER_MODEL shaderModelMax = D3D_SHADER_MODEL_5_1;
    {
        D3D_SHADER_MODEL shaderModelsToTest[] = {
            D3D_SHADER_MODEL_6_7,	
            D3D_SHADER_MODEL_6_6,	
            D3D_SHADER_MODEL_6_5,	
            D3D_SHADER_MODEL_6_4,	
            D3D_SHADER_MODEL_6_3,	
            D3D_SHADER_MODEL_6_2,	
            D3D_SHADER_MODEL_6_1,	
            D3D_SHADER_MODEL_6_0,	
        };
        D3D12_FEATURE_DATA_SHADER_MODEL shaderModelFeature{};

        for(D3D_SHADER_MODEL shaderModel: shaderModelsToTest) {
            shaderModelFeature.HighestShaderModel = shaderModel;
            hr = device->CheckFeatureSupport(
                    D3D12_FEATURE_SHADER_MODEL, 
                    &shaderModelFeature, 
                    sizeof(shaderModelFeature));
            if(SUCCEEDED(hr)) {
                shaderModelMax = shaderModelFeature.HighestShaderModel;
                break;
            }
        }
    }

    // Check resource tiers
    D3D12_FEATURE_DATA_D3D12_OPTIONS featureOptions{};
    device->CheckFeatureSupport(
            D3D12_FEATURE_D3D12_OPTIONS, &featureOptions, sizeof(featureOptions));

    return DeviceCreateResult{
        .device = std::move(device),
        .props = {
            .adapterIndex = index,
            .adapterDesc = desc,
            .nonLocalMemory = nonLocalMemory,
            .featureLevelMax = featureLevelMax,
            .shaderModelMax = shaderModelMax,
            .resourceBindingTier = featureOptions.ResourceBindingTier,
            .resourceHeapTier = featureOptions.ResourceHeapTier
        }
    };
}

// static
std::vector<Device::Properties> Device::EnumerateAdapters() {
    RefCountedPtr<IDXGIFactory6> dxgiFactory;
    const DWORD dxgiFactoryFlags = 0;
    auto hr = CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&dxgiFactory));
    DASSERT_HR(hr);

    std::vector<Device::Properties> adaptersInfo;
    std::expected<DeviceCreateResult, Error> result;
    int i = 0;

    for(;;) {
        result = TryCreateDevice(dxgiFactory.Get(), i);
        if(!result) {
            continue;
        }
        adaptersInfo.push_back(result->props);
    }
    return adaptersInfo;
}

// static
std::unique_ptr<Device> Device::Create(int adapterIndex,
                                       bool enableDebugLayer) {
    auto out = std::make_unique<Device>();
    // DXGI debug layer
    if(enableDebugLayer) {
        RefCountedPtr<ID3D12Debug> debug;
        auto hr = D3D12GetDebugInterface(IID_PPV_ARGS(&debug));
        DASSERT_HR(hr);
        debug->EnableDebugLayer();

        // Enable GPU validation
        RefCountedPtr<ID3D12Debug1> debug1;
        hr = debug->QueryInterface(IID_PPV_ARGS(&debug1));
        if(SUCCEEDED(hr)) {
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
    Device::Properties           deviceProps;
    RefCountedPtr<ID3D12Device>  device;
    {
        const DWORD flags = 0;
        auto hr = CreateDXGIFactory2(flags, IID_PPV_ARGS(&dxgiFactory));
        DASSERT_HR(hr);

        std::expected<DeviceCreateResult, Error> result;
        result = TryCreateDevice(dxgiFactory.Get(), adapterIndex);
        DASSERT_F(result, "Cannot create a device with index {}: {}", 
                adapterIndex, result.error());
        deviceProps = result->props;
        device = std::move(result->device);
    }

    // Set main messages filtering
    if(enableDebugLayer) {
        RefCountedPtr<ID3D12InfoQueue> infoQueue;
        auto hr = device->QueryInterface(IID_PPV_ARGS(&infoQueue));
        DASSERT_HR(hr);

        D3D12_INFO_QUEUE_FILTER filter{};
        // Disable info messages
        D3D12_MESSAGE_SEVERITY severities[] = { D3D12_MESSAGE_SEVERITY_INFO };
        filter.DenyList.NumSeverities = (UINT)std::size(severities);
        filter.DenyList.pSeverityList = severities;

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

        filter.DenyList.NumSeverities = (UINT)std::size(severities);
        filter.DenyList.pSeverityList = severities;
        filter.DenyList.NumIDs = (UINT)std::size(ids);
        filter.DenyList.pIDList = ids;

        infoQueue->PushStorageFilter(&filter);
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
    } 
    out->device1_ = std::move(device);
    out->props_ = deviceProps;
    return std::move(out);
}

} // namespace dx12







