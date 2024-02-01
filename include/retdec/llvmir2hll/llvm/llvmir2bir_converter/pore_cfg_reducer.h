#ifndef PORE_CFG_REDUCER_H
#define PORE_CFG_REDUCER_H

#include "retdec/utils/container.h"
#include "retdec/llvmir2hll/support/smart_ptr.h"
#include "retdec/llvmir2hll/support/types.h"
#include "retdec/llvmir2hll/support/debug.h"
#include "retdec/llvmir2hll/llvm/llvmir2bir_converter/cfg_node.h"
#include "retdec/llvmir2hll/llvm/llvmir2bir_converter/base_cfg_reducer.h"

namespace retdec {
namespace llvmir2hll {

class BaseCFGReducer;

class PoRECFGReducer final: protected BaseCFGReducer {

    friend class StructureConverter;

public:
    using BaseCFGReducer::BaseCFGReducer;

protected:
    using BaseCFGReducer::inspectCFGNode;
    bool inspectCFGNode(ShPtr<CFGNode> node);
	void inspectCFGNodeComplete(ShPtr<CFGNode> node);

private:

    /// @name Detection of constructions
	/// @{
	bool tryReduceSequence(ShPtr<CFGNode> node);
	bool tryReduceIf(ShPtr<CFGNode> node);
	bool tryReduceLoop(ShPtr<CFGNode> node);
    bool tryReduceSwitch(ShPtr<CFGNode> node);
	/// @}

};

}
}

#endif