#pragma once

#include <LevelData.h>
#include <LevelLoader.h>

#include <algorithm>
#include <cstddef>
#include <iostream>
#include <memory>
#include <string>

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

namespace K4E = ::Ken4lowEngine;

/// DebugSceneだけで既存Blender JSON -> LevelDataの読み込み結果を確認する読み取り専用診断。
class LevelDataValidation
{
public:
	struct Summary
	{
		std::size_t objectCount = 0;
		std::size_t staticMeshCount = 0;
		std::size_t colliderCount = 0;
		std::size_t spawnPointCount = 0;
		std::size_t knownPropertyObjectCount = 0;
		bool parentHierarchyPreserved = false;
		bool arbitraryCustomPropertiesPreserved = false;
	};

	LevelDataValidation()
	{
		Load("stages/fps_stage00.json");
	}

	bool Load(const std::string& relativePath)
	{
		sourcePath_ = relativePath;
		levelData_ = K4E::LevelLoader::LoadLevel(relativePath);
		summary_ = {};

		if (!levelData_)
		{
			std::cerr << "[LevelDataValidation] Failed to load: " << sourcePath_ << '\n';
			return false;
		}

		for (const K4E::ObjectData& object : levelData_->objects)
		{
			++summary_.objectCount;
			if (IsStaticMeshType(object.type)) ++summary_.staticMeshCount;
			if (object.collider.enabled) ++summary_.colliderCount;
			if (IsSpawnPointType(object.type)) ++summary_.spawnPointCount;
			if (HasKnownProperties(object)) ++summary_.knownPropertyObjectCount;
		}

		// 現行LevelDataがBlender由来情報をどこまで保持しているかをDebug出力で固定確認する。
		WriteSummaryToLog();
		return true;
	}

	void DrawImGui()
	{
#ifdef USE_IMGUI
		if (!ImGui::Begin("LevelData 検証"))
		{
			ImGui::End();
			return;
		}

		ImGui::Text("読み込みファイル: %s", sourcePath_.c_str());
		if (ImGui::Button("再読み込み"))
		{
			Load(sourcePath_);
		}

		ImGui::SeparatorText("読み込み結果");
		ImGui::Text("Object: %zu", summary_.objectCount);
		ImGui::Text("StaticMesh: %zu", summary_.staticMeshCount);
		ImGui::Text("Collider: %zu", summary_.colliderCount);
		ImGui::Text("SpawnPoint: %zu", summary_.spawnPointCount);
		ImGui::Text("KnownProps: %zu", summary_.knownPropertyObjectCount);

		ImGui::SeparatorText("現在のLevelData保持状況");
		DrawSupportState("親子関係", summary_.parentHierarchyPreserved);
		DrawSupportState("任意Custom Properties", summary_.arbitraryCustomPropertiesPreserved);
		ImGui::TextDisabled("親Transformは合成済みですが、親子関係そのものはLevelDataに保持されません。");
		ImGui::TextDisabled("C++側で定義済みのpropsだけが現在のLevelDataへ保存されます。");

		if (levelData_)
		{
			DrawObjectPreview();
			DrawSpecialObjectPreview();
		}

		ImGui::End();
#endif // USE_IMGUI
	}

	const Summary& GetSummary() const noexcept { return summary_; }
	const K4E::LevelData* GetLevelData() const noexcept { return levelData_.get(); }
	const std::string& GetSourcePath() const noexcept { return sourcePath_; }

private:
	static bool IsStaticMeshType(const std::string& type)
	{
		return type == "MESH" || type == "StaticMesh";
	}

	static bool IsSpawnPointType(const std::string& type)
	{
		return type == "PlayerSpawnPoint" ||
			type == "EnemySpawnPoint" ||
			type == "BossSpawnPoint";
	}

	static bool HasKnownProperties(const K4E::ObjectData& object)
	{
		return object.hasSpawnProps ||
			object.hasIntroCameraProps ||
			object.hasDeviceObjectiveProps ||
			object.hasDefenseTargetProps ||
			object.hasEscapePointProps ||
			object.hasBossPhaseTriggerProps;
	}

#ifdef USE_IMGUI
	static void DrawSupportState(const char* label, bool supported)
	{
		ImGui::Text("%s: %s", label, supported ? "保持" : "未保持");
	}

	void DrawObjectPreview() const
	{
		constexpr std::size_t kPreviewCount = 10;
		const std::size_t previewCount = std::min(kPreviewCount, levelData_->objects.size());

		ImGui::SeparatorText("Object Preview");
		if (ImGui::BeginTable("LevelDataObjectPreview", 6,
			ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY,
			ImVec2(0.0f, 220.0f)))
		{
			ImGui::TableSetupColumn("Name");
			ImGui::TableSetupColumn("Type");
			ImGui::TableSetupColumn("Model");
			ImGui::TableSetupColumn("Position");
			ImGui::TableSetupColumn("Rotation");
			ImGui::TableSetupColumn("Scale");
			ImGui::TableHeadersRow();

			for (std::size_t index = 0; index < previewCount; ++index)
			{
				const K4E::ObjectData& object = levelData_->objects[index];
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::TextUnformatted(object.name.c_str());
				ImGui::TableSetColumnIndex(1);
				ImGui::TextUnformatted(object.type.c_str());
				ImGui::TableSetColumnIndex(2);
				ImGui::TextUnformatted(object.modelName.c_str());
				ImGui::TableSetColumnIndex(3);
				ImGui::Text("%.2f, %.2f, %.2f", object.position.x, object.position.y, object.position.z);
				ImGui::TableSetColumnIndex(4);
				ImGui::Text("%.2f, %.2f, %.2f", object.rotation.x, object.rotation.y, object.rotation.z);
				ImGui::TableSetColumnIndex(5);
				ImGui::Text("%.2f, %.2f, %.2f", object.scale.x, object.scale.y, object.scale.z);
			}
			ImGui::EndTable();
		}
	}

	void DrawSpecialObjectPreview() const
	{
		constexpr std::size_t kPreviewCount = 10;
		std::size_t specialCount = 0;

		ImGui::SeparatorText("Collider / Known Props Preview");
		for (const K4E::ObjectData& object : levelData_->objects)
		{
			if (!object.collider.enabled && !HasKnownProperties(object)) continue;

			ImGui::PushID(static_cast<int>(specialCount));
			if (ImGui::TreeNode("%s [%s]", object.name.c_str(), object.type.c_str()))
			{
				if (object.collider.enabled)
				{
					ImGui::Text("Collider Type: %s", object.collider.type.c_str());
					ImGui::Text("Collision Type: %s / Id: %d", object.collider.collisionType.c_str(), object.collider.collisionTypeId);
					ImGui::Text("Center: %.2f, %.2f, %.2f", object.collider.center.x, object.collider.center.y, object.collider.center.z);
					ImGui::Text("Size: %.2f, %.2f, %.2f", object.collider.size.x, object.collider.size.y, object.collider.size.z);
				}
				if (object.hasSpawnProps)
				{
					ImGui::Text("Spawn: Wave=%d Group=%d Count=%d", object.spawnProps.wave, object.spawnProps.group, object.spawnProps.count);
					ImGui::Text("Archetype=%s EnemyType=%s", object.spawnProps.archetype.c_str(), object.spawnProps.enemyType.c_str());
				}
				if (object.hasIntroCameraProps) ImGui::Text("IntroCamera Props: あり");
				if (object.hasDeviceObjectiveProps) ImGui::Text("DeviceObjective Props: あり");
				if (object.hasDefenseTargetProps) ImGui::Text("DefenseTarget Props: あり");
				if (object.hasEscapePointProps) ImGui::Text("EscapePoint Props: あり");
				if (object.hasBossPhaseTriggerProps) ImGui::Text("BossPhaseTrigger Props: あり");
				ImGui::TreePop();
			}
			ImGui::PopID();

			if (++specialCount >= kPreviewCount) break;
		}
	}
#endif // USE_IMGUI

	static void WriteObjectTransformToLog(std::size_t index, const K4E::ObjectData& object)
	{
		std::clog
			<< "  [" << index << "]"
			<< " Name=" << object.name
			<< " Type=" << object.type
			<< " Model=" << object.modelName
			<< " Pos=(" << object.position.x << ", " << object.position.y << ", " << object.position.z << ")"
			<< " Rot=(" << object.rotation.x << ", " << object.rotation.y << ", " << object.rotation.z << ")"
			<< " Scale=(" << object.scale.x << ", " << object.scale.y << ", " << object.scale.z << ")"
			<< " Collider=" << (object.collider.enabled ? "true" : "false")
			<< '\n';
	}

	static void WriteSpecialObjectToLog(const K4E::ObjectData& object)
	{
		std::clog << "    Name=" << object.name << " Type=" << object.type;

		if (object.collider.enabled)
		{
			std::clog
				<< " Collider{Type=" << object.collider.type
				<< ", CollisionType=" << object.collider.collisionType
				<< ", Id=" << object.collider.collisionTypeId
				<< ", Center=(" << object.collider.center.x << ", " << object.collider.center.y << ", " << object.collider.center.z << ")"
				<< ", Size=(" << object.collider.size.x << ", " << object.collider.size.y << ", " << object.collider.size.z << ")"
				<< ", Rotation=(" << object.collider.rotation.x << ", " << object.collider.rotation.y << ", " << object.collider.rotation.z << ")}";
		}

		if (object.hasSpawnProps)
		{
			std::clog
				<< " SpawnProps{Wave=" << object.spawnProps.wave
				<< ", Group=" << object.spawnProps.group
				<< ", Count=" << object.spawnProps.count
				<< ", Archetype=" << object.spawnProps.archetype
				<< ", EnemyType=" << object.spawnProps.enemyType << "}";
		}
		if (object.hasIntroCameraProps)
		{
			std::clog
				<< " IntroCameraProps{Order=" << object.introCameraProps.order
				<< ", Duration=" << object.introCameraProps.duration
				<< ", Fov=" << object.introCameraProps.fov
				<< ", Target=" << object.introCameraProps.targetName << "}";
		}
		if (object.hasDeviceObjectiveProps) std::clog << " DeviceObjectiveProps=true";
		if (object.hasDefenseTargetProps) std::clog << " DefenseTargetProps=true";
		if (object.hasEscapePointProps) std::clog << " EscapePointProps=true";
		if (object.hasBossPhaseTriggerProps) std::clog << " BossPhaseTriggerProps=true";
		std::clog << '\n';
	}

	void WriteSummaryToLog() const
	{
		std::clog
			<< "[LevelDataValidation] Source=" << sourcePath_ << '\n'
			<< "  Objects=" << summary_.objectCount
			<< " StaticMesh=" << summary_.staticMeshCount
			<< " Collider=" << summary_.colliderCount
			<< " SpawnPoint=" << summary_.spawnPointCount
			<< " KnownProps=" << summary_.knownPropertyObjectCount << '\n'
			<< "  ParentHierarchyPreserved=false"
			<< " ArbitraryCustomPropertiesPreserved=false\n";

		if (!levelData_) return;

		constexpr std::size_t kPreviewCount = 10;
		const std::size_t previewCount = (std::min)(kPreviewCount, levelData_->objects.size());
		std::clog << "  ObjectPreview(first " << previewCount << "):\n";
		for (std::size_t index = 0; index < previewCount; ++index)
		{
			WriteObjectTransformToLog(index, levelData_->objects[index]);
		}

		std::clog << "  ColliderAndKnownPropsPreview(first " << kPreviewCount << "):\n";
		std::size_t specialCount = 0;
		for (const K4E::ObjectData& object : levelData_->objects)
		{
			if (!object.collider.enabled && !HasKnownProperties(object)) continue;
			WriteSpecialObjectToLog(object);
			if (++specialCount >= kPreviewCount) break;
		}
	}

private:
	std::string sourcePath_;
	std::unique_ptr<K4E::LevelData> levelData_;
	Summary summary_;
};
