#pragma once
#include <Sprite.h>
#include <TextureManager.h>
#include <Object3D.h>
#include <Object3DCommon.h>
#include <WavLoader.h>
#include <SRVManager.h>
#include <Input.h>
#include <BaseScene.h>

// クライアント領域サイズ
static const uint32_t kClientWidth = 1280;
static const uint32_t kClientHeight = 720;

/// ---------- 前方宣言 ---------- ///
class DirectXCommon;
class ImGuiManager;

/// -------------------------------------------------------------
///				　		ゲームプレイシーン
/// -------------------------------------------------------------
class GamePlayScene : public BaseScene
{
public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize() override;

	// 更新処理
	void Update() override;

	// 描画処理
	void Draw() override;

	// 終了処理
	void Finalize() override;

	void DrawImGui() override;

private: /// ---------- メンバ変数 ---------- ///

	DirectXCommon* dxCommon_ = nullptr;
	TextureManager* textureManager = nullptr;
	Input* input_ = nullptr;

	// トランスフォーム
	Transform transform_;

	std::unique_ptr<SRVManager> srvManager;
	std::unique_ptr<Camera> camera_;
	std::unique_ptr<WavLoader> wavLoader_;

	std::vector<std::unique_ptr<Sprite>> sprites_;
	std::unique_ptr<Object3D> playerObject_;
	std::unique_ptr<Object3DCommon> object3DCommon_;

	// テクスチャのパスをリストで管理
	std::vector<std::string> texturePaths_;
	std::vector<std::string> objectFiles;
	std::vector<Vector3> initialPositions;


	std::array<float, 3> lanePositions_ = { -3.0f, 0.0f, 3.0f };// レーンのX座標
	int currentLaneIndex_ = 1; // 現在のレーン（中央）
	float moveSpeed_ = 0.1f; // 線形補間のスピード

	// ジャンプ処理用
	bool isJumping_ = false;           // ジャンプ中フラグ
	float jumpVelocity_ = 0.2f;        // ジャンプ初速度
	float gravity_ = -0.01f;           // 重力加速度
	float jumpHeight_ = -1.0f;          // ジャンプのY座標オフセット

	// 回転処理用
	bool isRotating_ = false;          // 回転中フラグ
	float rotationAngle_ = 0.0f;       // 現在の回転角度
	float rotationSpeed_ = 10.0f;      // 回転速度 (度/フレーム)
	int rotationCount_ = 0;            // 回転カウント
};

