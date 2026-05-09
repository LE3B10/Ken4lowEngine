#pragma once
#include "BaseScene.h"
#include "CollisionManager.h"
#include "BulletManager.h"
#include "Enemy.h"
#include "Player.h"
#include "Derived/GuardianBoss/GuardianBoss.h"
#include "Disintegration/ModelBlockEffectSequence.h"
#include "Disintegration/ModelDisintegrationEffect.h"
#include "Disintegration/ModelReconstructionEffect.h"
#include "Object3D.h"

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

private: /// ---------- 型 ---------- ///

	enum class DebugModelBlockSequenceRequest
	{
		None,
		PlaySpawnThenDisintegrate,
		PlayDisintegrateThenReconstruct,
		PlayLoop,
		Stop,
		Reset,
		Reload,
	};

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

	/// モデル崩壊エフェクトの単体検証更新
	void UpdateDebugDisintegrationTest(float deltaTime);

	/// モデル崩壊エフェクトをデバッグ位置で再生
	void PlayDebugDisintegrationEffect();

	/// モデル崩壊テスト用モデルを読み直す
	void ReloadDebugDisintegrationModel();

	/// 予約されたモデル崩壊テスト操作を描画前に処理
	void ProcessDebugDisintegrationRequest();

	/// モデル再構築エフェクトの単体検証更新
	void UpdateDebugReconstructionTest(float deltaTime);

	/// モデル再構築エフェクトをデバッグ位置で再生
	void PlayDebugReconstructionEffect();

	/// モデル再構築テスト用モデルを読み直す
	void ReloadDebugReconstructionModel();

	/// 予約されたモデル再構築テスト操作を描画前に処理
	void ProcessDebugReconstructionRequest();

	/// モデルブロック演出シーケンスの更新
	void UpdateDebugModelBlockSequence(float deltaTime);

	/// モデルブロック演出シーケンス用モデルを読み直す
	void ReloadDebugModelBlockSequenceModel();

	/// 予約されたモデルブロック演出シーケンス操作を描画前に処理
	void ProcessDebugModelBlockSequenceRequest();

	/// モデルブロック演出シーケンスのワールド行列を作成
	K4E::Matrix4x4 MakeDebugModelBlockSequenceWorldMatrix() const;

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

	// --- モデル崩壊エフェクト単体テスト用 ---
	std::unique_ptr<K4E::Object3D> debugDisintegrationModel_;
	std::unique_ptr<ModelDisintegrationEffect> debugDisintegrationEffect_;
	K4E::Vector3 debugDisintegrationPosition_{ -4.0f, 2.25f, 18.0f };
	K4E::Vector3 debugDisintegrationRotation_{ 0.0f, 0.0f, 0.0f };
	K4E::Vector3 debugDisintegrationScale_{ 1.0f, 1.0f, 1.0f };
	std::string debugDisintegrationModelPath_ = "Characters/body.gltf";
	std::string debugDisintegrationLog_ = "Press F9 or the ImGui button to play.";
	bool debugDisintegrationModelVisible_ = true;
	bool pendingDebugDisintegrationPlay_ = false;
	bool pendingDebugDisintegrationReload_ = false;
	bool pendingDebugDisintegrationReset_ = false;
	bool debugDisintegrationPaused_ = false;
	bool debugDisintegrationStepFrame_ = false;
	std::string pendingDebugDisintegrationPath_;

	// --- モデル再構築エフェクト単体テスト用 ---
	std::unique_ptr<K4E::Object3D> debugReconstructionModel_;
	std::unique_ptr<ModelReconstructionEffect> debugReconstructionEffect_;
	K4E::Vector3 debugReconstructionPosition_{ 4.0f, 2.25f, 18.0f };
	K4E::Vector3 debugReconstructionRotation_{ 0.0f, 0.0f, 0.0f };
	K4E::Vector3 debugReconstructionScale_{ 1.0f, 1.0f, 1.0f };
	std::string debugReconstructionModelPath_ = "Characters/body.gltf";
	std::string debugReconstructionLog_ = "Press F10 or the ImGui button to play.";
	bool debugReconstructionModelVisible_ = true;
	bool pendingDebugReconstructionPlay_ = false;
	bool pendingDebugReconstructionReload_ = false;
	std::string pendingDebugReconstructionPath_;

	// --- モデルブロック演出シーケンステスト用 ---
	std::unique_ptr<K4E::Object3D> debugModelBlockSequenceModel_;
	std::unique_ptr<ModelDisintegrationEffect> debugSequenceDisintegrationEffect_;
	std::unique_ptr<ModelReconstructionEffect> debugSequenceReconstructionEffect_;
	std::unique_ptr<ModelBlockEffectSequence> debugModelBlockSequence_;
	K4E::Vector3 debugModelBlockSequencePosition_{ 0.0f, 2.25f, 18.0f };
	K4E::Vector3 debugModelBlockSequenceRotation_{ 0.0f, 0.0f, 0.0f };
	K4E::Vector3 debugModelBlockSequenceScale_{ 1.0f, 1.0f, 1.0f };
	std::string debugModelBlockSequenceModelPath_ = "Characters/body.gltf";
	std::string debugModelBlockSequenceLog_ = "Use the ImGui sequence buttons to play.";
	DebugModelBlockSequenceRequest debugModelBlockSequenceRequest_ = DebugModelBlockSequenceRequest::None;
	std::string pendingDebugModelBlockSequencePath_;
};

