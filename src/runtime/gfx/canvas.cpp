#include "canvas.h"

namespace gfx {

// Simple rect with all corners with the same rounding
void Canvas::DrawRect(const Rect& rect,
                const Color4f& color,
                uint32_t rounding = 0,
                Corners cornerMask) {
    // TODO: Validate rounding
    const Rect rectTransformed = rect.Translate(totalTranslation_);
    const ImVec2 min = {rectTransformed.Min().x, rectTransformed.Min().y};
    const ImVec2 max = {rectTransformed.Max().x, rectTransformed.Max().y};
    const ImU32 colorU32 = color.ToRGBAU32();
    const ImDrawFlags flags = 0;
    currentDrawList_->AddRectFilled(min, max, colorU32, rounding, flags);
}

void Canvas::SaveContext() {
    if (context_.hasClipRect) {
        currentDrawList_->PushClipRect(context_.clipRect.min,
                                       context_.clipRect.max);
    }
    totalTranslation_ += context_.translation;
    contextStack_.push_back(context_);
    context_ = Context();
}

void Canvas::RestoreContext() {
    DASSERT(!contextStack_.empty());
    auto& lastContext = contextStack_.back();
    totalTranslation_ -= lastContext.translation;

    if (lastContext.hasClipRect) {
        currentDrawList_->PopClipRect();
    }
    contextStack_.pop_back();
}

void Canvas::ClearContext() {
    while (!contextStack_.empty()) {
        RestoreContext();
    }
    DASSERT(Vec2f(std::round(totalTranslation_.x),
                  std::round(totalTranslation_.y)) == Vec2f());
}

std::unique_ptr<gpu::DrawPass> Canvas::CreateDrawPass() {

}

} // namespace gfx