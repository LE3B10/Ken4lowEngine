#pragma once
#include "IPlayerState.h"

#include <Windows.h>

class Input;

class RunningState : public IPlayerState
{
public:

    // 初期化処理
    void Initialize() override;
   
    // 更新処理
    void Update() override;
   
    // 入力処理
    void HandleInput(Input* input) override;
    
    // 描画処理
    void Draw() override;
    
    // 終了処理
    void Exit() override;
};

