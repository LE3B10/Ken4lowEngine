#pragma once
#include "Collider.h"
#include "Object3D.h"
#include "WorldTransformEx.h"

#include <string>
#include <vector>

namespace K4E = ::Ken4lowEngine;

/// ---------- 前方宣言 ---------- ///
namespace Ken4lowEngine { class Camera; }

/// -------------------------------------------------------------
///					　キャラクター基底クラス
/// -------------------------------------------------------------
class BaseCharacter : public K4E::Collider
{
public: /// ---------- 構造体 ---------- ///

	/// ---------- 部位データ ---------- ///
	struct BodyPart
	{
		std::unique_ptr<K4E::Object3D> object; // 部位の3Dオブジェクト
		K4E::WorldTransformEx transform;		  // 部位のワールド変換情報
		bool active = true;				  // 描画/非描画
	};

	// 各部位のインデックス
	struct PartIndices
	{
		const uint32_t head = 0;	 // 頭
		const uint32_t leftArm = 1;  // 左腕
		const uint32_t rightArm = 2; // 右腕
		const uint32_t leftLeg = 3;	 // 左脚
		const uint32_t rightLeg = 4; // 右脚
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

	virtual void UpdateShadowMatrix(const K4E::Matrix4x4& lightViewProjection);

	virtual void DrawShadow();

	// 衝突判定を行う
	virtual void OnCollision(K4E::Collider* other) override = 0;

	// 体幹部位のワールド変換行列を取得
	const K4E::WorldTransformEx* GetWorldTransform() const { return &body_.transform; }

	// 中心座標を取得
	virtual K4E::Vector3 GetCenterPosition() const override;

	// 全部位にスキンを適用する静的関数
	static void ApplySkinTo(K4E::Object3D* obj, const std::string& texPath)
	{
		if (!obj) return; // nullチェック

		// 全サブメッシュを同じテクスチャに差し替える
		obj->SetTextureForAll(texPath);
	}

public: /// ---------- アクセッサ ---------- ///

	BodyPart& GetBody() { return body_; }

	std::vector<BodyPart>& GetBodyParts() { return parts_; }

	// 各部位のインデックスを取得
	PartIndices& GetPartIndices() { return partIndices_; }

protected: /// ---------- メンバ関数 ---------- ///

	// 体幹部位の描画/非描画設定
	void SetBodyActive(bool a) { body_.active = a; }

	// 全部位の描画/非描画設定
	void SetAllPartsActive(bool a) { for (auto& p : parts_) p.active = a; }

	// 指定部位の描画/非描画設定
	void SetPartActive(size_t i, bool a) { if (i < parts_.size()) parts_[i].active = a; }

	// 全部位にスキンを適用
	void ApplySkinToAllParts(const std::string& texPath)
	{
		// 体幹部位
		if (body_.object) {
			ApplySkinTo(body_.object.get(), texPath);
		}

		// 各部位
		for (auto& part : parts_) {
			if (part.object) {
				ApplySkinTo(part.object.get(), texPath);
			}
		}
	}

private: /// ---------- メンバ関数 ---------- ///

	// 階層更新
	void UpdateHierarchy();

protected: /// ---------- メンバ変数 ---------- ///

	// 体幹部位
	BodyPart body_;

	// 部位データ配列
	std::vector<BodyPart> parts_;

	// 各部位のインデックス
	PartIndices partIndices_ = {};
};

