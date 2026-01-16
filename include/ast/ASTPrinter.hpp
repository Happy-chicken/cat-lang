// #ifndef ASTPRINTER_HPP_
// #define ASTPRINTER_HPP_

// #include "Expr.hpp"
// #include "Stmt.hpp"
// #include <memory>
// #include
// #include <utility>
// #include <vector>

// using std::shared_ptr;
// using std::string;

// class AstPrinter : public Visitor,
//                    public std::enable_shared_from_this<AstPrinter> {
// public:
//     string print(shared_ptr<Expr> expr) {
//         return expr.accept(shared_from_this());
//     }
//     string visitLiteralExpr(const Literal &expr) {
//         return expr.value == "" ? "nil" : expr.value;
//     }
//     string visitAssignExpr(const Assign &expr) {
//         return "(" +
//                expr.name.lexeme +
//                " " +
//                expr.value->accept(shared_from_this()) +
//                " )";
//     }
//     string visitBinaryExpr(const Binary &expr) {
//         return "(" +
//                expr.left->accept(shared_from_this()) +
//                " " +
//                expr.operation.lexeme +
//                " " +
//                expr.right->accept(shared_from_this()) +
//                " )";
//     }
//     string visitGroupingExpr(const Grouping &expr) {
//         return "(" + expr.expression->accept(shared_from_this()) + ")";
//     }
//     string visitUnaryExpr(const Unary &expr) {
//         return "(" + expr.operation.lexeme + " " + expr.right->accept(shared_from_this()) + ")";
//     }
//     string visitVariableExpr(const Variable &expr) {
//         return expr.name.lexeme;
//     }
//     string visitLogicalExpr(const Logical &expr) {
//         return "(" +
//                expr.left->accept(shared_from_this()) +
//                " " +
//                expr.operation.lexeme +
//                " " +
//                expr.right->accept(shared_from_this()) +
//                " )";
//     }
//     string visitCallExpr(const Call &expr) {
//         string str = "(" + expr.callee->accept(shared_from_this());
//         for (const auto &arg: expr.arguments) {
//             str += " " + arg->accept(shared_from_this());
//         }
//         return str + ")";
//     }
// };

// #endif// ASTPRINTER_HPP_
