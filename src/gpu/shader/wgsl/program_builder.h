#pragma once
#include "base/bump_alloc.h"
#include "base/common.h"

#include "ast_attribute.h"
#include "ast_expression.h"
#include "ast_function.h"
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
    void PushGlobalDecl(ast::Struct* var);
    void PushGlobalDecl(ast::Function* fn);

public:
    bool ShouldStopParsing();

    template <class... Args>
    std::unexpected<ErrorCode> ReportError(SourceLoc loc,
                                           ErrorCode code,
                                           std::format_string<Args...> fmt,
                                           Args&&... args) {
        return ReportError(loc, code,
                           std::format(fmt, std::forward<Args>(args)...));
    }

    std::unexpected<ErrorCode> ReportError(SourceLoc loc, ErrorCode code) {
        return ReportErrorImpl(loc, code, "");
    }

    std::unexpected<ErrorCode> ReportError(SourceLoc loc,
                                           ErrorCode code,
                                           const std::string& msg) {
        return ReportErrorImpl(loc, code, msg);
    }

public:
    Expected<ConstVariable*> CreateConstVar(
        SourceLoc loc,
        const Ident& ident,
        const std::optional<Ident>& typeSpecifier,
        const Expression* initializer);

    // 'var' variable declaration
    Expected<VarVariable*> CreateVar(SourceLoc loc,
                                     const Ident& ident,
                                     std::optional<AddressSpace> addrSpace,
                                     std::optional<AccessMode> accessMode,
                                     const std::optional<Ident>& typeSpecifier,
                                     AttributeList& attributes,
                                     const Expression* initializer);

    Expected<Struct*> CreateStruct(SourceLoc loc,
                                   const Ident& ident,
                                   MemberList& members);

    Expected<Member*> CreateMember(SourceLoc loc,
                                   const Ident& ident,
                                   const Ident& typeSpecifier,
                                   AttributeList& attributes);

    Expected<Expression*> CreateBinaryExpr(SourceLoc loc,
                                           OpCode op,
                                           const Expression* lhs,
                                           const Expression* rhs);

    Expected<Expression*> CreateUnaryExpr(SourceLoc loc,
                                          OpCode op,
                                          const Expression* rhs);

    Expected<IdentExpression*> CreateIdentExpr(const Ident& ident);

    Expected<Expression*> CreateFnCallExpr(const Ident& ident,
                                           const ExpressionList& args);

    Expected<IntLiteralExpression*> CreateIntLiteralExpr(SourceLoc loc,
                                                         int64_t value,
                                                         ScalarKind type);


    Expected<FloatLiteralExpression*> CreateFloatLiteralExpr(SourceLoc loc,
                                                             double value,
                                                             ScalarKind type);

    Expected<BoolLiteralExpression*> CreateBoolLiteralExpr(SourceLoc loc,
                                                           bool value);

    Expected<ast::Attribute*> CreateAttribute(SourceLoc loc,
                                              AttributeName attr,
                                              const Expression* expr = nullptr);

private:
    Expected<Expression*> ResolveArithmeticUnaryOp(SourceLoc loc,
                                                   OpCode op,
                                                   const Expression* rhs);

    Expected<Expression*> ResolveLogicalUnaryOp(SourceLoc loc,
                                                OpCode op,
                                                const Expression* rhs);

    Expected<Expression*> ResolveBitwiseUnaryOp(SourceLoc loc,
                                                OpCode op,
                                                const Expression* rhs);

    Expected<Expression*> ResolveArithmeticBinaryOp(SourceLoc loc,
                                                    OpCode op,
                                                    const Expression* lhs,
                                                    const Expression* rhs);

    Expected<Expression*> ResolveLogicalBinaryOp(SourceLoc loc,
                                                 OpCode op,
                                                 const Expression* lhs,
                                                 const Expression* rhs);

    Expected<Expression*> ResolveBitwiseBinaryOp(SourceLoc loc,
                                                 OpCode op,
                                                 const Expression* lhs,
                                                 const Expression* rhs);


    Expected<const ast::Type*> ResolveTypeName(const Ident& typeSpecifier);

    Expected<const ast::Array*> ResolveArray(const Ident& ident);
    Expected<const ast::Vec*> ResolveVec(const Ident& ident, VecKind kind);
    Expected<const ast::Matrix*> ResolveMatrix(const Ident& ident);
    Expected<const ast::BuiltinFunction*> ResolveBuiltinFunc(
        const Ident& symbol);

private:
    template <class T>
    ErrorCode CheckOverflow(const ast::Scalar* type, T val) const {
        if (type->kind == ast::ScalarKind::U32) {
            if (val > std::numeric_limits<uint32_t>::max()) {
                return ErrorCode::ConstOverflow;
            }
        }
        if (type->kind == ast::ScalarKind::I32) {
            if (val > std::numeric_limits<int32_t>::max()) {
                return ErrorCode::ConstOverflow;
            }
        }
        if (type->kind == ast::ScalarKind::F32) {
            if (val > std::numeric_limits<float>::max()) {
                return ErrorCode::ConstOverflow;
            }
        }
        return ErrorCode::Ok;
    }

    Expected<const ast::Scalar*> ResolveBinaryExprTypes(SourceLoc loc,
                                              const ast::Expression* lhs,
                                              const ast::Expression* rhs);

    ErrorCode CheckExpressionArg(SourceLoc loc, const ast::Expression* arg);

    void CreateBuiltinSymbols();

    std::unexpected<ErrorCode> ReportErrorImpl(SourceLoc loc,
                                               ErrorCode code,
                                               const std::string& msg = {}) {
        const auto m =
            msg.empty() ? std::string(ErrorCodeDefaultMsg(code)) : msg;
        FormatAndAddMsg(loc, code, m);
        return std::unexpected(code);
    }

    void FormatAndAddMsg(SourceLoc loc, ErrorCode code, const std::string& msg);

private:
    std::unique_ptr<Program> program_;
    // Current scope, owned by the program
    Program::Scope* currentScope_ = nullptr;
    Parser* parser_ = nullptr;
    bool stopParsing_ = false;
};


}  // namespace wgsl