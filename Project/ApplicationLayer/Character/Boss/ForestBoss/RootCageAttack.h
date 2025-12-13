#pragma once
#include "IBossAttack.h"

/// -------------------------------------------------------------
///						根の檻攻撃クラス
/// -------------------------------------------------------------
class RootCageAttack : public IBossAttack
{
public: /// ---------- メンバ関数 ---------- ///

	// デストラクタ
	virtual ~RootCageAttack() = default;

	// 攻撃名を取得
	const char* GetName() const override { return "RootCageAttack"; }

	// 初期化処理
	void Initialize() override;

	// クールダウン処理
	void TickCooldown(float deltaTime) override;

	// 攻撃可能か
	bool CanAttack() const override;

	// 攻撃実行
	void Attack() override;

	// 更新処理
	void Update(Boss* boss, float deltaTime, float bossYawRad, const Vector3& playerPosition) override;

	// 攻撃がアクティブか
	bool IsActive() const override;

	// 描画処理
	void Draw() override;

	// ImGui描画処理
#ifdef USE_IMGUI
	void DrawImGui(Boss& boss) override;
#endif

private: /// ---------- メンバ変数 ---------- ///
};

