#include "ComponentFactory.h"

#include "Actor.h"
#include "BillboardComponent.h"
#include "ModelComponent.h"
#include "CameraComponent.h"
#include "ColliderComponent.h"
#include "GaugeComponent.h"
#include "RigidbodyComponent.h"
#include "InstancedModelComponent.h"
#include "LightComponent.h"
#include "SpriteComponent.h"
#include "TextComponent.h"
#include "WorldGaugeComponent.h"
#include "WorldSpriteComponent.h"
#include "WorldTextComponent.h"

#include <type_traits>
#include <utility>

namespace Ken4lowEngine
{
	namespace
	{
		template<class T>
		ComponentFactory::ComponentTypeInfo MakeComponentTypeInfo(std::string className, bool allowMultiple, std::string displayName, std::string category, std::string description)
		{
			static_assert(std::is_base_of_v<ActorComponent, T>, "T must inherit from ActorComponent.");

			ComponentFactory::ComponentTypeInfo typeInfo{};
			typeInfo.className = std::move(className);
			typeInfo.displayName = std::move(displayName);
			typeInfo.category = std::move(category);
			typeInfo.description = std::move(description);
			typeInfo.allowMultiple = allowMultiple;
			typeInfo.canBeRoot = std::is_base_of_v<SceneComponent, T>;
			typeInfo.createFunc = [](Actor* owner) -> ActorComponent*
				{
					if (!owner)
					{
						return nullptr; // 所有Actorが無い場合はComponentを生成できない
					}

					return &owner->AddComponent<T>(); // ComponentをActorに追加する
				};
			typeInfo.createRootFunc = [](Actor* owner) -> SceneComponent*
				{
					if (!owner)
					{
						return nullptr; // 所有Actorが無い場合はComponentを生成できない
					}

					if constexpr (std::is_base_of_v<SceneComponent, T>)
					{
						return &owner->CreateRootComponent<T>(); // ComponentをRootComponentとして生成する
					}
					else
					{
						return nullptr; // SceneComponentではないComponentはRootにできない
					}
				};

			return typeInfo;
		}

		const std::vector<ComponentFactory::ComponentTypeInfo> kRegisteredComponentTypes =
		{
			MakeComponentTypeInfo<SceneComponent>(
				"SceneComponent",
				true,
				"シーンコンポーネント",
				"トランスフォーム",
				"位置・回転・スケールと親子関係を持つ基本Componentです。"),
			MakeComponentTypeInfo<ModelComponent>(
				"ModelComponent",
				true,
				"モデルコンポーネント",
				"描画",
				"Actorに3Dモデルの描画機能を追加します。"),
				MakeComponentTypeInfo<BillboardComponent>(
					"BillboardComponent",
					true,
					"ビルボードコンポーネント",
					"描画",
					"3D空間上で常にカメラ方向を向く板ポリ表示用Componentです。"),
				MakeComponentTypeInfo<CameraComponent>(
					"CameraComponent",
					false,
					"カメラコンポーネント",
					"カメラ",
					"Actorを視点として使うためのカメラ機能を追加します。"),
				MakeComponentTypeInfo<ColliderComponent>(
					"ColliderComponent",
					true,
					"コライダーコンポーネント",
					"物理",
					"Actorに衝突判定用の形状と当たり判定設定を追加します。"),
				MakeComponentTypeInfo<RigidbodyComponent>(
					"RigidbodyComponent",
					false,
					"剛体コンポーネント",
					"物理",
					"Actorに速度や重力などの物理挙動を追加します。"),
				MakeComponentTypeInfo<InstancedModelComponent>(
					"InstancedModelComponent",
					true,
					"インスタンスモデルコンポーネント",
					"描画",
					"同じモデルを複数描画するためのインスタンシング機能を追加します。"),
				MakeComponentTypeInfo<LightComponent>(
					"LightComponent",
					true,
					"ライトコンポーネント",
					"描画",
					"Actorにライト情報を持たせるためのComponentです。"),
				MakeComponentTypeInfo<SpriteComponent>(
					"SpriteComponent",
					true,
					"スプライトコンポーネント",
					"描画",
					"画面上に2D画像を表示するためのComponentです。"),
				MakeComponentTypeInfo<WorldSpriteComponent>(
					"WorldSpriteComponent",
					true,
					"ワールドスプライトコンポーネント",
					"描画",
					"Actorの3D位置を画面座標へ変換してSpriteを表示するComponentです。"),
				MakeComponentTypeInfo<TextComponent>(
					"TextComponent",
					true,
					"テキストコンポーネント",
					"UI",
					"画面上に文字を表示するためのComponentです。"),
				MakeComponentTypeInfo<WorldTextComponent>(
					"WorldTextComponent",
					true,
					"ワールドテキストコンポーネント",
					"UI",
					"Actorの3D位置を画面座標へ変換して文字を表示するComponentです。"),
				MakeComponentTypeInfo<GaugeComponent>(
					"GaugeComponent",
					true,
					"ゲージコンポーネント",
					"UI",
					"画面上にHPバーや進行度バーを表示するためのComponentです。"),
				MakeComponentTypeInfo<WorldGaugeComponent>(
					"WorldGaugeComponent",
					true,
					"ワールドゲージコンポーネント",
					"UI",
					"Actorの3D位置を画面座標へ変換してゲージを表示するComponentです。"),
		};
	}

	ActorComponent* ComponentFactory::CreateComponent(Actor* owner, std::string_view className)
	{
		if (!owner)
		{
			return nullptr; // Actorがnullptrの場合はComponentを生成しない
		}

		const ComponentTypeInfo* typeInfo = FindComponentType(className);

		if (!typeInfo)
		{
			return nullptr; // 未登録のComponentClass名の場合は生成しない
		}

		return typeInfo->createFunc(owner);
	}

	SceneComponent* ComponentFactory::CreateRootSceneComponent(Actor* owner, std::string_view className)
	{
		if (!owner)
		{
			return nullptr; // Actorがnullptrの場合はComponentを生成しない
		}

		const ComponentTypeInfo* typeInfo = FindComponentType(className);

		if (!typeInfo || !typeInfo->canBeRoot)
		{
			return nullptr; // RootComponentとして使用できるSceneComponentか確認する
		}

		return typeInfo->createRootFunc(owner);
	}

	const std::vector<ComponentFactory::ComponentTypeInfo>& ComponentFactory::GetRegisteredComponentTypes()
	{
		// Add Component UIとFactory生成対象を同じ一覧に揃える
		return kRegisteredComponentTypes;
	}

	bool ComponentFactory::IsAllowMultiple(std::string_view className)
	{
		const ComponentTypeInfo* typeInfo = FindComponentType(className);

		if (!typeInfo)
		{
			return false; // 未登録のComponentClass名の場合は複数追加不可とする
		}

		return typeInfo->allowMultiple; // 同一Actorに複数追加可能かどうかを返す
	}

	const ComponentFactory::ComponentTypeInfo* ComponentFactory::FindComponentType(std::string_view className)
	{
		for (const ComponentFactory::ComponentTypeInfo& typeInfo : kRegisteredComponentTypes)
		{
			if (typeInfo.className == className)
			{
				return &typeInfo; // Class名が一致するComponent情報を返す
			}
		}

		return nullptr; // 一致するComponent情報がない場合はnullptrを返す
	}

}
