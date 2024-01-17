#include <algorithm>
#include <cstddef>
#include <functional>


#include "retdec/llvmir2hll/ir/break_stmt.h"
#include "retdec/llvmir2hll/ir/continue_stmt.h"
#include "retdec/llvmir2hll/ir/empty_stmt.h"
#include "retdec/llvmir2hll/ir/expression.h"
#include "retdec/llvmir2hll/ir/for_loop_stmt.h"
#include "retdec/llvmir2hll/ir/statement.h"
#include "retdec/llvmir2hll/ir/while_loop_stmt.h"
#include "retdec/llvmir2hll/llvm/llvm_support.h"
#include "retdec/llvmir2hll/llvm/llvmir2bir_converter/cfg_node.h"
#include "retdec/llvmir2hll/llvm/llvmir2bir_converter/structure_converter.h"
#include "retdec/llvmir2hll/support/expression_negater.h"
#include "retdec/llvmir2hll/support/smart_ptr.h"
#include "retdec/llvmir2hll/support/debug.h"
#include "retdec/llvmir2hll/utils/ir.h"
#include "retdec/utils/container.h"
#include "retdec/llvmir2hll/llvm/llvmir2bir_converter/base_cfg_reducer.h"


namespace retdec {
namespace llvmir2hll {

using retdec::utils::hasItem;
using retdec::utils::removeItem;

/**
* @brief 遍历给定的控制流图 @a cfg 并尝试将某些节点化简为结构化语句.
*
* @returns 如果有节点被化简则返回 @c true 否则返回 @c false.
*
* @par 前置条件
*  - @a cfg 不为 @c nullptr .
*/
bool BaseCFGReducer::reduceCFG(ShPtr<CFGNode> cfg) {
	PRECONDITION_NON_NULL(cfg);

	return sc->BFSTraverse(cfg, [this](const auto &node) {
		return this->inspectCFGNode(node);
	});
}

/**
* @brief 遍历给定的控制流图节点 @a cfg 并将化简所有节点直至控制流图中只包含一个节点。
*        当节点 @a cfg 在循环中时, 仅化简该循环, 而不化简循环外的节点。
*
* @returns 如果有节点被化简则返回 @c true 否则返回 @c false.
*
* @par 前置条件
*  - @a cfg 不为 @c nullptr .
*/
void BaseCFGReducer::reduceCFGComplete(ShPtr<CFGNode> cfg) {
	PRECONDITION_NON_NULL(cfg);

	CFGNodeVector flattenedCFG;
	auto func = [&flattenedCFG](const auto &node) {
		flattenedCFG.push_back(node);
		return true;
	};

	auto loop = sc->getLoopFor(cfg);
	if (loop) {
		sc->BFSTraverseLoop(cfg, func);
	} else {
		sc->BFSTraverse(cfg, func);
	}

	for (auto node : flattenedCFG) {
		inspectCFGNodeComplete(node);
	}
}

/**
* @brief 检查给定的控制流图节点 @a node 是否可以被化简为结构化语句, 如果可以，则将其与相应后继节点
*        化简为结构化语句.
*
* @returns 如果有节点被化简则返回 @c true 否则返回 @c false.
*
* @par 前置条件
*  - @a node 不为 @c nullptr .
*/
bool BaseCFGReducer::inspectCFGNode(ShPtr<CFGNode> node) {
	PRECONDITION_NON_NULL(node);
	return false;
}

/**
* @brief 化简给定的控制流图节点 @a node , 合并该节点的所有后继节点.
*        例如, 将{a}->{b}->{c}化简为{a;goto b;b}->c.
*
* @par 前置条件
*  - @a node 不为 @c nullptr .
*/
void BaseCFGReducer::inspectCFGNodeComplete(ShPtr<CFGNode> node) {
	PRECONDITION_NON_NULL(node);
	while (node->getSuccNum() > 0) {
		node->removeSucc(0);
	}
}

/**
* @brief 判断给定节点 @a node 是否是循环的头部节点.
*        循环的头部节点即回边指向的节点.
*
* @par 前置条件
*  - @a node 不为 @c nullptr .
*/
bool BaseCFGReducer::isLoopHeader(const ShPtr<CFGNode> &node) const {
	return sc->isLoopHeader(node);
}

/**
* @brief 获取节点 @a node 所在循环的 @ref isLoopHeader "头部节点" .
*
* @par 前置条件
*  - @a node 不为 @c nullptr .
*/
ShPtr<CFGNode> BaseCFGReducer::getLoopHeader(const ShPtr<CFGNode> &node) const {
	PRECONDITION_NON_NULL(node);
	llvm::Loop *loop = sc->getLoopFor(node);
	if (!loop) {
		return nullptr;
	}
	return sc->loopHeaders.at(loop);
}

/**
 * @brief 化简给定的节点 @a node 及其后继节点.
 * 
 * @param node 待化简的节点
 * @param body 要添加或替换到 @a node 的语句
 * @param succs @a node 化简得到节点的所有后继节点, 不在该列表中的后继节点将被移除
 * @param append 如果设置为 @c true , 则将 @a body 中的语句添加到 @a node 原有语句之后, 否则直接替换所有语句
 *
 * @par 前置条件
 *  - @a node 不为 @c nullptr .
 *  - 当 @a append 为 @c false 时 @a body 不为 @c nullptr .
 */
void BaseCFGReducer::reduceNode(ShPtr<CFGNode> node, ShPtr<Statement> body, CFGNodeVector &succs, bool append) {
	PRECONDITION_NON_NULL(node);

	if (append) {
		if (body != nullptr) {
			node->appendToBody(body);
		}
	} else {
		PRECONDITION_NON_NULL(body);
		node->setBody(body);
	}

	CFGNodeVector succsToAdd(succs);

	size_t succ_idx = 0;
	while (node->getSuccNum() > succ_idx) {
		auto succ = node->getSucc(succ_idx);
		if (hasItem(succsToAdd, succ)) {
			// 保留succs中的节点
			removeItem(succsToAdd, succ);
			succ_idx += 1;
		}  else if (succ == node) {
			// 仅移除自环, 而不从CFG中移除
			node->removeSucc(succ_idx);
		} else if (succ->getPredsNum() <= 1) {
			// 当后继节点仅有一个父节点node, 将其从CFG中移除
			node->deleteSucc(succ_idx);
		} else {
			// 当后继节点有多个父节点, 仅移除一条边
			node->removeSucc(succ_idx);
		}
	}

	for (auto succ: succsToAdd) {
		node->addSuccessor(succ);
	}

	if (isLoopHeader(node)) {
		// 当node是循环头部节点时, 标记该循环已被化简
		sc->reducedLoops.insert(sc->getLoopFor(node));
	}
}

/**
* @brief 生成一条新的 @c goto 语句, 该语句从节点 @a node 跳转到节点 @a target .
* 
* @par 前置条件
*  - @a node 和 @a target 都不为 @c nullptr .
*/
ShPtr<GotoStmt> BaseCFGReducer::getGotoStmt(const ShPtr<CFGNode> &node, const ShPtr<CFGNode> &target) {
	PRECONDITION_NON_NULL(node);
	PRECONDITION_NON_NULL(target);

	auto gotoStmt = GotoStmt::create(target->getBody(), node->getBody()->getAddress());
	gotoStmt->setMetadata("goto -> " + sc->getLabel(target));

	sc->targetReferences[target].push_back(gotoStmt);
	sc->gotoTargetsToCfgNodes.emplace(target->getBody(), target);
	return gotoStmt;
}

/**
* @brief 生成一条新的 @c break 语句, 该语句从节点 @a node 跳转到节点 @a target .
*
* @par 前置条件
*  - @a node 和 @a target 都不为 @c nullptr .
*/
ShPtr<BreakStmt> BaseCFGReducer::getBreakStmt(const ShPtr<CFGNode> &node, const ShPtr<CFGNode> &target) {
	PRECONDITION_NON_NULL(node);
	PRECONDITION_NON_NULL(target);

	auto breakStmt = BreakStmt::create(node->getBody()->getAddress());
	breakStmt->setMetadata("break -> " + sc->getLabel(target));

	sc->loopTargets.emplace(breakStmt, node);
	return breakStmt;
}

/**
* @brief 生成一条新的 @c continue 语句, 该语句从节点 @a node 跳转到节点 @a target .
*
* @par 前置条件
*  - @a node 和 @a target 都不为 @c nullptr .
*/
ShPtr<ContinueStmt> BaseCFGReducer::getContinueStmt(const ShPtr<CFGNode> &node, const ShPtr<CFGNode> &target) {
	PRECONDITION_NON_NULL(node);
	PRECONDITION_NON_NULL(target);

	auto continueStmt = ContinueStmt::create(node->getBody()->getAddress());
	continueStmt->setMetadata("continue -> " + sc->getLabel(target));

	sc->loopTargets.emplace(continueStmt, node);
	return continueStmt;
}

/**
* @brief 生成一条新的 @c if 条件语句, 条件语句的条件为 @a cond, @c if 分支代码块由 @a trueBody 给出,
*        @c else 分支代码块由 @a falseBody 给出.
*
* 如果给定的 @a falseBody 为 @c nullptr 或只包含空语句, 则生成的条件语句不会包含 @c else 分支.
*
* @par 前置条件
*  - @a cond 和 @a trueBody 均不为 @c nullptr .
*/

ShPtr<IfStmt> BaseCFGReducer::getIfStmt(const ShPtr<Expression> &cond,
	const ShPtr<Statement> &trueBody, const ShPtr<Statement> &falseBody) {
	PRECONDITION_NON_NULL(cond);
	PRECONDITION_NON_NULL(trueBody);
	return sc->getIfStmt(cond, trueBody, falseBody);
}

/**
* @brief 生成一条新的 @c switch 分支语句, 该语句的条件为 @a cond, 分支代码块由 @a clauses 给出.
* 其中 @a clauses 为 @c switch 语句的所有分支, 每个分支由一组条件和这些条件下执行的 @c case 代码块组成.
*
* @par 前置条件
*  - @a cond 和 @a clauses 均不为 @c nullptr .
*/
ShPtr<SwitchStmt> BaseCFGReducer::getSwitchStmt(const ShPtr<CFGNode> switchNode, const ShPtr<Expression> &cond, const SwitchClauseBodyVector &clauses) {
	PRECONDITION_NON_NULL(switchNode);
	PRECONDITION_NON_NULL(cond);

	auto switchStmt = SwitchStmt::create(cond, nullptr,
		switchNode->getBody()->getAddress());

	for (const auto &clause: clauses) {
		auto conds = clause->first;
		auto clauseBody = clause->second;
		for (std::size_t i = 0, e = conds.size() - 1; i < e; ++i) {
			switchStmt->addClause(conds[i],
				EmptyStmt::create(nullptr, Statement::getLastStatement(clauseBody)->getAddress()));
		}
		switchStmt->addClause(conds.back(), clauseBody);
	}

	return switchStmt;
}

/**
* @brief 生成一条新的 @c while 循环语句, @a loopNode 表示循环的头部节点,
*        循环语句的条件为 @a cond, 循环体由 @a body 给出.
*
* @par 前置条件
*  - @a loopNode, @a cond 和 @a body 均不为 @c nullptr .
*/
ShPtr<WhileLoopStmt> BaseCFGReducer::getWhileLoopStmt(const ShPtr<CFGNode> loopNode, const ShPtr<Expression> &cond, const ShPtr<Statement> &body) {
	PRECONDITION_NON_NULL(cond);
	PRECONDITION_NON_NULL(body);

	return WhileLoopStmt::create(cond, body, nullptr, loopNode->getBody()->getAddress());
}

/**
* @brief 判断给定的节点 @a node 是否包含 @c switch 分支.
*
* @par 前置条件
*  - @a node 不为 @c nullptr .
*/
bool BaseCFGReducer::isSwitch(const ShPtr<CFGNode> &node) const {
	PRECONDITION_NON_NULL(node);
	return llvm::isa<llvm::SwitchInst>(node->getTerm());
}

/**
* @brief 获取给定 @c switch 节点 @a switchNode 的所有分支.
*
* @returns @c switch 节点的所有分支, 每个分支由一个条件和该条件下第一个执行的 @c case 节点组成. 
*
* @par 前置条件
*  - @a switchNode 不为 @c nullptr .
*/
BaseCFGReducer::SwitchClauseVector BaseCFGReducer::getSwitchClauses(
		const ShPtr<CFGNode> &switchNode) {
	PRECONDITION_NON_NULL(switchNode);
	PRECONDITION(isSwitch(switchNode), "node is not a switch");

	CFGNode::CFGNodeVector nodesOrder;
	MapCFGNodeToSwitchClause mapNodeToClause;

	auto switchInst = llvm::cast<llvm::SwitchInst>(switchNode->getTerm());
	for (auto &caseIt: switchInst->cases()) {
		auto cond = sc->converter->convertConstantToExpression(caseIt.getCaseValue());
		auto succ = switchNode->getSucc(caseIt.getSuccessorIndex());

		auto existingClauseIt = mapNodeToClause.find(succ);
		if (existingClauseIt != mapNodeToClause.end()) {
			auto existingClause = existingClauseIt->second;
			auto &clauseConds = existingClause->first;
			clauseConds.push_back(cond);
		} else {
			auto newClause = std::make_shared<SwitchClause>(
				ExprVector{cond}, succ);
			mapNodeToClause.emplace(succ, newClause);
			nodesOrder.push_back(succ);
		}
	}

	SwitchClauseVector clauses;
	for (const auto &clauseNode: nodesOrder) {
		clauses.push_back(mapNodeToClause[clauseNode]);
	}

	ShPtr<CFGNode> switchSuccessor = sc->getSwitchSuccessor(switchNode);
	bool hasDefault = switchSuccessor != switchNode->getSucc(0);
	if (hasDefault) {
		auto defaultClause = std::make_shared<SwitchClause>(
			ExprVector{nullptr}, switchNode->getSucc(0));
		clauses.push_back(defaultClause);
	}

	return clauses;
}

/**
* @brief 为给定的节点 node @a node 的后继节点 @a succ 生成新的代码块.
*
* @par 前置条件
*  - @a node 和 @a succ 均不为 @c nullptr .
*/
ShPtr<Statement> BaseCFGReducer::getSuccessorBody(const ShPtr<CFGNode> &node,
		const ShPtr<CFGNode> &succ) {
	return sc->getSuccessorsBody(node, succ);
}

/**
* @brief 为给定 @c if 分支 @a node 的分支节点 @a clause 生成新的代码块.
*        注意, 必须使用这个函数来获取 @c if 分支的代码块否则可能出现错误.
*
* @param[in] node 产生 @c if 分支的节点
* @param[in] clause 分支目标节点, 若表示跳转到 @c if 语句的后继节点, 则可以为 @c nullptr .
* @param[in] ifSuccessor if 语句的后继节点. 若为 @c nullptr , 语句无后继节点
*
* @par 前置条件
*  - @a node 不为 @c nullptr .
*/
ShPtr<Statement> BaseCFGReducer::getIfClauseBody(const ShPtr<CFGNode> &node,
		const ShPtr<CFGNode> &clause, const ShPtr<CFGNode> &ifSuccessor) {
	PRECONDITION_NON_NULL(node);
	
	if (!clause) {
		return sc->getAssignsToPHINodes(node, ifSuccessor);
	}

	auto body = sc->getSuccessorsBody(node, clause);
	if (ifSuccessor) {
		auto phiCopies = sc->getAssignsToPHINodes(clause, ifSuccessor);
		body = Statement::mergeStatements(body, phiCopies);
	}
	sc->addBranchMetadataToEndOfBodyIfNeeded(body, clause, ifSuccessor);
	return body;
}

/**
* @brief 为给定的自循环节点生成 @c while 语句的循环体.
* 
* @param node 自循环节点, 即该节点的后继节点包含该节点本身.
* @par 前置条件
*  - @a loopNode 不为 @c nullptr .
*/
ShPtr<Statement> BaseCFGReducer::getWhileBody(const ShPtr<CFGNode> &node) {
	PRECONDITION_NON_NULL(node);

	auto body = node->getBody();
	auto phiCopies = sc->getAssignsToPHINodes(node, node);
	body = Statement::mergeStatements(body, phiCopies);

	auto continueStmt = EmptyStmt::create(nullptr, node->getBody()->getAddress());
	continueStmt->setMetadata("continue -> " + sc->getLabel(node));
	body = Statement::mergeStatements(body, continueStmt);

	return body;
}

/**
* @brief 生成从 @a node 跳转到 @a target 的 @c goto 语句的目标代码块.
*        这一函数可以用来将不能识别的结构化语句化简为 @c goto 语句.
* 
* @par 前置条件
*  - @a node 和 @a target 均不为 @c nullptr .
*/
ShPtr<Statement> BaseCFGReducer::getGotoBody(const ShPtr<CFGNode> &node, const ShPtr<CFGNode> &target) {
	PRECONDITION_NON_NULL(node);
	PRECONDITION_NON_NULL(target);
	return sc->getGotoForSuccessor(node, target);
}

/**
* @brief 判断给定的节点 @a node 是否是当前CFGReducer正在分析的循环的头部节点.
* 
* @par 前置条件
*  - @a node 不为 @c nullptr .
*/
bool BaseCFGReducer::isLoopHeaderUnderAnalysis(const ShPtr<CFGNode> &node) const {
	PRECONDITION_NON_NULL(node);
	return hasItem(loopHeadersOnStack, node) 
		&& !loopHeaderStack.empty()
		&& loopHeaderStack.top() == node;
}

/**
* @brief 获取当前CFGReducer正在分析的循环的头部节点.
*/
ShPtr<CFGNode> BaseCFGReducer::getLoopHeaderUnderAnalysis() const {
	if (loopHeaderStack.empty()) {
		return nullptr;
	}
	return loopHeaderStack.top();
}

/**
* @brief 告诉CFGReducer开始分析给定的循环头部节点 @a node .
* 
* @par 前置条件
*  - @a node 不为 @c nullptr .
*  - @a node 是循环头部节点.
*/
void BaseCFGReducer::enterLoop(ShPtr<CFGNode> node) {
	PRECONDITION_NON_NULL(node);
	PRECONDITION(isLoopHeader(node), "node is not a loop header");
	sc->loopHeaders.emplace(sc->getLoopFor(node), node);
	auto loopSuccessor = getLoopSuccessor(node);
	node->setStatementSuccessor(loopSuccessor);

	loopHeaderStack.push(node);
	loopSuccsUnderAnalysis.push_back(loopSuccessor);
	loopHeadersOnStack.insert(node);
}

/**
* @brief 告诉CFGReducer当前循环的分析结束了.
*/
void BaseCFGReducer::leaveLoop() {
	auto node = loopHeaderStack.top();
	loopHeaderStack.pop();
	loopSuccsUnderAnalysis.pop_back();
	loopHeadersOnStack.erase(node);
}

/**
* @brief 判断节点 @a node 所在的所有循环是否均已被化简.
*
* @par 前置条件
*  - @a node 不为 @c nullptr .
*/
bool BaseCFGReducer::isLoopReduced(const ShPtr<CFGNode> &node) const {
	PRECONDITION_NON_NULL(node);
	return sc->getLoopFor(node) == nullptr;
}

/**
* @brief 获取 @a loopNode 所在未被化简的最内层循环的后继节点.
*
* 若循环没有后继节点, 则返回 @c nullptr .
*
* @par 前置条件
*  - @a loopNode 不为 @c nullptr .
*  - @a loopNode 位于未被化简的循环中.
*/
ShPtr<CFGNode> BaseCFGReducer::getLoopSuccessor(ShPtr<CFGNode> loopNode) {
	PRECONDITION_NON_NULL(sc->getLoopFor(loopNode));
	return sc->getLoopSuccessor(loopNode);
}

/**
* @brief 获取所有CFGReducer正在分析的循环的后继节点.
*
* @returns 从内层循环到外层循环的后继节点列表. 若无循环正在被分析, 则返回空列表.
*/
BaseCFGReducer::CFGNodeVector BaseCFGReducer::getLoopSuccsUnderAnalysis() {
	return loopSuccsUnderAnalysis;
}

}
}
