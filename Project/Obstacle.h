#pragma once
#include <Object3D.h>
#include <Transform.h>

/// ---------- 前方宣言 ---------- ///
class Camera;


/// -------------------------------------------------------------
///				　	障害物を生成するクラス
/// -------------------------------------------------------------
class Obstacle
{
public: /// --------- メンバ関数 ---------- ///

	// デフォルトコンストラクタ
	Obstacle() = default;

	// ムーブコンストラクタとムーブ代入演算子
	Obstacle(Obstacle&& other) noexcept = default;
	Obstacle& operator=(Obstacle&& other) noexcept = default;

	// コピーコンストラクタとコピー代入演算子を削除
	Obstacle(const Obstacle&) = delete;
	Obstacle& operator=(const Obstacle&) = delete;

	// 初期化処理
	void Initialize(Object3DCommon* object3DCommon, const std::string& modelFile, const Transform& initialTransform);

	// 更新処理
	void Update(float scrollSpeed);

	// 描画処理
	void Draw(const Camera* camera);

	// 当たり判定用の座標を取得
	const Transform& GetTransform() const { return transform_; }  // const オブジェクト用
	Transform& GetTransform() { return transform_; }              // 非 const オブジェクト用

private: /// ---------- メンバ変数 ---------- ///
	
	std::unique_ptr<Object3D> obstacleObject_;
	Transform transform_;

};

