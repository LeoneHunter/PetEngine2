#include "program_builder.h"
#include "ast_alias.h"
#include "parser.h"
#include "program.h"

#include <set>

#define EXPECT_OK(expr)                    \
    do {                                   \
        auto code = expr;                  \
        if (code != ErrorCode::Ok) {       \
            return ReportError(loc, code); \
        }                                  \
    } while (false)

// Try expression, return if errored
#define TRY_UNWRAP(out, expr)                    \
    do {                                         \
        auto res = expr;                         \
        if (res) {                               \
            out = *res;                          \
        } else {                                 \
            return std::unexpected(res.error()); \
        }                                        \
    } while (false)

// Check boolean expr, return if false
#define EXPECT_TRUE(expr, ...)               \
    do {                                     \
        if (!(expr)) {                       \
            return ReportError(__VA_ARGS__); \
        }                                    \
    } while (false)

namespace wgsl {

namespace {

using EvalResult = std::variant<double, int64_t, bool>;

template <class T, class R>
std::optional<EvalResult> EvalBinaryOp(OpCode op,
                                       const ast::Expression* lhs,
                                       const ast::Expression* rhs) {
    auto lval = lhs->TryGetConstValueAs<T>();
    auto rval = rhs->TryGetConstValueAs<T>();
    if (lval && rval) {
        auto value = [&]() -> R {
            using Op = OpCode;
            if constexpr (std::same_as<R, bool>) {
                switch (op) {
                    case Op::Less: return *lval < *rval;
                    case Op::Greater: return *lval > *rval;
                    case Op::LessEqual: return *lval <= *rval;
                    case Op::GreaterEqual: return *lval >= *rval;
                    case Op::Equal: return *lval == *rval;
                    case Op::NotEqual: return *lval != *rval;
                    case Op::LogAnd: return *lval && *rval;
                    case Op::LogOr: return *lval || *rval;
                }
            } else if constexpr (std::same_as<R, int64_t> ||
                                 std::same_as<R, uint64_t>) {
                switch (op) {
                    case Op::Add: return *lval + *rval;
                    case Op::Sub: return *lval - *rval;
                    case Op::Mul: return *lval * *rval;
                    case Op::Div: return *lval / *rval;
                    case Op::BitAnd: return *lval & *rval;
                    case Op::BitOr: return *lval | *rval;
                    case Op::BitXor: return *lval ^ *rval;
                    case Op::BitLsh: return *lval << *rval;
                    case Op::BitRsh: return *lval >> *rval;
                }
            } else if constexpr (std::same_as<R, double>) {
                switch (op) {
                    case Op::Add: return *lval + *rval;
                    case Op::Sub: return *lval - *rval;
                    case Op::Mul: return *lval * *rval;
                    case Op::Div: return *lval / *rval;
                }
            }
            return R();
        }();
        return EvalResult(static_cast<R>(value));
    }
    return std::nullopt;
}

}  // namespace

//==============================================================//

ProgramBuilder::ProgramBuilder() : program_(new Program()) {}

ProgramBuilder::~ProgramBuilder() {}

void ProgramBuilder::CreateBuiltinSymbols() {
    Program::Scope& scope = *program_->globalScope_;
    Program& program = *program_;

    // Predeclare scalars
    const auto declareScalar = [&](ScalarKind kind) {
        auto* type = program.Allocate<ast::Scalar>(kind);
        scope.InsertSymbol(to_string(kind), type);
        return type;
    };
    declareScalar(ScalarKind::Bool);
    declareScalar(ScalarKind::Float);
    declareScalar(ScalarKind::Int);
    auto scU32 = declareScalar(ScalarKind::U32);
    auto scI32 = declareScalar(ScalarKind::I32);
    auto scF32 = declareScalar(ScalarKind::F32);

    // Predeclare vectors
    const auto declareVec = [&](ast::VecKind kind, Scalar* valueType,
                                std::string_view symbol,
                                std::string_view alias = "") {
        auto* type = program.Allocate<ast::Vec>(kind, valueType);
        scope.InsertSymbol(symbol, type);

        if (!alias.empty()) {
            auto* aliasType =
                program.Allocate<ast::Alias>(SourceLoc(), alias, type);
            scope.InsertSymbol(alias, aliasType);
        }
    };
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
    const auto declareMat = [&](ast::MatrixKind kind, std::string_view symbol,
                                std::string_view alias = "") {
        auto* type = program.Allocate<ast::Matrix>(kind, scF32);
        scope.InsertSymbol(symbol, type);

        if (!alias.empty()) {
            auto* aliasType =
                program.Allocate<ast::Alias>(SourceLoc(), alias, type);
            scope.InsertSymbol(alias, aliasType);
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

void ProgramBuilder::Build(std::string_view code) {
    // Copy source code
    program_->sourceCode_ = std::string(code);
    code = program_->sourceCode_;
    currentScope_ = program_->globalScope_;
    CreateBuiltinSymbols();

    auto parser = Parser(code, this);
    parser_ = &parser;
    parser.Parse();
}

std::unique_ptr<Program> ProgramBuilder::Finalize() {
    // TODO: Do whole program optimizations
    return std::move(program_);
}

void ProgramBuilder::PushGlobalDecl(ast::Variable* decl) {
    program_->globalScope_->PushDecl(decl);
}

void ProgramBuilder::PushGlobalDecl(ast::Struct* decl) {
    program_->globalScope_->PushDecl(decl);
}

void ProgramBuilder::PushGlobalDecl(ast::Function* decl) {
    program_->globalScope_->PushDecl(decl);
}

bool ProgramBuilder::ShouldStopParsing() {
    return stopParsing_;
}

Expected<ConstVariable*> ProgramBuilder::CreateConstVar(
    SourceLoc loc,
    const Ident& ident,
    const std::optional<Ident>& typeSpecifier,
    const Expression* initializer) {
    // 1. The result type is a result type of the initializer
    // 2. The result type is a user specified type
    // 3. Both are present, check if types are convertible
    if (!initializer) {
        return ReportError(loc, ErrorCode::ConstDeclNoInitializer);
    }
    // Result type of the variable [struct | builtin]
    const ast::Scalar* effectiveType = nullptr;
    // Validate user defined type
    if (typeSpecifier) {
        auto res = ResolveTypeName(*typeSpecifier);
        if (!res) {
            return std::unexpected(res.error());
        }
        const auto* scalar = res.value()->As<ast::Scalar>();
        if (!scalar) {
            return ReportError(loc, ErrorCode::TypeError,
                               "type of 'const' should be a scalar type");
        }
        effectiveType = scalar;
    }
    // Validate initializer expression type
    // Types should be the same or builtin and convertible:
    //   const a : i32 = (AbstrFloat) 3.4;
    //   const a : i32 = (i32) 3i;
    //   const a : i32 = (AbstrInt) 3 * 10;
    DASSERT(initializer->type);
    if (!initializer->type->Is<ast::Scalar>()) {
        return ReportError(
            loc, ErrorCode::TypeError,
            "type of 'const' initializer should be a scalar type");
    }
    const auto* initType = initializer->type->As<ast::Scalar>();
    // If explicit type is specified, validate compatibility
    if (effectiveType) {
        // Assert types are convertible
        if (initType != effectiveType &&
            !initType->AutoConvertibleTo(effectiveType)) {
            const SourceLoc exprLoc = initializer->GetLoc();
            // Types are not convertible
            return ReportError(exprLoc, ErrorCode::TypeError,
                               "this expression cannot be assigned to a const "
                               "value of type {}",
                               to_string(effectiveType->kind));
        }
    } else {
        // No explicit type specified
        // Result type equals initializer type
        effectiveType = initType;
    }
    // Check identifier
    EXPECT_TRUE(!currentScope_->FindSymbol(ident.name), loc,
                ErrorCode::SymbolAlreadyDefined,
                "identifier '{}' already defined", ident.name);
    auto* decl = program_->Allocate<ConstVariable>(loc, ident.name,
                                                   effectiveType, initializer);
    currentScope_->InsertSymbol(ident.name, decl);
    LOG_VERBOSE("WGSL: Created ConstVariable node. ident: {}, type: {}",
                ident.name, to_string(effectiveType->kind));
    return decl;
}

Expected<VarVariable*> ProgramBuilder::CreateVar(
    SourceLoc loc,
    const Ident& ident,
    std::optional<AddressSpace> srcAddrSpace,
    std::optional<AccessMode> srcAccMode,
    const std::optional<Ident>& typeSpecifier,
    AttributeList& attributes,
    const Expression* initializer) {
    // The access mode always has a default value, and except for
    //   variables in the storage address space, must not be specified in the
    //   source
    if (srcAccMode) {
        EXPECT_TRUE(srcAddrSpace && *srcAddrSpace == AddressSpace::Storage, loc,
                    ErrorCode::InvalidAddrSpace,
                    "access mode may be specified only when address space "
                    "'storage' is used");
    }
    auto addrSpace = srcAddrSpace.value_or(AddressSpace::Function);
    auto accessMode = srcAccMode.value_or(AccessMode::ReadWrite);

    if (addrSpace == AddressSpace::Storage && !srcAccMode) {
        accessMode = AccessMode::Read;
    }
    // Resolve type
    const Ident typeIdent = *typeSpecifier;
    const ast::Type* valueType = nullptr;

    if (auto res = ResolveTypeName(typeIdent)) {
        valueType = *res;
    }
    // Validate global vars: uniform, storage, texture, sampler
    // This variables have a unique subtype and can be used only in specific
    // contexts
    if (currentScope_->IsGlobal()) {
        EXPECT_TRUE(addrSpace != AddressSpace::Function, loc,
                    ErrorCode::InvalidAddrSpace);
        // Global vars should have an explicit type
        EXPECT_TRUE(
            addrSpace != AddressSpace::Private || typeSpecifier, loc,
            ErrorCode::ExpectedType,
            "this var declaration must have an explicit type specifier");
        // Globar vars may not have initializers
        EXPECT_TRUE(addrSpace != AddressSpace::Private || !initializer, loc,
                    ErrorCode::GlobalVarInitializer);
        // TODO: Implement private address space
        EXPECT_TRUE(addrSpace != AddressSpace::Private, loc,
                    ErrorCode::Unimplemented);
        EXPECT_TRUE(valueType, loc, ErrorCode::InvalidVarBinding,
                    "this var declaration requires an explicit type");

        const bool isTextureSampler =
            valueType->Is<ast::Texture>() || valueType->Is<ast::Sampler>();

        if (isTextureSampler) {
            addrSpace = AddressSpace::Handle;
            accessMode = AccessMode::Read;
        }
        // Value type should be concrete host-shearable
        if (addrSpace == AddressSpace::Storage ||
            addrSpace == AddressSpace::Uniform) {
            EXPECT_TRUE(
                !isTextureSampler, loc, ErrorCode::InvalidVarBinding,
                "'storage' var must not be of type 'texture' or 'sampler'");
        }
        // The address space must be specified for all address spaces except
        // handle and function. Specifying the function address space is
        // optional.
        EXPECT_TRUE(
            isTextureSampler || srcAddrSpace, loc, ErrorCode::InvalidAddrSpace,
            "the address space must be specified for all address spaces "
            "except handle and function");
        auto* var = program_->Allocate<VarVariable>(
            loc, ident.name, addrSpace, accessMode, valueType,
            std::move(attributes), initializer);
        currentScope_->InsertSymbol(ident.name, var);
        return var;
    }
    // Function scope 'normal' variable
    EXPECT_TRUE(addrSpace == AddressSpace::Function, loc,
                ErrorCode::InvalidAddrSpace,
                "this var address space may only be 'function' or empty");


    // TODO: Match initializer type with the specified type
    if (initializer) {
    }
    auto* var = program_->Allocate<VarVariable>(
        loc, ident.name, addrSpace, accessMode, valueType,
        std::move(attributes), initializer);
    currentScope_->InsertSymbol(ident.name, var);
    return var;
}

Expected<Struct*> ProgramBuilder::CreateStruct(SourceLoc loc,
                                               const Ident& ident,
                                               MemberList& members) {
    EXPECT_TRUE(!currentScope_->FindSymbol(ident.name), loc,
                ErrorCode::SymbolAlreadyDefined,
                "identifier '{}' already defined", ident.name);
    EXPECT_TRUE(currentScope_->IsGlobal(), loc, ErrorCode::InvalidScope,
                "a struct declaration may not appear in a function scope");
    // Check member name uniqueness
    std::set<std::string_view> identifiers;
    for (const ast::Member* member : members) {
        const auto [it, ok] = identifiers.emplace(member->name);
        if (!ok) {
            return ReportError(member->GetLoc(), ErrorCode::DuplicateMember);
        }
    }
    ast::Struct* out =
        program_->Allocate<ast::Struct>(loc, ident.name, std::move(members));
    currentScope_->InsertSymbol(ident.name, out);
    return out;
}

Expected<Member*> ProgramBuilder::CreateMember(SourceLoc loc,
                                               const Ident& ident,
                                               const Ident& typeSpecifier,
                                               AttributeList& attributes) {
    // Validate type
    const ast::Type* type = nullptr;
    TRY_UNWRAP(type, ResolveTypeName(typeSpecifier));
    EXPECT_TRUE(!type->Is<ast::Texture>() && !type->Is<ast::Sampler>(), loc,
                ErrorCode::TypeError,
                "struct member type must be scalar, array, mat, vec or "
                "struct of such types");
    // TODO: validate that type has fixed footprint
    return program_->Allocate<ast::Member>(loc, ident.name, type,
                                           std::move(attributes));
}

Expected<Expression*> ProgramBuilder::CreateBinaryExpr(SourceLoc loc,
                                                       OpCode op,
                                                       const Expression* lhs,
                                                       const Expression* rhs) {
    // Base check
    EXPECT_OK(CheckExpressionArg(loc, lhs));
    EXPECT_OK(CheckExpressionArg(loc, rhs));

    if (IsOpArithmetic(op)) {
        return ResolveArithmeticBinaryOp(loc, op, lhs, rhs);
    } else if (IsOpLogical(op)) {
        return ResolveLogicalBinaryOp(loc, op, lhs, rhs);
    }
    return ResolveBitwiseBinaryOp(loc, op, lhs, rhs);
}

// op (ident | literal)
Expected<Expression*> ProgramBuilder::CreateUnaryExpr(SourceLoc loc,
                                                      OpCode op,
                                                      const Expression* rhs) {
    // Basic check that arg is ident or literal
    EXPECT_OK(CheckExpressionArg(loc, rhs));
    if (IsOpArithmetic(op)) {
        return ResolveArithmeticUnaryOp(loc, op, rhs);
    }
    if (IsOpLogical(op)) {
        return ResolveLogicalUnaryOp(loc, op, rhs);
    }
    return ResolveBitwiseUnaryOp(loc, op, rhs);
}

// Resolve symbol in templates and expressions
// - A type name: f32, array, MyStruct
// - A variable: a, my_var
// - A function: my_fn(), calc(1, 3)
Expected<IdentExpression*> ProgramBuilder::CreateIdentExpr(const Ident& ident) {
    // ident (expression, expression*)?
    const bool templated = !ident.templateList.empty();
    const ast::Symbol* symbol = currentScope_->FindSymbol(ident.name);
    // Check symbol
    if (symbol && !templated) {
        if (const auto* type = symbol->As<ast::Type>()) {
            return program_->Allocate<ast::IdentExpression>(ident.loc, type);
        }
        if (const auto* var = symbol->As<ast::Variable>()) {
            return program_->Allocate<ast::IdentExpression>(ident.loc, var);
        }
        const auto* fn = symbol->As<ast::Function>();
        DASSERT(fn);
        return program_->Allocate<ast::IdentExpression>(ident.loc, fn);
    }
    // Could be builtin type
    // vec2, mat2x3, array,
    if (auto res = ResolveTypeName(ident)) {
        return program_->Allocate<ast::IdentExpression>(ident.loc, *res);
    }
    // Could be a builtin function
    // interpolate, log2
    if (auto res = ResolveBuiltinFunc(ident)) {
        return program_->Allocate<ast::IdentExpression>(ident.loc, *res);
    }
    // Not a type, variable or function -> error
    return ReportError(ident.loc, ErrorCode::SymbolNotFound,
                       "identifier '{}' is undefined", ident.name);
}

// ident <template> (args, *)
Expected<Expression*> ProgramBuilder::CreateFnCallExpr(
    const Ident& ident,
    const ExpressionList& args) {

    const ast::Symbol* symbol = currentScope_->FindSymbol(ident.name);
    if (!symbol) {
        return ReportError(ident.loc, ErrorCode::SymbolNotFound,
                           "symbol '{}' not found in the current scope",
                           ident.name);
    }
    // Symbol could be a user function or a builtin type generator
    // I.e. user_fn(a, b) or vec2u(2, 2)
    return std::unexpected(ErrorCode::Unimplemented);
}

Expected<IntLiteralExpression*> ProgramBuilder::CreateIntLiteralExpr(
    SourceLoc loc,
    int64_t value,
    ScalarKind type) {

    const ast::Symbol* typeNode = currentScope_->FindSymbol(to_string(type));
    DASSERT(typeNode && typeNode->Is<ast::Type>());
    // Check for overflow for result type
    if (type == ScalarKind::U32) {
        if (value > std::numeric_limits<uint32_t>::max()) {
            return ReportError(loc, ErrorCode::LiteralInitValueTooLarge);
        }
    } else {
        // AbstractInt || i32
        if (value > std::numeric_limits<int32_t>::max()) {
            return ReportError(loc, ErrorCode::LiteralInitValueTooLarge);
        }
    }
    return program_->Allocate<IntLiteralExpression>(
        loc, typeNode->As<ast::Type>(), value);
}

Expected<FloatLiteralExpression*> ProgramBuilder::CreateFloatLiteralExpr(
    SourceLoc loc,
    double value,
    ScalarKind type) {

    const ast::Symbol* typeNode = currentScope_->FindSymbol(to_string(type));
    DASSERT(typeNode && typeNode->Is<ast::Type>());
    // Check for overflow for result type
    if (type == ScalarKind::F32 || type == ScalarKind::Float) {
        if (value > std::numeric_limits<float>::max()) {
            return ReportError(loc, ErrorCode::LiteralInitValueTooLarge);
        }
    } else {
        return ReportError(loc, ErrorCode::Unimplemented,
                           "half types are not implemented");
    }
    return program_->Allocate<FloatLiteralExpression>(
        loc, typeNode->As<ast::Type>(), value);
}

Expected<BoolLiteralExpression*> ProgramBuilder::CreateBoolLiteralExpr(
    SourceLoc loc,
    bool value) {

    ast::Type* type = currentScope_->FindType(to_string(ScalarKind::Bool));
    DASSERT(type);
    return program_->Allocate<BoolLiteralExpression>(loc, type, value);
}

Expected<ast::Attribute*> ProgramBuilder::CreateWorkGroupAttr(
    SourceLoc loc,
    const ast::Expression* x,
    const ast::Expression* y,
    const ast::Expression* z) {
    EXPECT_TRUE(x && x->Is<ast::IntLiteralExpression>(), loc,
                ErrorCode::InvalidAttribute,
                "@workgroup value must be a const i32 or u32");
    EXPECT_TRUE(!y && y->Is<ast::IntLiteralExpression>(), loc,
                ErrorCode::InvalidAttribute,
                "@workgroup value must be a const i32 or u32");
    EXPECT_TRUE(!z && z->Is<ast::IntLiteralExpression>(), loc,
                ErrorCode::InvalidAttribute,
                "@workgroup value must be a const i32 or u32");
    const auto xval = x->TryGetConstValueAs<int64_t>();
    const auto yval = y ? y->TryGetConstValueAs<int64_t>() : 0;
    const auto zval = z ? z->TryGetConstValueAs<int64_t>() : 0;
    return program_->Allocate<ast::WorkgroupAttribute>(loc, *xval, *yval,
                                                       *zval);
}

Expected<ast::Attribute*> ProgramBuilder::CreateAttribute(
    SourceLoc loc,
    wgsl::AttributeName attr,
    const Expression* expr) {
    // Valueless attributes
    switch (attr) {
        case AttributeName::MustUse:
        case AttributeName::Invariant:
        case AttributeName::Vertex:
        case AttributeName::Compute:
        case AttributeName::Fragment: {
            return program_->Allocate<ast::Attribute>(loc, attr);
        }
    }
    // Expect a single i32 or u32 expr
    // AttributeName::Align:
    // AttributeName::Binding:
    // AttributeName::BlendSrc:
    // AttributeName::Group:
    // AttributeName::ID:
    // AttributeName::Location:
    // AttributeName::Size:
    EXPECT_TRUE(expr && expr->Is<ast::IntLiteralExpression>(), loc,
                ErrorCode::InvalidAttribute,
                "@'{}' value must be a const i32 or u32", to_string(attr));
    const auto value = expr->TryGetConstValueAs<int64_t>();
    DASSERT(value);
    return program_->Allocate<ast::ScalarAttribute>(loc, attr, *value);
}

Expected<ast::Attribute*> ProgramBuilder::CreateBuiltinAttribute(
    SourceLoc loc,
    Builtin value) {
    return program_->Allocate<ast::BuiltinAttribute>(loc, value);
}

//===========================================================================//

Expected<const ast::Type*> ProgramBuilder::ResolveTypeName(
    const Ident& typeSpecifier) {

    auto [loc, name, templateList] = typeSpecifier;
    const ast::Symbol* symbol = currentScope_->FindSymbol(name);
    const ast::Type* type = nullptr;
    // Process templated type
    // Parse top level template name: vec2, array, mat2x3
    if (!templateList.empty()) {
        if (name == "array") {
            if (auto res = ResolveArray(typeSpecifier)) {
                type = res.value();
            } else {
                return std::unexpected(res.error());
            }
        } else if (auto vecKind = VecKindFromString(name)) {
            if (auto res = ResolveVec(typeSpecifier, *vecKind)) {
                type = res.value();
            } else {
                return std::unexpected(res.error());
            }
        } else if (auto matKind = MatrixKindFromString(name)) {
            if (auto res = ResolveMatrix(typeSpecifier)) {
                type = res.value();
            } else {
                return std::unexpected(res.error());
            }
        } else {
            if (!symbol) {
                return ReportError(loc, ErrorCode::TypeNotDefined);
            } else {
                return ReportError(loc, ErrorCode::TypeNotDefined,
                                   "type '{}' may not have template parameters",
                                   name);
            }
        }
    } else {
        EXPECT_TRUE(symbol, loc, ErrorCode::SymbolNotFound,
                    std::format("identifier '{}' is undefined", name));
        // Template list is empty
        // Unwrap aliases
        while (symbol->Is<ast::Alias>()) {
            symbol = symbol->As<ast::Alias>()->parent;
            DASSERT(symbol);
        }
        EXPECT_TRUE(symbol->Is<ast::Type>(), loc, ErrorCode::IdentNotType,
                    std::format("identifier '{}' is not a type name", name));
        type = symbol->As<ast::Type>();
    }
    return type;
}

Expected<const ast::BuiltinFunction*> ProgramBuilder::ResolveBuiltinFunc(
    const Ident& symbol) {
    // TODO: Implement
    return std::unexpected(ErrorCode::Ok);
}

Expected<Expression*> ProgramBuilder::ResolveArithmeticUnaryOp(
    SourceLoc loc,
    OpCode op,
    const Expression* arg) {

    DASSERT(arg);
    DASSERT(arg->type);
    EXPECT_TRUE(arg->type->Is<ast::Scalar>(), loc, ErrorCode::InvalidArg,
                "argument of '{}' should be a scalar", to_string(op));
    const auto* argType = arg->type->As<ast::Scalar>();
    EXPECT_TRUE(argType->IsArithmetic(), loc, ErrorCode::InvalidArg,
                "argument of '{}' should be of arithmetic type", to_string(op));

    // Try evaluate if const
    std::optional<EvalResult> value;
    if (argType->IsFloat()) {
        if (auto val = arg->TryGetConstValueAs<double>()) {
            value = EvalResult(-*val);
        }
    } else if (argType->IsInteger()) {
        if (auto val = arg->TryGetConstValueAs<int64_t>()) {
            value = EvalResult(-*val);
        }
    }
    // Create nodes
    if (value) {
        if (argType->IsInteger()) {
            return program_->Allocate<ast::IntLiteralExpression>(
                loc, argType, std::get<int64_t>(*value));
        }
        if (argType->IsFloat()) {
            return program_->Allocate<ast::FloatLiteralExpression>(
                loc, argType, std::get<double>(*value));
        }
    }
    return program_->Allocate<UnaryExpression>(loc, arg->type, op, arg);
}

Expected<Expression*> ProgramBuilder::ResolveLogicalUnaryOp(
    SourceLoc loc,
    OpCode op,
    const Expression* arg) {

    DASSERT(arg);
    DASSERT(arg->type);
    EXPECT_TRUE(arg->type->Is<ast::Scalar>(), loc, ErrorCode::InvalidArg,
                "argument of '{}' should be a scalar", to_string(op));
    const auto* scalar = arg->type->As<ast::Scalar>();
    // Check type
    if (!scalar->IsBool()) {
        return ReportError(loc, ErrorCode::InvalidArg,
                           "argument of '{}' should be bool", to_string(op));
    }
    // Try evaluate if const
    std::optional<EvalResult> value;
    if (auto val = arg->TryGetConstValueAs<bool>()) {
        value = EvalResult(!*val);
    }
    if (value) {
        return program_->Allocate<ast::BoolLiteralExpression>(
            loc, scalar, std::get<bool>(*value));
    }
    return program_->Allocate<UnaryExpression>(loc, scalar, op, arg);
}

Expected<Expression*> ProgramBuilder::ResolveBitwiseUnaryOp(
    SourceLoc loc,
    OpCode op,
    const Expression* arg) {

    DASSERT(arg);
    DASSERT(arg->type);
    EXPECT_TRUE(arg->type->Is<ast::Scalar>(), loc, ErrorCode::InvalidArg,
                "argument of '{}' should be a scalar", to_string(op));
    const auto* scalar = arg->type->As<ast::Scalar>();
    // Check type
    if (!scalar->IsInteger()) {
        return ReportError(loc, ErrorCode::InvalidArg,
                           "argument of '{}' should be an integet",
                           to_string(op));
    }
    // Try evaluate if const
    std::optional<EvalResult> value;
    if (auto val = arg->TryGetConstValueAs<int64_t>()) {
        int64_t resultVal = 0;
        if (scalar->IsSigned()) {
            resultVal = ~*val;
        } else {
            EXPECT_OK(CheckOverflow(scalar, *val));
            const auto val32 = ~(static_cast<uint32_t>(*val));
            resultVal = static_cast<int64_t>(val32);
        }
        value = EvalResult(resultVal);
    }
    // Create node
    if (value) {
        return program_->Allocate<ast::IntLiteralExpression>(
            loc, scalar, std::get<int64_t>(*value));
    }
    return program_->Allocate<UnaryExpression>(loc, scalar, op, arg);
}

Expected<Expression*> ProgramBuilder::ResolveArithmeticBinaryOp(
    SourceLoc loc,
    OpCode op,
    const Expression* lhs,
    const Expression* rhs) {

    const ast::Scalar* commonType = nullptr;
    TRY_UNWRAP(commonType, ResolveBinaryExprTypes(loc, lhs, rhs));

    if (!commonType->IsArithmetic()) {
        return ReportError(loc, ErrorCode::InvalidArg,
                           "arguments of '{}' should be of arithmetic type",
                           to_string(op));
    }
    std::optional<EvalResult> value;
    // Try evaluate if const
    if (commonType->IsFloat()) {
        value = EvalBinaryOp<double, double>(op, lhs, rhs);
    } else if (commonType->IsInteger()) {
        if (commonType->IsSigned()) {
            if (value = EvalBinaryOp<int64_t, int64_t>(op, lhs, rhs)) {
                EXPECT_OK(CheckOverflow(commonType, std::get<int64_t>(*value)));
            }
        } else {
            if (value = EvalBinaryOp<uint64_t, int64_t>(op, lhs, rhs)) {
                EXPECT_OK(CheckOverflow(commonType, std::get<int64_t>(*value)));
            }
        }
    }
    // Create node
    if (value) {
        if (commonType->IsInteger()) {
            return program_->Allocate<ast::IntLiteralExpression>(
                loc, commonType, std::get<int64_t>(*value));
        }
        if (commonType->IsFloat()) {
            return program_->Allocate<ast::FloatLiteralExpression>(
                loc, commonType, std::get<double>(*value));
        }
    }
    return program_->Allocate<BinaryExpression>(loc, commonType, lhs, op, rhs);
}

Expected<Expression*> ProgramBuilder::ResolveLogicalBinaryOp(
    SourceLoc loc,
    OpCode op,
    const Expression* lhs,
    const Expression* rhs) {

    const ast::Scalar* commonType = nullptr;
    TRY_UNWRAP(commonType, ResolveBinaryExprTypes(loc, lhs, rhs));
    const auto symbol = currentScope_->FindSymbol("bool");
    DASSERT(symbol);
    const ast::Scalar* returnType = symbol->As<ast::Scalar>();

    std::optional<EvalResult> value;
    // Try evaluate if const
    if (commonType->IsFloat()) {
        value = EvalBinaryOp<double, bool>(op, lhs, rhs);
    } else if (commonType->IsInteger()) {
        value = EvalBinaryOp<int64_t, bool>(op, lhs, rhs);
    } else if (commonType->IsBool()) {
        value = EvalBinaryOp<bool, bool>(op, lhs, rhs);
    }
    // Create node
    if (value) {
        return program_->Allocate<ast::IntLiteralExpression>(
            loc, returnType, std::get<bool>(*value));
    }
    return program_->Allocate<BinaryExpression>(loc, returnType, lhs, op, rhs);
}

Expected<Expression*> ProgramBuilder::ResolveBitwiseBinaryOp(
    SourceLoc loc,
    OpCode op,
    const Expression* lhs,
    const Expression* rhs) {

    const ast::Scalar* commonType = nullptr;
    TRY_UNWRAP(commonType, ResolveBinaryExprTypes(loc, lhs, rhs));

    if (!commonType->IsInteger()) {
        return ReportError(loc, ErrorCode::InvalidArg,
                           "arguments of '{}' should be of integer type",
                           to_string(op));
    }
    std::optional<EvalResult> value;
    // Try evaluate if const
    if (commonType->IsInteger()) {
        if (commonType->IsSigned()) {
            if (value = EvalBinaryOp<int64_t, int64_t>(op, lhs, rhs)) {
                EXPECT_OK(CheckOverflow(commonType, std::get<int64_t>(*value)));
            }
        } else {
            if (value = EvalBinaryOp<uint64_t, int64_t>(op, lhs, rhs)) {
                EXPECT_OK(CheckOverflow(commonType, std::get<int64_t>(*value)));
            }
        }
    }
    // Create node
    if (value) {
        if (commonType->IsInteger()) {
            return program_->Allocate<ast::IntLiteralExpression>(
                loc, commonType, std::get<int64_t>(*value));
        }
        if (commonType->IsFloat()) {
            return program_->Allocate<ast::FloatLiteralExpression>(
                loc, commonType, std::get<double>(*value));
        }
    }
    return program_->Allocate<BinaryExpression>(loc, commonType, lhs, op, rhs);
}

Expected<const ast::Array*> ProgramBuilder::ResolveArray(const Ident& ident) {
    auto [loc, name, params] = ident;
    // Basic validation
    EXPECT_TRUE(params.size() == 2, loc, ErrorCode::InvalidTemplateParam,
                "array type must have a value type and a size specified");
    EXPECT_TRUE(params[0]->Is<ast::IdentExpression>(), loc,
                ErrorCode::InvalidTemplateParam, "expected a type specifier");
    EXPECT_TRUE(params[1]->Is<ast::IntLiteralExpression>(), loc,
                ErrorCode::InvalidTemplateParam, "expected a const value");
    const auto* identExpr = params[0]->As<ast::IdentExpression>();
    EXPECT_TRUE(identExpr->symbol->Is<ast::Type>(), loc,
                ErrorCode::InvalidTemplateParam, "expected a type specifier");
    const auto* valueType = identExpr->symbol->As<ast::Type>();
    const auto* literal = params[1]->As<ast::IntLiteralExpression>();
    const int64_t arraySize = literal->value;
    EXPECT_TRUE(arraySize > 0, loc, ErrorCode::InvalidTemplateParam,
                "array size must be positive");
    // An array element type must be one of:
    // - a scalar type
    // - a vector type
    // - a matrix type
    // - an atomic type
    // - an array type having a creation-fixed footprint
    // - a structure type having a creation-fixed footprint.
    if (auto* scalar = valueType->As<ast::Scalar>()) {
        if (literal->value > std::numeric_limits<uint32_t>::max()) {
            return ReportError(params[0]->GetLoc(), ErrorCode::InvalidArg,
                               "size of the static array is too large");
        }
        const std::string_view fullTypeName = program_->EmbedString(
            std::format("array<{},{}>", scalar->name, arraySize));
        // Create a symbol for a type
        // Check if already defined
        const ast::Symbol* symbol = currentScope_->FindSymbol(fullTypeName);
        if (symbol && symbol->Is<ast::Array>()) {
            return symbol->As<ast::Array>();
        }
        ast::Array* type =
            program_->Allocate<ast::Array>(scalar, arraySize, fullTypeName);
        currentScope_->InsertSymbol(fullTypeName, type);
        return type;
    }
    return ReportError(params[0]->GetLoc(), ErrorCode::InvalidArg,
                       "expected a scalar type");
}

Expected<const ast::Vec*> ProgramBuilder::ResolveVec(const Ident& ident,
                                                     VecKind) {
    auto [loc, name, params] = ident;
    EXPECT_TRUE(params.size() == 1 && params[0]->Is<ast::IdentExpression>(),
                loc, ErrorCode::InvalidTemplateParam,
                "expected a type specifier");
    const auto* identExpr = params[0]->As<ast::IdentExpression>();
    const auto* valueType = identExpr->symbol->As<ast::Scalar>();
    EXPECT_TRUE(valueType, loc, ErrorCode::InvalidTemplateParam,
                "vec type param must be of scalar type");
    const std::string fullName =
        std::format("{}<{}>", ident.name, valueType->name);

    // All vec types are predeclared
    const ast::Symbol* symbol = currentScope_->FindSymbol(fullName);
    DASSERT(symbol && symbol->Is<ast::Vec>());
    return symbol->As<ast::Vec>();
}

Expected<const ast::Matrix*> ProgramBuilder::ResolveMatrix(const Ident& ident) {
    auto [loc, name, params] = ident;
    EXPECT_TRUE(params.size() == 1 && params[0]->Is<ast::IdentExpression>(),
                loc, ErrorCode::InvalidTemplateParam,
                "expected a type specifier");
    const auto* identExpr = params[0]->As<ast::IdentExpression>();
    const auto* valueType = identExpr->symbol->As<ast::Scalar>();
    EXPECT_TRUE(valueType, loc, ErrorCode::InvalidTemplateParam,
                "matrix type must be f32 or f16");
    EXPECT_TRUE(valueType->IsFloat(), loc, ErrorCode::InvalidTemplateParam,
                "matrix type must be f32 or f16");
    const std::string fullName =
        std::format("{}<{}>", ident.name, valueType->name);

    // All matrix types are predeclared
    const ast::Symbol* symbol = currentScope_->FindSymbol(fullName);
    DASSERT(symbol && symbol->Is<ast::Vec>());
    return symbol->As<ast::Matrix>();
}


//=========================================================================//

Expected<const ast::Scalar*> ProgramBuilder::ResolveBinaryExprTypes(
    SourceLoc loc,
    const ast::Expression* lhs,
    const ast::Expression* rhs) {
    // Check binary op. I.e. "a + b", "7 + 1", "1.0f + 1"
    DASSERT(lhs->type && rhs->type);
    EXPECT_TRUE(lhs->type->Is<ast::Scalar>(), loc, ErrorCode::InvalidArg);
    EXPECT_TRUE(rhs->type->Is<ast::Scalar>(), loc, ErrorCode::InvalidArg);
    const auto* lhsType = lhs->type->As<ast::Scalar>();
    const auto* rhsType = rhs->type->As<ast::Scalar>();

    if (lhsType == rhsType) {
        return lhsType->As<ast::Scalar>();
    }
    // Concrete
    constexpr uint32_t kMax = std::numeric_limits<uint32_t>::max();
    const auto lr = lhsType->GetConversionRankTo(rhsType);
    const auto rl = rhsType->GetConversionRankTo(lhsType);
    // Not convertible to each other
    if (lr == kMax && rl == kMax) {
        return ReportError(loc, ErrorCode::InvalidArg);
    }
    // Find common type
    auto* resultType = lhsType;
    if (rl > lr) {
        resultType = rhsType;
    }
    return resultType;
}

ErrorCode ProgramBuilder::CheckExpressionArg(SourceLoc loc,
                                             const ast::Expression* arg) {
    bool isLiteral = arg->Is<ast::LiteralExpression>();
    bool isIdent = arg->Is<ast::IdentExpression>();
    if (isLiteral) {
        return ErrorCode::Ok;
    }
    if (isIdent) {
        const auto* ident = arg->As<ast::IdentExpression>();
        if (ident->symbol->Is<ast::Variable>()) {
            return ErrorCode::Ok;
        }
        if (ident->symbol->Is<ast::Type>()) {
            return ReportError(loc, ErrorCode::SymbolNotVariable,
                               "type names are not allowed")
                .error();
        }
    }
    FATAL("Expression arg could be Type | Variable | Literal");
    return ErrorCode::Ok;
}

void ProgramBuilder::FormatAndAddMsg(SourceLoc loc,
                                     ErrorCode code,
                                     const std::string& msg) {
    // Break on 'unimplemented' messages
    DASSERT(code != ErrorCode::Unimplemented);
    // Whether the loc points to an empty line
    // So instead print the previous non empty line
    auto [sourceLine, isLocOnEmpty] = [&] {
        std::string_view line = parser_->GetLine(loc.line);
        if (!line.empty()) {
            return std::make_pair(line, false);
        }
        uint32_t prevLine = loc.line - 1;
        // Source text is empty
        // Lines are 1 based
        if (prevLine == 0) {
            return std::make_pair(line, false);
        }
        for (; prevLine > 0 && line.empty(); --prevLine) {
            line = parser_->GetLine(prevLine);
        }
        return std::make_pair(line, true);
    }();
    // Trim line leading whitespaces
    uint32_t leadSpaceLen = 0;
    if (!isLocOnEmpty) {
        while (!sourceLine.empty() && ASCII::IsSpace(sourceLine.front())) {
            sourceLine = sourceLine.substr(1);
            ++leadSpaceLen;
        }
    }
    // Generate the "^^^" pointing to the error pos. I.e.
    // var my_var = ;
    //             ^ expected an expression
    std::string msgLine;
    for (uint32_t i = 0; i < loc.col - 1 - leadSpaceLen; ++i) {
        msgLine.push_back(' ');
    }
    // ^^^
    for (uint32_t i = 0; i < loc.len; ++i) {
        msgLine.push_back('^');
    }
    msgLine.push_back(' ');
    msgLine.append(msg);
    // Combine
    const auto lineNum = std::format("{}", loc.line);
    std::string result;
    // clang-format off
    result.append("\n");
    result.append(" ").append(lineNum.size(), ' ').append(" | \n");
    result.append(" ").append(lineNum            ).append(" | ").append(sourceLine).append("\n");
    result.append(" ").append(lineNum.size(), ' ').append(" | ").append(msgLine).append("\n");
    result.append(" ").append(lineNum.size(), ' ').append(" | \n");
    // clang-format on
    program_->diags_.push_back(
        {Program::DiagMsg::Type::Error, loc, code, result});
    stopParsing_ = true;
}

}  // namespace wgsl
