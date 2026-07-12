#pragma once

#include <json.hpp>

#include <algorithm>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Ken4lowEngine
{
	/// <summary>Scene切り替え時に使用するTransition設定です。</summary>
	struct SceneTransitionDefinition
	{
		std::string type = "Fade";
		int coverHoldFrames = 4;
		int uncoverDelayFrames = 3;
		float durationSeconds = 1.0f;
	};

	/// <summary>Scene開始時に使用するBGM設定です。</summary>
	struct SceneBgmDefinition
	{
		std::string path;
		bool loop = true;
		float volume = 1.0f;
	};

	/// <summary>
	/// Sceneの進行設定をJSONから読み込むためのデータです。
	/// C++側はScene固有ロジックを持ち、使用Levelや遷移先などの構成値をこの定義へ分離します。
	/// </summary>
	struct SceneDefinition
	{
		static constexpr int kCurrentVersion = 1;
		static constexpr std::string_view kFormat = "Ken4lowScene";

		std::string sceneName;
		std::string sceneClass;
		std::string levelPath;
		std::string gameModeClass;
		std::string playerActorPath;
		SceneBgmDefinition bgm;
		std::vector<std::string> uiAssets;
		SceneTransitionDefinition transition;
		std::string nextScene;
		std::string retryScene;
		nlohmann::json parameters = nlohmann::json::object();
		std::filesystem::path sourcePath;

		[[nodiscard]] bool IsValid(std::string& outError) const
		{
			if (sceneName.empty())
			{
				outError = "SceneName is empty.";
				return false;
			}
			if (sceneClass.empty())
			{
				outError = "SceneClass is empty: " + sceneName;
				return false;
			}
			if (transition.coverHoldFrames < 0 || transition.uncoverDelayFrames < 0)
			{
				outError = "Transition frame count must be zero or greater: " + sceneName;
				return false;
			}
			if (transition.durationSeconds < 0.0f)
			{
				outError = "Transition duration must be zero or greater: " + sceneName;
				return false;
			}
			if (bgm.volume < 0.0f)
			{
				outError = "BGM volume must be zero or greater: " + sceneName;
				return false;
			}
			return true;
		}

		[[nodiscard]] static bool FromJson(
			const nlohmann::json& json,
			const std::filesystem::path& sourcePath,
			SceneDefinition& outDefinition,
			std::string& outError)
		{
			if (!json.is_object())
			{
				outError = "Scene definition root must be an object: " + sourcePath.generic_string();
				return false;
			}
			if (json.value("Format", std::string{}) != std::string(kFormat))
			{
				outError = "Unsupported scene format: " + sourcePath.generic_string();
				return false;
			}

			const int version = json.value("Version", 0);
			if (version <= 0 || version > kCurrentVersion)
			{
				outError = "Unsupported scene version " + std::to_string(version) + ": " + sourcePath.generic_string();
				return false;
			}

			SceneDefinition definition{};
			definition.sceneName = json.value("SceneName", json.value("Name", std::string{}));
			definition.sceneClass = json.value("SceneClass", std::string{});
			definition.levelPath = json.value("LevelPath", std::string{});
			definition.gameModeClass = json.value("GameMode", std::string{});
			definition.playerActorPath = json.value("PlayerActor", std::string{});
			definition.nextScene = json.value("NextScene", std::string{});
			definition.retryScene = json.value("RetryScene", std::string{});
			definition.sourcePath = sourcePath;

			if (json.contains("BGM") && json["BGM"].is_object())
			{
				const nlohmann::json& bgmJson = json["BGM"];
				definition.bgm.path = bgmJson.value("Path", std::string{});
				definition.bgm.loop = bgmJson.value("Loop", true);
				definition.bgm.volume = bgmJson.value("Volume", 1.0f);
			}
			else if (json.contains("BGM") && json["BGM"].is_string())
			{
				definition.bgm.path = json["BGM"].get<std::string>(); // 旧来の文字列指定も読み込み互換として扱う。
			}

			if (json.contains("UI") && json["UI"].is_array())
			{
				for (const nlohmann::json& uiEntry : json["UI"])
				{
					if (uiEntry.is_string()) definition.uiAssets.push_back(uiEntry.get<std::string>());
				}
			}

			if (json.contains("Transition") && json["Transition"].is_object())
			{
				const nlohmann::json& transitionJson = json["Transition"];
				definition.transition.type = transitionJson.value("Type", definition.transition.type);
				definition.transition.coverHoldFrames = transitionJson.value("CoverHoldFrames", definition.transition.coverHoldFrames);
				definition.transition.uncoverDelayFrames = transitionJson.value("UncoverDelayFrames", definition.transition.uncoverDelayFrames);
				definition.transition.durationSeconds = transitionJson.value("Duration", definition.transition.durationSeconds);
			}

			if (json.contains("Parameters") && json["Parameters"].is_object())
			{
				definition.parameters = json["Parameters"];
			}

			if (!definition.IsValid(outError)) return false;
			outDefinition = std::move(definition);
			return true;
		}

		[[nodiscard]] nlohmann::json ToJson() const
		{
			nlohmann::json json = {
				{ "Format", std::string(kFormat) }, // JSONへは所有権を持つ文字列として明示的に書き出す。
				{ "Version", kCurrentVersion },
				{ "SceneName", sceneName },
				{ "SceneClass", sceneClass },
				{ "LevelPath", levelPath },
				{ "GameMode", gameModeClass },
				{ "PlayerActor", playerActorPath },
				{ "BGM", {
					{ "Path", bgm.path },
					{ "Loop", bgm.loop },
					{ "Volume", bgm.volume },
				} },
				{ "UI", uiAssets },
				{ "Transition", {
					{ "Type", transition.type },
					{ "CoverHoldFrames", transition.coverHoldFrames },
					{ "UncoverDelayFrames", transition.uncoverDelayFrames },
					{ "Duration", transition.durationSeconds },
				} },
				{ "NextScene", nextScene },
				{ "RetryScene", retryScene },
				{ "Parameters", parameters },
			};
			return json;
		}
	};
} // namespace Ken4lowEngine
