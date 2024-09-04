#pragma once
#include "base/bump_alloc.h"
#include "base/common.h"

#include "ast_attribute.h"
#include "ast_expression.h"
#include "ast_variable.h"

#include "parser.h"
#include "program.h"

namespace wgsl {

using namespace ast;

// Program builder and validator
// Performs lexical analysis and validates nodes on creation
class ProgramBuilder {
public:
    ProgramBuilder();

    std::unique_ptr<Program> Finalize();

    void PushGlobalDecl(ast::Variable* var);

    // void PushGlobalDecl(ast::Struct* var);
    
    // void PushGlobalDecl(ast::Function* fn);

public:
    Expected<ConstVariable*> CreateConstVar(LocationRange loc,
                                            const std::string& ident,
                                            Expression* initializer);

    Expected<BinaryExpression*> CreateBinaryExpr(LocationRange loc,
                                                 Expression* lhs,
                                                 BinaryExpression::OpCode op,
                                                 Expression* rhs);

    Expected<UnaryExpression*> CreateUnaryExpr(LocationRange loc,
                                               UnaryExpression::OpCode op,
                                               Expression* rhs);

    Expected<IntLiteralExpression*> CreateIntLiteralExpr(
        LocationRange loc,
        int64_t value,
        IntLiteralExpression::Type type);


    Expected<FloatLiteralExpression*> CreateFloatLiteralExpr(
        LocationRange loc,
        double value,
        FloatLiteralExpression::Type type);

    Expected<BoolLiteralExpression*> CreateBoolLiteralExpr(LocationRange loc,
                                                           bool value);

    Expected<ast::Attribute*> CreateAttribute(LocationRange loc,
                                              wgsl::Attribute attr,
                                              Expression* expr = nullptr);

private:
    bool ValidateType();
    bool ValidateIdentifier();
    bool ValidateAttribute();

private:
    std::unique_ptr<Program> program_;
};


}  // namespace wgsl