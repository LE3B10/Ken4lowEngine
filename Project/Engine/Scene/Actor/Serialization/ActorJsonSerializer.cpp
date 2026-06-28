#include "ActorJsonSerializer.h"
#include "Actor.h"
#include "ActorComponent.h"
#include "SceneComponent.h"
#include "ComponentFactory.h"
#include "ActorFactory.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>
#include <json.hpp>

namespace Ken4lowEngine
{
	bool ActorJsonSerializer::SaveActorToFile(const Actor& actor, std::string_view filePath)
	{
		const std::filesystem::path path(filePath);

		if (path.has_parent_path())
		{
			std::filesystem::create_directories(path.parent_path()); // 親ディレクトリが存在しない場合は作成する
		}

		std::ofstream file(path);
		if (!file.is_open())
		{
			return false; // ファイルが開けなかった場合は失敗を返す
		}

		nlohmann::json actorJson;
		actor.ToJson(actorJson); // Actorの情報をJSONへ保存する

		file << actorJson.dump(4); // JSONデータをフォーマットしてファイルに書き込む
		return true;
	}

	bool ActorJsonSerializer::LoadActorFromFile(Actor& actor, std::string_view filePath)
	{
		const std::filesystem::path path{ std::string(filePath) }; // string_viewを安全にpathへ変換する。

		std::ifstream file{ path }; // Most Vexing Parseを避けるため、波括弧でifstreamを生成する。
		if (!file.is_open())
		{
			return false; // ファイルを開けなかった場合は読み込み失敗。
		}

		nlohmann::json actorJson;
		file >> actorJson; // JSONファイルを読み込む。

		if (!actorJson.is_object())
		{
			return false; // Actor用JSONではない場合は読み込まない。
		}

		if (!actorJson.contains("Components") || !actorJson["Components"].is_array())
		{
			return false; // Component配列が無い場合はActor構成として扱わない。
		}

		for (const auto& componentJson : actorJson["Components"])
		{
			if (!componentJson.is_object())
			{
				return false; // 不正なComponent要素がある場合は、Actorを壊す前に読み込みを中止する。
			}

			if (!componentJson.contains("Class") || !componentJson["Class"].is_string())
			{
				return false; // Classが無いComponentは復元できないため、Actorを壊す前に読み込みを中止する。
			}
		}

		actor.ClearComponents(); // 既存Componentを破棄して、JSON構成で作り直す。
		actor.FromJson(actorJson); // Actor名などの共通情報を復元する。

		std::unordered_map<std::string, SceneComponent*> sceneComponentsByName;
		std::vector<std::pair<SceneComponent*, std::string>> pendingAttachments;

		for (const auto& componentJson : actorJson["Components"])
		{
			const std::string className = componentJson["Class"].get<std::string>();

			std::string componentType = "ActorComponent";
			if (componentJson.contains("Type") && componentJson["Type"].is_string())
			{
				componentType = componentJson["Type"].get<std::string>(); // SceneComponentかActorComponentかを取得する。
			}

			std::string parentName;
			if (componentJson.contains("Parent") && componentJson["Parent"].is_string())
			{
				parentName = componentJson["Parent"].get<std::string>(); // 後でAttachToするため親名を保持する。
			}

			ActorComponent* createdComponent = nullptr;

			if (componentType == "SceneComponent" && parentName.empty())
			{
				createdComponent = ComponentFactory::CreateRootSceneComponent(&actor, className); // 親が無いSceneComponentはRootとして生成する。
			}
			else
			{
				createdComponent = ComponentFactory::CreateComponent(&actor, className); // 通常Componentとして生成する。
			}

			if (!createdComponent)
			{
				continue; // 未対応Classなら無視する。
			}

			createdComponent->FromJson(componentJson); // Component固有情報を復元する。

			if (SceneComponent* sceneComponent = dynamic_cast<SceneComponent*>(createdComponent))
			{
				sceneComponentsByName[sceneComponent->GetName()] = sceneComponent; // 親子接続用に名前で登録する。

				if (!parentName.empty())
				{
					pendingAttachments.emplace_back(sceneComponent, parentName); // 全Component生成後に親子接続する。
				}
				else if (!actor.GetRootComponent())
				{
					actor.SetRootComponent(sceneComponent); // Rootが未設定なら親無しSceneComponentをRootにする。
				}
			}
		}

		for (const auto& [child, parentName] : pendingAttachments)
		{
			if (!child)
			{
				continue; // 子が無効なら接続しない。
			}

			const auto parentIt = sceneComponentsByName.find(parentName);
			if (parentIt == sceneComponentsByName.end())
			{
				continue; // 親名に一致するSceneComponentが無い場合は接続しない。
			}

			child->AttachTo(parentIt->second); // JSONに保存されていた親子関係を復元する。
		}

		actor.InitializeComponents(); // 派生ActorのInitializeを呼ばず、復元したComponentだけを初期化する
		return true;
	}

	std::unique_ptr<Actor> ActorJsonSerializer::CreateActorFromJson(std::string_view filePath, const ActorSpawnOptions& options)
	{
		const std::filesystem::path path{ std::string(filePath) }; // string_viewを安全にpathへ変換する

		std::ifstream file{ path }; // Most Vexing Parseを避けるため、波括弧でifstreamを生成する
		if (!file.is_open())
		{
			return nullptr; // ファイルを開けなかった場合はnullptrを返す
		}

		nlohmann::json actorJson;
		file >> actorJson; // JSONファイルを読み込む

		if (!actorJson.is_object())
		{
			return nullptr; // Actor用JSONではない場合はnullptrを返す
		}

		if (!actorJson.contains("Class") || !actorJson["Class"].is_string())
		{
			return nullptr; // Classが無いActorは生成できないため、nullptrを返す
		}

		const std::string actorClass = actorJson["Class"].get<std::string>();

		std::unique_ptr<Actor> actor = ActorFactory::CreateActor(actorClass); // ActorFactoryで指定ClassのActorを生成する
		if (!actor)
		{
			return nullptr; // 未対応Classならnullptrを返す
		}

		if (!LoadActorFromFile(*actor, filePath))
		{
			return nullptr; // JSON読み込みに失敗した場合はnullptrを返す
		}

		if (options.applySpawnOffset && actor->GetRootComponent())
		{
			if (SceneComponent* root = actor->GetRootComponent())
			{
				Vector3 position = Vector3::Add(root->GetLocalPosition(), options.spawnOffset);
				root->SetLocalPosition(position); // SpawnOffsetをRootComponentのローカル位置に適用する
				root->RefreshWorldTransform(); // SpawnOffsetを適用した後にWorldTransformを更新する
			}
		}


		return actor;
	}

}