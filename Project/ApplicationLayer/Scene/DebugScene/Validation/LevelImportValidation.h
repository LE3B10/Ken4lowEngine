#pragma once

#include <BlenderLevelImporter.h>
#include <BlenderSceneLoader.h>

#include <algorithm>
#include <string>

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

namespace K4E = ::Ken4lowEngine;

/// DebugSceneだけでBlenderSceneData -> Ken4lowLevel変換結果を確認する読み取り専用診断。
class LevelImportValidation
{
public:
	LevelImportValidation()
	{
		Reload();
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
		options.stageActorName = "Imported_fps_stage00";
		options.stageActorId = "ImportedStageActor";
		importResult_ = K4E::BlenderLevelImporter::Import(blenderResult_.scene, options);
		lastMessage_ = importResult_.message;
	}

	void DrawImGui()
	{
#ifdef USE_IMGUI
		if (!ImGui::Begin("LevelImport 検証"))
		{
			ImGui::End();
			return;
		}

		ImGui::Text("Source JSON: %s", sourceJsonPath_.c_str());
		ImGui::Text("Stage Model: %s", stageModelPath_.c_str());
		if (ImGui::Button("Phase 3 再変換")) Reload();
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
		ImGui::TextDisabled("Blenderの55 Object / 28 MeshはImportSource Manifestへ保持し、Phase 4ではまだ生成しません。");

		if (!importResult_.succeeded)
		{
			ImGui::End();
			return;
		}

		DrawLevelSummary();
		DrawActorPreview();
		DrawImportManifestPreview();
		ImGui::End();
#endif // USE_IMGUI
	}

	const K4E::BlenderLevelImporter::Result& GetImportResult() const noexcept { return importResult_; }

private:
#ifdef USE_IMGUI
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
	std::string sourceJsonPath_ = "stages/fps_stage00.json";
	std::string stageModelPath_ = "Stages/fps_stage00.gltf";
	std::string lastMessage_;
	K4E::BlenderSceneLoader::Result blenderResult_;
	K4E::BlenderLevelImporter::Result importResult_;
};
