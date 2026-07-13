#pragma once
#include "Collider.h"
#include <Scene/Actor/Character/HumanoidVisualComponent.h>
#include "WorldTransformEx.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace K4E = ::Ken4lowEngine;

/// -------------------------------------------------------------
///					　キャラクター基底クラス
/// -------------------------------------------------------------
class BaseCharacter : public K4E::Collider
{
public: /// ---------- 構造体 ---------- ///

	/// 旧APIの型を共通Componentの部位型へ合わせ、移行中の参照互換性を保つ。
	using BodyPart = K4E::HumanoidVisualComponent::BodyPart;

	/// モデル、ローカル姿勢、スケールをまとめた部位生成用の定義。
	struct BodyPartDefinition
	{
		std::string modelPath;
		K4E::Vector3 localPosition{};
		K4E::Vector3 localRotation{};
		K4E::Vector3 scale{ 1.0f, 1.0f, 1.0f };
	};

	/// ---------- 各部位のインデックス ---------- ///
	struct PartIndices
	{
		uint32_t head = 0;	   // 頭
		uint32_t leftArm = 1;  // 左腕
		uint32_t rightArm = 2; // 右腕
		uint32_t leftLeg = 3;  // 左脚
		uint32_t rightLeg = 4; // 右脚
	};

public: /// ---------- メンバ関数 ---------- ///

	// デストラクタ
	virtual ~BaseCharacter();

	// 初期化処理
	virtual void Initialize();

	// 更新処理
	virtual void Update(float deltaTime);

	// 描画処理
	virtual void Draw();

	// ImGui描画処理
	virtual void DrawImGui() = 0;

	// シャドウマトリクスの更新
	virtual void UpdateShadowMatrix(const K4E::Matrix4x4& lightViewProjection);

	// シャドウ描画処理
	virtual void DrawShadow();

	// 衝突判定を行う
	virtual void OnCollision(K4E::Collider* other) override = 0;

public: /// ---------- アクセッサ ---------- ///

	// 中心座標を取得
	virtual K4E::Vector3 GetCenterPosition() const override;

	// 体幹部位のワールド変換行列を取得
	const K4E::WorldTransformEx* GetWorldTransform() const { return &GetBody().transform; }

	// 体幹部位のアクセス
	BodyPart& GetBody();

	// 体幹部位のアクセス（const版）
	const BodyPart& GetBody() const;

	// 各部位のアクセス
	std::vector<BodyPart>& GetBodyParts();

	// 各部位のアクセス（const版）
	const std::vector<BodyPart>& GetBodyParts() const;

	// 各部位のインデックスを取得
	PartIndices& GetPartIndices() { return partIndices_; }

	// 各部位のインデックスを取得（const版）
	const PartIndices& GetPartIndices() const { return partIndices_; }

	// 体幹部位の描画/非描画設定
	void SetBodyActive(bool active);

	// 全部位の描画/非描画設定
	void SetAllPartsActive(bool active);

	// 指定部位の描画/非描画設定
	void SetPartActive(size_t index, bool active);

	/// Adapter接続中の共通人型表示Componentを返す。
	K4E::HumanoidVisualComponent* GetHumanoidVisualComponent() { return humanoidVisualComponent_.get(); }

	/// Adapter接続中の共通人型表示Componentを返すconst版。
	const K4E::HumanoidVisualComponent* GetHumanoidVisualComponent() const { return humanoidVisualComponent_.get(); }

public: /// ---------- スキン適用 ---------- ///

	// 全部位にスキンを適用
	void ApplySkinToAllParts(const std::string& texPath);
	
	// 全部位にスキンを適用する静的関数
	static void ApplySkinTo(K4E::Object3D* obj, const std::string& texPath);

protected:
	/// 胴体と子部位を同じ生成経路で構築し、子部位を胴体Transformへ接続する。
	void BuildBodyHierarchy(const BodyPartDefinition& bodyDefinition, const std::vector<BodyPartDefinition>& partDefinitions);

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

	// 旧部位の所有権を維持しつつ更新・描画を単一経路へ委譲する移行用Component。
	std::unique_ptr<K4E::HumanoidVisualComponent> humanoidVisualComponent_;
};
