#pragma once
#include "ast_type.h"
#include "common.h"

namespace wgsl::ast {

class Alias final : public Type {
public:
    static constexpr auto kStaticType = NodeType::Alias;
    // - Another alias
    // - Struct
    // - Builtin
    const ast::Type* parent;

public:
    Alias(SourceLoc loc, std::string_view name, const Type* parent)
        : Type(loc, kStaticType, name), parent(parent) {}
};

} // namespace wgsl::ast