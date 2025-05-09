#include <ft2build.h>
#include FT_FREETYPE_H

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <limits>
#include <thread>
#include <unordered_map>
#include <vector>

// libpng
#include <png.h>

#include "gfx2/font.h"
#include "gfx2/glyph_cache.h"
#include "gpu/d3d12/common.h"
#include "gpu/gpu.h"

#undef max
#undef min

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
    window->handle_ =
      ::CreateWindowEx(0L, window->class_.lpszClassName, L"Hello Triangle",
                       WS_OVERLAPPEDWINDOW, windowPosX, windowPosY, width,
                       height, NULL, NULL, window->class_.hInstance, NULL);

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

static void AppendRect(std::vector<uint8_t>& vertices,
                       std::vector<uint32_t>& indices,
                       gfx::Rect positions,
                       gfx::Rect texcoords) {
  // Convert the position and texture coordinates to raw bytes and append to
  // buffer
  auto appendFloat = [&vertices](float value) {
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&value);
    vertices.insert(vertices.end(), bytes, bytes + sizeof(float));
  };
  // First triangle
  // Vertex 1: Top-left
  appendFloat(positions.x);
  appendFloat(positions.y);
  appendFloat(texcoords.x);
  appendFloat(texcoords.y);
  // Vertex 2: Top-right
  appendFloat(positions.x + positions.width);
  appendFloat(positions.y);
  appendFloat(texcoords.x + texcoords.width);
  appendFloat(texcoords.y);
  // Vertex 3: Bottom-left
  appendFloat(positions.x);
  appendFloat(positions.y + positions.height);
  appendFloat(texcoords.x);
  appendFloat(texcoords.y + texcoords.height);

  // Second triangle
  // Vertex 1: Bottom-left (same as above)
  appendFloat(positions.x);
  appendFloat(positions.y + positions.height);
  appendFloat(texcoords.x);
  appendFloat(texcoords.y + texcoords.height);
  // Vertex 2: Top-right (same as above)
  appendFloat(positions.x + positions.width);
  appendFloat(positions.y);
  appendFloat(texcoords.x + texcoords.width);
  appendFloat(texcoords.y);
  // Vertex 3: Bottom-right
  appendFloat(positions.x + positions.width);
  appendFloat(positions.y + positions.height);
  appendFloat(texcoords.x + texcoords.width);
  appendFloat(texcoords.y + texcoords.height);
}

class Renderer {
public:
  void Init(Window* window, uint32_t width, uint32_t height) {
    device_ = TRY(gpu::Device::CreateD3D12(0, kDebugBuild));
    swapChain_ =
      device_->CreateSwapChainForWindow(window->GetHandle(), width, height, 2);

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

    gpu::ShaderCompileResult vertexShader = device_->CompileShader(
      "main", vertexShaderCode, gpu::ShaderUsage::Vertex, kDebugBuild);

    gpu::ShaderCompileResult pixelShader = device_->CompileShader(
      "main", pixelShaderCode, gpu::ShaderUsage::Pixel, kDebugBuild);

    // clang-format off
    const auto psoDesc = gpu::PipelineStateDesc()
      .AddRenderTarget(gpu::TextureFormat::R8Unorm)
      .SetInputLayout(gpu::InputLayout::Element(gpu::Semantic::Position,
                                                gpu::VertexFormat::Float32x4),
                      gpu::InputLayout::Element(gpu::Semantic::Texcoord,
                                                gpu::VertexFormat::Float32x2))
      .SetVertexShader(vertexShader.bytecode, {}, {}, {})
      .SetPixelShader(pixelShader.bytecode, {}, {}, {});
    // clang-format on
    pso_ = TRY(device_->CreatePipelineState(psoDesc));

    // Create a screen size rect
  }

  void BuildAtlas(std::span<const uint8_t> data, uint32_t rowWidth) {
    DASSERT(!pendingBuildReq_);
    // Round up to the power of two
    const auto atlasWidth =
      std::bit_ceil(std::max((uint32_t)data.size() / rowWidth, rowWidth));
    const auto atlasHeight = atlasWidth;

    // Create a texture
    // Upload data
    // Render
    gpu::TextureDesc desc;
    desc.format = gpu::TextureFormat::R8Unorm;
    desc.usage = gpu::TextureUsage::ReadOnly;
    desc.width = atlasWidth;
    desc.height = atlasHeight;

    auto texture = TRY(device_->CreateTexture(desc));


    TRY(texture->Write(0, 0, atlasData.data(), width * height));
  }

  void Render() {
    Ref<gpu::CommandContext> context = device_->CreateCommandContext();

    // Write buffers
    context->WriteTexture();

    context->SetRenderTarget(
      swapChain_->GetCurrentBackBufferRenderTarget().Get());
    context->SetPipelineState(pso_.Get());
    context->SetVertexBuffer(vbo_.Get(), SizeBytes(vertices));
    context->SetIndexBuffer(ibo.Get(), gpu::IndexFormat::Uint32,
                            SizeBytes(indices));
    context->DrawIndexed(std::size(indices), 0, 0);

    device_->Submit(std::move(context));

    swapChain_->Present();
    std::this_thread::sleep_for(std::chrono::milliseconds(16));
  }

private:
  Ref<gpu::Device> device_;
  Ref<gpu::PipelineState> pso_;
  Ref<gpu::SwapChain> swapChain_;
};

using namespace gfx;

struct {
  uint32_t start = 32;
  uint32_t end = 126;
} static constexpr kRangeLatin;

Renderer* g_Renderer = nullptr;

// Called from glyph cache to build the atlas
void OnBuildAtlas(std::span<const uint8_t> data, size_t rowWidth) {
  g_Renderer->BuildAtlas(data, rowWidth);
}

int main(int argv, const char* argc[]) {
  if (argv < 4) {
    std::cout << "Please provide: font_filename font_size output_name\n";
    return 1;
  }
  auto input = std::filesystem::path(argc[0]).parent_path() / argc[1];
  // Rasterize a font atlas
  auto fontSize = atoi(argc[3]);

  auto res = Typeface::CreateFromFile(input);
  DASSERT(res);

  Ref<Typeface> font = res.value();
  std::unique_ptr<GlyphCache> glyphCache = GlyphCache::Create();
  glyphCache->SetOnBuildHandler(OnBuildAtlas);

  // Create a window and a renderer
  auto window = Window::Create(1280, 720);
  auto renderer = std::make_unique<Renderer>();
  renderer->Init(window.get(), 1280, 720);
  g_Renderer = renderer.get();

  // Add font and rasterize the Latin script
  glyphCache->AddFont(font);
  glyphCache->PrecacheCharacterRange(fontSize, kRangeLatin.start,
                                     kRangeLatin.end);

  while (window->PumpMessage()) {
    std::this_thread::yield();
  }
}