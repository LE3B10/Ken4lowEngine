#pragma once
#include "Collider.h"
#include "Object3D.h"

#include <string>
#include <vector>

/// -------------------------------------------------------------
///					　キャラクター基底クラス
/// -------------------------------------------------------------
class BaseCharacter : public Collider
{
protected: /// ---------- 構造体 ---------- ///

	/// ---------- 部位データ ---------- ///
	struct BodyPart
	{
		std::unique_ptr<Object3D> object; // 部位モデル

		// 階層
		int parentIndex = -1;         // 親部位のインデックス（-1なら親なし）
		std::string name;			  // 部位名

		Vector3 traslate;             // 部位の位置
		Vector3 rotate;               // 部位の回転
		Vector3 scale;                // 部位のスケール

		Matrix4x4 localMatrix;      // ローカル変換行列
		Matrix4x4 worldMatrix;      // ワールド変換行列

		Vector3 worldRotate{ 0,0,0 };
		Vector3 worldScale{ 1,1,1 };

		// 可視化描画レイヤ
		bool visible = true;      // 可視化フラグ
	};

	std::vector<BodyPart> parts_; // 部位データ配列

	// 階層更新
	void UpdateHierarchy();

public: /// ---------- メンバ関数 ---------- ///

	// デストラクタ
	virtual ~BaseCharacter() = default;

	// 初期化処理
	virtual void Initialize() = 0;

	// 更新処理
	virtual void Update() = 0;

	// 描画処理
	virtual void Draw() = 0;

	// ImGui描画処理
	virtual void DrawImGui() = 0;

	// 衝突判定を行う
	virtual void OnCollision(Collider* other) override = 0;

	// 中心座標を取得
	virtual Vector3 GetCenterPosition() const override;
};

