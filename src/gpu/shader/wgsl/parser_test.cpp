#include "base/tree_printer.h"
#include "program.h"

#include <doctest.h>

using namespace wgsl;
using namespace ast;

namespace {

void PrintAst(Program* program) {
    JsonTreePrinter printer;
    program->PrintAst(&printer);
    LOG_INFO("WGSL: Program ast: \n{}", printer.Finalize());
}

void ExpectErrNum(std::unique_ptr<Program>& program, uint8_t num) {
    auto errs = program->GetDiags();
    CHECK_EQ(errs.size(), num);
    if (!errs.empty()) {
        const std::string diag = program->GetDiagsAsString();
        LOG_ERROR("WGSL: Errors : \"{}\"", diag);
    }
}

// Expect a program to have an error |code|
void CheckErrImpl(Program* program, ErrorCode code) {
    auto& errs = program->GetDiags();
    bool hasErrorCode = false;
    for (const auto& msg : errs) {
        if (msg.code == code) {
            hasErrorCode = true;
            break;
        }
    }
    LOG_VERBOSE("Program: {}\n Errors: {}\n", program->GetSource(),
                program->GetDiagsAsString());
    CHECK_MESSAGE(hasErrorCode, "expected an error code '",
                  ErrorCodeString(code),
                  "'. Got: ", program->GetDiagsAsString());
}

// Expect a program to have an error |code| || ...
template <class... Args>
void ExpectError(std::string_view code, Args... codes) {
    auto program = Program::Create(code);
    (CheckErrImpl(program.get(), codes), ...);
}

void ExpectNoErrors(std::string_view code) {
    auto program = Program::Create(code);
    ExpectErrNum(program, 0);
}

struct ProgramTest {

    void Build(std::string_view code) { program = Program::Create(code); }

    void ExpectGlobalConst(std::string_view s, ScalarKind kind) {
        auto* symbol = program->FindSymbol(s);
        auto* var = symbol->As<ast::ConstVariable>();
        CHECK(var != nullptr);
        CHECK(var->type != nullptr);
        auto* scalar = var->type->As<ast::Scalar>();
        CHECK_EQ(scalar->kind, kind);
        if (scalar->kind != kind) {
            auto errs = program->GetDiags();
            if (!errs.empty()) {
                const std::string diag = program->GetDiagsAsString();
                LOG_ERROR("WGSL: Errors : \"{}\"", diag);
            }
        }
    }

    void ExpectErrNum(uint8_t num) {
        auto errs = program->GetDiags();
        CHECK_EQ(errs.size(), num);
        if (!errs.empty()) {
            const std::string diag = program->GetDiagsAsString();
            LOG_ERROR("WGSL: Errors : \"{}\"", diag);
        }
    }

    void ExpectStruct(std::string_view name) {
        const ast::Symbol* symbol = program->FindSymbol(name);
        CHECK(symbol);
        const ast::Struct* structType = symbol->As<ast::Struct>();
        CHECK_MESSAGE(structType, "expected a struct with the name ", name);
    }

    // Checks whether a struct has a member with type 'Type'
    //   and a name 'name'
    // Full "c++ namespace" style name
    // MyStruct::a, MyStruct::my_member_a
    template <class Type>
    void ExpectStructMember(std::string_view name) {
        const size_t sepPos = name.find("::");
        DASSERT(sepPos != name.npos);
        const std::string_view structName = name.substr(0, sepPos);
        const std::string_view memberName = name.substr(sepPos + 2);

        const ast::Symbol* symbol = program->FindSymbol(structName);
        CHECK(symbol);
        const ast::Struct* structType = symbol->As<ast::Struct>();
        CHECK_MESSAGE(structType, "expected a struct '", name, "'");

        const ast::Member* member = [&] {
            for (const ast::Member* member : structType->members) {
                if (member->name == memberName) {
                    return member;
                }
            }
            return (const Member*)nullptr;
        }();
        CHECK_MESSAGE(member, "expected a struct member '", name, "'");
        CHECK_MESSAGE(member->type->Is<Type>(), "expected a struct member '",
                      name, "' with type '", to_string(Type::kStaticType), "'");
    }

    std::unique_ptr<Program> program;
};

}  // namespace

//============================================================//

TEST_CASE("[WGSL] basic errors") {
    ExpectError(" const a = 4 ", ErrorCode::UnexpectedToken);
    ExpectError(" const a 4; ", ErrorCode::ConstDeclNoInitializer);
    ExpectError(" const a = ; ", ErrorCode::ExpectedExpr);
    ExpectError(" const = 4; ", ErrorCode::UnexpectedToken);
    ExpectError(" const a : = 4; ", ErrorCode::ExpectedType);
    ExpectError(" const a :  ", ErrorCode::ExpectedType);
    ExpectError("  = 4; ", ErrorCode::ExpectedDecl);
    ExpectError(" @invalid const a = 4; ", ErrorCode::InvalidAttribute);
    ExpectError(" @vertex const a = 4; ", ErrorCode::ConstNoAttr);
    // FIXME: Gets ErrorCode::ExpectedIdent
    // ExpectError(" const auto = 3; ", ErrorCode::IdentReserved);
    ExpectError(" const a : i32; ", ErrorCode::ConstDeclNoInitializer);
    ExpectError(" const a : p32 = 3; ", ErrorCode::SymbolNotFound);
    ExpectError(" const a = 3; const b : a = 4; ", ErrorCode::IdentNotType);
    ExpectError(" const a : i32 = 3.2f; ", ErrorCode::TypeError);
    ExpectError(" const a = 3; const a = 4; ", ErrorCode::SymbolAlreadyDefined);
    ExpectError(" const a = b * 4; ", ErrorCode::SymbolNotFound);
    ExpectError(" const a = 3; const b = i32 * 4; ",
                ErrorCode::SymbolNotVariable);
    ExpectError(" const a : u32 = 0x200000000; ",
                ErrorCode::LiteralInitValueTooLarge);
    ExpectError(" const a : i32 = 0x80000000; ",
                ErrorCode::LiteralInitValueTooLarge);
}

constexpr auto kConstVariables = R"(
const a = 4;                  // AbstractInt with a value of 4.
const b : i32 = 4;           // i32 with a value of 4.
const c : u32 = 4;            // u32 with a value of 4.
const d : f32 = 4;            // f32 with a value of 4.
const f = 2.0;                // AbstractFloat with a value of 2.

const h = a * b;              // i32
const g = a * c;              // u32
const k = a * d;              // f32
const l = a * f;              // AbstractFloat
)";

TEST_CASE_FIXTURE(ProgramTest, "[WGSL] type checks") {
    Build(kConstVariables);
    ExpectGlobalConst("a", ScalarKind::Int);
    ExpectGlobalConst("b", ScalarKind::I32);
    ExpectGlobalConst("c", ScalarKind::U32);
    ExpectGlobalConst("d", ScalarKind::F32);
    ExpectGlobalConst("f", ScalarKind::Float);
    ExpectGlobalConst("h", ScalarKind::I32);
    ExpectGlobalConst("g", ScalarKind::U32);
    ExpectGlobalConst("k", ScalarKind::F32);
    ExpectGlobalConst("l", ScalarKind::Float);
    ExpectErrNum(0);
}

TEST_CASE("[WGSL] type check errors") {
    ExpectError(" const a : i32 = 1.0; ", ErrorCode::TypeError);
    ExpectError(" const a : u32 = 1234567890123456890; ",
                ErrorCode::LiteralInitValueTooLarge);
    ExpectError(" const a : i32 = 1234567890123456890; ",
                ErrorCode::LiteralInitValueTooLarge);
    ExpectError(" const a : i32 = 2147483649; ",
                ErrorCode::LiteralInitValueTooLarge);
}

TEST_CASE("[WGSL] unary operators") {
    ExpectNoErrors(" const a = ~3;");
    ExpectNoErrors(" const a = ~3u;");
    ExpectNoErrors(" const a = ~3i;");

    ExpectNoErrors(" const a = -3;");
    ExpectNoErrors(" const a = -3i;");
    ExpectNoErrors(" const a = -3u;");
    ExpectNoErrors(" const a = -3.0;");
    ExpectNoErrors(" const a = -3.0f;");

    ExpectNoErrors(" const a = !false;");
    ExpectNoErrors(" const a = !!false;");
    ExpectNoErrors(" const a = !true;");
    ExpectNoErrors(" const a = !!true;");
}

TEST_CASE("[WGSL] unary operators errors") {
    ExpectError(" const a = ~4.0; ", ErrorCode::InvalidArg);
    ExpectError(" const a = ~true; ", ErrorCode::InvalidArg);
    ExpectError(" const a = -true; ", ErrorCode::InvalidArg);
    ExpectError(" const a = !4.0; ", ErrorCode::InvalidArg);
    ExpectError(" const a = !3; ", ErrorCode::InvalidArg);
}

TEST_CASE("[WGSL] binary operators") {
    ExpectNoErrors(" const a = 3 * 3;");
    ExpectNoErrors(" const a = 4 - 5;");
    ExpectNoErrors(" const a = 8 + 10 * 12;");
    ExpectNoErrors(" const a = 8 % 4 * 2;");

    ExpectNoErrors(" const a = 1 > 2;");
    ExpectNoErrors(" const a = 3 <= 2;");
    ExpectNoErrors(" const a = true && false;");
    ExpectNoErrors(" const a = true || false;");
    ExpectNoErrors(" const a = (3 > 7u) || (3.0 < 4.5f);");
}

TEST_CASE("[WGSL] binary operators errors") {
    ExpectError(" const a = 3 * true; ", ErrorCode::InvalidArg);
    ExpectError(" const a = ~1u + 100; ", ErrorCode::ConstOverflow);

    ExpectError(" const a = 3 < false; ", ErrorCode::InvalidArg);
    ExpectError(" const a = 3f < 3u; ", ErrorCode::InvalidArg);
    ExpectError(" const a = false && 3.0; ", ErrorCode::InvalidArg);
}

TEST_CASE_FIXTURE(ProgramTest, "[WGSL] operators type checks") {
    Build(R"(
        const a = 1.0 + 2 + 3 + 4;
        const b = 2 + 1.0 + 3 + 4;
        const c = 1f + ((2 + 3) + 4);
        const d = ((2 + (3 + 1f)) + 4);
    )");
    ExpectGlobalConst("a", ScalarKind::Float);
    ExpectGlobalConst("b", ScalarKind::Float);
    ExpectGlobalConst("c", ScalarKind::F32);
    ExpectGlobalConst("d", ScalarKind::F32);
    ExpectErrNum(0);
}

TEST_CASE_FIXTURE(ProgramTest, "[WGSL] struct type, templates") {
    Build(R"(
        struct MyStruct { 
            a : vec2<f32>,
            b : vec2f,  
            c : vec3<i32>,
            d : vec4<f32>,
            e : f32,
            f : array<f32, 10>
            h : mat4x4f
        };
    )");
    ExpectStruct("MyStruct");
    ExpectStructMember<ast::Vec>("MyStruct::a");
    ExpectStructMember<ast::Vec>("MyStruct::b");
    ExpectStructMember<ast::Vec>("MyStruct::c");
    ExpectStructMember<ast::Vec>("MyStruct::d");
    ExpectStructMember<ast::Scalar>("MyStruct::e");
    ExpectStructMember<ast::Array>("MyStruct::f");
    ExpectStructMember<ast::Matrix>("MyStruct::h");
    ExpectErrNum(0);
}