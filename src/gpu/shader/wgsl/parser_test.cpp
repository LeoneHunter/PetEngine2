#include "base/tree_printer.h"
#include "program.h"

#include <doctest.h>

using namespace wgsl;
using TypeName = ast::Type::Kind;

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
    CHECK_MESSAGE(hasErrorCode, "expected an error code '",
                  ErrorCodeString(code), "'");
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

    void Build(std::string_view code) {
        program = Program::Create(code);
    }

    void ExpectGlobalConst(std::string_view s, TypeName type) {
        auto* symbol = program->FindSymbol(s);
        auto* var = symbol->As<ast::ConstVariable>();
        if (!var || !var->type || var->type->kind != type) {
            auto errs = program->GetDiags();
            if (!errs.empty()) {
                const std::string diag = program->GetDiagsAsString();
                LOG_ERROR("WGSL: Errors : \"{}\"", diag);
            }
        }
        CHECK(var != nullptr);
        CHECK(var->type != nullptr);
        CHECK_EQ(var->type->kind, type);
    }

    void ExpectErrNum(uint8_t num) {
        auto errs = program->GetDiags();
        CHECK_EQ(errs.size(), num);
        if (!errs.empty()) {
            const std::string diag = program->GetDiagsAsString();
            LOG_ERROR("WGSL: Errors : \"{}\"", diag);
        }
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
    ExpectError(" const a : p32 = 3; ", ErrorCode::TypeNotDefined);
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
    ExpectGlobalConst("a", TypeName::AbstrInt);
    ExpectGlobalConst("b", TypeName::I32);
    ExpectGlobalConst("c", TypeName::U32);
    ExpectGlobalConst("d", TypeName::F32);
    ExpectGlobalConst("f", TypeName::AbstrFloat);
    ExpectGlobalConst("h", TypeName::I32);
    ExpectGlobalConst("g", TypeName::U32);
    ExpectGlobalConst("k", TypeName::F32);
    ExpectGlobalConst("l", TypeName::AbstrFloat);
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
    ExpectError(" const a = ~4.0; ", ErrorCode::InvalidArgs);
    ExpectError(" const a = ~true; ", ErrorCode::InvalidArgs);
    ExpectError(" const a = -true; ", ErrorCode::InvalidArgs);
    ExpectError(" const a = !4.0; ", ErrorCode::InvalidArgs);
    ExpectError(" const a = !3; ", ErrorCode::InvalidArgs);
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
    ExpectError(" const a = 3 * true; ", ErrorCode::InvalidArgs);
    ExpectError(" const a = ~1u + 100; ", ErrorCode::ConstOverflow);

    ExpectError(" const a = 3 < false; ", ErrorCode::InvalidArgs);
    ExpectError(" const a = 3f < 3u; ", ErrorCode::InvalidArgs);
    ExpectError(" const a = false && 3.0; ", ErrorCode::InvalidArgs);
}

TEST_CASE_FIXTURE(ProgramTest, "[WGSL] operators type checks") {
    Build(R"(
        const a = 1.0 + 2 + 3 + 4;
        const b = 2 + 1.0 + 3 + 4;
        const c = 1f + ((2 + 3) + 4);
        const d = ((2 + (3 + 1f)) + 4);
    )");
    ExpectGlobalConst("a", TypeName::AbstrFloat);
    ExpectGlobalConst("b", TypeName::AbstrFloat);
    ExpectGlobalConst("c", TypeName::F32);
    ExpectGlobalConst("d", TypeName::F32);
    ExpectErrNum(0);
}