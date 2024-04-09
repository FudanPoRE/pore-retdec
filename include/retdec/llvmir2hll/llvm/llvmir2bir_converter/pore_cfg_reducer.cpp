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

    /* 
    * TODO: 根据CFG的代码模式识别高级语言结构, 并生成结构化代码, 从而化简CFG.
    * 提示: 模版给定的代码仅作参考, 可以任意修改pore_cfg_reducer.cpp和pore_cfg_reducer.h,
    *      仅需保证这两个代码文件在框架中能够被正确编译和运行, 完成指定任务.
    */

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

    /* 
     * TODO: 处理有后继节点, 但是没有被inspectCFGNode识别和化简的CFGNode.
     * 提示: 需要考虑三种类型: 
     *     (1) node是switch语句;
     *     (2) node有两个后继节点;
     *     (3) node有一个后继节点, 已经编写了示例.
     */

    CFGNodeVector succs;
    if (node->getSuccNum() > 0) {
        // 提示: 下面是一个化简CFG的示例代码, 所有CFG的化简都可以使用reduceNode完成
        reduceNode(node, node->getBody(), succs, false);

        // 提示: 可以通过下面的方式在测试时打印Log
        // LOG << "Completely ignore node successors: " << node->getDebugStr() << "\n";
    }
}

bool PoRECFGReducer::tryReduceSequence(ShPtr<CFGNode> node) {
    /*
    * TODO: 识别并化简顺序结构, 注意有些顺序结构有多个前继节点
    */
    return false;
}

bool PoRECFGReducer::tryReduceIf(ShPtr<CFGNode> node) {
    /*
    * TODO: 识别并化简 if 结构，优先考虑下面两种简单结构
    * 第一种 if-else:
    * node
    *  ├── ifNode
    *  |     └── ifSucc
    *  └── elseNode
    *        └── ifSucc
    *
    * 第二种 简单if:
    * node
    *  ├── ifNode
    *  |     └── ifSucc
    *  └── ifSucc
    */

    return false;
}

bool PoRECFGReducer::tryReduceLoop(ShPtr<CFGNode> node) {
    /* 
    * TODO: 识别并化简 if 结构, 优先考虑下面的简单结构
    * 注意查阅API文档
    *
    * while-true:
    * node
    *  └── node
    */
    return false;
}

bool PoRECFGReducer::tryReduceSwitch(ShPtr<CFGNode> node) {
    /* 
    * TODO: 识别并化简 switch 结构, 注意查阅API文档, 并注意switch
    * 相关的函数及其返回类型定义
    */
    return false;
}

}
}
