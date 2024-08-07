#pragma once
#include "runtime/common.h"
#include "runtime/gfx/common.h"

namespace gpu {

// A unit of a drawing pipeline
// Connects high level drawing primitives like text or rects and a draw context
// Could be a UI pass to draw ui shapes or a postprocess pass to adjust gamma
class DrawPass {
public:
    virtual ~DrawPass() = default;

};

// High level api for rendering
// Wraps d3d12 apis and manages memory
class DrawContext {
public:
    std::unique_ptr<DrawContext> Create();

    void SubmitDrawPass(std::unique_ptr<DrawPass> pass);

private:
};


} // namespace gpu