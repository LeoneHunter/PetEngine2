#include "program_builder.h"

namespace wgsl {

ProgramBuilder::ProgramBuilder() { }

std::unique_ptr<Program> ProgramBuilder::Finalize() {
    return std::move(program_);
}

Expected<ConstVariable*> ProgramBuilder::CreateConstVar(
    LocationRange loc,
    const std::string& ident,
    Expression* initializer) {
    // Validate scope
    // Validate initializer
    // Validate identifier is unique

    return program_->alloc_.Allocate<ConstVariable>(loc, ident, initializer);
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

Expected<ast::Attribute*> ProgramBuilder::CreateAttribute(LocationRange loc,
                                                          wgsl::Attribute attr,
                                                          Expression* expr) {
    // Validate expression
    return program_->alloc_.Allocate<ast::Attribute>(loc, attr, expr);
}

} // namespace wgsl
