#pragma once
#include <Object3D.h>
#include <WorldTransform.h>

#include <memory>
#include <string>

class Weapon
{
public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize(const std::string& modelPath);

	// 更新処理
	void Update();

	// 描画処理
	void Draw();

public: /// ---------- ゲッタ ---------- ///

	// ワールド変換を取得
	const WorldTransform* GetWorldTransform() { return &worldTransform_; }

	// 座標を取得
	Vector3 GetTranslate() const { return worldTransform_.translate_; }

	// 回転を取得
	Vector3 GetRotate() const { return worldTransform_.rotate_; }

	// スケールを取得
	Vector3 GetScale() const { return worldTransform_.scale_; }

public: /// ---------- セッタ ---------- ///

	// 右腕への装備位置を設定
	void SetParentTransform(const WorldTransform* parent) { parentTransform_ = parent; }

	// 座標を設定
	void SetTranslate(const Vector3& translate) { worldTransform_.translate_ = translate; }

	// 回転を設定
	void SetRotate(const Vector3& rotate) { worldTransform_.rotate_ = rotate; }

	// スケールを設定
	void SetScale(const Vector3& scale) { worldTransform_.scale_ = scale; }

private: /// ---------- メンバ変数 ---------- ///

	std::unique_ptr<Object3D> object_; // 武器のオブジェクト
	WorldTransform worldTransform_; // 武器のワールド変換
	const WorldTransform* parentTransform_ = nullptr; // 親のワールド変換
	Vector3 offset_ = { 3.5f, 0.0f, 0.0f };
};

