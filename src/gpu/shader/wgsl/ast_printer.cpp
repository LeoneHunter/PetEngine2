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
    if(auto expr = node->As<BinaryExpression>()) {
        TreePrinter::AutoObject a(printer_, "binary_expression");
        printer_->Entry("op", to_string(expr->op));

        printer_->OpenObject("lhs");
        PrintExpression(expr->lhs);
        printer_->CloseObject();

        printer_->OpenObject("rhs");
        PrintExpression(expr->rhs);
        printer_->CloseObject();
        return;
    }
    if(auto expr = node->As<UnaryExpression>()) {
        TreePrinter::AutoObject u(printer_, "unary_expression");
        printer_->Entry("op", to_string(expr->op));

        printer_->OpenObject("rhs");
        PrintExpression(expr->rhs);
        printer_->CloseObject();
        return;
    }
    if(auto literal = node->As<LiteralExpression>()) {
        if(auto iLiteral = node->As<IntLiteralExpression>()) {
            TreePrinter::AutoObject i(printer_, "int_literal");
            printer_->Entry("type", to_string(iLiteral->type->kind));
            printer_->Entry("value", iLiteral->value);
        } else if(auto fLiteral = node->As<FloatLiteralExpression>()) {
            TreePrinter::AutoObject f(printer_, "float_literal");
            printer_->Entry("type", to_string(fLiteral->type->kind));
            printer_->Entry("value", fLiteral->value);
        } else if(auto bLiteral = node->As<BoolLiteralExpression>()) {
            TreePrinter::AutoObject b(printer_, "bool_literal");
            printer_->Entry("value", bLiteral->value ? "true" : "false");
        }
    }
}

} // namespace wgsl::ast
