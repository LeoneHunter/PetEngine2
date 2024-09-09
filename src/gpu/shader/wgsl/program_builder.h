#pragma once
#include "base/bump_alloc.h"
#include "base/common.h"

#include "ast_attribute.h"
#include "ast_expression.h"
#include "ast_type.h"
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
    ~ProgramBuilder();

    void Build(std::string_view code);

    std::unique_ptr<Program> Finalize();

    void PushGlobalDecl(ast::Variable* var);

    // void PushGlobalDecl(ast::Struct* var);

    // void PushGlobalDecl(ast::Function* fn);

public:
    // Error with message
    template <class... Args>
    void AddError(SourceLoc loc,
                  ErrorCode code,
                  std::format_string<Args...> fmt,
                  Args&&... args) {
        const auto msg = std::format(fmt, std::forward<Args>(args)...);
        FormatAndAddMsg(loc, code, msg);
    }

    void AddError(SourceLoc loc, ErrorCode code, const std::string& str) {
        FormatAndAddMsg(loc, code, str);
    }

    // Error with default message
    void AddError(SourceLoc loc, ErrorCode code) {
        FormatAndAddMsg(loc, code, std::string(ErrorCodeDefaultMsg(code)));
    }

    bool ShouldStopParsing();

public:
    Expected<ConstVariable*> CreateConstVar(
        SourceLoc loc,
        Ident ident,
        const std::optional<TypeInfo>& typeInfo,
        Expression* initializer);

    // 'var' variable declaration
    Expected<VarVariable*> CreateVar(
        SourceLoc loc,
        Ident ident,
        const std::optional<TemplateList>& varTemplate,
        const std::optional<TypeInfo>& typeSpecifier,
        const std::vector<ast::Attribute*>& attributes,
        Expression* initializer);

    Expected<Expression*> CreateBinaryExpr(SourceLoc loc,
                                           Expression* lhs,
                                           OpCode op,
                                           Expression* rhs);

    Expected<Expression*> CreateUnaryExpr(SourceLoc loc,
                                          OpCode op,
                                          Expression* rhs);

    Expected<IdentExpression*> CreateIdentExpr(const Ident& ident);

    Expected<IntLiteralExpression*> CreateIntLiteralExpr(SourceLoc loc,
                                                         int64_t value,
                                                         Type::Kind type);


    Expected<FloatLiteralExpression*> CreateFloatLiteralExpr(SourceLoc loc,
                                                             double value,
                                                             Type::Kind type);

    Expected<BoolLiteralExpression*> CreateBoolLiteralExpr(SourceLoc loc,
                                                           bool value);

    Expected<ast::Attribute*> CreateAttribute(SourceLoc loc,
                                              wgsl::AttributeName attr,
                                              Expression* expr = nullptr);

private:
    // If node is integer literal or const integer ident, returns the value
    std::optional<int64_t> TryGetConstInt(ast::Node* node);
    std::optional<double> ReadConstValueDouble(ast::Node* node);

    bool IsNodeConst(ast::Node* node);

    void FormatAndAddMsg(SourceLoc loc, ErrorCode code, const std::string& msg);

    Expected<ast::Type*> TypeCheckAndConvert(ast::Expression* lhs,
                                             ast::Expression* rhs);

private:
    std::unique_ptr<Program> program_;
    // Current scope, owned by the program
    Program::Scope* currentScope_ = nullptr;
    Parser* parser_ = nullptr;
    bool stopParsing_ = false;

    struct OpTable;
    // Helper to check and evaluate builtin operations
    std::unique_ptr<OpTable> opTable_;
};


}  // namespace wgsl