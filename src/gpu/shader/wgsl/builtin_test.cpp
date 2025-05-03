#include "builtin.h"
#include "ast_alias.h"
#include "ast_scope.h"

#include <doctest/doctest.h>

using namespace wgsl;
using namespace wgsl::ast::internal;

struct SignatureTest {
    SignatureTest() : al(), symbols(nullptr), b(&al, &symbols) {
        InitSymbols();
    }

    void InitSymbols() {
        using namespace wgsl::ast;
        // Predeclare scalars
        const auto declareScalar = [&](ScalarKind kind) {
            auto* type = al.Allocate<ast::Scalar>(kind);
            symbols.InsertSymbol(to_string(kind), type);
            return type;
        };
        declareScalar(ScalarKind::Float);
        declareScalar(ScalarKind::Int);
        auto scBool = declareScalar(ScalarKind::Bool);
        auto scU32 = declareScalar(ScalarKind::U32);
        auto scI32 = declareScalar(ScalarKind::I32);
        auto scF32 = declareScalar(ScalarKind::F32);

        // Predeclare vectors
        const auto declareVec = [&](ast::VecKind kind, ast::Scalar* valueType,
                                    std::string_view symbol,
                                    std::string_view alias = "") {
            auto* type = al.Allocate<ast::Vec>(kind, valueType, symbol);
            symbols.InsertSymbol(symbol, type);

            if (!alias.empty()) {
                auto* aliasType =
                    al.Allocate<ast::Alias>(SourceLoc(), alias, type);
                symbols.InsertSymbol(alias, aliasType);
            }
        };
        declareVec(ast::VecKind::Vec2, scBool, "vec2<bool>");
        declareVec(ast::VecKind::Vec3, scBool, "vec3<bool>");
        declareVec(ast::VecKind::Vec4, scBool, "vec4<bool>");
        declareVec(ast::VecKind::Vec2, scF32, "vec2<f32>", "vec2f");
        declareVec(ast::VecKind::Vec2, scI32, "vec2<i32>", "vec2i");
        declareVec(ast::VecKind::Vec2, scU32, "vec2<u32>", "vec2u");
        declareVec(ast::VecKind::Vec3, scF32, "vec3<f32>", "vec3f");
        declareVec(ast::VecKind::Vec3, scI32, "vec3<i32>", "vec3i");
        declareVec(ast::VecKind::Vec3, scU32, "vec3<u32>", "vec3u");
        declareVec(ast::VecKind::Vec4, scF32, "vec4<f32>", "vec4f");
        declareVec(ast::VecKind::Vec4, scI32, "vec4<i32>", "vec4i");
        declareVec(ast::VecKind::Vec4, scU32, "vec4<u32>", "vec4u");

        // Predeclare matrices
        const auto declareMat = [&](ast::MatrixKind kind,
                                    std::string_view symbol,
                                    std::string_view alias = "") {
            auto* type = al.Allocate<ast::Matrix>(kind, scF32);
            symbols.InsertSymbol(symbol, type);

            if (!alias.empty()) {
                auto* aliasType =
                    al.Allocate<ast::Alias>(SourceLoc(), alias, type);
                symbols.InsertSymbol(alias, aliasType);
            }
        };
        declareMat(ast::MatrixKind::Mat2x2, "mat2x2<f32>", "mat2x2f");
        declareMat(ast::MatrixKind::Mat2x3, "mat2x3<f32>", "mat2x3f");
        declareMat(ast::MatrixKind::Mat2x4, "mat2x4<f32>", "mat2x4f");
        declareMat(ast::MatrixKind::Mat3x2, "mat3x2<f32>", "mat3x2f");
        declareMat(ast::MatrixKind::Mat3x3, "mat3x3<f32>", "mat3x3f");
        declareMat(ast::MatrixKind::Mat3x4, "mat3x4<f32>", "mat3x4f");
        declareMat(ast::MatrixKind::Mat4x2, "mat4x2<f32>", "mat4x2f");
        declareMat(ast::MatrixKind::Mat4x3, "mat4x3<f32>", "mat4x3f");
        declareMat(ast::MatrixKind::Mat4x4, "mat4x4<f32>", "mat4x4f");
    }

    template <class... Args>
    bool Check(const Signature& sig, Args... arg) {
        std::vector<const ast::Type*> args;
        (args.push_back(ToType(arg)), ...);
        return sig.Match(args);
    }

    // Last arg is return arg
    template <class... Args>
    bool CheckRet(const Signature& sig, Args... arg) {
        std::vector<const ast::Type*> args;
        (args.push_back(ToType(arg)), ...);
        auto actual = sig.Match(std::span(args.begin(), args.size() - 1));
        auto expected = args.back();
        return actual == expected;
    }

    const ast::Type* ToType(std::string_view name) {
        if (auto symbol = symbols.FindSymbol(name)) {
            if (auto type = symbol->As<ast::Type>()) {
                return type;
            }
        }
        DASSERT_F(false, "Unknown type name {}", name);
        return nullptr;
    }

    BumpAllocator al;
    ast::SymbolTable symbols;
    SignatureParser b;
};

#define Test(...) CHECK(Check(sig, __VA_ARGS__))
#define TestFalse(...) CHECK_FALSE(Check(sig, __VA_ARGS__))
#define TestReturn(...) CHECK(CheckRet(sig, __VA_ARGS__))

TEST_CASE_FIXTURE(SignatureTest, "[WGSL::Builtin] bool") {
    auto res = b.Parse("[T: scalar] @const @must_use fn bool(e : T) -> bool");
    CHECK(res.has_value());
    auto sig = std::move(res.value());
    Test("f32");
    Test("u32");
    Test("i32");
    Test("float");
    Test("int");
    Test("bool");
    TestFalse("mat2x2<f32>");
    TestFalse("vec2<f32>");
    TestReturn("f32", "bool");
}

TEST_CASE_FIXTURE(SignatureTest, "[WGSL::Builtin] abs") {
    auto res = b.Parse(
        "[T: numeric | vec<numeric>] @const @must_use fn abs(e: T ) -> T");
    CHECK(res.has_value());
    auto sig = std::move(res.value());
    Test("f32");
    Test("u32");
    Test("i32");
    Test("float");
    Test("int");
    Test("vec2<f32>");
    Test("vec3<u32>");
    Test("vec4<i32>");
    TestFalse("bool");
    TestFalse("vec2<bool>");
    TestFalse("vec3<bool>");
    TestFalse("vec4<bool>");
    TestFalse("mat2x2<f32>");
    TestReturn("f32", "f32");
}

TEST_CASE_FIXTURE(SignatureTest, "[WGSL::Builtin] length") {
    auto res = b.Parse(
        "[S: float, T: S | vec<S>] @const @must_use fn length(e: T) -> S");
    CHECK(res.has_value());
    auto sig = std::move(res.value());
    Test("float");
    Test("f32");
    Test("vec2<f32>");
    Test("vec3<f32>");
    Test("vec4<f32>");
    TestFalse("u32");
    TestFalse("i32");
    TestFalse("int");
    TestFalse("bool");
    TestFalse("vec3<u32>");
    TestFalse("vec2<bool>");
    TestFalse("mat2x2<f32>");
    TestReturn("f32", "f32");
    TestReturn("vec2<f32>", "f32");
}

TEST_CASE_FIXTURE(SignatureTest, "[WGSL::Builtin] clamp") {
    auto res = b.Parse(
        "[T: numeric | vec<numeric>] "
        "@const @must_use fn clamp(e: T, low: T, high: T) -> T");
    CHECK(res.has_value());
    auto sig = std::move(res.value());
    Test("f32", "f32", "f32");
    Test("vec2<f32>", "vec2<f32>", "vec2<f32>");
    TestFalse("f32", "i32", "f32");
    TestFalse("bool", "bool", "bool");
    TestFalse("bool");
    TestFalse("mat2x2<f32>");
    TestReturn("f32", "f32", "f32", "f32");
}