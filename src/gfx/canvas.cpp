#include "canvas.h"
#include "gpu/draw_context.h"

#include "gpu/shader/hlsl_codegen.h"
#include "gpu/shader/hlsl_compiler.h"
#include "gpu/shader/shader_dsl_api.h"


// Bind indices used by shaders
static constexpr int kTransformMatrixBindIndex = 0;
static constexpr int kTextureBindIndex = 1;

namespace {

std::unique_ptr<gpu::ShaderCode> CreatePixelBasic() {
    gpu::ShaderDSLContext ctx;
    gpu::HLSLCodeGenerator gen;

    using namespace gpu;
    using namespace gpu::shader_dsl;

    auto projMatrix = Uniform<Float4x4>() | BindIndex(0);

    Function("main");
    {
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

    auto code = gen.Build(gpu::ShaderType::Vertex, "main", &ctx);
    auto compileResult = gpu::HLSLShaderCompiler::Compile(
        code->mainId, code->code, gpu::ShaderType::Vertex, kDebugBuild);
    DASSERT(compileResult);

    return code;
}

std::unique_ptr<gpu::ShaderCode> CreateVertexBasic() {
    gpu::ShaderDSLContext ctx;
    gpu::HLSLCodeGenerator gen;

    {
        using namespace gpu;
        using namespace gpu::shader_dsl;

        auto texture = Texture2D() | BindIndex(0);
        auto sampler = Sampler() | BindIndex(1);

        Function("main");
        {
            auto inCol = Input<Float4>() | Semantic::Color;
            auto inUV = Input<Float2>() | Semantic::Texcoord;

            auto outCol = Output<Float4>() | Semantic::Color;

            outCol = inCol * SampleTexture(texture, sampler, inUV);
            EndFunction();
        }
    }

    auto code = gen.Build(gpu::ShaderType::Pixel, "main", &ctx);
    auto compileResult = gpu::HLSLShaderCompiler::Compile(
        code->mainId, code->code, gpu::ShaderType::Pixel, kDebugBuild);
    DASSERT(compileResult);

    return code;
}

void CreateShaders() {
    auto pso = gpu::PipelineState::Create();
    {
        auto code = CreateVertexBasic();
        auto compileResult = gpu::HLSLShaderCompiler::Compile(
            code->mainId, code->code, gpu::ShaderType::Pixel, kDebugBuild);
        DASSERT(compileResult);

        auto data = std::span<const uint8_t>(
            (uint8_t*)compileResult.blob->GetBufferPointer(),
            compileResult.blob->GetBufferSize());
        pso->SetVertexShader(data, code->uniforms, code->inputs, code->outputs);
    }
    {
        auto code = CreateVertexBasic();
        auto compileResult = gpu::HLSLShaderCompiler::Compile(
            code->mainId, code->code, gpu::ShaderType::Pixel, kDebugBuild);
        DASSERT(compileResult);

        auto data = std::span<const uint8_t>(
            (uint8_t*)compileResult.blob->GetBufferPointer(),
            compileResult.blob->GetBufferSize());
        pso->SetPixelShader(data, code->uniforms, code->inputs, code->outputs);
    }
}

}  // namespace

namespace gfx {

std::unique_ptr<Canvas> Canvas::Create() {
    return std::unique_ptr<Canvas>(new Canvas());
}

Canvas::Canvas() {
    drawListSharedData_.reset(new ImDrawListSharedData());
    drawListSharedData_->InitialFlags = ImDrawListFlags_None;
    drawListSharedData_->InitialFlags |=
        ImDrawListFlags_AntiAliasedLines;
    drawListSharedData_->InitialFlags |=
        ImDrawListFlags_AntiAliasedLinesUseTex;
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
class DrawPass : public gpu::DrawPass {
public:
    DrawPass(std::list<ImDrawList>&& drawLists)
        : drawLists_(std::move(drawLists)) {}

    void Init(gpu::DrawContext& ctx) override {
        size_t totalVertexNum = 0;
        size_t totalIndexNum = 0;
        // Count the total number of vertices in all draw lists
        for (const ImDrawList& drawList : drawLists_) {
            totalVertexNum += drawList.VtxBuffer.Size;
            totalIndexNum += drawList.IdxBuffer.Size;
        }
        // Upload vertex/index buffers into VRAM
        // Merge all data into a single buffer
        std::vector<uint8_t> vertexBufferData;
        std::vector<uint8_t> indexBufferData;
        {
            vertexBufferData.resize(totalVertexNum * sizeof(ImDrawVert));
            indexBufferData.resize(totalIndexNum * sizeof(ImDrawIdx));
            uint8_t* vtxPtr = vertexBufferData.data();
            uint8_t* idxPtr = indexBufferData.data();
            for (const ImDrawList& drawList : drawLists_) {
                memcpy(vtxPtr, drawList.VtxBuffer.Data,
                       drawList.VtxBuffer.Size * sizeof(ImDrawVert));
                vtxPtr += drawList.VtxBuffer.Size;
                memcpy(idxPtr, drawList.IdxBuffer.Data,
                       drawList.IdxBuffer.Size * sizeof(ImDrawIdx));
                idxPtr += drawList.IdxBuffer.Size;
            }
        };
        vertexBuffer_ = ctx.CreateBuffer(totalVertexNum * sizeof(ImDrawVert));
        indexBuffer_ = ctx.CreateBuffer(totalIndexNum * sizeof(ImDrawIdx));

        ctx.UploadResourceData(vertexBuffer_.Get(), vertexBufferData);
        ctx.UploadResourceData(indexBuffer_.Get(), indexBufferData);

        // Upload viewport orthogonal projection matrix
        const float L = viewport_.Left();
        const float R = viewport_.Right();
        const float T = viewport_.Top();
        const float B = viewport_.Bottom();
        const float mvp[4][4] = {
            {2.0f / (R - L), 0.0f, 0.0f, 0.0f},
            {0.0f, 2.0f / (T - B), 0.0f, 0.0f},
            {0.0f, 0.0f, 0.5f, 0.0f},
            {(R + L) / (L - R), (T + B) / (B - T), 0.5f, 1.0f},
        };

        // TODO: Handle alignment
        // Maybe use StructuredBuffer.
        // E.g. auto offset = buffer.Add<int>();
        constants_ =
            ctx.CreateBuffer(sizeof(mvp), gpu::ResourceFlags::Constant);
        ctx.UploadResourceData(constants_.Get(), {(uint8_t*)&mvp, sizeof(mvp)});
    }

    // TODO: Setup shader
    void Run(gpu::DrawContext& ctx) override {
        // ctx.SetVertexBuffer(vertexBuffer_.Get());
        // ctx.SetIndexBuffer(indexBuffer_.Get());
        // ctx.SetShader(basicShader);
        ctx.SetShaderArgument(kTransformMatrixBindIndex, constants_.Get());

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
                ctx.SetClipRect(clipRect);

                const auto* texture = (gpu::Texture*)cmd->GetTexID();
                ctx.SetShaderArgument(kTextureBindIndex, texture);
                DASSERT(ctx.IsReadyToDraw());

                ctx.DrawIndexed(cmd->ElemCount,
                                cmd->IdxOffset + global_idx_offset,
                                cmd->VtxOffset + global_vtx_offset);
            }
            global_idx_offset += cmd_list.IdxBuffer.Size;
            global_vtx_offset += cmd_list.VtxBuffer.Size;
        }
    }

private:
    gfx::Rect viewport_;
    std::list<ImDrawList> drawLists_;
    std::list<gpu::Texture> textures_;

    RefCountedPtr<gpu::Buffer> constants_;
    RefCountedPtr<gpu::Buffer> vertexBuffer_;
    RefCountedPtr<gpu::Buffer> indexBuffer_;
};

std::unique_ptr<gpu::DrawPass> Canvas::CreateDrawPass() {
    // TODO: Create shaders here and bind them to draw pass
    // Cache shaders globally until |this| is deleted
    auto out =
        std::unique_ptr<gpu::DrawPass>(new DrawPass(std::move(drawLists_)));
    Clear();
    return out;
}

}  // namespace gfx