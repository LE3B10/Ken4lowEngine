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

	/// ArmorBreakを連続発生させて、元オブジェクトが欠けていくように見せるテスト
	void StartDebugArmorBreakDissolve(uint32_t meshId, const std::string& meshModelPath, const Vector3& center, float radius);
	void UpdateDebugArmorBreakDissolve(float deltaTime);

	/// Object3Dを実際に欠けさせながらMeshParticleを出す崩壊テスト
	void ResetDebugBreakObject(const std::string& modelPath, const Vector3& center, float scale);
	void StartDebugBreakObjectDissolve();
	void UpdateDebugBreakObjectDissolve(float deltaTime);

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

	// 実際にDissolveで欠けさせる確認用Object3D
	std::unique_ptr<K4E::Object3D> debugBreakObject_;

	// --- 仮ヒット確認用パラメータ ---
	bool debugBossHitTestEnabled_ = true; // 仮ヒット確認ON/OFF
	float debugHitRadius_ = 0.75f;        // 攻撃球の半径
	float debugBaseDamage_ = 10.0f;       // 基礎ダメージ

	std::string debugHitLog_ = "Press H to test hit.";

	// --- GPUパーティクルテスト用 ---
	std::string debugParticleLog_ = "Press 1/2/3 to test GPU particles.";

	// --- ArmorBreak Dissolve テスト用 ---
	bool debugArmorBreakDissolveActive_ = false;
	float debugArmorBreakDissolveTimer_ = 0.0f;
	float debugArmorBreakDissolveDuration_ = 1.35f;
	float debugArmorBreakDissolveEmitTimer_ = 0.0f;
	float debugArmorBreakDissolveEmitInterval_ = 0.055f;
	float debugArmorBreakDissolveRadius_ = 0.35f;
	uint32_t debugArmorBreakDissolveMeshId_ = 1000;
	uint32_t debugArmorBreakDissolveCountPerBurst_ = 10;
	Vector3 debugArmorBreakDissolveCenter_{ 0.0f, 2.5f, 18.0f };
	std::string debugArmorBreakDissolveMeshModelPath_ = "Test/cube.gltf";

	// --- Object3D崩壊テスト用 ---
	bool debugBreakObjectVisible_ = true;
	bool debugBreakObjectDissolveActive_ = false;
	float debugBreakObjectDissolveTimer_ = 0.0f;
	float debugBreakObjectDissolveDuration_ = 1.80f;
	float debugBreakObjectEmitTimer_ = 0.0f;
	float debugBreakObjectEmitInterval_ = 0.050f;
	float debugBreakObjectScale_ = 1.0f;
	float debugBreakObjectParticleRadius_ = 0.45f;
	uint32_t debugBreakObjectMeshId_ = 1000;
	uint32_t debugBreakObjectCountPerBurst_ = 12;
	Vector3 debugBreakObjectCenter_{ 0.0f, 2.5f, 18.0f };
	std::string debugBreakObjectModelPath_ = "Test/cube.gltf";
};

