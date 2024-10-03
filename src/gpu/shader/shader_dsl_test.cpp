#include "hlsl_codegen.h"
#include "hlsl_compiler.h"
#include "shader_dsl_api.h"

#include <doctest/doctest.h>

using namespace gpu;

namespace {

constexpr std::string StripSpaces(const std::string& str) {
    std::string out;
    out.reserve(str.size());
    for (char c : str) {
        switch (c) {
            case ' ':
            case '\t':
            case '\n': continue;
        }
        out.push_back(c);
    }
    return out;
}

constexpr bool CompareStringStripSpaces(const std::string& lhs,
                                        const std::string& rhs) {
    return StripSpaces(lhs) == StripSpaces(rhs);
}

}  // namespace

TEST_SUITE_BEGIN("ShaderDSL");

TEST_CASE("Global Variable") {
    gpu::ShaderDSLContext ctx;
    HLSLCodeGenerator gen;
    using namespace shader_dsl;

    auto var1 = Float2("var1") | BindIndex(10);
    auto var2 = Float3("var2") | Semantic::Position;
    auto var3 = Texture2D("var3") | BindIndex(13);
    auto var4 = Sampler("var4") | BindIndex(12);

    constexpr auto expected = R"(
        float2 var1 : register(b10);
        float3 var2 : POSITION;
        Texture2D var3 : register(t13);
        SamplerState var4 : register(s12);
    )";

    auto result = gen.Generate(ShaderUsage::Vertex, "", &ctx);
    CHECK(result);
    CHECK(CompareStringStripSpaces(result->code, expected));
}

TEST_CASE("Function Mul Assign") {
    ShaderDSLContext ctx;
    HLSLCodeGenerator gen;
    using namespace shader_dsl;

    Function("func"); {
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

    auto result = gen.Generate(ShaderUsage::Vertex, "", &ctx);
    CHECK(result);
    CHECK(CompareStringStripSpaces(result->code, expected));
}

TEST_CASE("Function Component Access") {
    ShaderDSLContext ctx;
    HLSLCodeGenerator gen;
    using namespace shader_dsl;

    Function("func"); {
        auto param1 = Input<Float2>("param1");
        auto ret1 = Output<Float>("ret1");

        ret1 = param1.X();
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

    auto result = gen.Generate(ShaderUsage::Vertex, "", &ctx);
    CHECK(result);
    CHECK(CompareStringStripSpaces(result->code, expected));
}

TEST_CASE("Function Multiple Returns") {
    ShaderDSLContext ctx;
    HLSLCodeGenerator gen;
    using namespace shader_dsl;

    Function("func"); {
        auto param1 = Input<Float2>("param1");
        auto ret1 = Output<Float>("ret1");
        auto ret2 = Output<Float>("ret2");

        ret1 = param1.X();
        ret2 = param1.Y();
        EndFunction();
    }

    constexpr auto expected = R"(
        struct Struct_0 {
            float ret1;
            float ret2;
        };
        Struct_0 func(float2 param1) {
            Struct_0 local_0;
            float local_1 = param1.x;
            local_0.ret1 = local_1;

            float local_2 = param1.y;
            local_0.ret2 = local_2;

            return local_0;
        }
    )";

    auto result = gen.Generate(ShaderUsage::Vertex, "func", &ctx);
    CHECK(result);
    CHECK(CompareStringStripSpaces(result->code, expected));
}

TEST_CASE("Function basic vertex shader") {
    ShaderDSLContext ctx;
    HLSLCodeGenerator gen;
    {
        using namespace shader_dsl;

        auto projMatrix = Uniform<Float4x4>() | BindIndex(0);

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

    constexpr auto expected = R"(
        float4x4 global_0 : register(b0);
        struct Struct_0 {
            float4 field_0 : POSITION;
            float4 field_1 : COLOR;
            float2 field_2 : TEXCOORD;
        };
        Struct_0 main(float2 local_3 : POSITION, 
                      float4 local_4 : COLOR, 
                      float2 local_5 : TEXCOORD) {
            Struct_0 local_6;
            float local_7 = 1.000000;
            float local_8 = 0.000000;
            float4 local_9;
            local_9 = float4(local_3, local_8, local_7);
            float4 local_10 = mul(global_0, local_9);
            local_6.field_0 = local_10;
            local_6.field_1 = local_4;
            local_6.field_2 = local_5;
            return local_6;
        }
    )";

    auto result = gen.Generate(ShaderUsage::Vertex, "main", &ctx);
    CHECK(result);
    CHECK(CompareStringStripSpaces(result->code, expected));

    auto compileResult = HLSLShaderCompiler::Compile(
        result->mainId, result->code, ShaderUsage::Vertex, kDebugBuild);
    CHECK(!compileResult.hasErrors);
}

TEST_CASE("Function basic pixel shader") {
    ShaderDSLContext ctx;
    HLSLCodeGenerator gen;
    {
        using namespace shader_dsl;

        auto texture = Texture2D() | BindIndex(0);
        auto sampler = Sampler() | BindIndex(1);

        Function("main"); {
            auto inCol = Input<Float4>() | Semantic::Color;
            auto inUV = Input<Float2>() | Semantic::Texcoord;

            auto outCol = Output<Float4>() | Semantic::Color;

            outCol = inCol * SampleTexture(texture, sampler, inUV);
            EndFunction();
        }
    }

    constexpr auto expected = R"(
        Texture2D global_0 : register(t0);
        SamplerState global_1 : register(s1);

        float4 main(float4 local_0 : COLOR, 
                    float2 local_1 : TEXCOORD) : SV_Target {
            float4 local_2;
            float4 local_3 = global_0.Sample(global_1, local_1);
            float4 local_4 = local_0 * local_3;
            local_2 = local_4;
            return local_2;
        }
    )";

    auto result = gen.Generate(ShaderUsage::Pixel, "main", &ctx);
    CHECK(result);
    CHECK(CompareStringStripSpaces(result->code, expected));

    auto compileResult = HLSLShaderCompiler::Compile(
        result->mainId, expected, ShaderUsage::Pixel, kDebugBuild);
    CHECK(!compileResult.hasErrors);
}

TEST_SUITE_END();