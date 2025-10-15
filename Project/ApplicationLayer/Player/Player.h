#pragma once
#include <BaseCharacter.h>
#include <PlayerController.h>
#include <Object3D.h>
#include <FpsCamera.h>

#include <memory>

/// ---------- 前方宣言 ---------- ///
class Input;

/// ---------- モデルの状態を表す列挙型 ---------- ///
enum class ModelState
{
	Idle,       // アイドル状態
	Walking,    // 歩行状態
	Running,    // 走行状態
	Jumping,    // ジャンプ状態
	Crouching,  // しゃがみ状態
	Shooting,   // 射撃状態
	Aiming,     // エイム
	Reloading,  // リロード状態
	Dead        // 死亡状態
};

enum Id
{
	kBody,
	kHead,
	kLeftArm,
	kRightArm,
	kLeftLeg,
	kRightLeg
};

/// -------------------------------------------------------------
///					　プレイヤークラス
/// -------------------------------------------------------------
class Player : public BaseCharacter
{
public: /// ---------- メンバ関数 ---------- ///

	// デストラクタ
	~Player();

	// 初期化処理
	void Initialize() override;

	// 更新処理
	void Update() override;

	// 描画処理
	void Draw() override;

	// ImGui描画処理
	void DrawImGui() override;

	// 衝突時に呼ばれる仮想関数
	void OnCollision(Collider* other) override {};

public: /// ---------- アクセサー関数 ---------- ///

	// デバッグカメラフラグ取得
	bool IsDebugCamera() const { return isDebugCamera_; }
	void SetDebugCamera(bool isDebug) { isDebugCamera_ = isDebug; }

	// FPSカメラ取得
	FpsCamera* GetFpsCamera() const { return fpsCamera_.get(); }

	// プレイヤーコントローラー取得
	PlayerController* GetPlayerController() const { return playerController_.get(); }

	// ワールド座標設定
	void SetWorldPosition(const Vector3& pos) { worldPosition_ = pos; }
	Vector3 GetWorldPosition() const { return worldPosition_; }

private: /// ---------- メンバ関数 ---------- ///

	// 各部位の初期化
	void InitializeBodyParts();

private: /// ---------- デバッグカメラフラグ ---------- ///

	Input* input_ = nullptr; // 入力クラス

	std::unique_ptr<Object3D> playerModel_; // プレイヤーモデル
	std::unique_ptr<PlayerController> playerController_; // プレイヤーコントローラー
	std::unique_ptr<FpsCamera> fpsCamera_; // FPSカメラ

	bool isDebugCamera_ = false; // デバッグカメラフラグ

	Vector3 worldPosition_ = { 0.0f, 0.0f, 0.0f }; // ワールド座標
};

