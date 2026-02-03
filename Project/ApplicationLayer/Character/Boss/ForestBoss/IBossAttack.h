#pragma once
#include <Vector3.h>

namespace K4E = ::Ken4lowEngine;

/// ---------- 前方宣言 ---------- ///
class Boss;

/// -------------------------------------------------------------
///					　		ボス攻撃インターフェース
/// -------------------------------------------------------------
class IBossAttack
{
public: /// ---------- 純粋仮想関数 ---------- ///

	// デストラクタ
	virtual ~IBossAttack() = default;

	// 攻撃名を取得
	virtual const char* GetName() const = 0;

	// 初期化処理
	virtual void Initialize() = 0;

	// クールダウン処理
	virtual void TickCooldown(float deltaTime) = 0;

	// 攻撃可能か
	virtual bool CanAttack() const = 0;

	// 攻撃実行
	virtual void Attack() = 0;

	// 更新処理
	virtual void Update(Boss* boss, float deltaTime, float bossYawRad, const K4E::Vector3& playerPosition) = 0;

	// 攻撃がアクティブか
	virtual bool IsActive() const = 0;

	// 描画処理
	virtual void Draw() = 0;

	// ImGui描画処理
#ifdef USE_IMGUI
	virtual void DrawImGui(Boss& boss) = 0;
#endif
};

