#pragma once
#include "common.h"
#include "shaper.h"

#include "thirdparty/imgui/imgui.h"
#include "thirdparty/imgui/imgui_internal.h"

#include <expected>
#include <stack>

namespace gpu {
class Texture;
class Buffer;
class DrawPass;
class Shader;
}  // namespace gpu

namespace gfx {

class Image;

// High level drawing context
// Internally creates draw commands and places them into a buffer
// The buffer then can be rendered via RenderPass interface
// Canvas owns shaders and gpu resources needed to draw itself 
//   so it's quite expensive to create one
class Canvas {
public:

    static std::unique_ptr<Canvas> Create();
    ~Canvas();

    // Delete all recorded draws
    void Clear() {}

    // Simple rect with all corners with the same rounding
    void DrawRect(const Rect& rect,
                  const Color4f& color,
                  uint32_t rounding = 0,
                  Corners cornerMask = gfx::Corners::All);

    // Draws text directly, shaped internally
    // TODO: Add flags (direction, orientation), shadow, font stack, etc.
    void DrawText(std::string_view text, const TextStyle& style) {}

    // Draws an already shaped text
    void DrawGlyphRun(const GlyphRun& textRun, const gpu::Texture& atlas) {}

    // Draws a whole text with the |font|. Unsupported characters are drawn with
    // replacement glyph
    void DrawTextSimple(std::string_view text, Font* font) {}

    // Texcoords: 0.-1. normalized texture coordinates
    // void DrawImage(RefCountedPtr<Image> image,
    //                const Rect& frame,
    //                const Rect& texcoords) {}

    // Fills whole canvas
    void DrawFill(const Color4f& color) {}

    // Sets a new clip rect overriding the old
    void SetClipRect(const Rect& clipRect) {}

    // Translates the origin of the canvas to the point x,y
    // I.e. Translation + Point
    void SetTranslation(uint32_t x, uint32_t y) {}

    // Scales all subsequent draws by x,y
    // I.e. Scale * Point
    void SetScale(uint32_t x, uint32_t y) {}

    // Saves current parameters (transformtions, clip rect) on a stack so that
    // another set of parameters can be applied on top
    // Useful with relative transformations of parent-child elements
    // E.g. if a child's transformation is defined relative to the parent origin
    void SaveContext();
    void RestoreContext();
    void ClearContext();

    void Resize(uint32_t width, uint32_t height);

    // TODO: Maybe use immediate context here. I.e. submit commands for
    // rendering in the DrawXXX functions.
    // Return a render pass object which can be executed to actualy render the
    // canvas. The draw pass owns all the data and owned by the user
    std::unique_ptr<gpu::DrawPass> CreateDrawPass();

private:
    Canvas();

private:
    static constexpr int kMaxVerticesPerDrawList =
        std::numeric_limits<uint16_t>::max();

    std::unique_ptr<ImDrawListSharedData> drawListSharedData_;
    std::list<ImDrawList> drawLists_;
    ImDrawList* currentDrawList_;

    gfx::Rect viewport_;

    struct Context {
        Vec2f translation;
        Rect clipRect;
        bool hasClipRect = false;
    };
    Context context_;
    std::deque<Context> contextStack_;

    Vec2f totalTranslation_;
};

}  // namespace gfx