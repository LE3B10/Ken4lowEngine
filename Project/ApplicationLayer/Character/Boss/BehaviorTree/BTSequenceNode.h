#pragma once
#include "IBTNode.h"

#include <memory>
#include <vector>

class BTSequenceNode : public IBTNode
{
public:
    void AddChild(std::unique_ptr<IBTNode> child);
    BTNodeResult Tick(float deltaTime) override;

private:
    std::vector<std::unique_ptr<IBTNode>> children_;
};
