#pragma once
#include "base/common.h"
#include "base/bump_alloc.h"

#include "ast_expression.h"
#include "ast_variable.h"
#include "ast_attribute.h"

namespace wgsl {

class Program {
public:
    static std::unique_ptr<Program> Create(std::string_view code);

public:
    friend class ProgramBuilder;

    BumpAllocator alloc_;
    std::vector<ast::Variable*> globalDecls_;
};

} // namespace wgsl