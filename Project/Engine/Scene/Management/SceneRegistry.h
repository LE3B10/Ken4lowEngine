#pragma once

#include "SceneDefinition.h"

#include <json.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Ken4lowEngine
{
	/// <summary>Resources/JSON/Scenes配下のScene定義を検索・検証します。</summary>
	class SceneRegistry
	{
	public:
		static constexpr std::string_view kStartupFormat = "Ken4lowSceneStartup";

		SceneRegistry()
		{
			LoadDirectory("Resources/JSON/Scenes"); // SceneManager生成時に標準Scene定義を読み込み、既存ChangeScene呼び出しをそのままデータ駆動化する。
		}

		bool LoadDirectory(const std::filesystem::path& directory)
		{
			definitions_.clear();
			errors_.clear();
			warnings_.clear();
			debugStartScene_.clear();
			releaseStartScene_.clear();
			directory_ = directory;

			std::error_code error;
			if (!std::filesystem::exists(directory_, error) || error)
			{
				errors_.push_back("Scene definition directory was not found: " + directory_.generic_string());
				return false;
			}

			std::vector<std::filesystem::path> jsonFiles;
			for (const auto& entry : std::filesystem::recursive_directory_iterator(directory_, error))
			{
				if (error) break;
				if (entry.is_regular_file() && entry.path().extension() == ".json") jsonFiles.push_back(entry.path());
			}
			std::sort(jsonFiles.begin(), jsonFiles.end()); // 読込順を固定して重複エラーやログの順序を再現可能にする。

			for (const std::filesystem::path& path : jsonFiles) LoadFile(path);
			ValidateReferences();

			if (definitions_.empty())
			{
				errors_.push_back("No Ken4lowScene definitions were loaded from: " + directory_.generic_string());
			}
			return errors_.empty();
		}

		[[nodiscard]] const SceneDefinition* Find(std::string_view sceneNameOrClass) const
		{
			const auto direct = definitions_.find(std::string(sceneNameOrClass));
			if (direct != definitions_.end()) return &direct->second;

			for (const auto& [name, definition] : definitions_)
			{
				(void)name;
				if (definition.sceneClass == sceneNameOrClass) return &definition;
			}
			return nullptr;
		}

		[[nodiscard]] std::string GetStartupSceneName(bool debugBuild, std::string_view fallback) const
		{
			const std::string& configured = debugBuild ? debugStartScene_ : releaseStartScene_;
			if (!configured.empty() && Find(configured)) return configured;
			return std::string(fallback);
		}

		[[nodiscard]] const std::unordered_map<std::string, SceneDefinition>& GetDefinitions() const { return definitions_; }
		[[nodiscard]] const std::vector<std::string>& GetErrors() const { return errors_; }
		[[nodiscard]] const std::vector<std::string>& GetWarnings() const { return warnings_; }
		[[nodiscard]] const std::filesystem::path& GetDirectory() const { return directory_; }
		[[nodiscard]] bool IsLoaded() const { return !definitions_.empty() && errors_.empty(); }

		[[nodiscard]] static SceneDefinition MakeLegacyDefinition(std::string_view sceneClass)
		{
			SceneDefinition definition{};
			definition.sceneName = std::string(sceneClass);
			definition.sceneClass = std::string(sceneClass);
			definition.parameters["LegacyFallback"] = true; // JSONが無い場合も従来のScene名指定を壊さず段階移行できるようにする。
			return definition;
		}

	private:
		void LoadFile(const std::filesystem::path& path)
		{
			try
			{
				std::ifstream file(path);
				if (!file.is_open())
				{
					errors_.push_back("Failed to open scene definition: " + path.generic_string());
					return;
				}

				nlohmann::json json;
				file >> json;
				if (!json.is_object())
				{
					errors_.push_back("Scene JSON root must be an object: " + path.generic_string());
					return;
				}

				const std::string format = json.value("Format", std::string{});
				if (format == SceneDefinition::kFormat)
				{
					SceneDefinition definition{};
					std::string parseError;
					if (!SceneDefinition::FromJson(json, path, definition, parseError))
					{
						errors_.push_back(parseError);
						return;
					}
					if (definitions_.contains(definition.sceneName))
					{
						errors_.push_back("Duplicate SceneName: " + definition.sceneName + " in " + path.generic_string());
						return;
					}
					definitions_.emplace(definition.sceneName, std::move(definition));
					return;
				}

				if (format == kStartupFormat)
				{
					const int version = json.value("Version", 0);
					if (version != 1)
					{
						errors_.push_back("Unsupported startup scene version: " + path.generic_string());
						return;
					}
					debugStartScene_ = json.value("Debug", std::string{});
					releaseStartScene_ = json.value("Release", std::string{});
					return;
				}

				warnings_.push_back("Skipped unknown JSON format in Scenes directory: " + path.generic_string());
			}
			catch (const std::exception& exception)
			{
				errors_.push_back("Failed to parse " + path.generic_string() + ": " + exception.what());
			}
		}

		void ValidateReferences()
		{
			for (const auto& [name, definition] : definitions_)
			{
				if (!definition.nextScene.empty() && !Find(definition.nextScene))
				{
					warnings_.push_back("NextScene was not found: " + name + " -> " + definition.nextScene);
				}
				if (!definition.retryScene.empty() && !Find(definition.retryScene))
				{
					warnings_.push_back("RetryScene was not found: " + name + " -> " + definition.retryScene);
				}
			}

			if (!debugStartScene_.empty() && !Find(debugStartScene_))
			{
				warnings_.push_back("Debug startup scene was not found: " + debugStartScene_);
			}
			if (!releaseStartScene_.empty() && !Find(releaseStartScene_))
			{
				warnings_.push_back("Release startup scene was not found: " + releaseStartScene_);
			}
		}

		std::filesystem::path directory_;
		std::unordered_map<std::string, SceneDefinition> definitions_;
		std::vector<std::string> errors_;
		std::vector<std::string> warnings_;
		std::string debugStartScene_;
		std::string releaseStartScene_;
	};
} // namespace Ken4lowEngine
