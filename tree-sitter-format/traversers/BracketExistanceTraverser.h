#pragma once

#include <tree-sitter-format/traversers/Traverser.h>

namespace tree_sitter_format {

class BracketExistanceTraverser : public Traverser {
protected:
    void reset(const TraverserContext& context) override;
    
    void visitLeaf(TSNode node, TraverserContext& context) override;
    void preVisitChild(TSNode node, uint32_t childIndex, TraverserContext& context) override;
    void postVisitChild(TSNode node, uint32_t childIndex, TraverserContext& context) override;
};

}
