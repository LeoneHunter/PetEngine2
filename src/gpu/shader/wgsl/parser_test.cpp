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

void CheckSymbol(std::unique_ptr<Program>& program,
                 std::string_view s,
                 TypeName type) {
    auto* symbol = program->FindSymbol(s);
    auto* var = symbol->As<ast::ConstVariable>();
    CHECK(var != nullptr);
    CHECK(var->type != nullptr);
    CHECK_EQ(var->type->kind, type);
}

void CheckErrNum(std::unique_ptr<Program>& program, uint8_t num) {
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
void CheckAnyErr(std::string_view code, Args... codes) {
    auto program = Program::Create(code);
    (CheckErrImpl(program.get(), codes), ...);
}

}  // namespace

//============================================================//

TEST_CASE("[WGSL] var decl errors") {
    CheckAnyErr(" const a = 4 ", ErrorCode::UnexpectedToken);
    CheckAnyErr(" const a 4; ", ErrorCode::ConstDeclNoInitializer);
    CheckAnyErr(" const a = ; ", ErrorCode::ExpectedExpr);
    CheckAnyErr(" const = 4; ", ErrorCode::UnexpectedToken);
    CheckAnyErr(" const a : = 4; ", ErrorCode::ExpectedType);
    CheckAnyErr(" const a :  ", ErrorCode::ExpectedType);
    CheckAnyErr("  = 4; ", ErrorCode::ExpectedDecl);
    CheckAnyErr(" @invalid const a = 4; ", ErrorCode::InvalidAttribute);
    CheckAnyErr(" @vertex const a = 4; ", ErrorCode::ConstNoAttr);
    // FIXME: Gets ErrorCode::ExpectedIdent
    // CheckAnyErr(" const auto = 3; ", ErrorCode::IdentReserved);
    CheckAnyErr(" const a : i32; ", ErrorCode::ConstDeclNoInitializer);
    CheckAnyErr(" const a : p32 = 3; ", ErrorCode::TypeNotDefined);
    CheckAnyErr(" const a = 3; const b : a = 4; ", ErrorCode::IdentNotType);
    CheckAnyErr(" const a : i32 = 3.2f; ", ErrorCode::TypeError);
    CheckAnyErr(" const a = 3; const a = 4; ", ErrorCode::SymbolAlreadyDefined);
    CheckAnyErr(" const a = b * 4; ", ErrorCode::SymbolNotFound);
    CheckAnyErr(" const a = 3; const b = i32 * 4; ", ErrorCode::SymbolNotVariable);
}

constexpr auto kConstVariables = R"(
const a = 4;                  // AbstractInt with a value of 4.
const b : i32 = 4;            // i32 with a value of 4.
const c : u32 = 4;            // u32 with a value of 4.
const d : f32 = 4;            // f32 with a value of 4.
const f = 2.0;                // AbstractFloat with a value of 2.

const h = a * b;              // i32
const g = a * c;              // u32
const k = a * d;              // f32
const l = a * f;              // AbstractFloat
)";

TEST_CASE("[WGSL] type checks") {
    auto program = Program::Create(kConstVariables);
    CheckSymbol(program, "a", TypeName::AbstrInt);
    CheckSymbol(program, "b", TypeName::I32);
    CheckSymbol(program, "c", TypeName::U32);
    CheckSymbol(program, "d", TypeName::F32);
    CheckSymbol(program, "f", TypeName::AbstrFloat);
    CheckSymbol(program, "h", TypeName::I32);
    CheckSymbol(program, "g", TypeName::U32);
    CheckSymbol(program, "k", TypeName::F32);
    CheckSymbol(program, "l", TypeName::AbstrFloat);
    CheckErrNum(program, 0);
}