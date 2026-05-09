#pragma once
#include "Disintegration/ModelBlockEffectSequence.h"
#include "Disintegration/ModelDisintegrationEffect.h"
#include "Disintegration/ModelReconstructionEffect.h"
#include "Object3D.h"

#include <memory>
#include <string>

namespace Ken4lowEngine { class Input; }
namespace K4E = ::Ken4lowEngine;

/// -------------------------------------------------------------
/// モデル崩壊・再構築・ブロック演出のDebugScene専用確認処理
/// -------------------------------------------------------------
class DisintegrationDebugController
{
public:
	void Initialize();
	void Update(float deltaTime);
	void Draw3DObjects();
	void DrawShadowObjects();
	void DrawImGui();
	void Finalize();

private:
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

private:
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

private:
	K4E::Input* input_ = nullptr;

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
	bool debugModelBlockSequencePaused_ = false;
	bool debugModelBlockSequenceStepFrame_ = false;
	std::string pendingDebugModelBlockSequencePath_;
};
