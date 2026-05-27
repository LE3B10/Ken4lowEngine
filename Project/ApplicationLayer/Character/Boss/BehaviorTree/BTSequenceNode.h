#pragma once
#include "IBTNode.h"

#include <memory>
#include <vector>

class BTSequenceNode : public IBTNode
{
public:
    void AddChild(std::unique_ptr<IBTNode> child)
    {
        children_.push_back(std::move(child));
    }

    BTNodeResult Tick(float deltaTime) override
    {
        for (auto& child : children_)
        {
            const BTNodeResult result = child->Tick(deltaTime);
            if (result != BTNodeResult::Success)
            {
                return result;
            }
        }

        return BTNodeResult::Success;
    }

private:
    std::vector<std::unique_ptr<IBTNode>> children_;
};
