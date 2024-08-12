#include "shader_dsl.h"
#include "hlsl_codegen.h"
#include "thirdparty/doctest/doctest.h"

using namespace gpu;

constexpr std::string StripSpaces(const std::string& str) {
    std::string out;
    out.reserve(str.size());
    for (char c : str) {
        switch (c) {
            case ' ':
            case '\t':
            case '\n':
                continue;
        }
        out.push_back(c);
    }
    return out;
}

constexpr bool CompareStringStripSpaces(const std::string& lhs,
                                        const std::string& rhs) {
    return StripSpaces(lhs) == StripSpaces(rhs);
}

TEST_SUITE_BEGIN("ShaderDSL");

TEST_CASE("Global Variable") {
    gpu::ShaderDSLContext ctx;
    HLSLCodeGenerator gen;
    using namespace shader_dsl;

    auto var1 = Declare<Float2>("var1") | BindIndex(10);
    auto var2 = Declare<Float3>("var2") | Semantic::Position;
    auto var3 = Declare<Texture2D>("var3") | BindIndex(13);
    auto var4 = Declare<Sampler>("var4") | BindIndex(12);

    constexpr auto expected = R"(
        float2 var1 : register(b10);
        float3 var2 : Position;
        Texture2D var3 : register(t13);
        SamplerState var4 : register(s12);
    )";

    auto result = ctx.BuildHLSL(&gen);
    CHECK(result);
    CHECK(CompareStringStripSpaces(result->code, expected));
}

TEST_CASE("Function Mul Assign") {
    ShaderDSLContext ctx;
    HLSLCodeGenerator gen;
    using namespace shader_dsl;

    Function("func");
    {
        auto param1 = Input<Float2>("param1");
        auto param2 = Input<Float2>("param2");
        auto ret1 = Output<Float2>("ret1");

        ret1 = param1 * param2;
        EndFunction();
    }

    constexpr auto expected = R"(
        float2 func(float2 param1, float2 param2) {
            float2 ret1;
            float2 local_0 = param1 * param2;
            ret1 = local_0;
            return ret1;
        }
    )";

    auto result = ctx.BuildHLSL(&gen);
    CHECK(result);
    CHECK(CompareStringStripSpaces(result->code, expected));
}

TEST_CASE("Function Component Access") {
    ShaderDSLContext ctx;
    HLSLCodeGenerator gen;
    using namespace shader_dsl;

    Function("func");
    {
        auto param1 = Input<Float2>("param1");
        auto ret1 = Output<Float>("ret1");

        ret1 = GetX(param1);
        EndFunction();
    }

    constexpr auto expected = R"(
        float func(float2 param1) {
            float ret1;
            float local_0 = param1.x;
            ret1 = local_0;
            return ret1;
        }
    )";

    auto result = ctx.BuildHLSL(&gen);
    CHECK(result);
    CHECK(CompareStringStripSpaces(result->code, expected));
}

// TEST_CASE("[ShaderDSL] sk") {
//     shader_dsl::ShaderDSLContext builder;
//     std::unique_ptr<ShaderCode> vertexCode;
//     std::unique_ptr<ShaderCode> pixelCode;

//     constexpr BindIndex kProjMatrixBindIndex = 0;
//     constexpr BindIndex kSamplerBindIndex = 1;
//     constexpr BindIndex kTextureBindIndex = 2;

//     // Vertex shader
//     {
//         using namespace shader_dsl;
//         auto in_pos = Input<Float2>() | Semantic::Position;
//         auto in_col = Input<Float4>() | Semantic::Color;
//         auto in_uv = Input<Float2>() | Semantic::Texcoord;

//         auto out_pos = Output<Float4>() | Semantic::Position;
//         auto out_col = Output<Float4>() | Semantic::Color;
//         auto out_uv = Output<Float2>() | Semantic::Texcoord;

//         auto projMatrix = Uniform<Float4x4>() |
//         BindIndex(kProjMatrixBindIndex);

//         FuncMain(); {
//             auto pos = CreateFloat4(GetX(in_pos), GetY(in_pos),
//                                     CreateFloat(0.f), CreateFloat(1.f));
//             out_pos = projMatrix * pos;
//             out_col = in_col;
//             out_uv = in_uv;
//             EndFunc();
//         }
//     }
//     vertexCode = builder.BuildHLSL();
//     builder.Clear();

//     // Pixel
//     {
//         using namespace shader_dsl;
//         auto sampler = Declare<Sampler>() | BindIndex(kSamplerBindIndex);
//         auto texture = Declare<Texture2D>() | BindIndex(kTextureBindIndex);

//         auto in_col = Input<Float4>() | Semantic::Color;
//         auto in_uv = Input<Float2>() | Semantic::Texcoord;

//         auto out_col = Output<Float4>() | Semantic::Color;

//         FuncMain(); {
//             out_col = in_col * SampleTexture(texture, sampler, in_uv);
//             EndFunc();
//         }
//     }
//     pixelCode = builder.BuildHLSL();
// }

TEST_SUITE_END();