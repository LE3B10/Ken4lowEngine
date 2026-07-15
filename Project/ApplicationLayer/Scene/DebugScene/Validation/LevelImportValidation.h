#pragma once

#include <ActorWorld.h>
#include <BlenderLevelImporter.h>
#include <BlenderSceneLoader.h>
#include <CameraManager.h>
#include <ModelComponent.h>

#include <algorithm>
#include <string>

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

namespace K4E = ::Ken4lowEngine;

/// DebugSceneだけでBlenderSceneData -> Ken4lowLevel変換とStage Actor実体生成を確認する診断。
class LevelImportValidation
{
public:
	explicit LevelImportValidation(K4E::ActorWorld* actorWorld = nullptr)
		: actorWorld_(actorWorld)
	{
		Reload();
		RefreshSpawnState();
	}

	void Reload()
	{
		blenderResult_ = K4E::BlenderSceneLoader::Load(sourceJsonPath_);
		importResult_ = {};

		if (!blenderResult_.succeeded)
		{
			lastMessage_ = blenderResult_.message;
			return;
		}

		K4E::BlenderLevelImporter::Options options{};
		options.levelName = "fps_stage00_import_preview";
		options.sourceJsonPath = sourceJsonPath_;
		options.stageModelPath = stageModelPath_;
		options.stageActorName = importedActorName_;
		options.stageActorId = "ImportedStageActor";
		importResult_ = K4E::BlenderLevelImporter::Import(blenderResult_.scene, options);
		lastMessage_ = importResult_.message;
	}

	void DrawImGui()
	{
#ifdef USE_IMGUI
		RefreshSpawnState();
		if (!ImGui::Begin("LevelImport 検証"))
		{
			ImGui::End();
			return;
		}

		ImGui::Text("Source JSON: %s", sourceJsonPath_.c_str());
		ImGui::Text("Stage Model: %s", stageModelPath_.c_str());
		if (ImGui::Button("再変換")) Reload();
		ImGui::SameLine();
		ImGui::TextColored(
			importResult_.succeeded ? ImVec4(0.35f, 1.0f, 0.45f, 1.0f) : ImVec4(1.0f, 0.4f, 0.35f, 1.0f),
			"%s",
			lastMessage_.c_str());

		ImGui::SeparatorText("Phase 3 変換結果");
		ImGui::Text("Source Object: %zu", importResult_.sourceObjectCount);
		ImGui::Text("Source Mesh: %zu", importResult_.sourceMeshCount);
		ImGui::Text("Imported Actor: %zu", importResult_.importedActorCount);
		ImGui::Text("Import Mode: IntegratedStageModel");
		ImGui::TextDisabled("現行Stageと同じく一体型GLTFを1つのActor + ModelComponentで表現します。");
		ImGui::TextDisabled("BlenderのObject / Mesh情報はImportSource Manifestへ保持します。");

		if (!importResult_.succeeded)
		{
			ImGui::End();
			return;
		}

		DrawPhase4SpawnValidation();
		DrawLevelSummary();
		DrawActorPreview();
		DrawImportManifestPreview();
		ImGui::End();
#endif // USE_IMGUI
	}

	const K4E::BlenderLevelImporter::Result& GetImportResult() const noexcept { return importResult_; }

private:
	void SpawnImportedStageActor()
	{
		if (!actorWorld_)
		{
			spawnMessage_ = "ActorWorldが接続されていません。";
			return;
		}
		if (!importResult_.succeeded || !importResult_.levelJson.contains("Actors") || importResult_.levelJson["Actors"].empty())
		{
			spawnMessage_ = "生成可能なImport結果がありません。";
			return;
		}
		if (actorWorld_->FindActorByName(importedActorName_) != nullptr)
		{
			spawnMessage_ = "ImportedStageActorは既にActorWorldへ存在します。";
			return;
		}

		const nlohmann::json& actorData = importResult_.levelJson["Actors"][0]["Data"];
		K4E::Actor* actor = actorWorld_->SpawnActorFromJsonData(actorData);
		if (!actor)
		{
			spawnMessage_ = "Ken4lowLevelのActor DataからStage Actorを生成できませんでした。";
			RefreshSpawnState();
			return;
		}

		// Runtime生成後のModelComponentにも現在のMain Cameraを明示的に接続する。
		if (K4E::ModelComponent* modelComponent = actor->GetComponent<K4E::ModelComponent>())
		{
			modelComponent->SetCamera(K4E::CameraManager::GetInstance()->GetMainCamera());
		}

		spawnMessage_ = "ImportedStageActorをActorWorldへ1体生成しました。";
		RefreshSpawnState();
	}

	void DestroyImportedStageActor()
	{
		if (!actorWorld_)
		{
			spawnMessage_ = "ActorWorldが接続されていません。";
			return;
		}

		K4E::Actor* actor = actorWorld_->FindActorByName(importedActorName_);
		if (!actor)
		{
			spawnMessage_ = "削除対象のImportedStageActorがありません。";
			RefreshSpawnState();
			return;
		}

		const bool destroyed = actorWorld_->DestroyActor(actor);
		spawnMessage_ = destroyed
			? "ImportedStageActorの安全な遅延削除を予約しました。"
			: "ImportedStageActorの削除予約に失敗しました。";
		RefreshSpawnState();
	}

	void RefreshSpawnState()
	{
		stageActorExists_ = false;
		modelComponentExists_ = false;
		if (!actorWorld_) return;

		K4E::Actor* actor = actorWorld_->FindActorByName(importedActorName_);
		if (!actor || actor->IsPendingDestroy()) return;

		stageActorExists_ = true;
		modelComponentExists_ = actor->GetComponent<K4E::ModelComponent>() != nullptr;
	}

#ifdef USE_IMGUI
	void DrawPhase4SpawnValidation()
	{
		ImGui::SeparatorText("Phase 4 Actor生成確認");
		ImGui::Text("ActorWorld接続: %s", actorWorld_ ? "OK" : "未接続");
		ImGui::Text("ImportedStageActor: %s", stageActorExists_ ? "存在" : "未生成");
		ImGui::Text("ModelComponent: %s", modelComponentExists_ ? "存在" : "未確認");
		ImGui::Text("期待ModelPath: %s", stageModelPath_.c_str());

		if (!stageActorExists_)
		{
			if (ImGui::Button("Stage Actor生成")) SpawnImportedStageActor();
		}
		else
		{
			if (ImGui::Button("Stage Actor削除")) DestroyImportedStageActor();
		}
		ImGui::SameLine();
		ImGui::TextColored(
			stageActorExists_ && modelComponentExists_ ? ImVec4(0.35f, 1.0f, 0.45f, 1.0f) : ImVec4(1.0f, 0.72f, 0.25f, 1.0f),
			"%s",
			spawnMessage_.empty() ? "未実行" : spawnMessage_.c_str());
		ImGui::TextDisabled("SceneLevelLoaderのWorld全消去は使わず、生成済みActor Dataだけを既存ActorWorldへ追加します。");
	}

	void DrawLevelSummary() const
	{
		const nlohmann::json& level = importResult_.levelJson;
		ImGui::SeparatorText("生成Ken4lowLevel");
		ImGui::Text("Format: %s", level.value("Format", std::string{}).c_str());
		ImGui::Text("Version: %d", level.value("Version", 0));
		ImGui::Text("Name: %s", level.value("Name", std::string{}).c_str());
		ImGui::Text("Actors: %zu", level["Actors"].size());
		ImGui::Text("Source Manifest Objects: %zu", level["ImportSource"]["Objects"].size());
	}

	void DrawActorPreview() const
	{
		const nlohmann::json& actorEntry = importResult_.levelJson["Actors"][0];
		const nlohmann::json& actorData = actorEntry["Data"];
		const nlohmann::json& components = actorData["Components"];

		ImGui::SeparatorText("Actor / Component Preview");
		if (ImGui::BeginTable("Phase3ActorPreview", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
		{
			DrawKeyValueRow("Actor Id", actorEntry.value("Id", std::string{}));
			DrawKeyValueRow("Actor Class", actorData.value("Class", std::string{}));
			DrawKeyValueRow("Actor Name", actorData.value("Name", std::string{}));
			DrawKeyValueRow("Layer", actorData.value("Layer", std::string{}));
			DrawKeyValueRow("Component Class", components[0].value("Class", std::string{}));
			DrawKeyValueRow("Component Type", components[0].value("Type", std::string{}));
			DrawKeyValueRow("ModelPath", components[0].value("ModelPath", std::string{}));
			ImGui::EndTable();
		}
	}

	void DrawImportManifestPreview() const
	{
		const nlohmann::json& objects = importResult_.levelJson["ImportSource"]["Objects"];
		ImGui::SeparatorText("ImportSource Manifest Preview");
		ImGui::BeginChild("Phase3ImportManifest", ImVec2(0.0f, 260.0f), true);
		const std::size_t previewCount = (std::min)(static_cast<std::size_t>(12), objects.size());
		for (std::size_t index = 0; index < previewCount; ++index)
		{
			const nlohmann::json& object = objects[index];
			ImGui::Text("%s [%s] Parent=%s",
				object.value("Path", std::string{}).c_str(),
				object.value("Type", std::string{}).c_str(),
				object.value("ParentPath", std::string{}).c_str());
		}
		ImGui::EndChild();
	}

	static void DrawKeyValueRow(const char* key, const std::string& value)
	{
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::TextUnformatted(key);
		ImGui::TableSetColumnIndex(1);
		ImGui::TextUnformatted(value.c_str());
	}
#endif // USE_IMGUI

private:
	K4E::ActorWorld* actorWorld_ = nullptr;
	std::string sourceJsonPath_ = "stages/fps_stage00.json";
	std::string stageModelPath_ = "Stages/fps_stage00.gltf";
	std::string importedActorName_ = "Imported_fps_stage00";
	std::string lastMessage_;
	std::string spawnMessage_;
	bool stageActorExists_ = false;
	bool modelComponentExists_ = false;
	K4E::BlenderSceneLoader::Result blenderResult_;
	K4E::BlenderLevelImporter::Result importResult_;
};
