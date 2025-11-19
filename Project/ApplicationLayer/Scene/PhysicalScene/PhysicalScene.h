#pragma once
#include <BaseScene.h>
#include <Object3D.h>

#include <vector>

/// ---------- 前方宣言 ---------- ///
class DirectXCommon;
class Input;
class Camera;
class GpuParticleEmitter;

/// -------------------------------------------------------------
//				物理シーン（デバッグテスト用・サブシーン）
/// -------------------------------------------------------------
class PhysicalScene : public BaseScene
{
	// ステージの状態
	enum class StageState
	{
		Locked,      // 真っ黒・未開放
		Available,   // 未クリア（普通）
		Unlocking,   // 解放演出中
		Cleared      // 解放済み
	};

public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize() override;

	// 更新処理
	void Update() override;

	// 3Dオブジェクトの描画
	void Draw3DObjects() override;

	// 2Dオブジェクトの描画
	void Draw2DSprites() override;

	// 終了処理
	void Finalize() override;

	// ImGui描画処理
	void DrawImGui() override;

private: /// ---------- メンバ関数 ---------- ///

	void ApplyLockedVisual();

	void ApplyAvailableVisual();

	void ApplyClearedVisual();

	void StartUnlock();

	void UpdateUnlock(float deltaTime);

private: /// ---------- メンバ変数 ---------- ///

	DirectXCommon* dxCommon_ = nullptr; // DirectX共通管理クラス
	Input* input_ = nullptr; // 入力管理クラス
	Camera* camera = nullptr; // カメラ

	std::unique_ptr<Object3D> object3D_; // 3Dオブジェクト

	// GPUパーティクル（解放エフェクト用）
	GpuParticleEmitter* unlockEmitter_ = nullptr;

	StageState state_ = StageState::Locked;

	// 解放演出用タイマー
	float unlockTimer_ = 0.0f;
	float unlockDuration_ = 1.0f; // 1秒くらいで解放

	Vector3 baseTranslate_{};   // 浮遊の基準となる位置
	float   floatTimer_ = 0.0f; // 上下移動用のタイマー
	bool    isSelected_ = true; // このキューブが「現在選択中か」
};

