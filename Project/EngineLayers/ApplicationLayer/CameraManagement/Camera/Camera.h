#pragma once
#include "WorldTransform.h"
#include "Matrix4x4.h"

/// -------------------------------------------------------------
///						　カメラクラス
/// -------------------------------------------------------------
class Camera
{
public: /// ---------- メンバ関数 ---------- ///

	virtual ~Camera() = default;

	// デフォルトコンストラクタ
	Camera();

	// 更新処理
	void Update();

	void DrawImGui();

public: /// ---------- セッター ---------- ///

	void SetRotate(const Vector3& rotate) { transform.rotate = rotate; }
	void SetTranslate(const Vector3& translate) { transform.translate = translate; }
	void SetFovY(const float fovY) { fovY_ = fovY; }
	void SetAspectRatio(const float aspectRatio) { aspectRatio_ = aspectRatio; }
	void SetNearClip(const float nearClip) { nearClip_ = nearClip; }
	void SetFarClip(const float farClip) { farClip_ = farClip; }
	void SetTarget(const Vector3& targetPosition) { target_ = targetPosition; }
	void SetTargetPosition(const Vector3& target) { targetPosition_ = target; }
	void SetFollowSpeed(float speed) { followSpeed_ = speed; }

public: /// ---------- ゲッター ---------- ///

	const Matrix4x4& GetWorldMatrix() const { return worldMatrix; }
	const Matrix4x4& GetViewMatrix() const { return viewMatrix; }
	const Matrix4x4& GetProjectionMatrix() const { return projectionMatrix; }
	const Matrix4x4& GetViewProjectionMatrix() const { return viewProjevtionMatrix; }
	const Vector3& GetRotate() const { return transform.rotate; }
	const Vector3& GetTranslate() const { return transform.translate; }


private: /// ---------- メンバ変数 ----- ///
	float DegreesToRadians(float degrees) {
		return degrees * (3.14159265359f / 180.0f); // π / 180
	}

	bool followPlayer = true;

	float kWidth, kHeight;

	// ビュー行列関連データ
	Transform transform;
	Matrix4x4 worldMatrix;
	Matrix4x4 viewMatrix;

	// プロジェクション行列関連データ
	Matrix4x4 projectionMatrix;
	float fovY_;		// 水平方向視野角
	float aspectRatio_; // アスペクト比
	float nearClip_;    // ニアクリップ
	float farClip_;	    // ファークリップ

	// 合成行列
	Matrix4x4 viewProjevtionMatrix;

	Vector3 target_; // カメラが注視するターゲットの位置

	Vector3 targetPosition_; // プレイヤーの現在位置
	float followSpeed_ = 0.05f; // カメラの追従速度
};

