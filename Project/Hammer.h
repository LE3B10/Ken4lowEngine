#pragma once
#include <Object3D.h>


/// -------------------------------------------------------------
///						　ハンマークラス
/// -------------------------------------------------------------
class Hammer
{
public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize();

	// 更新処理
	void Update();

	// 描画処理
	void Draw();

public: /// ---------- ゲッター ---------- ///

	// WorldTransformの取得
	WorldTransform& GetWorldTransform() { return worldTransform_; }

	// 回転を取得
	const Vector3& GetRotation() const { return worldTransform_.rotate_; }

	// 座標を取得
	const Vector3& GetTranslate() const { return worldTransform_.translation_; }

public: /// ---------- セッター ---------- ///

	// 親Transformを設定
	void SetParentTransform(const WorldTransform* parent) { parentTransform_ = parent; }

	// 回転を設定
	void SetRotation(const Vector3& rotation) { worldTransform_.rotate_ = rotation; }

	// 座標を設定
	void SetTranslate(const Vector3& translate) { worldTransform_.translation_ = translate; }

private: /// ---------- メンバ変数 ---------- ///

	std::unique_ptr<Object3D> object_;
	WorldTransform worldTransform_;
	const WorldTransform* parentTransform_ = nullptr;

	Vector3 offset_ = { 0.0f, 4.5f,0.0f };
};

