#include "ast_printer.h"

namespace wgsl::ast {

void AstPrinter::Print(TreePrinter* printer, const Node* root) {
    printer_ = printer; 
    if(auto n = root->As<ConstVariable>()) {
        PrintConstVar(n);
    } else if(auto e = root->As<Expression>()) {
        PrintExpression(e);
    }
}

void AstPrinter::PrintConstVar(const ConstVariable* node) {
    TreePrinter::AutoObject o(printer_, "const_value");
    printer_->Entry("ident", node->ident);
    TreePrinter::AutoObject i(printer_, "initializer");
    // TODO: Print type
    if(node->initializer) {
        PrintExpression(node->initializer);
    }
}

void AstPrinter::PrintExpression(const Expression* node) {
}

} // namespace wgsl::ast
