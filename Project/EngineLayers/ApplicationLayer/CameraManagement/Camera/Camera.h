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

public: /// ---------- セッター ---------- ///

	// スケールの設定
	void SetScale(const Vector3& scale) { worldTransform_.scale_ = scale; }

	// 回転の設定
	void SetRotate(const Vector3& rotate) { worldTransform_.rotate_ = rotate; }

	// 移動の設定
	void SetTranslate(const Vector3& translate) { worldTransform_.translate_ = translate; }

	// 水平方向視野角の設定
	void SetFovY(const float fovY) { fovY_ = fovY; }

	// アスペクト比の設定
	void SetAspectRatio(const float aspectRatio) { aspectRatio_ = aspectRatio; }

	// ニアクリップの設定
	void SetNearClip(const float nearClip) { nearClip_ = nearClip; }

	// ファークリップの設定
	void SetFarClip(const float farClip) { farClip_ = farClip; }

	// ビュー行列の設定
	void SetViewMatrix(const Matrix4x4& viewMatrix) { viewMatrix_ = viewMatrix; }

	// 射影行列の設定
	void SetProjectionMatrix(const Matrix4x4& projectionMatrix) { projectionMatrix_ = projectionMatrix; }

	// ビュー射影行列の設定
	void SetViewProjectionMatrix(const Matrix4x4& viewProjectionMatrix) { viewProjectionMatrix_ = viewProjectionMatrix; }

public: /// ---------- ゲッター ---------- ///

	// スケールの取得
	const Vector3& GetScale() const { return worldTransform_.scale_; }

	// 回転の取得
	const Vector3& GetRotate() const { return worldTransform_.rotate_; }

	// 移動の取得
	const Vector3& GetTranslate() const { return worldTransform_.translate_; }

	// ワールド行列データを取得
	const Matrix4x4& GetWorldMatrix() const { return worldMatrix_; }

	// ビュー行列データを取得
	const Matrix4x4& GetViewMatrix() const { return viewMatrix_; }

	// プロジェクション行列データを取得
	const Matrix4x4& GetProjectionMatrix() const { return projectionMatrix_; }

	// 合成行列データを取得
	const Matrix4x4& GetViewProjectionMatrix() const { return viewProjectionMatrix_; }

private: /// ---------- メンバ変数 ----- ///

	float kWidth, kHeight;

	// Transform情報
	WorldTransform worldTransform_;

	// ワールド行列データ
	Matrix4x4 worldMatrix_;

	// ビュー行列データ
	Matrix4x4 viewMatrix_;

	// プロジェクション行列データ
	Matrix4x4 projectionMatrix_;
	float fovY_;		   // 水平方向視野角
	float aspectRatio_; // アスペクト比
	float nearClip_;	   // ニアクリップ
	float farClip_;	   // ファークリップ

	// 合成行列
	Matrix4x4 viewProjectionMatrix_;

};

