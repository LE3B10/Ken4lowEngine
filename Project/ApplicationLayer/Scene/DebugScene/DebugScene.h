#pragma once
#include "BaseScene.h"
#include "CollisionManager.h"

#include <cstdint>
#include <memory>

class PhysicsDebugController;
class AnimationModelBatchTest;

/// ---------- 前方宣言 ---------- ///
namespace Ken4lowEngine
{
	class Input;
	class InstancedObject3DRenderer;
}

namespace K4E = ::Ken4lowEngine;

/// -------------------------------------------------------------
///					　	デバッグシーン
/// -------------------------------------------------------------
class DebugScene : public BaseScene
{
public: /// ---------- メンバ関数 ---------- ///

	DebugScene();
	~DebugScene() override;

	// 仮想初期化処理
	void Initialize() override;

	// 仮想更新処理
	void Update() override;
	// Editor停止中もDebugScene専用負荷検証だけは更新する。
	void UpdateEditor(float deltaTime) override;

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
	// ImGui設定から大量配置データを再構築する。
	void RebuildInstancingTest();

private: /// ---------- メンバ変数 ---------- ///

	K4E::Input* input_ = nullptr; // Inputのポインタ
	bool isDebugCamera_ = false; // デバッグカメラ使用フラグ

	std::unique_ptr<CollisionManager> collisionManager_; // 衝突管理マネージャー

	std::unique_ptr<PhysicsDebugController> physicsDebugController_; // DebugScene専用の物理確認コントローラ
	std::unique_ptr<AnimationModelBatchTest> animationModelBatchTest_; // AnimationModel大量描画のDebugScene専用テスト

	// 3万個のObject3Dを生成せず、専用GPUインスタンシング経路をON/OFFして負荷確認する。
	std::unique_ptr<K4E::InstancedObject3DRenderer> instancingTestRenderer_;
	bool isInstancingTestEnabled_ = false;
	int instancingTestCount_ = 30000;
	float instancingTestSpacing_ = 2.0f;
	int instancingSafeCount_ = 30000;
	uint64_t instancingIndexBudget_ = 50'000'000ull;
	bool instancingAutoClamp_ = true;

	bool instancingRandomScale_ = false;
	bool instancingRandomRotation_ = false;
	bool instancingRandomColor_ = false;
	bool instancingFrustumCulling_ = false;
};
