#include "program_builder.h"
#include "parser.h"
#include "program.h"

namespace wgsl {

ProgramBuilder::ProgramBuilder() : program_(new Program()) {}

ProgramBuilder::~ProgramBuilder() = default;

void ProgramBuilder::Build(std::string_view code) {
    program_->sourceCode_ = std::string(code);
    code = program_->sourceCode_;
    auto parser = Parser(code, this);
    parser.Parse();
}

std::unique_ptr<Program> ProgramBuilder::Finalize() {
    // TODO: Do whole program optimizations
    return std::move(program_);
}

void ProgramBuilder::PushGlobalDecl(ast::Variable* var) {
    program_->globalDecls_.push_back(var);
}

void ProgramBuilder::AddSyntaxError(LocationRange loc, const std::string& msg) {
    program_->diags_.push_back({loc, msg, Program::DiagMsg::Type::Error});
    // TODO: Maybe add an option to continue parsing on errors
    stopParsing_ = true;
}

bool ProgramBuilder::ShouldStopParsing() {
    return stopParsing_;
}

Expected<ConstVariable*> ProgramBuilder::CreateConstVar(
    LocationRange loc,
    std::string_view ident,
    const std::optional<TypeInfo>& typeInfo,
    Expression* initializer) {

    // Validate scope
    // Validate initializer
    // Validate identifier is unique

    return program_->alloc_.Allocate<ConstVariable>(loc, ident, initializer);
}

Expected<VarVariable*> ProgramBuilder::CreateVar(
    LocationRange loc,
    std::string_view ident,
    const std::optional<TemplateList>& varTemplate,
    const std::optional<TypeInfo>& typeSpecifier,
    const std::vector<ast::Attribute*>& attributes,
    Expression* initializer) {
    // Check template parameters
    // Check symbols
    // Check types

    return program_->alloc_.Allocate<VarVariable>(loc, ident, attributes,
                                                  initializer);
}

Expected<BinaryExpression*> ProgramBuilder::CreateBinaryExpr(
    LocationRange loc,
    Expression* lhs,
    BinaryExpression::OpCode op,
    Expression* rhs) {
    // Validate types
    // Validate symbols

    return program_->alloc_.Allocate<BinaryExpression>(loc, lhs, op, rhs);
}

Expected<UnaryExpression*> ProgramBuilder::CreateUnaryExpr(
    LocationRange loc,
    UnaryExpression::OpCode op,
    Expression* rhs) {
    // Validate types
    // Validate symbols

    return program_->alloc_.Allocate<UnaryExpression>(loc, op, rhs);
}

Expected<IntLiteralExpression*> ProgramBuilder::CreateIntLiteralExpr(
    LocationRange loc,
    int64_t value,
    IntLiteralExpression::Type type) {
    // Validate size

    return program_->alloc_.Allocate<IntLiteralExpression>(loc, value, type);
}

Expected<FloatLiteralExpression*> ProgramBuilder::CreateFloatLiteralExpr(
    LocationRange loc,
    double value,
    FloatLiteralExpression::Type type) {
    // Validate size

    return program_->alloc_.Allocate<FloatLiteralExpression>(loc, value, type);
}

Expected<BoolLiteralExpression*> ProgramBuilder::CreateBoolLiteralExpr(
    LocationRange loc,
    bool value) {
    return program_->alloc_.Allocate<BoolLiteralExpression>(loc, value);
}

Expected<ast::Attribute*> ProgramBuilder::CreateAttribute(
    LocationRange loc,
    wgsl::AttributeName attr,
    Expression* expr) {
    // Validate expression
    return program_->alloc_.Allocate<ast::Attribute>(loc, attr, expr);
}

}  // namespace wgsl
