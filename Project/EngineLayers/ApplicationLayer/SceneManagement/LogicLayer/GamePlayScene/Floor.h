#pragma once
#include <Object3D.h>
#include <WorldTransform.h>
#include <vector>

/// ---------- 前方宣言 ---------- ///
class Object3DCommon;
class Camera;

/// -------------------------------------------------------------
///                     床の管理クラス
/// -------------------------------------------------------------
class Floor
{
public: /// ---------- 構造体 ---------- ///

	struct Tile
	{
		Transform transform; // 床の位置や回転を保持
		int laneIndex;		 // レーンのインデックス｛-1: 左, 0： 中央, 1: 右｝
		float theta;
	};

public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize(Object3DCommon* object3DCommon, int tileCount, float startZ, float tileSpacing,float laneWidth);

	// 更新処理
	void Update(float scrollSpeed, const Camera* camera);

	// 描画処理
	void Draw(const Camera* camera);

	float GetFloorHeightAt(float x, float z) const; // 指定した位置の床の高さを取得

private: /// ---------- メンバ変数 ---------- ///

	Camera* camera = nullptr;

	std::vector<std::unique_ptr<Object3D>> floorObjects_; // 床の3Dオブジェクト
	std::vector<Tile> tiles_; // 各床タイルの情報
	float radius_ = 0.0f;
};

