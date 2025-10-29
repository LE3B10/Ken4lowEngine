#pragma once
#include "Collider.h"
#include "Object3D.h"
#include "WorldTransformEx.h"

#include <string>
#include <vector>

/// ---------- 前方宣言 ---------- ///
class Camera;

/// -------------------------------------------------------------
///					　キャラクター基底クラス
/// -------------------------------------------------------------
class BaseCharacter : public Collider
{
protected: /// ---------- 構造体 ---------- ///

	/// ---------- 部位データ ---------- ///
	struct BodyPart
	{
		std::unique_ptr<Object3D> object; // 部位の3Dオブジェクト
		WorldTransformEx transform;		  // 部位のワールド変換情報
		bool active = true;				  // 描画/非描画
	};

public: /// ---------- メンバ関数 ---------- ///

	// デストラクタ
	virtual ~BaseCharacter() = default;

	// 初期化処理
	virtual void Initialize();

	// 更新処理
	virtual void Update(float deltaTime);

	// 描画処理
	virtual void Draw();

	// ImGui描画処理
	virtual void DrawImGui() = 0;

	// 衝突判定を行う
	virtual void OnCollision(Collider* other) override = 0;

	// 体幹部位のワールド変換行列を取得
	const WorldTransformEx* GetWorldTransform() const { return &body_.transform; }

	// 中心座標を取得
	virtual Vector3 GetCenterPosition() const override;

	// 全部位にスキンを適用する静的関数
	static void ApplySkinTo(Object3D* obj, const std::string& texPath)
	{
		if (!obj) return; // nullチェック

		// 全サブメッシュを同じテクスチャに差し替える
		obj->SetTextureForAll(texPath);
	}

protected: /// ---------- メンバ関数 ---------- ///

	// 体幹部位の描画/非描画設定
	void SetBodyActive(bool a) { body_.active = a; }

	// 全部位の描画/非描画設定
	void SetAllPartsActive(bool a) { for (auto& p : parts_) p.active = a; }

	// 指定部位の描画/非描画設定
	void SetPartActive(size_t i, bool a) { if (i < parts_.size()) parts_[i].active = a; }

private: /// ---------- メンバ関数 ---------- ///

	// 階層更新
	void UpdateHierarchy();

protected: /// ---------- メンバ変数 ---------- ///

	// 体幹部位
	BodyPart body_;

	// 部位データ配列
	std::vector<BodyPart> parts_;
};

