#pragma once
#include "IBossState.h"
#include "BossAttackState.h"
#include "BossDownState.h"
#include "BossIdleState.h"
#include "EnragedState.h"

/// -------------------------------------------------------------
///							ボスクラス
/// -------------------------------------------------------------
class Boss
{
private: /// ---------- メンバ変数 ---------- ///
	
	// 現在のゲーム状態を表す
	IBossState* currentState;

public: /// ---------- メンバ関数 ---------- ///

	// コンストラクタ：初期状態nullを設定
	Boss() : currentState(nullptr) {}

	// デストラクタ
	~Boss();

	// 状態変更
	void ChangeState(IBossState* newState);

	// 更新処理
	void Update();

	// 描画処理
	void Draw();
};

