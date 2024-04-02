#ifndef PORE_BASE_CFG_REDUCER_H
#define PORE_BASE_CFG_REDUCER_H

#include <functional>
#include <iostream>
#include <queue>
#include <stack>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <llvm/Analysis/LoopInfo.h>

#include "retdec/llvmir2hll/ir/and_op_expr.h"
#include "retdec/llvmir2hll/ir/assign_stmt.h"
#include "retdec/llvmir2hll/ir/break_stmt.h"
#include "retdec/llvmir2hll/ir/const_bool.h"
#include "retdec/llvmir2hll/ir/const_int.h"
#include "retdec/llvmir2hll/ir/continue_stmt.h"
#include "retdec/llvmir2hll/ir/empty_stmt.h"
#include "retdec/llvmir2hll/ir/expression.h"
#include "retdec/llvmir2hll/ir/for_loop_stmt.h"
#include "retdec/llvmir2hll/ir/goto_stmt.h"
#include "retdec/llvmir2hll/ir/if_stmt.h"
#include "retdec/llvmir2hll/ir/lt_op_expr.h"
#include "retdec/llvmir2hll/ir/not_op_expr.h"
#include "retdec/llvmir2hll/ir/or_op_expr.h"
#include "retdec/llvmir2hll/ir/statement.h"
#include "retdec/llvmir2hll/ir/switch_stmt.h"
#include "retdec/llvmir2hll/ir/ufor_loop_stmt.h"
#include "retdec/llvmir2hll/ir/variable.h"
#include "retdec/llvmir2hll/ir/while_loop_stmt.h"
#include "retdec/llvmir2hll/support/debug.h"
#include "retdec/llvmir2hll/support/smart_ptr.h"
#include "retdec/llvmir2hll/support/types.h"
#include "retdec/llvmir2hll/llvm/llvmir2bir_converter/cfg_node.h"
#include "retdec/llvmir2hll/llvm/llvmir2bir_converter/basic_block_converter.h"
#include "retdec/llvmir2hll/llvm/llvmir2bir_converter/llvm_value_converter.h"
#include "retdec/llvmir2hll/llvm/llvmir2bir_converter/structure_converter.h"
#include "retdec/utils/io/log.h"
#include "retdec/utils/io/logger.h"

#ifdef LOG
#undef LOG
#endif
#define LOG \
	retdec::utils::io::Logger(std::cerr)

namespace llvm {

class Function;
class Loop;
class LoopInfo;
class Pass;
class ScalarEvolution;

} // namespace llvm

namespace retdec {
namespace llvmir2hll {

class LLVMValueConverter;
class StructureConverter;

class BaseCFGReducer {

    friend class StructureConverter;

protected:
    using SuccList = std::vector<ShPtr<CFGNode>>;
	using CFGNodeVector = std::vector<ShPtr<CFGNode>>;

    using CondToNodeList = std::vector<std::pair<ShPtr<Expression>, ShPtr<CFGNode>>>;

    using SwitchClause = std::pair<ExprVector, ShPtr<CFGNode>>;
    using SwitchClauseVector = std::vector<ShPtr<SwitchClause>>;
    using SwitchClauseBody = std::pair<ExprVector, ShPtr<Statement>>;
    using SwitchClauseBodyVector = std::vector<ShPtr<SwitchClauseBody>>;

private:
    using CFGNodeStack = std::stack<ShPtr<CFGNode>>;
    using MapCFGNodeToSwitchClause = std::unordered_map<ShPtr<CFGNode>, ShPtr<SwitchClause>>;

private:
    StructureConverter *sc;

public:
	BaseCFGReducer(StructureConverter *sc): sc(sc) {};

    bool reduceCFG(ShPtr<CFGNode> cfg);
    void reduceCFGComplete(ShPtr<CFGNode> cfg);

protected:

    virtual bool inspectCFGNode(ShPtr<CFGNode> node);
    virtual void inspectCFGNodeComplete(ShPtr<CFGNode> node);

    /// @name Reduction of nodes
	/// @{
    void reduceNode(ShPtr<CFGNode> node, ShPtr<Statement> body, CFGNodeVector &succs, bool append=true);
    /// @}

    /// @name Create statements
    /// @{
    ShPtr<GotoStmt> getGotoStmt(const ShPtr<CFGNode> &node, const ShPtr<CFGNode> &target);
    ShPtr<BreakStmt> getBreakStmt(const ShPtr<CFGNode> &node, const ShPtr<CFGNode> &target);
    ShPtr<ContinueStmt> getContinueStmt(const ShPtr<CFGNode> &node, const ShPtr<CFGNode> &target);
    ShPtr<IfStmt> getIfStmt(const ShPtr<Expression> &cond, const ShPtr<Statement> &trueBody, const ShPtr<Statement> &falseBody);
    ShPtr<SwitchStmt> getSwitchStmt(const ShPtr<CFGNode> switchNode, const ShPtr<Expression> &cond, const SwitchClauseBodyVector &clauses);
    ShPtr<WhileLoopStmt> getWhileLoopStmt(const ShPtr<CFGNode> loopNode, const ShPtr<Expression> &cond, const ShPtr<Statement> &body);
    /// @}

    bool isSwitch(const ShPtr<CFGNode> &node) const;
    SwitchClauseVector getSwitchClauses(const ShPtr<CFGNode> &switchNode);

    /// @name Create statement bodies
    /// @{
    ShPtr<Statement> getSuccessorBody(const ShPtr<CFGNode> &node, const ShPtr<CFGNode> &succ);
    ShPtr<Statement> getIfClauseBody(const ShPtr<CFGNode> &node,
		const ShPtr<CFGNode> &clause, const ShPtr<CFGNode> &ifSuccessor);
    ShPtr<Statement> getWhileBody(const ShPtr<CFGNode> &node);
    ShPtr<Statement> getGotoBody(const ShPtr<CFGNode> &node, const ShPtr<CFGNode> &target);

    /// @name Create expressions
    /// @{
    ShPtr<ConstBool> getBoolConst(bool value) const;
    ShPtr<NotOpExpr> getLogicalNot(const ShPtr<Expression> &expr) const;
    ShPtr<AndOpExpr> getLogicalAnd(const ShPtr<Expression> &lhs, const ShPtr<Expression> &rhs) const;
    ShPtr<Expression> getLogicalOr(const ShPtr<Expression> &lhs, const ShPtr<Expression> &rhs) const;
    ShPtr<Expression> getNodeCondExpr(const ShPtr<CFGNode> &node) const;
    ShPtr<Expression> getValueExpr(llvm::Value *value) const;
    /// @}

    /// @name Helper of loop reduction
    // llvm::Loop *getLoopFor(const ShPtr<CFGNode> &node) const;
    bool isLoopHeader(const ShPtr<CFGNode> &node) const;
    ShPtr<CFGNode> getLoopHeader(const ShPtr<CFGNode> &node) const;
    bool isLoopHeaderUnderAnalysis(const ShPtr<CFGNode> &node) const;
    ShPtr<CFGNode> getLoopHeaderUnderAnalysis() const;
    ShPtr<CFGNode> getLoopSuccessor(ShPtr<CFGNode> loopNode);
    CFGNodeVector getLoopSuccsUnderAnalysis();
    bool isLoopReduced(const ShPtr<CFGNode> &node) const;
    void enterLoop(ShPtr<CFGNode> node);
    void leaveLoop();
    /// @}

private:
    // A stack representing hierarchy of the parent loop headers during structuring.
	CFGNodeStack loopHeaderStack;

    // A stack representing hierarchy of the parent loop headers during structuring.
	CFGNodeVector loopSuccsUnderAnalysis;

	// A set of loop headers that are already on the stack.
	CFGNode::CFGNodeSet loopHeadersOnStack;
};


}
}

#endif