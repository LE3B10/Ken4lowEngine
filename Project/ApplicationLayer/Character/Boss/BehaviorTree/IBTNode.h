#pragma once
enum class BTNodeResult
{
    Success,
    Failure,
    Running
};

class IBTNode
{
public:
    virtual ~IBTNode() = default;
    virtual BTNodeResult Tick(float deltaTime) = 0;
};