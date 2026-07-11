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

namespace Ken4lowEngine
{
	namespace
	{
		bool ValidateActorJson(const nlohmann::json& actorJson)
		{
			if (!actorJson.is_object() || !actorJson.contains("Components") || !actorJson["Components"].is_array()) return false;
			for (const auto& componentJson : actorJson["Components"])
			{
				if (!componentJson.is_object() || !componentJson.contains("Class") || !componentJson["Class"].is_string()) return false;
			}
			return true;
		}
	}

	nlohmann::json ActorJsonSerializer::SerializeActor(const Actor& actor)
	{
		nlohmann::json actorJson;
		actor.ToJson(actorJson); // Command履歴はファイルを経由せずActor構成を保持する。
		return actorJson;
	}

	bool ActorJsonSerializer::SaveActorToFile(const Actor& actor, std::string_view filePath)
	{
		const std::filesystem::path path(filePath);
		if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path());

		std::ofstream file(path);
		if (!file.is_open()) return false;
		file << SerializeActor(actor).dump(4);
		return true;
	}

	bool ActorJsonSerializer::LoadActorFromFile(Actor& actor, std::string_view filePath)
	{
		std::ifstream file{ std::filesystem::path{ std::string(filePath) } };
		if (!file.is_open()) return false;

		nlohmann::json actorJson;
		file >> actorJson;
		return LoadActorFromJson(actor, actorJson);
	}

	bool ActorJsonSerializer::LoadActorFromJson(Actor& actor, const nlohmann::json& actorJson)
	{
		if (!ValidateActorJson(actorJson)) return false;

		actor.ClearComponents();
		actor.FromJson(actorJson);

		std::unordered_map<std::string, SceneComponent*> sceneComponentsByName;
		std::vector<std::pair<SceneComponent*, std::string>> pendingAttachments;

		for (const auto& componentJson : actorJson["Components"])
		{
			const std::string className = componentJson["Class"].get<std::string>();
			const std::string componentType = componentJson.value("Type", std::string("ActorComponent"));
			const std::string parentName = componentJson.value("Parent", std::string{});

			ActorComponent* createdComponent = nullptr;
			if (componentType == "SceneComponent" && parentName.empty())
			{
				createdComponent = ComponentFactory::CreateRootSceneComponent(&actor, className);
			}
			else
			{
				createdComponent = ComponentFactory::CreateComponent(&actor, className);
			}
			if (!createdComponent) continue;

			createdComponent->FromJson(componentJson);
			if (SceneComponent* sceneComponent = dynamic_cast<SceneComponent*>(createdComponent))
			{
				sceneComponentsByName[sceneComponent->GetName()] = sceneComponent;
				if (!parentName.empty()) pendingAttachments.emplace_back(sceneComponent, parentName);
				else if (!actor.GetRootComponent()) actor.SetRootComponent(sceneComponent);
			}
		}

		for (const auto& [child, parentName] : pendingAttachments)
		{
			const auto parentIt = sceneComponentsByName.find(parentName);
			if (child && parentIt != sceneComponentsByName.end()) child->AttachTo(parentIt->second);
		}

		actor.InitializeComponents();
		return true;
	}

	std::unique_ptr<Actor> ActorJsonSerializer::CreateActorFromJson(std::string_view filePath, const ActorSpawnOptions& options)
	{
		std::ifstream file{ std::filesystem::path{ std::string(filePath) } };
		if (!file.is_open()) return nullptr;

		nlohmann::json actorJson;
		file >> actorJson;
		return CreateActorFromJson(actorJson, options);
	}

	std::unique_ptr<Actor> ActorJsonSerializer::CreateActorFromJson(const nlohmann::json& actorJson, const ActorSpawnOptions& options)
	{
		if (!ValidateActorJson(actorJson) || !actorJson.contains("Class") || !actorJson["Class"].is_string()) return nullptr;

		std::unique_ptr<Actor> actor = ActorFactory::CreateActor(actorJson["Class"].get<std::string>());
		if (!actor || !LoadActorFromJson(*actor, actorJson)) return nullptr;

		if (options.applySpawnOffset && actor->GetRootComponent())
		{
			SceneComponent* root = actor->GetRootComponent();
			root->SetLocalPosition(Vector3::Add(root->GetLocalPosition(), options.spawnOffset));
			root->RefreshWorldTransform(); // Actor生成CommandのRedoでも同じSpawnOffsetを再現する。
		}
		return actor;
	}
} // namespace Ken4lowEngine
