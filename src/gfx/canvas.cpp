#include "canvas.h"
#include "gpu/gpu.h"

// Bind indices used by shaders
static constexpr int kTransformMatrixBindIndex = 0;
static constexpr int kTextureBindIndex = 1;

namespace {}  // namespace

namespace gfx {

std::unique_ptr<Canvas> Canvas::Create() {
    return std::unique_ptr<Canvas>(new Canvas());
}

Canvas::Canvas() {
    drawListSharedData_.reset(new ImDrawListSharedData());
    drawListSharedData_->InitialFlags = ImDrawListFlags_None;
    drawListSharedData_->InitialFlags |= ImDrawListFlags_AntiAliasedLines;
    drawListSharedData_->InitialFlags |= ImDrawListFlags_AntiAliasedLinesUseTex;
    drawListSharedData_->InitialFlags |= ImDrawListFlags_AntiAliasedFill;
    drawListSharedData_->InitialFlags |= ImDrawListFlags_AllowVtxOffset;

    drawLists_.emplace_back(drawListSharedData_.get());
    currentDrawList_ = &drawLists_.front();
    currentDrawList_->_ResetForNewFrame();

    // CreateShaders();
}

Canvas::~Canvas() = default;

// Simple rect with all corners with the same rounding
void Canvas::DrawRect(const Rect& rect,
                      const Color4f& color,
                      uint32_t rounding,
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
        const ImVec2 min = {context_.clipRect.Min().x,
                            context_.clipRect.Min().y};
        const ImVec2 max = {context_.clipRect.Max().x,
                            context_.clipRect.Max().y};
        currentDrawList_->PushClipRect(min, max);
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

void Canvas::Resize(uint32_t width, uint32_t height) {
    viewport_.SetSize(width, height);
}

// Passed to gpu::Device for execution on GPU
class ImDrawPass : public Canvas::DrawPass {
public:
    ImDrawPass(std::list<ImDrawList>&& drawLists)
        : drawLists_(std::move(drawLists)) {}

    struct CompiledShader {
        std::unique_ptr<gpu::ShaderCode> code;
        std::vector<uint8_t> bytecode;
    };

    CompiledShader CreateVertexShader(gpu::ShaderCodeGenerator* gen,
                                      gpu::Device* device) {
        gpu::ShaderDSLContext ctx;
        // clang-format off
        {
            using namespace gpu;
            using namespace gpu::shader_dsl;

            auto projMatrix = Uniform<Float4x4>() | BindIndex(kTransformMatrixBindIndex);

            Function("main"); {
                auto inPos = Input<Float2>() | Semantic::Position;
                auto inCol = Input<Float4>() | Semantic::Color;
                auto inUV = Input<Float2>() | Semantic::Texcoord;

                auto outPos = Output<Float4>() | Semantic::Position;
                auto outCol = Output<Float4>() | Semantic::Color;
                auto outUV = Output<Float2>() | Semantic::Texcoord;

                outPos = projMatrix * Float4(inPos, 0.f, 1.f);
                outCol = inCol;
                outUV = inUV;

                EndFunction();
            }
        }
        // clang-format on
        std::unique_ptr<gpu::ShaderCode> code =
            gen->Generate(gpu::ShaderUsage::Vertex, "main", &ctx);

        gpu::ShaderCompileResult bytecode = device->CompileShader(
            "main", code->code, gpu::ShaderUsage::Vertex, kDebugBuild);

        CompiledShader result;
        result.code = std::move(code);
        result.bytecode = std::move(bytecode.bytecode);
        return result;
    }

    CompiledShader CreatePixelShader(gpu::ShaderCodeGenerator* gen,
                                     gpu::Device* device) {
        gpu::ShaderDSLContext ctx;
        // clang-format off
        {
            using namespace gpu;
            using namespace gpu::shader_dsl;

            auto texture = Texture2D() | BindIndex(kTextureBindIndex);
            auto sampler = Sampler() | BindIndex(1);

            Function("main"); {
                auto inPos = Input<Float4>() | Semantic::Position;
                auto inCol = Input<Float4>() | Semantic::Color;
                auto inUV = Input<Float2>() | Semantic::Texcoord;

                auto outCol = Output<Float4>() | Semantic::Color;

                outCol = inCol * SampleTexture(texture, sampler, inUV);
                EndFunction();
            }
        }
        // clang-format on
        std::unique_ptr<gpu::ShaderCode> code =
            gen->Generate(gpu::ShaderUsage::Pixel, "main", &ctx);
        gpu::ShaderCompileResult bytecode = device->CompileShader(
            "main", code->code, gpu::ShaderUsage::Pixel, kDebugBuild);

        CompiledShader result;
        result.code = std::move(code);
        result.bytecode = std::move(bytecode.bytecode);
        return result;
    }

    RefCountedPtr<gpu::Buffer> CreateVertexBuffer(gpu::Device* device) {
        size_t totalVertexNum = 0;
        // Count the total number of vertices in all draw lists
        for (const ImDrawList& drawList : drawLists_) {
            totalVertexNum += drawList.VtxBuffer.Size;
        }
        // Merge all data into a single buffer
        std::vector<uint8_t> data;
        data.resize(totalVertexNum * sizeof(ImDrawVert));
        uint8_t* dst = data.data();

        for (const ImDrawList& drawList : drawLists_) {
            const auto* src = drawList.VtxBuffer.Data;
            const size_t size = drawList.VtxBuffer.Size * sizeof(ImDrawVert);
            DASSERT((intptr_t)size <= data.data() + data.size() - dst);
            memcpy(dst, src, size);
            dst += size;
        }
        auto result =
            device->CreateBuffer(data.size(), gpu::BufferUsage::Vertex);
        DASSERT(result);
        ctx_->WriteBuffer(result->Get(), data);
        return *result;
    }

    RefCountedPtr<gpu::Buffer> CreateIndexBuffer(gpu::Device* device) {
        size_t totalIndexNum = 0;
        // Count the total number of vertices in all draw lists
        for (const ImDrawList& drawList : drawLists_) {
            totalIndexNum += drawList.IdxBuffer.Size;
        }
        // Merge all data into a single buffer
        std::vector<uint8_t> data;
        data.resize(totalIndexNum * sizeof(ImDrawIdx));
        uint8_t* dst = data.data();

        for (const ImDrawList& drawList : drawLists_) {
            const auto* src = drawList.IdxBuffer.Data;
            const size_t size = drawList.IdxBuffer.Size * sizeof(ImDrawIdx);
            DASSERT((intptr_t)size <= data.data() + data.size() - dst);
            memcpy(dst, src, size);
            dst += size;
        }
        auto result =
            device->CreateBuffer(data.size(), gpu::BufferUsage::Index);
        DASSERT(result);
        ctx_->WriteBuffer(result->Get(), data);
        return *result;
    }

    void RecordCommands(gpu::CommandContext* ctx) override {
        gpu::Device* device = ctx->GetParentDevice();
        ctx_ = ctx;

        RefCountedPtr<gpu::Buffer> vertexBuffer = CreateVertexBuffer(device);
        RefCountedPtr<gpu::Buffer> indexBuffer = CreateIndexBuffer(device);

        RefCountedPtr<gpu::Buffer> projMat = [&] {
            auto projMat = gfx::Mat44f::CreateOrthographic(
                viewport_.Top(), viewport_.Right(), viewport_.Bottom(),
                viewport_.Left());

            auto result = device->CreateBuffer(projMat.Data().size(),
                                               gpu::BufferUsage::Uniform);
            DASSERT(result);
            ctx->WriteBuffer(result->Get(), projMat.Data());
            return *result;
        }();

        RefCountedPtr<gpu::PipelineState> pso = [&] {
            auto shaderCodeGen = device->CreateShaderCodeGenerator();
            const CompiledShader vertexShader =
                CreateVertexShader(shaderCodeGen.get(), device);
            const CompiledShader pixelShader =
                CreatePixelShader(shaderCodeGen.get(), device);
            // clang-format off
            const auto psoDesc = gpu::PipelineStateDesc()
                .AddRenderTarget(gpu::TextureFormat::RGBA32Float)
                .SetInputLayout(
                    gpu::InputLayout::Element(
                        gpu::Semantic::Position,
                        gpu::VertexFormat::Float32x2,
                        offsetof(ImDrawVert, pos)),
                    gpu::InputLayout::Element(
                        gpu::Semantic::Texcoord,
                        gpu::VertexFormat::Float32x2,
                        offsetof(ImDrawVert, uv)),
                    gpu::InputLayout::Element(
                        gpu::Semantic::Color,
                        gpu::VertexFormat::Unorm8x4,
                        offsetof(ImDrawVert, col)))
                .SetVertexShader(
                    vertexShader.bytecode, 
                    vertexShader.code->uniforms, 
                    vertexShader.code->inputs,
                    vertexShader.code->outputs)
                .SetPixelShader(
                    pixelShader.bytecode, 
                    pixelShader.code->uniforms, 
                    pixelShader.code->inputs,
                    pixelShader.code->outputs);
            // clang-format on

            auto result = device->CreatePipelineState(psoDesc);
            DASSERT(result);
            return *result;
        }();

        ctx->SetPipelineState(pso.Get());
        ctx->SetUniform(kTransformMatrixBindIndex, projMat.Get());

        // (Because we merged all buffers into a single one, we maintain our own
        // offset into them)
        int global_vtx_offset = 0;
        int global_idx_offset = 0;

        for (ImDrawList& cmd_list : drawLists_) {
            for (int cmdIdx = 0; cmdIdx < cmd_list.CmdBuffer.Size; cmdIdx++) {
                const ImDrawCmd* cmd = &cmd_list.CmdBuffer[cmdIdx];
                // Project scissor/clipping rectangles into framebuffer space
                ImVec2 clip_min(cmd->ClipRect.x, cmd->ClipRect.y);
                ImVec2 clip_max(cmd->ClipRect.z, cmd->ClipRect.w);
                if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y) {
                    continue;
                }
                // Apply Scissor/clipping rectangle, Bind texture, Draw
                const auto clipRect = gfx::Rect::FromMinMax(
                    clip_min.x, clip_min.y, clip_max.x, clip_max.y);
                ctx->SetClipRect(clipRect);

                auto* texture = (gpu::Texture*)cmd->GetTexID();
                ctx->SetUniform(kTextureBindIndex, texture);

                ctx->DrawIndexed(cmd->ElemCount,
                                 cmd->IdxOffset + global_idx_offset,
                                 cmd->VtxOffset + global_vtx_offset);
            }
            global_idx_offset += cmd_list.IdxBuffer.Size;
            global_vtx_offset += cmd_list.VtxBuffer.Size;
        }
    }

private:
    gpu::CommandContext* ctx_{};

    gfx::Rect viewport_;
    std::list<ImDrawList> drawLists_;
    // std::list<gpu::Texture> textures_;

    RefCountedPtr<gpu::Buffer> vbuf_;
    RefCountedPtr<gpu::Buffer> ibuf;
};

std::unique_ptr<Canvas::DrawPass> Canvas::CreateDrawPass() {
    // TODO: Create shaders here and bind them to draw pass
    // Cache shaders globally until |this| is deleted
    auto out = std::unique_ptr<Canvas::DrawPass>(
        new ImDrawPass(std::move(drawLists_)));
    Clear();
    return out;
}

}  // namespace gfx