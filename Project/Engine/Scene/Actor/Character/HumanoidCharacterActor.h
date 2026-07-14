#pragma once

#include "CharacterActor.h"
#include "HumanoidVisualComponent.h"

#include <cstdint>
#include <string>

namespace Ken4lowEngine
{
	/// Character共通ComponentとHumanoidVisualComponentだけで人型キャラクターを構成するActor。
	class HumanoidCharacterActor : public CharacterActor
	{
	public:
		using BodyPart = HumanoidVisualComponent::BodyPart;

		/// 部位配列はHumanoidVisualComponentの定義順に従う。標準定義ではBodyが0番、以降が子部位となる。
		struct PartIndices
		{
			uint32_t body = 0;
			uint32_t head = 1;
			uint32_t leftArm = 2;
			uint32_t rightArm = 3;
			uint32_t leftLeg = 4;
			uint32_t rightLeg = 5;
		};

		/// Character共通Componentを生成した後、人型表示Componentを1つだけ接続する。
		void Initialize() override
		{
			CharacterActor::Initialize();
			HumanoidVisualComponent* visual = GetHumanoidVisualComponent();
			if (!visual)
			{
				visual = &AddComponent<HumanoidVisualComponent>();
				visual->SetName("Humanoid Visual");
				visual->SetUpdateOrder(-40);
				visual->SetDrawOrder(0);
				visual->SetCastShadowEnabled(true);
				if (SceneComponent* root = GetRootComponent()) visual->AttachTo(root);
			}
			visual->InitializeForWorld(); // 直接Initializeされる旧GamePlayWorld経路でも部位を即時利用できるようにする。
		}

		std::string GetClassTypeName() const override { return "HumanoidCharacterActor"; }

		HumanoidVisualComponent* GetHumanoidVisualComponent() { return GetCharacterComponent<HumanoidVisualComponent>(); }
		const HumanoidVisualComponent* GetHumanoidVisualComponent() const { return GetCharacterComponent<HumanoidVisualComponent>(); }

		BodyPart& GetBody()
		{
			HumanoidVisualComponent* visual = GetHumanoidVisualComponent();
			BodyPart* body = visual ? visual->FindPart("Body") : nullptr;
			if (body) return *body;
			return GetBodyParts().front();
		}

		const BodyPart& GetBody() const
		{
			const HumanoidVisualComponent* visual = GetHumanoidVisualComponent();
			const BodyPart* body = visual ? visual->FindPart("Body") : nullptr;
			if (body) return *body;
			return GetBodyParts().front();
		}

		std::vector<BodyPart>& GetBodyParts()
		{
			return GetHumanoidVisualComponent()->GetParts(); // 部位データの所有権はHumanoidVisualComponentだけが持つ。
		}

		const std::vector<BodyPart>& GetBodyParts() const
		{
			return GetHumanoidVisualComponent()->GetParts();
		}

		PartIndices& GetPartIndices() { return partIndices_; }
		const PartIndices& GetPartIndices() const { return partIndices_; }

		WorldTransformEx* GetWorldTransform() { return &GetBody().transform; }
		const WorldTransformEx* GetWorldTransform() const { return &GetBody().transform; }

		Vector3 GetCenterPosition() const
		{
			if (const SceneComponent* root = GetRootComponent()) return root->GetWorldPosition();
			return GetBody().transform.worldTranslate_;
		}

		void SetBodyActive(bool active)
		{
			if (HumanoidVisualComponent* visual = GetHumanoidVisualComponent()) visual->SetPartVisible("Body", active);
		}

		void SetAllPartsActive(bool active)
		{
			if (HumanoidVisualComponent* visual = GetHumanoidVisualComponent()) visual->SetAllPartsVisible(active);
		}

		void SetPartActive(size_t index, bool active)
		{
			HumanoidVisualComponent* visual = GetHumanoidVisualComponent();
			if (!visual || index >= visual->GetParts().size()) return;
			visual->SetPartVisible(visual->GetParts()[index].id, active);
		}

		void ApplySkinToAllParts(const std::string& texturePath)
		{
			if (HumanoidVisualComponent* visual = GetHumanoidVisualComponent()) visual->ApplySkinToAllParts(texturePath);
		}

		static void ApplySkinTo(Object3D* object, const std::string& texturePath)
		{
			if (object) object->SetTextureForAll(texturePath);
		}

		void UpdateShadowMatrix(const Matrix4x4& lightViewProjection)
		{
			if (HumanoidVisualComponent* visual = GetHumanoidVisualComponent()) visual->UpdateShadowMatrices(lightViewProjection);
		}

	private:
		PartIndices partIndices_{}; // インデックスはComponent所有配列への参照規約だけを保持し、部位実体は複製しない。
	};
} // namespace Ken4lowEngine
