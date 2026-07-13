#include "BaseCharacter.h"

#include <array>
#include <memory>
#include <utility>

using namespace Ken4lowEngine;

BaseCharacter::~BaseCharacter()
{
	if (humanoidVisualComponent_) humanoidVisualComponent_->FinalizeForWorld(); // 借用中の部位より先にAdapter接続を解除する。
}

/// -------------------------------------------------------------
///				　			　 更新処理
/// -------------------------------------------------------------
void BaseCharacter::Initialize()
{
	BuildBodyHierarchy(
		{ "Characters/body.gltf", { 0.0f, 2.25f, 0.0f }, {}, { 1.0f, 1.0f, 1.0f } },
		{
			{ "Characters/head.gltf", { 0.0f, 0.75f, 0.0f } },
			{ "Characters/left_arm.gltf", { -0.75f, 0.75f, 0.0f } },
			{ "Characters/right_arm.gltf", { 0.75f, 0.75f, 0.0f } },
			{ "Characters/left_leg.gltf", { -0.25f, -0.75f, 0.0f } },
			{ "Characters/right_leg.gltf", { 0.25f, -0.75f, 0.0f } },
		});
}

void BaseCharacter::BuildBodyHierarchy(
	const BodyPartDefinition& bodyDefinition,
	const std::vector<BodyPartDefinition>& partDefinitions)
{
	if (humanoidVisualComponent_) humanoidVisualComponent_->FinalizeForWorld();
	parts_.clear();
	body_ = {};
	body_.id = "Body";
	body_.object = std::make_unique<Object3D>();
	body_.object->Initialize(bodyDefinition.modelPath);
	body_.transform = {};
	body_.transform.translate_ = bodyDefinition.localPosition;
	body_.transform.rotate_ = bodyDefinition.localRotation;
	body_.transform.scale_ = bodyDefinition.scale;
	body_.active = true;
	body_.visible = true;
	body_.object->SetTranslate(bodyDefinition.localPosition);
	body_.object->SetRotate(bodyDefinition.localRotation);
	body_.object->SetScale(bodyDefinition.scale);

	static constexpr std::array<const char*, 5> kCompatibilityPartIds = {
		"Head", "LeftArm", "RightArm", "LeftLeg", "RightLeg"
	};
	parts_.reserve(partDefinitions.size());
	for (size_t partIndex = 0; partIndex < partDefinitions.size(); ++partIndex)
	{
		const auto& definition = partDefinitions[partIndex];
		BodyPart part{};
		part.id = partIndex < kCompatibilityPartIds.size() ? kCompatibilityPartIds[partIndex] : "Part" + std::to_string(partIndex);
		part.parentId = "Body";
		part.object = std::make_unique<Object3D>();
		part.object->Initialize(definition.modelPath);
		part.transform.translate_ = definition.localPosition;
		part.transform.rotate_ = definition.localRotation;
		part.transform.scale_ = definition.scale;
		part.transform.parent_ = &body_.transform;
		part.object->SetTranslate(definition.localPosition);
		part.object->SetRotate(definition.localRotation);
		part.object->SetScale(definition.scale);
		parts_.push_back(std::move(part));
	}

	if (!humanoidVisualComponent_) humanoidVisualComponent_ = std::make_unique<HumanoidVisualComponent>();
	humanoidVisualComponent_->BindCompatibilityHierarchy(body_, parts_);
	humanoidVisualComponent_->InitializeForWorld(); // 以降は旧階層更新を止め、Componentだけを実行する。
}

/// -------------------------------------------------------------
///				　			　 更新処理
/// -------------------------------------------------------------
void BaseCharacter::Update(float deltaTime)
{
	if (humanoidVisualComponent_)
	{
		humanoidVisualComponent_->Update(deltaTime);
		return; // 新旧の階層更新を同一フレームで重ねない。
	}
	UpdateHierarchy();
}

/// -------------------------------------------------------------
///				　			　 描画処理
/// -------------------------------------------------------------
void BaseCharacter::Draw()
{
	if (humanoidVisualComponent_)
	{
		humanoidVisualComponent_->Draw();
		return; // Adapter接続後は旧Drawを実行しない。
	}
	// 体幹部位の描画
	if (body_.active && body_.object)
	{
		body_.object->Draw();
	}
	// 各部位の描画
	for (const auto& part : parts_)
	{
		if (part.active && part.object)
		{
			part.object->Draw();
		}
	}
}

/// -------------------------------------------------------------
///				　	シャドウマトリクスの更新
/// -------------------------------------------------------------
void BaseCharacter::UpdateShadowMatrix(const Matrix4x4& lightViewProjection)
{
	if (humanoidVisualComponent_)
	{
		humanoidVisualComponent_->UpdateShadowMatrices(lightViewProjection);
		return;
	}
	if (body_.object) body_.object->UpdateShadowMatrix(lightViewProjection);
	for (const auto& part : parts_)
	{
		if (part.active && part.object)
		{
			part.object->UpdateShadowMatrix(lightViewProjection);
		}
	}
}

/// -------------------------------------------------------------
///				　		シャドウ描画処理
/// -------------------------------------------------------------
void BaseCharacter::DrawShadow()
{
	if (humanoidVisualComponent_)
	{
		humanoidVisualComponent_->DrawShadow();
		return; // Shadowも通常描画と同じComponent経路だけを使う。
	}
	// 体幹部位のシャドウ描画
	if (body_.object) body_.object->DrawShadow();

	// 各部位のシャドウ描画
	for (const auto& part : parts_)
	{
		if (part.active && part.object)
		{
			part.object->DrawShadow();
		}
	}
}

/// -------------------------------------------------------------
///				　			スキン適用
/// -------------------------------------------------------------
void BaseCharacter::ApplySkinToAllParts(const std::string& texPath)
{
	if (humanoidVisualComponent_)
	{
		humanoidVisualComponent_->ApplySkinToAllParts(texPath);
		return; // 共有Material/Texture処理をComponentへ集約する。
	}
	// 体幹部位
	if (body_.object) ApplySkinTo(body_.object.get(), texPath);

	// 各部位
	for (auto& part : parts_)
	{
		// 描画されている部位にのみスキンを適用
		if (part.object && part.active)
		{
			ApplySkinTo(part.object.get(), texPath);
		}
	}
}

/// -------------------------------------------------------------
///				　	スキン適用（静的関数）
/// -------------------------------------------------------------
void BaseCharacter::ApplySkinTo(Object3D* obj, const std::string& texPath)
{
	if (!obj) return; // nullチェック

	// 全サブメッシュを同じテクスチャに差し替える
	obj->SetTextureForAll(texPath);
}

/// -------------------------------------------------------------
///				　			階層更新
/// -------------------------------------------------------------
void BaseCharacter::UpdateHierarchy()
{
	// 体のワールド変換を更新
	body_.transform.Update();
	body_.object->SetTranslate(body_.transform.translate_); // 位置を適用
	body_.object->SetRotate(body_.transform.rotate_);		// 回転を適用
	body_.object->Update();									// オブジェクト更新

	// 各部位のワールド変換を更新
	for (auto& part : parts_)
	{
		// 親の回転を適用
		part.transform.worldRotate_ = body_.transform.worldRotate_;

		// 親の影響を受ける
		part.transform.Update();

		// ワールド変換をオブジェクトに適用
		if (part.transform.useQuaternionRotation_)
		{
			// ワールド行列を直接オブジェクトに適用
			part.object->UpdateWithWorldMatrix(part.transform.worldMatrix_);
		}
		else
		{
			part.object->SetTranslate(part.transform.worldTranslate_); // ワールド座標を適用
			part.object->SetRotate(part.transform.worldRotate_);	   // ワールド回転を適用
			part.object->Update();									   // オブジェクト更新
		}
	}
}

/// -------------------------------------------------------------
///					　	中心座標を取得
/// -------------------------------------------------------------
Vector3 BaseCharacter::GetCenterPosition() const
{
	// ローカル座標でのオフセット
	const Vector3 offset = { 0.0f, 0.0f, 0.0f };

	// ワールド座標に変換
	Vector3 worldPosition = GetBody().transform.translate_ + offset;

	// ワールド回転を考慮してオフセットを回転させる
	return worldPosition;
}

BaseCharacter::BodyPart& BaseCharacter::GetBody()
{
	if (humanoidVisualComponent_)
	{
		if (BodyPart* body = humanoidVisualComponent_->GetCompatibilityBody()) return *body;
	}
	return body_;
}

const BaseCharacter::BodyPart& BaseCharacter::GetBody() const
{
	if (humanoidVisualComponent_)
	{
		if (const BodyPart* body = humanoidVisualComponent_->GetCompatibilityBody()) return *body;
	}
	return body_;
}

std::vector<BaseCharacter::BodyPart>& BaseCharacter::GetBodyParts()
{
	if (humanoidVisualComponent_)
	{
		if (auto* parts = humanoidVisualComponent_->GetCompatibilityParts()) return *parts;
	}
	return parts_;
}

const std::vector<BaseCharacter::BodyPart>& BaseCharacter::GetBodyParts() const
{
	if (humanoidVisualComponent_)
	{
		if (const auto* parts = humanoidVisualComponent_->GetCompatibilityParts()) return *parts;
	}
	return parts_;
}

void BaseCharacter::SetBodyActive(bool active)
{
	BodyPart& body = GetBody();
	body.active = active;
	body.visible = active;
}

void BaseCharacter::SetAllPartsActive(bool active)
{
	for (BodyPart& part : GetBodyParts())
	{
		part.active = active;
		part.visible = active;
	}
}

void BaseCharacter::SetPartActive(size_t index, bool active)
{
	auto& parts = GetBodyParts();
	if (index >= parts.size()) return;
	parts[index].active = active;
	parts[index].visible = active; // 旧active APIとComponent表示状態を常に一致させる。
}
