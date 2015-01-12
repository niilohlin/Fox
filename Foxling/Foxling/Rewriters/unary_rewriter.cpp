#include "unary_rewriter.h"
#include "clang/AST/ASTContext.h"
#include "llvm/support/Casting.h"
#include "helpers.h"

using namespace llvm;
using namespace clang;

bool Foxling::UnaryRewriter::shouldEmitResult(const UnaryOperator *op) const {
    auto dynNode = ast_type_traits::DynTypedNode::create(*op);
    ArrayRef<ast_type_traits::DynTypedNode> parentNodes = Context.getParents(dynNode);
    for (auto it = parentNodes.begin(); it != parentNodes.end(); it++) {
        if (it->get<Expr>() || it->get<VarDecl>() || it->get<ReturnStmt>()) {
            return true;
        }
    }
    return false;
}

void Foxling::UnaryRewriter::run(const MatchFinder::MatchResult &Result) {
    if (const UnaryOperator *op = Result.Nodes.getNodeAs<UnaryOperator>(BindKey)) {
        // only ++ and -- operators are rewritten
        if (op->getOpcode() != UO_PreInc &&
            op->getOpcode() != UO_PreDec &&
            op->getOpcode() != UO_PostInc &&
            op->getOpcode() != UO_PostDec) {
            return;
        }
        auto subexpr = Rewrite.getRewrittenText(op->getSubExpr()->getSourceRange());
        auto operatorStr = op->getOpcodeStr(op->getOpcode());

        char snapshotExpr[50]; // TODO: allocate memory
        if (op->isPostfix()) {
            sprintf(snapshotExpr, "__snapshot__%lu%s", (uintptr_t)op, operatorStr.str().c_str());
        } else {
            sprintf(snapshotExpr, "%s__snapshot__%lu", operatorStr.str().c_str(), (uintptr_t)op);
        }

        char *str;
        allocate_sprintf(&str, "({ __typeof(%s) __snapshot__%lu = ",
                         subexpr.c_str(),
                         (uintptr_t)op);
        Rewrite.InsertText(op->getLocStart(), str);
        free(str);

        if (shouldEmitResult(op)) {
            allocate_sprintf(&str,
                             "; %s __typeof(%s) __result__%lu = %s; %s = __snapshot__%lu; __result__%lu; })",
                             InjectCode.c_str(),
                             subexpr.c_str(),
                             (uintptr_t)op,
                             snapshotExpr,
                             subexpr.c_str(),
                             (uintptr_t)op,
                             (uintptr_t)op);
        } else {
            allocate_sprintf(&str, "; %s %s; %s = __snapshot__%lu; })",
                             InjectCode.c_str(),
                             snapshotExpr,
                             subexpr.c_str(),
                             (uintptr_t)op);
        }
        Rewrite.InsertTextAfterToken(op->getLocEnd(), str);
        free(str);
        Rewrite.RemoveText(op->getOperatorLoc(), operatorStr.size());
    }
}