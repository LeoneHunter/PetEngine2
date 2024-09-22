#pragma once
#include "ast_node.h"
#include "base/tree_printer.h"

#include "ast_attribute.h"
#include "ast_expression.h"
#include "ast_variable.h"

namespace wgsl::ast {

// Serializes an ast tree into a TreePrinter
class AstPrinter {
public:
    void Print(TreePrinter* printer, const Node* root);

private:
    void PrintConstVar(const ConstVariable* node);
    void PrintExpression(const Expression* node);

private:
    TreePrinter* printer_ = nullptr;
};

} // namespace wgsl::ast