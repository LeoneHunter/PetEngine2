#include "swap_chain.h"
#include "common.h"
#include "context.h"
#include "device.h"
#include "texture.h"

namespace gpu::d3d12 {

SwapChainD3D12::SwapChainD3D12(DeviceD3D12* device,
                               ID3D12CommandQueue* cmdQueue,
                               void* window,
                               uint32_t width,
                               uint32_t height,
                               uint32_t numFrames)
    : device_(device), cmdQueue_(cmdQueue), currentFrameIdx_(0) {

    DASSERT(numFrames <= kMaxNumFrames);
    numFrames = std::clamp(numFrames, 1U, kMaxNumFrames);

    RefCountedPtr<IDXGIFactory4> dxgiFactory;
    RefCountedPtr<IDXGISwapChain1> swapChain1;
    auto hr = CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory));
    DASSERT_HR(hr);

    format_ = TextureFormat::RGBA8Unorm;
    // clang-format off
    const auto desc = DXGI_SWAP_CHAIN_DESC1{
        .Width = width,
        .Height = height,
        .Format = DXGIFormat(format_),
        .Stereo = false,
        .SampleDesc = {
            .Count = 1,
            .Quality = 0,
        },
        .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
        .BufferCount = numFrames,
        .Scaling = DXGI_SCALING_STRETCH,
        .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
        .AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED,
        .Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT,
    };
    // clang-format on
    dxgiFactory->CreateSwapChainForHwnd(cmdQueue, (HWND)window, &desc, nullptr,
                                        nullptr, &swapChain1);
    DASSERT_HR(hr);
    hr = swapChain1->QueryInterface(IID_PPV_ARGS(&swapChain_));
    DASSERT_HR(hr);

    swapChain_->SetMaximumFrameLatency(numFrames);
    currentFrameIdx_ = swapChain_->GetCurrentBackBufferIndex();
}

SwapChainD3D12::~SwapChainD3D12() = default;

RefCountedPtr<Texture> SwapChainD3D12::GetCurrentBackBufferRenderTarget() {
    DXGI_SWAP_CHAIN_DESC swapChainDesc;
    auto hr = swapChain_->GetDesc(&swapChainDesc);
    DASSERT_HR(hr);

    const auto textureDesc = TextureDesc{
        .dimension = TextureDimension::Dim2D,
        .format = format_,
        .width = swapChainDesc.BufferDesc.Width,
        .height = swapChainDesc.BufferDesc.Height,
    };
    RefCountedPtr<ID3D12Resource> backBufferRes;
    hr = swapChain_->GetBuffer(currentFrameIdx_, IID_PPV_ARGS(&backBufferRes));
    DASSERT_HR(hr);

    return MakeRefCounted<TextureD3D12>(device_.Get(), textureDesc,
                                        std::move(backBufferRes));
}

void SwapChainD3D12::Present() {
    // 1 = force vsync
    constexpr int kFrameInterval = 0;
    swapChain_->Present(kFrameInterval, 0);
    currentFrameIdx_ = swapChain_->GetCurrentBackBufferIndex();
}

}  // namespace gpu::d3d12
