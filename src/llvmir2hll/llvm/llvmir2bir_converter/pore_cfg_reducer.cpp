#include "retdec/llvmir2hll/llvm/llvmir2bir_converter/pore_cfg_reducer.h"
#include "retdec/llvmir2hll/ir/expression.h"
#include "retdec/llvmir2hll/ir/statement.h"
#include "retdec/llvmir2hll/llvm/llvmir2bir_converter/base_cfg_reducer.h"
#include "retdec/llvmir2hll/llvm/llvmir2bir_converter/cfg_node.h"
#include "retdec/llvmir2hll/support/smart_ptr.h"
#include "retdec/utils/io/log.h"


namespace retdec {
namespace llvmir2hll {

bool PoRECFGReducer::inspectCFGNode(ShPtr<CFGNode> node) {
    PRECONDITION_NON_NULL(node);

    // utils::io::Log::phase("TESTTEST");
    if (tryReduceLoop(node)) {
        return true;
    } else if (tryReduceSequence(node)) {
        return true;
    } else if (tryReduceSwitch(node)) {
        return true;
    } else if (tryReduceIf(node)) {
        return true;
    }

    return false;
}

void PoRECFGReducer::inspectCFGNodeComplete(ShPtr<CFGNode> node) {
    PRECONDITION_NON_NULL(node);

    CFGNodeVector succs;
    if (isSwitch(node)) {
        // TODO: reduce switch
    } else if (node->getSuccNum() == 2) {
        auto cond = getNodeCondExpr(node);
        auto ifTrue = getGotoBody(node, node->getSucc(0));
        auto ifFalse = getGotoBody(node, node->getSucc(1));
        auto body = getIfStmt(cond, ifTrue, ifFalse);
        reduceNode(node, body, succs);
        LOG << "Completely reduced if: " << node->getDebugStr() << "\n";
    } else if (node->getSuccNum() == 1) {
        auto body = getGotoBody(node, node->getSucc(0));
        succs = node->getSuccessors();
        reduceNode(node, body, succs);
        LOG << "Completely reduced goto: " << node->getDebugStr() << "\n";
    } else if (node->getSuccNum() > 0) {
        // Completely reduce the node.
        reduceNode(node, node->getBody(), succs, false);
        LOG << "Completely ignore node: " << node->getDebugStr() << "\n";
    }
}

bool PoRECFGReducer::tryReduceSequence(ShPtr<CFGNode> node) {
    if (node->getSuccNum() != 1) {
        return false;
    }

    auto succ = node->getSucc(0);
    if (succ->getPredsNum() != 1 || succ == node) {
        return false;
    }

    LOG << "Try reduce sequence: " << node->getDebugStr() << "\n";

    ShPtr<Statement> succBody = succ->getBody();
    CFGNodeVector succSuccs = succ->getSuccessors();
    reduceNode(node, succBody, succSuccs);

    LOG << "Reduced sequence: " << node->getDebugStr() << "\n";
    return true;
}

bool PoRECFGReducer::tryReduceIf(ShPtr<CFGNode> node) {
    if (node->getSuccNum() != 2) {
        return false;
    }

    ShPtr<CFGNode> ifSucc;
    ShPtr<CFGNode> ifNode, elseNode;

    ShPtr<Statement> ifBody, elseBody;
    ShPtr<Expression> cond = getNodeCondExpr(node);
    ShPtr<Statement> ifStmt = nullptr;

    // Is if-else statement
    // node
    //  ├── ifNode
    //  |     └── ifSucc
    //  └── elseNode
    //        └── ifSucc
    ifNode = node->getSucc(0);
    elseNode = node->getSucc(1);

    if (ifNode->getPredsNum() == 1 && ifNode->getSuccNum() == 1
        && elseNode->getPredsNum() == 1 && elseNode->getSuccNum() == 1
        && ifNode->getSucc(0) == elseNode->getSucc(0)) {

        LOG << "Try reduce if-else: " << node->getDebugStr() << "\n";
        ifSucc = ifNode->getSucc(0);
        ifBody = getIfClauseBody(node, ifNode, ifSucc);
        elseBody = getIfClauseBody(node, elseNode, ifSucc);
        ifStmt = getIfStmt(cond, ifBody, elseBody);
    } 
    
    if (!ifStmt) {

        // is if statement
        // node
        //  ├── ifNode
        //  |     └── ifSucc
        //  └── ifSucc
        for (int index = 0; index < 2; index++) {
            ifNode = node->getSucc(index);
            ifSucc = node->getSucc(1 - index);

            if (ifNode->getPredsNum() == 1 && ifNode->getSuccNum() == 1
                && ifNode->getSucc(0) == ifSucc) {
                LOG << "Try reduce if: " << node->getDebugStr() << "\n";

                ifBody = getIfClauseBody(node, ifNode, ifSucc);
                elseBody = getIfClauseBody(node, nullptr, ifSucc);
                // cond(succ1) is !cond(succ0)
                if (index == 1) {
                    cond = getLogicalNot(cond);
                }
                ifStmt = getIfStmt(cond, ifBody, elseBody);
                break;
            }
        }
    }

    if (ifStmt) {
        CFGNodeVector succs {ifSucc};
        reduceNode(node, ifStmt, succs);
        LOG << "Reduced if: " << node->getDebugStr() << "\n";
        return true;
    }

    return false;
}

bool PoRECFGReducer::tryReduceLoop(ShPtr<CFGNode> node) {

    // is while true statement
    // node
    //  └── node
    if (isLoopHeader(node) && !isLoopHeaderUnderAnalysis(node)) {
        LOG << "Try reduce loop: " << node->getDebugStr() << "\n";
		enterLoop(node);
		
        while (!isLoopReduced(node) && reduceCFG(node)) {}

        if (!isLoopReduced(node)) {
            reduceCFGComplete(node);
            tryReduceLoop(node);
        }

		leaveLoop();
		return true;
	}
    if (node->getSuccNum() == 1 && node->getSucc(0) == node) {
        CFGNodeVector succs;
        auto body = getWhileBody(node);
        auto cond = getBoolConst(true);
        auto whileStmt = getWhileLoopStmt(node, cond, body);
        reduceNode(node, whileStmt, succs, false);

        LOG << "Reduced loop: " << node->getDebugStr() << "\n";
        return true;
    }
    return false;
}

bool PoRECFGReducer::tryReduceSwitch(ShPtr<CFGNode> node) {
    return false;
}

}
}
