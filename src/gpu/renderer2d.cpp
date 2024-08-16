#include "renderer2d.h"
#include <d3dcompiler.h>

struct Renderer2D::FrameContext {
    std::vector<Handle> resources;
};

namespace {

// Constants passed to shaders
struct ShaderConstants {
    gfx::Mat44f mvp;
    gfx::Color4f color;
};

struct ShaderCode {
    std::string vertex;
    std::string pixel;
};

// Generate shaders based on input layout
// A shape has the constant color or textured if Texcoords are provided
ShaderCode GenerateShaders(std::span<InputLayoutElement> layout) {
    std::string vsInput;
    std::string vsOut;
    std::string vsMain;
    bool useTexture = false;

    for (const InputLayoutElement& elem : layout) {
        switch (elem.semantic) {
            case InputLayoutElement::Semantic::Position: {
                vsInput += "float2 pos: POSITION;";
                vsOut += "float2 pos: SV_POSITION;";
                vsMain +=
                    "output.pos = mul("
                    "constants.mvp, float4(input.pos.xy, 0.f, 1.f));";
            } break;
        }



        if (elem.semantic == InputLayoutElement::Semantic::Position) {
            vsInput += "float2 pos: POSITION;";
            vsOut += "float2 pos: SV_POSITION;";
            vsMain +=
                "output.pos = mul("
                "constants.mvp, float4(input.pos.xy, 0.f, 1.f));";
        } else if (elem.semantic == InputLayoutElement::Semantic::Texcoord) {
            vsInput += "float2 texcoord: TEXCOORD0;";
            vsOut += "float2 texcoord: TEXCOORD0;";
            vsMain += "output.texcoord = input.texcoord;";
            useTexture = true;
        } else if (elem.semantic == InputLayoutElement::Semantic::Color) {
            vsInput += "float4 color: COLOR0;";
            vsOut += "float4 color: COLOR0;";
            vsMain += "output.color = input.color;";
        } else {
            UNREACHABLE();
        }
    }
    ShaderCode out;
    // Vertex code
    out.vertex += "cbuffer constants { float4x4 mvp; float4 color; };";
    out.vertex += "struct Input {" + vsInput + "};";
    out.vertex += "struct Output {" + vsOut + "};";
    out.vertex +=
        "Output main(Input input) { Output output;" + vsMain + "return out; };";
    // Pixel code
    out.pixel += "cbuffer constants { float4x4 mvp; float4 color; };";
    out.pixel += "struct Input {" + vsOut + "};";
    if (useTexture) {
        out.pixel +=
            "SamplerState sampler0 : register(s0);Texture2D texture0 : "
            "register(t0);";
    }
    out.pixel +=
        "float4 main(Input input): SV_Target { float4 output;"
        "output = constants.color;";
    if (useTexture) {
        out.pixel +=
            "output = output * texture0.Sample("
            "sampler0, input.texcoord);";
    }
    out.pixel += "};";
    return out;
}

RefCountedPtr<ID3DBlob> CompileShader(const std::string& code,
                                      const std::string& entrypoint,
                                      const std::string& target,
                                      bool isDebug) {
    UINT compileFlags = 0;
    if (isDebug) {
        compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
    }
    RefCountedPtr<ID3DBlob> byteCode;
    RefCountedPtr<ID3DBlob> errors;
    HRESULT hr = D3DCompile(code.c_str(), code.size(), "", nullptr, nullptr,
                            entrypoint.c_str(), target.c_str(), compileFlags, 0,
                            &byteCode, &errors);
    DASSERT_F(SUCCEEDED(hr), "Error compiling shader. {}",
              (char*)errors->GetBufferPointer());
    return byteCode;
}

RefCountedPtr<ID3D12PipelineState> CreatePSO(
    ID3D12Device* device,
    ID3D12RootSignature* rootSignature,
    std::span<InputLayoutElement> inputLayout) {
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
    psoDesc.NodeMask = 1;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.pRootSignature = rootSignature;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.SampleDesc.Count = 1;
    psoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

    // - Shaders
    const ShaderCode code = GenerateShaders(inputLayout);
    RefCountedPtr<ID3DBlob> vertexShaderBlob;
    RefCountedPtr<ID3DBlob> pixelShaderBlob;
    vertexShaderBlob =
        CompileShader(code.vertex, "main", "vs_5_0", kDebugBuild);
    pixelShaderBlob = CompileShader(code.pixel, "main", "ps_5_0", kDebugBuild);

    // - Input layout
    std::vector<D3D12_INPUT_ELEMENT_DESC> d3d12InputLayout;
    d3d12InputLayout.reserve(inputLayout.size());
    uint32_t offset = 0;

    for (const InputLayoutElement& elem : inputLayout) {
        D3D12_INPUT_ELEMENT_DESC& out = d3d12InputLayout.emplace_back();
        switch (elem.semantic) {
            case InputLayoutElement::Semantic::Position: {
                out = {"POSITION",
                       0,
                       DXGI_FORMAT_R32G32_FLOAT,
                       0,
                       D3D12_APPEND_ALIGNED_ELEMENT,
                       D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                       0};
            } break;
            case InputLayoutElement::Semantic::Texcoord: {
                out = {"TEXCOORD0",
                       0,
                       DXGI_FORMAT_R32G32_FLOAT,
                       0,
                       D3D12_APPEND_ALIGNED_ELEMENT,
                       D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                       0};
            } break;
            case InputLayoutElement::Semantic::Color: {
                out = {"COLOR0",
                       0,
                       DXGI_FORMAT_R8G8B8A8_UNORM,
                       0,
                       D3D12_APPEND_ALIGNED_ELEMENT,
                       D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                       0};
            } break;
            default:
                UNREACHABLE();
        }
    }
    psoDesc.VS = {vertexShaderBlob->GetBufferPointer(),
                  vertexShaderBlob->GetBufferSize()};
    psoDesc.PS = {pixelShaderBlob->GetBufferPointer(),
                  pixelShaderBlob->GetBufferSize()};
    psoDesc.InputLayout = {d3d12InputLayout.data(),
                           (UINT)d3d12InputLayout.size()};

    // - Blending setup
    psoDesc.BlendState.AlphaToCoverageEnable = false;
    psoDesc.BlendState.RenderTarget[0].BlendEnable = true;
    psoDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
    psoDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    psoDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    psoDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
    psoDesc.BlendState.RenderTarget[0].DestBlendAlpha =
        D3D12_BLEND_INV_SRC_ALPHA;
    psoDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
    psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask =
        D3D12_COLOR_WRITE_ENABLE_ALL;

    // - Rasterizer state
    psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    psoDesc.RasterizerState.FrontCounterClockwise = FALSE;
    psoDesc.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
    psoDesc.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
    psoDesc.RasterizerState.SlopeScaledDepthBias =
        D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
    psoDesc.RasterizerState.DepthClipEnable = true;
    psoDesc.RasterizerState.MultisampleEnable = FALSE;
    psoDesc.RasterizerState.AntialiasedLineEnable = FALSE;
    psoDesc.RasterizerState.ForcedSampleCount = 0;
    psoDesc.RasterizerState.ConservativeRaster =
        D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

    // - Depth-stencil State
    psoDesc.DepthStencilState.DepthEnable = false;
    psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    psoDesc.DepthStencilState.StencilEnable = false;
    psoDesc.DepthStencilState.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
    psoDesc.DepthStencilState.FrontFace.StencilDepthFailOp =
        D3D12_STENCIL_OP_KEEP;
    psoDesc.DepthStencilState.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
    psoDesc.DepthStencilState.FrontFace.StencilFunc =
        D3D12_COMPARISON_FUNC_ALWAYS;
    psoDesc.DepthStencilState.BackFace = psoDesc.DepthStencilState.FrontFace;

    RefCountedPtr<ID3D12PipelineState> pso;
    HRESULT result =
        device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pso));
    DASSERT_HR(result);
    return pso;
}

}  // namespace

void Renderer2D::Init(void* windowHandle) {
    DASSERT(windowHandle);
    // TODO: Decide how to select adapter
    const int currentAdapter = 0;
    device_ = d3d12::Device::Create(currentAdapter, kDebugBuild);
    descriptorAlloc_ = d3d12::PooledDescriptorAllocator::Create(device_.get());

    D3D12_DESCRIPTOR_HEAP_DESC props{};
    props.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    props.NumDescriptors = 128;
    props.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    props.NodeMask = 0;
    descriptorAlloc_->CreateHeap(props);

    frameContexts.resize(kNumFramesQueued);

    // Root signature
    {
        // Defines a descriptor table
        D3D12_DESCRIPTOR_RANGE textureDescriptorTable{};
        textureDescriptorTable.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        textureDescriptorTable.NumDescriptors = 1;
        textureDescriptorTable.BaseShaderRegister = 0;
        textureDescriptorTable.RegisterSpace = 0;
        textureDescriptorTable.OffsetInDescriptorsFromTableStart = 0;

        D3D12_ROOT_PARAMETER parameters[2]{};
        // Transform matrix
        parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        parameters[0].Constants.ShaderRegister = 0;
        parameters[0].Constants.RegisterSpace = 0;
        parameters[0].Constants.Num32BitValues = sizeof(ShaderConstants) / 4;
        parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
        // Texture
        parameters[1].ParameterType =
            D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        parameters[1].DescriptorTable.NumDescriptorRanges = 1;
        parameters[1].DescriptorTable.pDescriptorRanges =
            &textureDescriptorTable;
        parameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        // Tiled sampler for all textures: font atlas, icons, images
        D3D12_STATIC_SAMPLER_DESC staticSampler{};
        staticSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        staticSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        staticSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        staticSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        staticSampler.MipLODBias = 0.f;
        staticSampler.MaxAnisotropy = 0;
        staticSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        staticSampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
        staticSampler.MinLOD = 0.f;
        staticSampler.MaxLOD = 0.f;
        staticSampler.ShaderRegister = 0;
        staticSampler.RegisterSpace = 0;
        staticSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        D3D12_ROOT_SIGNATURE_DESC desc{};
        desc.NumParameters = static_cast<UINT>(std::size(parameters));
        desc.pParameters = parameters;
        desc.NumStaticSamplers = 1;
        desc.pStaticSamplers = &staticSampler;
        desc.Flags =
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

        RefCountedPtr<ID3D10Blob> blob;
        HRESULT result = D3D12SerializeRootSignature(
            &desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, nullptr);
        DASSERT_HR(result);

        result = device_->GetDevice()->CreateRootSignature(
            0, blob->GetBufferPointer(), blob->GetBufferSize(),
            IID_PPV_ARGS(&rootSignature_));
    }
}



#include "thirdparty/imgui/imgui.h"
#include "thirdparty/imgui/imgui_internal.h"

struct FrameContext {
    ID3D12CommandAllocator* CommandAllocator;
    UINT64 FenceValue;
};

struct ImGui_ImplDX12_RenderBuffers {
    ID3D12Resource* IndexBuffer;
    ID3D12Resource* VertexBuffer;
    int IndexBufferSize;
    int VertexBufferSize;
};

void ImGui_ImplDX12_SetupRenderState(ImDrawData* draw_data,
                                     ID3D12GraphicsCommandList* ctx,
                                     ImGui_ImplDX12_RenderBuffers* fr) {
    // - Upload constants
    ShaderConstants constants{};
    float L = draw_data->DisplayPos.x;
    float R = draw_data->DisplayPos.x + draw_data->DisplaySize.x;
    float T = draw_data->DisplayPos.y;
    float B = draw_data->DisplayPos.y + draw_data->DisplaySize.y;
    constants.mvp = gfx::Mat44f::CreateOrthographic(T, R, B, L);


    // Setup viewport
    D3D12_VIEWPORT vp{};
    vp.Width = draw_data->DisplaySize.x;
    vp.Height = draw_data->DisplaySize.y;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = vp.TopLeftY = 0.0f;
    ctx->RSSetViewports(1, &vp);

    // State
    ctx->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->SetPipelineState(m_pPipelineState);
    ctx->SetGraphicsRootSignature(m_pRootSignature);
    // Setup blend factor
    const float blend_factor[4] = {0.f, 0.f, 0.f, 0.f};
    ctx->OMSetBlendFactor(blend_factor);

    // Arguments
    // Bind shader and vertex buffers
    unsigned int stride = sizeof(ImDrawVert);
    unsigned int offset = 0;

    D3D12_VERTEX_BUFFER_VIEW vbv{};
    vbv.BufferLocation = fr->VertexBuffer->GetGPUVirtualAddress() + offset;
    vbv.SizeInBytes = fr->VertexBufferSize * stride;
    vbv.StrideInBytes = stride;
    ctx->IASetVertexBuffers(0, 1, &vbv);

    D3D12_INDEX_BUFFER_VIEW ibv{};
    ibv.BufferLocation = fr->IndexBuffer->GetGPUVirtualAddress();
    ibv.SizeInBytes = fr->IndexBufferSize * sizeof(ImDrawIdx);
    ibv.Format =
        sizeof(ImDrawIdx) == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
    ctx->IASetIndexBuffer(&ibv);

    ctx->SetGraphicsRoot32BitConstants(0, 16, &vertex_constant_buffer, 0);
}

void RenderDrawData(ImDrawData* draw_data, ID3D12GraphicsCommandList* ctx) {
    // Avoid rendering when minimized
    if (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f)
        return;

    // FIXME: I'm assuming that this only gets called once per frame!
    // If not, we can't just re-allocate the IB or VB, we'll have to do a
    // proper allocator.
    g_frameIndex = g_frameIndex + 1;
    ImGui_ImplDX12_RenderBuffers* fr =
        &m_pFrameResources[g_frameIndex % NUM_FRAMES_IN_FLIGHT];

    for (int n = 0; n < draw_data->CmdListsCount; n++) {
        ImDrawList* draw_list = draw_data->CmdLists[n];
        draw_list->_PopUnusedDrawCmd();
        draw_data->TotalVtxCount += draw_list->VtxBuffer.Size;
        draw_data->TotalIdxCount += draw_list->IdxBuffer.Size;
    }

    // Create and grow vertex/index buffers if needed
    if (fr->VertexBuffer == NULL ||
        fr->VertexBufferSize < draw_data->TotalVtxCount) {
        SafeRelease(fr->VertexBuffer);
        fr->VertexBufferSize = draw_data->TotalVtxCount + 5000;

        D3D12_HEAP_PROPERTIES props{};
        props.Type = D3D12_HEAP_TYPE_UPLOAD;
        props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

        D3D12_RESOURCE_DESC desc{};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Width = fr->VertexBufferSize * sizeof(ImDrawVert);
        desc.Height = 1;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.Format = DXGI_FORMAT_UNKNOWN;
        desc.SampleDesc.Count = 1;
        desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        desc.Flags = D3D12_RESOURCE_FLAG_NONE;

        if (g_pd3dDevice->CreateCommittedResource(
                &props, D3D12_HEAP_FLAG_NONE, &desc,
                D3D12_RESOURCE_STATE_GENERIC_READ, NULL,
                IID_PPV_ARGS(&fr->VertexBuffer)) < 0)
            return;
    }
    if (fr->IndexBuffer == NULL ||
        fr->IndexBufferSize < draw_data->TotalIdxCount) {
        SafeRelease(fr->IndexBuffer);
        fr->IndexBufferSize = draw_data->TotalIdxCount + 10000;

        D3D12_HEAP_PROPERTIES props{};
        props.Type = D3D12_HEAP_TYPE_UPLOAD;
        props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

        D3D12_RESOURCE_DESC desc{};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Width = fr->IndexBufferSize * sizeof(ImDrawIdx);
        desc.Height = 1;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.Format = DXGI_FORMAT_UNKNOWN;
        desc.SampleDesc.Count = 1;
        desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        desc.Flags = D3D12_RESOURCE_FLAG_NONE;

        auto result = g_pd3dDevice->CreateCommittedResource(
            &props, D3D12_HEAP_FLAG_NONE, &desc,
            D3D12_RESOURCE_STATE_GENERIC_READ, NULL,
            IID_PPV_ARGS(&fr->IndexBuffer));
        DASSERT(SUCCEEDED(result));
    }

    // Upload vertex/index data into a single contiguous GPU buffer
    void *vtx_resource, *idx_resource;
    D3D12_RANGE range{};
    if (fr->VertexBuffer->Map(0, &range, &vtx_resource) != S_OK)
        return;
    if (fr->IndexBuffer->Map(0, &range, &idx_resource) != S_OK)
        return;
    ImDrawVert* vtx_dst = (ImDrawVert*)vtx_resource;
    ImDrawIdx* idx_dst = (ImDrawIdx*)idx_resource;

    for (int n = 0; n < draw_data->CmdListsCount; n++) {
        const ImDrawList* cmd_list = draw_data->CmdLists[n];
        memcpy(vtx_dst, cmd_list->VtxBuffer.Data,
               cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
        memcpy(idx_dst, cmd_list->IdxBuffer.Data,
               cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
        vtx_dst += cmd_list->VtxBuffer.Size;
        idx_dst += cmd_list->IdxBuffer.Size;
    }
    fr->VertexBuffer->Unmap(0, &range);
    fr->IndexBuffer->Unmap(0, &range);

    // Setup desired DX state
    ImGui_ImplDX12_SetupRenderState(draw_data, ctx, fr);

    // Render command lists
    // (Because we merged all buffers into a single one, we maintain our own
    // offset into them)
    int global_vtx_offset = 0;
    int global_idx_offset = 0;
    ImVec2 clip_off = draw_data->DisplayPos;

    for (int n = 0; n < draw_data->CmdListsCount; n++) {
        const ImDrawList* cmd_list = draw_data->CmdLists[n];

        for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++) {
            const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
            // Project scissor/clipping rectangles into framebuffer space
            ImVec2 clip_min(pcmd->ClipRect.x - clip_off.x,
                            pcmd->ClipRect.y - clip_off.y);
            ImVec2 clip_max(pcmd->ClipRect.z - clip_off.x,
                            pcmd->ClipRect.w - clip_off.y);
            if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y)
                continue;

            // Apply Scissor/clipping rectangle, Bind texture, Draw
            const D3D12_RECT r = {(LONG)clip_min.x, (LONG)clip_min.y,
                                  (LONG)clip_max.x, (LONG)clip_max.y};
            D3D12_GPU_DESCRIPTOR_HANDLE texture_handle = {};
            texture_handle.ptr = (UINT64)pcmd->GetTexID();
            ctx->SetGraphicsRootDescriptorTable(1, texture_handle);
            ctx->RSSetScissorRects(1, &r);
            ctx->DrawIndexedInstanced(pcmd->ElemCount, 1,
                                      pcmd->IdxOffset + global_idx_offset,
                                      pcmd->VtxOffset + global_vtx_offset, 0);
        }
        global_idx_offset += cmd_list->IdxBuffer.Size;
        global_vtx_offset += cmd_list->VtxBuffer.Size;
    }
}