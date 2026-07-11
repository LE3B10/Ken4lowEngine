#pragma once

#include "EditorAssetDragDrop.h"
#include "EditorContext.h"

#include <Actor.h>
#include <ActorSpawnOptions.h>
#include <ActorWorld.h>
#include <BaseScene.h>
#include <CameraManager.h>
#include <Matrix4x4.h>
#include <ModelComponent.h>
#include <SceneComponent.h>
#include <SceneManager.h>
#include <Vector2.h>
#include <Vector3.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace Ken4lowEngine
{
	/// <summary>
	/// Content Browserから配置した通常モデルを、保存時は汎用Actorとして扱うEditor専用Actorです。
	/// </summary>
	class EditorPlacedModelActor final : public Actor
	{
	public:
		EditorPlacedModelActor(std::string modelPath, const Vector3& worldPosition)
		{
			SceneComponent& root = CreateRootComponent<SceneComponent>();
			root.SetName("Root Scene Component");
			root.SetLocalPosition(worldPosition);
			root.RefreshWorldTransform();

			ModelComponent& model = AddComponent<ModelComponent>();
			model.SetName("Model Component");
			model.SetModelPath(modelPath);
			model.AttachTo(&root);
		}

		std::string GetClassTypeName() const override
		{
			return "Actor"; // LevelやPrefabへ保存した際は既存ActorFactoryで復元できる形式を維持する。
		}
	};

	struct EditorAssetPlacementResult
	{
		Actor* spawnedActor = nullptr;
		std::string message;
		bool succeeded = false;
	};

	/// <summary>
	/// Content BrowserのPayloadを現在SceneのActorWorldへ配置するEditorサービスです。
	/// </summary>
	class EditorAssetPlacementService
	{
	public:
		static bool CalculateDropPosition(
			const Vector2& screenMouse,
			const Vector2& viewportScreenMin,
			const Vector2& viewportImageSize,
			Vector3& outPosition)
		{
			if (viewportImageSize.x <= 1.0f || viewportImageSize.y <= 1.0f)
			{
				return false;
			}

			const float localX = screenMouse.x - viewportScreenMin.x;
			const float localY = screenMouse.y - viewportScreenMin.y;
			if (localX < 0.0f || localY < 0.0f || localX > viewportImageSize.x || localY > viewportImageSize.y)
			{
				return false;
			}

			const float ndcX = (localX / viewportImageSize.x) * 2.0f - 1.0f;
			const float ndcY = 1.0f - (localY / viewportImageSize.y) * 2.0f;
			const Matrix4x4 projection = CameraManager::GetInstance()->GetActiveProjectionMatrix();
			if (std::abs(projection.m[0][0]) <= 0.00001f || std::abs(projection.m[1][1]) <= 0.00001f)
			{
				return false;
			}

			const Vector3 viewDirection{
				ndcX / projection.m[0][0],
				ndcY / projection.m[1][1],
				1.0f
			};
			const Matrix4x4 inverseView = Matrix4x4::Inverse(CameraManager::GetInstance()->GetActiveViewMatrix());
			Vector3 worldDirection{
				viewDirection.x * inverseView.m[0][0] + viewDirection.y * inverseView.m[1][0] + viewDirection.z * inverseView.m[2][0],
				viewDirection.x * inverseView.m[0][1] + viewDirection.y * inverseView.m[1][1] + viewDirection.z * inverseView.m[2][1],
				viewDirection.x * inverseView.m[0][2] + viewDirection.y * inverseView.m[1][2] + viewDirection.z * inverseView.m[2][2]
			};
			worldDirection = Vector3::NormalizeSafe(worldDirection, CameraManager::GetInstance()->GetActiveCameraForward());

			const Vector3 rayOrigin = CameraManager::GetInstance()->GetActiveCameraPosition();
			if (std::abs(worldDirection.y) > 0.0001f)
			{
				const float distanceToGround = -rayOrigin.y / worldDirection.y;
				if (distanceToGround > 0.1f && distanceToGround < 10000.0f)
				{
					outPosition = rayOrigin + worldDirection * distanceToGround;
					outPosition.y = 0.0f;
					return true;
				}
			}

			outPosition = rayOrigin + worldDirection * 10.0f; // 地面と交差しない視線ではドロップ瞬間のワールド位置へ固定する。
			return true;
		}

		static EditorAssetPlacementResult PlaceAsset(
			SceneManager* sceneManager,
			const EditorAssetDragDropPayload& payload,
			const Vector3& worldPosition)
		{
			BaseScene* scene = sceneManager ? sceneManager->GetCurrentScene() : nullptr;
			return PlaceAsset(scene, payload, worldPosition);
		}

		static EditorAssetPlacementResult PlaceAsset(
			BaseScene* scene,
			const EditorAssetDragDropPayload& payload,
			const Vector3& worldPosition)
		{
			EditorAssetPlacementResult result{};
			if (!scene)
			{
				result.message = "現在のシーンが存在しないため、アセットを配置できません。";
				return result;
			}

			ActorWorld* actorWorld = scene->GetEditorActorWorld();
			if (!actorWorld)
			{
				result.message = "現在のシーンはEditor用ActorWorldを公開していません。";
				return result;
			}

			const EditorAssetType assetType = GetPayloadAssetType(payload);
			const std::filesystem::path relativePath(payload.relativePath.data());
			if (assetType == EditorAssetType::Model)
			{
				const std::string logicalModelPath = ResolveModelLogicalPath(relativePath);
				if (logicalModelPath.empty())
				{
					result.message = "配置できるモデル形式はResources/Models/Sources内のgltf、glb、objです。";
					return result;
				}

				EditorPlacedModelActor& actor = actorWorld->SpawnActor<EditorPlacedModelActor>(logicalModelPath, worldPosition);
				actor.SetName(MakeUniqueActorName(*actorWorld, relativePath.stem().generic_string()));
				result.spawnedActor = &actor;
				result.succeeded = true;
				result.message = "モデルをビューポートへ配置しました: " + logicalModelPath;
			}
			else if (assetType == EditorAssetType::ActorPrefab)
			{
				ActorSpawnOptions options{};
				options.applySpawnOffset = false;
				options.disableAutoRegisterMainCamera = true;
				const std::string prefabPath = (std::filesystem::path("Resources") / relativePath).generic_string();
				Actor* actor = actorWorld->SpawnActorFromJson(prefabPath, options);
				if (!actor)
				{
					result.message = "アクタープリファブの読み込みに失敗しました: " + prefabPath;
					return result;
				}

				result.spawnedActor = actor;
				result.succeeded = true;
				result.message = "アクタープリファブをビューポートへ配置しました: " + prefabPath;
			}
			else
			{
				result.message = "このアセット種別はビューポート配置に対応していません。";
			}

			if (result.succeeded && result.spawnedActor)
			{
				if (SceneComponent* root = result.spawnedActor->GetRootComponent())
				{
					root->Detach();
					root->SetLocalPosition(worldPosition);
					root->RefreshWorldTransform(); // 配置確定時に親を外し、カメラ追従ではなくWorld座標へ固定する。
				}

				SelectSpawnedActor(*scene, *result.spawnedActor);
				EditorContext::GetInstance()->MarkLevelDirty();
			}
			return result;
		}

	private:
		static std::string ResolveModelLogicalPath(const std::filesystem::path& relativePath)
		{
			std::string path = relativePath.generic_string();
			constexpr const char* sourcesPrefix = "Models/Sources/";
			if (!path.starts_with(sourcesPrefix))
			{
				return {};
			}

			path.erase(0, std::char_traits<char>::length(sourcesPrefix));
			std::string extension = std::filesystem::path(path).extension().generic_string();
			std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char character)
				{
					return static_cast<char>(std::tolower(character));
				});
			if (extension != ".gltf" && extension != ".glb" && extension != ".obj")
			{
				return {};
			}
			return path;
		}

		static std::string MakeUniqueActorName(ActorWorld& actorWorld, std::string baseName)
		{
			if (baseName.empty())
			{
				baseName = "配置アクタ";
			}
			if (!actorWorld.FindActorByName(baseName))
			{
				return baseName;
			}

			for (uint32_t index = 2; index < 100000; ++index)
			{
				const std::string candidate = baseName + "_" + std::to_string(index);
				if (!actorWorld.FindActorByName(candidate))
				{
					return candidate;
				}
			}
			return baseName + "_Copy";
		}

		static void SelectSpawnedActor(BaseScene& scene, Actor& actor)
		{
			std::vector<EditorObjectInfo> objects;
			scene.CollectEditorObjects(objects);
			const auto found = std::find_if(objects.begin(), objects.end(), [&actor](const EditorObjectInfo& object)
				{
					return object.objectKind == EditorObjectKind::Actor && object.displayName == actor.GetName();
				});
			if (found != objects.end())
			{
				EditorContext::GetInstance()->GetSelection().Select(*found);
			}
		}
	};
} // namespace Ken4lowEngine
