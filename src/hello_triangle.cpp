#pragma once
#include "gfx_legacy/native_window.h"
#include "gpu/d3d12/common.h"
#include "gpu/gpu.h"

#include <thread>

class Window {
public:
    static std::unique_ptr<Window> Create(uint32_t width, uint32_t height) {
        auto window = std::make_unique<Window>();

        window->class_ = WNDCLASSEX{
            .cbSize = sizeof(WNDCLASSEX),
            .style = CS_CLASSDC | CS_HREDRAW | CS_VREDRAW,
            .lpfnWndProc = OnWindowEvent,
            .cbClsExtra = 0,
            .cbWndExtra = 0,
            .hInstance = GetModuleHandleW(NULL),
            .hIcon = 0,
            .hCursor = 0,
            .hbrBackground = 0,
            .lpszMenuName = L"",
            .lpszClassName = L"WindowClass",
            .hIconSm = 0,
        };
        ::RegisterClassEx(&window->class_);

        const auto windowPosX = (1920 - width) / 2;
        const auto windowPosY = (1080 - height) / 2;
        window->handle_ = ::CreateWindowEx(
            0L, window->class_.lpszClassName, L"Hello Triangle",
            WS_OVERLAPPEDWINDOW, windowPosX, windowPosY, width, height, NULL,
            NULL, window->class_.hInstance, NULL);

        ::ShowWindow(window->GetHandle(), SW_SHOWDEFAULT);
        ::UpdateWindow(window->GetHandle());
        return window;
    }

    ~Window() {
        ::DestroyWindow(handle_);
        ::UnregisterClass(class_.lpszClassName, class_.hInstance);
    }

    static LRESULT WINAPI OnWindowEvent(HWND hWnd,
                                        UINT msg,
                                        WPARAM wParam,
                                        LPARAM lParam) {
        switch (msg) {
            case WM_DESTROY: {
                ::PostQuitMessage(0);
                return 0;
            }
        }
        return ::DefWindowProc(hWnd, msg, wParam, lParam);
    }

    bool PumpMessage() {
        MSG msg;
        ::PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE);
        ::TranslateMessage(&msg);
        ::DispatchMessage(&msg);
        return msg.message != WM_QUIT;
    }

    HWND GetHandle() const { return handle_; }

private:
    HWND handle_;
    WNDCLASSEX class_;
};

// Asserts std::expected::value() is present
#define TRY(EXPR)           \
    [&] {                   \
        auto result = EXPR; \
        DASSERT(result);    \
        return *result;     \
    }();

int main(int argc, char* argv[]) {
    using namespace gpu;

    constexpr unsigned windowWidth = 800;
    constexpr unsigned windowHeight = 600;
    constexpr unsigned kFrameNum = 2;

    std::unique_ptr<Window> window = Window::Create(windowWidth, windowHeight);
    RefCountedPtr<Device> device = TRY(Device::CreateD3D12(0, kDebugBuild));
    RefCountedPtr<SwapChain> swapChain = device->CreateSwapChainForWindow(
        window->GetHandle(), windowWidth, windowHeight, kFrameNum);

    // float4 x 3 in normalized screen space coords (-1.0 : 1.0)
    // clang-format off
    const float vertices[12] = { 
         1.0f,  1.0f, 0.0f, 1.0f,  
        -1.0f,  1.0f, 0.0f, 1.0f, 
         1.0f, -1.0f, 0.0f, 1.0f,
    };
    const uint32_t indices[3] = {2, 1, 0};
    // clang-format on

    RefCountedPtr<Buffer> vbo = TRY(device->CreateBuffer(SizeBytes(vertices)));
    RefCountedPtr<Buffer> ibo = TRY(device->CreateBuffer(SizeBytes(indices)));

    const std::string vertexShaderCode = R"(
        void main(float4 pos : POSITION, out float4 outPos : SV_Position) {
            outPos = pos;
        };
    )";
    const std::string pixelShaderCode = R"(
        void main(float4 pos : SV_Position, out float4 outCol : SV_Target) {
            outCol = float4(1.0, 0.1, 0.1, 1.0);
        };
    )";

    ShaderCompileResult vertexShader = device->CompileShader(
        "main", vertexShaderCode, ShaderType::Vertex, kDebugBuild);

    ShaderCompileResult pixelShader = device->CompileShader(
        "main", pixelShaderCode, ShaderType::Pixel, kDebugBuild);

    // clang-format off
    const auto psoDesc = gpu::PipelineStateDesc()
        .AddRenderTarget(gpu::TextureFormat::RGBA8Unorm)
        .SetInputLayout(gpu::InputLayout::Element(
            gpu::Semantic::Position, 
            gpu::VertexFormat::Float32x4))
        .SetVertexShader(vertexShader.bytecode, {}, {}, {})
        .SetPixelShader(pixelShader.bytecode, {}, {}, {});
    // clang-format on
    RefCountedPtr<PipelineState> pso =
        TRY(device->CreatePipelineState(psoDesc));

    bool initFrame = true;

    while (window->PumpMessage()) {
        RefCountedPtr<CommandContext> context = device->CreateCommandContext();

        if (initFrame) {
            context->WriteBuffer(vbo.Get(), AsByteSpan(vertices));
            context->WriteBuffer(ibo.Get(), AsByteSpan(indices));
            initFrame = false;
        }
        context->SetRenderTarget(
            swapChain->GetCurrentBackBufferRenderTarget().Get());
        context->SetPipelineState(pso.Get());
        context->SetVertexBuffer(vbo.Get(), SizeBytes(vertices));
        context->SetIndexBuffer(ibo.Get(), IndexFormat::Uint32,
                                SizeBytes(indices));
        context->DrawIndexed(std::size(indices), 0, 0);

        device->Submit(std::move(context));

        swapChain->Present();
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
    // Cleanup
    device->WaitUntilIdle();
    return 0;
}
