#pragma once
#include "BaseScene.h"
#include "CollisionManager.h"
#include "BulletManager.h"
#include "Enemy.h"
#include "Player.h"
#include "Derived/GuardianBoss/GuardianBoss.h"
#include "Object3D.h"
#include "Vector3.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace K4E = ::Ken4lowEngine;

/// ---------- 前方宣言 ---------- ///
namespace Ken4lowEngine { class DirectXCommon; }
namespace Ken4lowEngine { class Input; }

/// -------------------------------------------------------------
///					　	デバッグシーン
/// -------------------------------------------------------------
class DebugScene : public BaseScene
{
private: /// ---------- エイリアス ---------- ///

	using Vector3 = K4E::Vector3;

	/// -------------------------------------------------------------
	/// Voxel Disintegration確認用ブロック
	/// -------------------------------------------------------------
	struct DebugVoxelBlock
	{
		std::unique_ptr<K4E::Object3D> object;
		Vector3 position{};
		bool visible = true;
		float breakTime = 0.0f;
		float ashEmitTimer = 0.0f;
	};

public: /// ---------- メンバ関数 ---------- ///

	// 仮想初期化処理
	void Initialize() override;

	// 仮想更新処理
	void Update() override;

	// 仮想3D描画処理
	void Draw3DObjects() override;

	// 仮想シャドウマップ描画処理
	void DrawShadowObjects() override;

	// 仮想2D描画処理
	void Draw2DSprites() override;

	// 仮想終了処理
	void Finalize() override;

	// ImGui描画処理
	void DrawImGui() override;

private: /// ---------- メンバ関数 ---------- ///

	// デバッグカメラの更新
	void UpdateDebug();

	/// -------------------------------------------------------------
	/// 仮ヒット確認
	/// Hキーなどでボスに対して簡易球判定を飛ばす
	/// -------------------------------------------------------------
	void UpdateDebugBossHitTest();

	/// テスト用GPUパーティクル発火
	void UpdateDebugParticleTest();

	/// Voxel Disintegration確認用
	void BuildDebugVoxelDisintegration();
	void StartDebugVoxelDisintegration();
	void StartDebugVoxelAshDisintegration();
	void UpdateDebugVoxelDisintegration(float deltaTime);
	void EmitDebugVoxelBreakParticle(const Vector3& position, uint32_t count);

	/// -------------------------------------------------------------
	/// BossHitPart を文字列へ変換
	/// ログ確認用
	/// -------------------------------------------------------------
	const char* ToString(BossHitPart part) const;

private: /// ---------- メンバ変数 ---------- ///

	K4E::DirectXCommon* dxCommon_ = nullptr; // DirectXCommonのポインタ
	K4E::Input* input_ = nullptr; // Inputのポインタ
	bool isDebugCamera_ = false; // デバッグカメラ使用フラグ

	std::unique_ptr<CollisionManager> collisionManager_; // 衝突管理マネージャー

	// 描画確認用ボス
	std::unique_ptr<GuardianBoss> debugBoss_;

	// --- 仮ヒット確認用パラメータ ---
	bool debugBossHitTestEnabled_ = true; // 仮ヒット確認ON/OFF
	float debugHitRadius_ = 0.75f;        // 攻撃球の半径
	float debugBaseDamage_ = 10.0f;       // 基礎ダメージ

	std::string debugHitLog_ = "Press H to test hit.";

	// --- GPUパーティクルテスト用 ---
	std::string debugParticleLog_ = "Press 1/2/3 to test GPU particles.";

	// --- Voxel Disintegration テスト用 ---
	std::vector<DebugVoxelBlock> debugVoxelBlocks_;
	bool debugVoxelDisintegrationActive_ = false;
	bool debugVoxelAshMode_ = false;
	float debugVoxelDisintegrationTimer_ = 0.0f;
	float debugVoxelDisintegrationDuration_ = 1.60f;
	float debugVoxelBlockScale_ = 0.32f;
	float debugVoxelSpacing_ = 0.34f;
	float debugVoxelParticleRadius_ = 0.28f;
	float debugVoxelAshLeadTime_ = 0.35f;
	float debugVoxelAshEmitInterval_ = 0.045f;
	float debugVoxelJitterRate_ = 0.65f;
	float debugVoxelPlaneNoiseRate_ = 0.22f;
	float debugVoxelRandomRotationRate_ = 1.0f;
	int debugVoxelGridX_ = 4;
	int debugVoxelGridY_ = 4;
	int debugVoxelGridZ_ = 3;
	uint32_t debugVoxelMeshId_ = 1000;
	uint32_t debugVoxelParticleCount_ = 10;
	Vector3 debugVoxelCenter_{ 0.0f, 2.5f, 18.0f };
	std::string debugVoxelModelPath_ = "Test/cube.gltf";
};

