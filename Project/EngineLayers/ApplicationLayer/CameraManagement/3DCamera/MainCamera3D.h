#pragma once
#include "Matrix4x4.h"
#include "WorldTransform.h"
#include "Vector3.h"

/// -------------------------------------------------------------
///						3D空間カメラクラス
/// -------------------------------------------------------------
class MainCamera3D
{
public: /// ---------- メンバ関数 ---------- ///

	// シングルトンインスタンス
	static MainCamera3D* GetInstance();

	// 初期化処理
	void Initialize();

	// 更新処理
	void Update();

	// ImGui描画処理
	void DrawImGui();

public: /// ---------- ゲッター ---------- ///

	Vector3 GetWorldPosition() const { return worldTransform_.translate; }
	Matrix4x4 GetWorldMatrix() const { return worldMatrix_; }
	Matrix4x4 GetViewMatrix() const { return viewMatirx_; }
	Matrix4x4 GetProjectionMatrix() { return projectionMatrix_; }

private: /// ---------- メンバ変数 ---------- ///

	Matrix4x4 worldMatrix_;
	Matrix4x4 viewMatirx_;
	Matrix4x4 projectionMatrix_;

	WorldTransform worldTransform_;
};

