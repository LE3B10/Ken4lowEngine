#pragma once
#include <Object3D.h>
#include <WorldTransform.h>
#include <Camera.h>

#include <vector>


/// ---------- 前方宣言 ---------- ///


/// ---------- 部位データ構造体 ---------- ///
struct BodyPart
{
	std::unique_ptr<Object3D> object;
	WorldTransform transform;
};


/// -------------------------------------------------------------
///					プレイヤークラス
/// -------------------------------------------------------------
class Player
{
public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize();
	
	// 更新処理
	void Update();
	
	// 描画処理
	void Draw();
	
public: /// ---------- ゲッタ ---------- ///

	// カメラの取得
	Camera* GetCamera() { return camera_.get(); }
	
	// ワールド変換の取得
	WorldTransform GetWorldTransform() { return worldTransform_; }

public: /// ---------- セッタ ---------- ///



private: /// ---------- メンバ変数 ---------- ///

	// カメラ
	std::unique_ptr<Camera> camera_;

	// ワールド変換
	WorldTransform worldTransform_;
	
	// 体（親）
	BodyPart body_;

	// 他の部位（子）
	std::vector<BodyPart> parts_;
};

