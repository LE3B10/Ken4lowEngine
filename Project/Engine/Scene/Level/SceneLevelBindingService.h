#pragma once

#include <string>
#include <unordered_map>
#include <utility>

namespace Ken4lowEngine
{
	class ActorWorld;

	/// <summary>SceneDefinitionのLevelPathを対象ActorWorldの初期化時まで保持します。</summary>
	class SceneLevelBindingService
	{
	public:
		struct PendingLevel
		{
			std::string sceneId;
			std::string levelPath;
		};

		/// <summary>Scene初期化前のActorWorldへLevel読込要求を関連付けます。</summary>
		static bool Queue(ActorWorld* actorWorld, std::string sceneId, std::string levelPath)
		{
			if (!actorWorld || levelPath.empty())
			{
				return false; // ActorWorldを公開しないSceneまたは空Levelは従来初期化を維持する。
			}

			pendingLevels_[actorWorld] = PendingLevel{ std::move(sceneId), std::move(levelPath) };
			return true;
		}

		/// <summary>指定ActorWorldに関連付いたLevel要求を一度だけ取り出します。</summary>
		static bool Consume(ActorWorld* actorWorld, PendingLevel& outPendingLevel)
		{
			const auto iterator = pendingLevels_.find(actorWorld);
			if (iterator == pendingLevels_.end())
			{
				return false;
			}

			outPendingLevel = std::move(iterator->second);
			pendingLevels_.erase(iterator); // 同じActorWorldの再InitializeでLevelを重複読込しない。
			return true;
		}

		/// <summary>Scene破棄などで不要になったLevel要求を解除します。</summary>
		static void Cancel(ActorWorld* actorWorld)
		{
			pendingLevels_.erase(actorWorld);
		}

	private:
		inline static std::unordered_map<ActorWorld*, PendingLevel> pendingLevels_{};
	};
} // namespace Ken4lowEngine
