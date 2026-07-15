#pragma once

#include <LevelData.h>
#include <LevelLoader.h>

#include <algorithm>
#include <cstddef>
#include <iostream>
#include <memory>
#include <string>

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
		Load("stages/hajimarinoheigen.json");
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
		const std::size_t previewCount = std::min(kPreviewCount, levelData_->objects.size());
		for (std::size_t index = 0; index < previewCount; ++index)
		{
			const K4E::ObjectData& object = levelData_->objects[index];
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
	}

private:
	std::string sourcePath_;
	std::unique_ptr<K4E::LevelData> levelData_;
	Summary summary_;
};
