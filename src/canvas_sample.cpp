#pragma once
#include "canvas.h"
#include "gpu/gpu.h"

#include <thread>
#include "gfx_legacy/native_window.h"
#include "gpu/d3d12/common.h"

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
            .hInstance = ::GetModuleHandle(NULL),
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

int main(int argc, char* argv[]) {

    using namespace gpu;
    const uint32_t kDeviceID = 0;
    const uint32_t kBackBufferNum = 2;
    const Vec2f windowSize = nativeWindow_->GetSize();
    RefCountedPtr<Device> device =
        TRY(Device::CreateD3D12(kDeviceID, kDebugBuild));
    RefCountedPtr<SwapChain> swapChain = device->CreateSwapChainForWindow(
        nativeWindow_->GetNativeHandle(), windowSize.x, windowSize.y,
        kBackBufferNum);
    RefCountedPtr<CommandContext> context = device->CreateCommandContext();
    pass->RecordCommands(context.Get());
    device->Submit(std::move(context));
    swapChain->Present();

    // - Create canvas and draw a shape
    const auto canvasSize = gfx::Size(1280, 800);
    const auto canvasCol = gfx::Color4f::FromHexCode("#3ba1a1");

    const auto shape = gfx::Rect::FromTLWH(500, 300, 100, 100);
    const auto shapeCol = gfx::Color4f::FromHexCode("#a1453b");

    std::unique_ptr<gfx::Canvas> canvas = gfx::Canvas::Create();

    canvas->Resize(1280, 800);
    canvas->DrawFill(canvasCol);
    canvas->DrawRect(shape, shapeCol);

    canvas->Render(context);
    canvas->Reset();

    // - Get draw pass
    auto drawPass = canvas->CreateDrawPass();
    auto window = Window::Create(canvasSize);
    window->Render(std::move(drawPass));

    // Wait for the user to close the window
    window->RunPump();
    return 0;
}
