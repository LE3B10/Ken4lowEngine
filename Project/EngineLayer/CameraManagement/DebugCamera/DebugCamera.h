#pragma once
#include "WorldTransform.h"
#include "Quaternion.h"


/// -------------------------------------------------------------
///						デバッグカメラクラス
/// -------------------------------------------------------------
class DebugCamera
{
public: /// ---------- メンバ関数 ---------- ///

	// シングルインスタンス
	static DebugCamera* GetInstance();

	// 初期化処理
	void Initialize();

	// 更新処理
	void Update();

private: /// ---------- メンバ関数 ---------- ///

	// 移動操作処理
	void Move();

	// 回転操作処理
	void UpdateViewProjection();

public: /// ---------- 設定 ---------- ///

	// ビュー行列の設定
	void SetViewMatrix(const Matrix4x4& viewMatrix) { viewMatrix_ = viewMatrix; }

	// 射影行列の設定
	void SetProjectionMatrix(const Matrix4x4& projectionMatrix) { projectionMatrix_ = projectionMatrix; }

	// ビュー射影行列の設定
	void SetViewProjectionMatrix(const Matrix4x4& viewProjectionMatrix) { viewProjectionMatrix_ = viewProjectionMatrix; }

	// 回転角の設定
	void SetRotate(const Vector3& rotate) { worldTransform_.rotate_ = rotate; }

	// 座標を設定
	void SetTranslate(const Vector3& translate) { worldTransform_.translate_ = translate; }

	// 水平方向視野角の設定
	void SetFovY(float fovY) { fovY_ = fovY; }

	// アスペクト比の設定
	void SetAspectRatio(float aspectRatio) { aspectRatio_ = aspectRatio; }

	// ニアクリップの設定
	void SetNearClip(float nearClip) { nearClip_ = nearClip; }

	// ファークリップの設定
	void SetFarClip(float farClip) { farClip_ = farClip; }

public: /// ---------- 取得 ---------- ///

	// ビュー行列を取得
	Matrix4x4 GetViewMatrix() const { return viewMatrix_; }

	// 射影行列を取得
	Matrix4x4 GetProjectionMatrix() const { return projectionMatrix_; }

	// ビュー射影行列を取得
	Matrix4x4 GetViewProjectionMatrix() const { return viewProjectionMatrix_; }

	// 回転角を取得
	Vector3 GetRotate() const { return worldTransform_.rotate_; }

	// 座標を取得
	Vector3 GetTranslate() const { return worldTransform_.translate_; }

private: /// ---------- メンバ変数 ---------- ///

	// ワールドトランスフォーム
	WorldTransform worldTransform_;

	// ワールド行列データ
	Matrix4x4 worldMatrix_;

	// 回転行列
	Matrix4x4 rotateMatrix_;

	// ビュー行列データ
	Matrix4x4 viewMatrix_;

	// プロジェクション行列データ
	Matrix4x4 projectionMatrix_;
	float fovY_ = 0.0f;		   // 水平方向視野角
	float aspectRatio_ = 0.0f; // アスペクト比
	float nearClip_ = 0.0f;    // ニアクリップ
	float farClip_ = 0.0f;	   // ファークリップ

	// 合成行列
	Matrix4x4 viewProjectionMatrix_;

	// クォータニオン
	Quaternion rotation_{};

private: /// ---------- コピー禁止 ---------- ///

	DebugCamera() = default;
	~DebugCamera() = default;
	DebugCamera(const DebugCamera&) = delete;
	DebugCamera& operator=(const DebugCamera&) = delete;
};

