#pragma once
#include "WorldTransform.h"
#include "Matrix4x4.h"

/// -------------------------------------------------------------
///						　カメラクラス
/// -------------------------------------------------------------
class Camera
{
public: /// ---------- メンバ関数 ---------- ///

	// デストラクタ
	virtual ~Camera() = default;

	// デフォルトコンストラクタ
	Camera();

	// 更新処理
	void Update();

	// ImGui描画処理
	void DrawImGui();

public: /// ---------- セッター ---------- ///

	// スケールの設定
	void SetScale(const Vector3& scale) { worldTransform.scale = scale; }

	// 回転の設定
	void SetRotate(const Vector3& rotate) { worldTransform.rotate = rotate; }

	// 移動の設定
	void SetTranslate(const Vector3& translate) { worldTransform.translate = translate; }

	// 水平方向視野角の設定
	void SetFovY(const float fovY) { fovY_ = fovY; }

	// アスペクト比の設定
	void SetAspectRatio(const float aspectRatio) { aspectRatio_ = aspectRatio; }

	// ニアクリップの設定
	void SetNearClip(const float nearClip) { nearClip_ = nearClip; }

	// ファークリップの設定
	void SetFarClip(const float farClip) { farClip_ = farClip; }

	void SetTarget(const Vector3& targetPosition) { target_ = targetPosition; }

	void SetTargetPosition(const Vector3& target) { targetPosition_ = target; }

	// カメラの回転を変更
	void Rotate(const Vector3& rotationDelta) { worldTransform.rotate += rotationDelta; }

public: /// ---------- ゲッター ---------- ///

	// スケールの取得
	const Vector3& GetScale() const { return worldTransform.scale; }

	// 回転の取得
	const Vector3& GetRotate() const { return worldTransform.rotate; }

	// 移動の取得
	const Vector3& GetTranslate() const { return worldTransform.translate; }

	// ワールド行列データを取得
	const Matrix4x4& GetWorldMatrix() const { return worldMatrix; }

	// ビュー行列データを取得
	const Matrix4x4& GetViewMatrix() const { return viewMatrix; }

	// プロジェクション行列データを取得
	const Matrix4x4& GetProjectionMatrix() const { return projectionMatrix; }

	// 合成行列データを取得
	const Matrix4x4& GetViewProjectionMatrix() const { return viewProjevtionMatrix; }

	// カメラを取得
	const Vector3& GetTarget() const { return target_; }

	// カメラの向きを取得
	Vector3 GetForwardDirection() const;

	float GetYaw() const { return yaw_; }

private: /// ---------- メンバ変数 ----- ///

	float kWidth, kHeight;

	// Transform情報
	WorldTransform worldTransform;

	// ワールド行列データ
	Matrix4x4 worldMatrix;

	// ビュー行列データ
	Matrix4x4 viewMatrix;

	// プロジェクション行列データ
	Matrix4x4 projectionMatrix;
	float fovY_;		   // 水平方向視野角
	float aspectRatio_; // アスペクト比
	float nearClip_;	   // ニアクリップ
	float farClip_;	   // ファークリップ

	float distance_ = 10.0f; // プレイヤーからの距離
	float yaw_ = 0.0f;       // 現在の Y軸回転角
	float pitch_ = 0.1f;     // 現在の X軸回転角

	float PI = 3.141592653589793246f;

	bool isViewChange_ = false;
	bool prevF5Pressed_ = false; // F5キーの前回の状態を記録

	// 合成行列
	Matrix4x4 viewProjevtionMatrix;

	Vector3 target_; // カメラが注視するターゲットの位置

	Vector3 targetPosition_; // プレイヤーの現在位置
};

