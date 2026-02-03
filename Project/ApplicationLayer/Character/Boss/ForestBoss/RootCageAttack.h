#pragma once
#include "IBossAttack.h"
#include "BossConfig.h"
#include <Object3D.h>
#include <vector>
#include <memory>

namespace K4E = ::Ken4lowEngine;

/// -------------------------------------------------------------
///						根の檻攻撃クラス
/// -------------------------------------------------------------
class RootCageAttack : public IBossAttack
{
private: /// ---------- 列挙型 ---------- ///

	// 根の檻攻撃フェーズ列挙型
	enum class Phase
	{
		Idle,	  // 待機
		Windup,	  // 溜め
		Active,	  // 有効
		Recovery, // 回復
		Cooldown, // クールダウン
	};

private: /// ---------- 構造体 ---------- ///

	// 根の檻攻撃フェーズ
	struct RootCageRuntime
	{
		Phase phase = Phase::Idle;
		float phaseTimer = 0.0f;     // 今フェーズ内経過
		float cooldownTimer = 0.0f;  // クールダウン残り
		float lockedYaw = 0.0f;      // 攻撃開始時の向き（Yaw）
		bool  didHit = false;        // 1回だけ当てる用
		bool  visible = false;       // 描画するか
		K4E::Vector3 center{};	 // 檻の中心位置
	} rootCage_;

	// ルート一本分の情報
	struct RootColumn
	{
		std::unique_ptr<K4E::Object3D> object; // 根オブジェクト
		K4E::Vector3 basePosition{};			  // 根の基点位置（ワールド座標）
		float height = 3.0f;			  // 現在の高さ（伸び具合）
		float growthSpeed = 0.0f;		  // 成長速度
	};

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
	void Update(Boss* boss, float deltaTime, float bossYawRad, const K4E::Vector3& playerPosition) override;

	// 攻撃がアクティブか
	bool IsActive() const override;

	// 描画処理
	void Draw() override;

	// ImGui描画処理
#ifdef USE_IMGUI
	void DrawImGui(Boss& boss) override;
#endif

private: /// ---------- フェーズの更新処理関数 ---------- ///

	// 待機フェーズ更新
	void UpdatePhase_Idle(Boss* boss, float deltaTime, float bossYaw, const K4E::Vector3& playerPos);

	// 溜めフェーズ更新
	void UpdatePhase_Windup(Boss* boss, float deltaTime);

	// 有効フェーズ更新
	void UpdatePhase_Active(Boss* boss, float deltaTime, const K4E::Vector3& playerPos);

	// 回復フェーズ更新
	void UpdatePhase_Recovery(Boss* boss, float deltaTime);

	// クールダウンフェーズ更新
	void UpdatePhase_Cooldown(float deltaTime);

private: /// ---------- メンバ関数 ----------/// 

	// パラメータショートカット
	const ForestBossParams::RootCage& Params(const Boss* boss) const;

	// 根の檻の柱を構築
	void BuildColumns(const ForestBossParams::RootCage& p, const K4E::Vector3& center, float yaw);

	// 伸びる更新
	void UpdateColumnsGrow(const ForestBossParams::RootCage& p, float tGrow);

	// 縮む更新
	void UpdateColumnsShrink(const ForestBossParams::RootCage& p, float tShrink);

private: /// ---------- メンバ変数 ---------- ///

	// 根の檻攻撃用データ
	std::vector<RootColumn> columns_;

	// Attack() からの開始リクエスト
	bool requestStart_ = false;
};

