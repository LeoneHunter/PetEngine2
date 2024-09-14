#pragma once
#include "base/bump_alloc.h"
#include "base/common.h"

#include "ast_attribute.h"
#include "ast_expression.h"
#include "ast_function.h"
#include "ast_scope.h"
#include "ast_statement.h"
#include "ast_type.h"
#include "ast_variable.h"

#include "parser.h"
#include "program.h"

namespace wgsl {

// Program builder and validator
// Performs lexical analysis and validates nodes on creation
class ProgramBuilder {
public:
    ProgramBuilder();
    ~ProgramBuilder();

    void Build(std::string_view code);
    std::unique_ptr<Program> Finalize();

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
    Expected<const ast::Parameter*> CreateParameter(
        SourceLoc loc,
        const Ident& ident,
        const Ident& typeSpecifier,
        ast::AttributeList& attributes);

    ExpectedVoid DeclareFunction(SourceLoc loc,
                                 const Ident& ident,
                                 ast::AttributeList& attributes,
                                 ast::ParameterList& params,
                                 const Ident& retTypeSpecifier,
                                 ast::AttributeList& retAttributes);

    ExpectedVoid CreateCompoundStatement(SourceLoc loc);

    // Indicates the end of a scoped statement
    void DeclareFuncEnd(SourceLoc loc);

    ExpectedVoid DeclareReturnStatement(SourceLoc loc,
                                        const ast::Expression* expr);

public:
    ExpectedVoid DeclareConst(SourceLoc loc,
                              const Ident& ident,
                              const std::optional<Ident>& typeSpecifier,
                              const ast::Expression* initializer);

    // 'var' variable declaration
    Expected<const ast::VarVariable*> DeclareVariable(
        SourceLoc loc,
        const Ident& ident,
        std::optional<AddressSpace> addrSpace,
        std::optional<AccessMode> accessMode,
        const std::optional<Ident>& typeSpecifier,
        ast::AttributeList& attributes,
        const ast::Expression* initializer);

    Expected<const ast::Struct*> DeclareStruct(SourceLoc loc,
                                               const Ident& ident,
                                               ast::MemberList& members);

    Expected<const ast::Member*> CreateMember(SourceLoc loc,
                                              const Ident& ident,
                                              const Ident& typeSpecifier,
                                              ast::AttributeList& attributes);

    Expected<const ast::Expression*> CreateBinaryExpr(
        SourceLoc loc,
        ast::OpCode op,
        const ast::Expression* lhs,
        const ast::Expression* rhs);

    Expected<const ast::Expression*>
    CreateUnaryExpr(SourceLoc loc, ast::OpCode op, const ast::Expression* rhs);

    Expected<const ast::IdentExpression*> CreateIdentExpr(const Ident& ident);

    Expected<const ast::Expression*> CreateFnCallExpr(
        const Ident& ident,
        const ExpressionList& args);

    Expected<const ast::IntLiteralExpression*>
    CreateIntLiteralExpr(SourceLoc loc, int64_t value, ast::ScalarKind type);


    Expected<const ast::FloatLiteralExpression*>
    CreateFloatLiteralExpr(SourceLoc loc, double value, ast::ScalarKind type);

    Expected<const ast::BoolLiteralExpression*> CreateBoolLiteralExpr(
        SourceLoc loc,
        bool value);

    Expected<const ast::Attribute*> CreateAttribute(
        SourceLoc loc,
        AttributeName attr,
        const ast::Expression* expr = nullptr);

    Expected<const ast::Attribute*> CreateBuiltinAttribute(SourceLoc loc,
                                                           Builtin value);

    Expected<const ast::Attribute*> CreateWorkGroupAttr(
        SourceLoc loc,
        const ast::Expression* x,
        const ast::Expression* y,
        const ast::Expression* z);

private:
    Expected<const ast::Expression*> ResolveArithmeticUnaryOp(
        SourceLoc loc,
        ast::OpCode op,
        const ast::Expression* rhs);

    Expected<const ast::Expression*> ResolveLogicalUnaryOp(
        SourceLoc loc,
        ast::OpCode op,
        const ast::Expression* rhs);

    Expected<const ast::Expression*> ResolveBitwiseUnaryOp(
        SourceLoc loc,
        ast::OpCode op,
        const ast::Expression* rhs);

    Expected<const ast::Expression*> ResolveArithmeticBinaryOp(
        SourceLoc loc,
        ast::OpCode op,
        const ast::Expression* lhs,
        const ast::Expression* rhs);

    Expected<const ast::Expression*> ResolveLogicalBinaryOp(
        SourceLoc loc,
        ast::OpCode op,
        const ast::Expression* lhs,
        const ast::Expression* rhs);

    Expected<const ast::Expression*> ResolveBitwiseBinaryOp(
        SourceLoc loc,
        ast::OpCode op,
        const ast::Expression* lhs,
        const ast::Expression* rhs);


    Expected<const ast::Type*> ResolveTypeName(const Ident& typeSpecifier);

    Expected<const ast::Array*> ResolveArray(const Ident& ident);
    Expected<const ast::Vec*> ResolveVec(const Ident& ident, ast::VecKind kind);
    Expected<const ast::Matrix*> ResolveMatrix(const Ident& ident);

    // Expected<const ast::BuiltinFunction*> ResolveBuiltinFunc(
    //     const Ident& symbol);

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

    Expected<const ast::Scalar*> ResolveBinaryExprTypes(
        SourceLoc loc,
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
    // Helper to track current scope context, function or global,
    // Adds a declaration or a statement to the current node
    class Scope {
    public:
        void Init(ast::GlobalScope* global) {
            globalScope_ = global;
            currentNode_ = global;
            currentSymbols_ = global->symbolTable;
        }

        ast::Symbol* FindSymbol(std::string_view name) {
            return currentSymbols_->FindSymbol(name);
        }

        template <std::derived_from<ast::Symbol> T>
        T* FindSymbol(std::string_view name) {
            if (auto res = currentSymbols_->FindSymbol(name)) {
                return res->As<T>();
            }
            return nullptr;
        }

        // Declares a symbol in the current scope
        void Declare(std::string_view name, ast::Symbol* symbol) {
            if (auto* global = currentNode_->As<ast::GlobalScope>()) {
                global->Declare(name, symbol);
            } else if (auto* func = currentNode_->As<ast::Function>()) {
                func->symbolTable->InsertSymbol(name, symbol);
            } else {
                FATAL("current scope is not global nor function");
            }
        }

        // Declare global builtin symbol
        void DeclareBuiltin(std::string_view name, ast::Symbol* symbol) {
            globalScope_->symbolTable->InsertSymbol(name, symbol);
        }

        bool IsGlobal() const { return currentNode_->Is<ast::GlobalScope>(); }

        // Opens a new scope with a new symbol table
        void PushScope(ast::Function* func) {
            parentNode_ = currentNode_;
            currentNode_ = func;
            currentSymbols_ = func->symbolTable;
        }

        // Closes the current scope
        void PopScope() {
            DASSERT(parentNode_);
            currentNode_ = parentNode_;
            if (auto* global = currentNode_->As<ast::GlobalScope>()) {
                currentSymbols_ = global->symbolTable;
            } else if (auto* func = currentNode_->As<ast::Function>()) {
                currentSymbols_ = func->symbolTable;
            } else {
                FATAL("current scope is not global nor function");
            }
        }

        // Adds a statement to the current function
        void AddStatement(ast::Statement* statement) {
            DASSERT(currentNode_->Is<ast::Function>());
            currentNode_->As<ast::Function>()->AddStatement(statement);
        }

        ast::SymbolTable* GetCurrentSymbolTable() { return currentSymbols_; }

    private:
        ast::GlobalScope* globalScope_ = nullptr;
        ast::Node* currentNode_ = nullptr;
        ast::SymbolTable* currentSymbols_ = nullptr;
        ast::Node* parentNode_ = nullptr;
    };

private:
    std::unique_ptr<Program> program_;
    Scope currentScope_;
    Parser* parser_ = nullptr;
    bool stopParsing_ = false;
};


}  // namespace wgsl