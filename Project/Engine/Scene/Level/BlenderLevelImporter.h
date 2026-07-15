#pragma once

#include "BlenderSceneData.h"

#include <json.hpp>

#include <cstddef>
#include <string>

namespace Ken4lowEngine
{
	/// BlenderSceneDataをActor/Componentで復元可能なKen4lowLevel JSONへ変換するImporter。
	class BlenderLevelImporter
	{
	public:
		struct Options
		{
			std::string levelName = "ImportedBlenderLevel";
			std::string sourceJsonPath;
			std::string stageModelPath;
			std::string stageActorName = "ImportedStage";
			std::string stageActorId = "ImportedStageActor";
		};

		struct Result
		{
			bool succeeded = false;
			nlohmann::json levelJson = nlohmann::json::object();
			std::size_t sourceObjectCount = 0;
			std::size_t sourceMeshCount = 0;
			std::size_t importedActorCount = 0;
			std::string message;
		};

		static Result Import(const BlenderSceneData& source, const Options& options)
		{
			Result result{};
			if (options.stageModelPath.empty())
			{
				result.message = "一体型ステージをActor化するためのStageModelPathが空です。";
				return result;
			}

			nlohmann::json sourceObjects = nlohmann::json::array();
			for (const BlenderObjectData& object : source.objects)
			{
				AppendSourceObject(object, std::string{}, 0, sourceObjects, result);
			}

			const nlohmann::json stageActor = BuildIntegratedStageActor(options);
			result.levelJson = {
				{ "Format", "Ken4lowLevel" },
				{ "Version", 1 },
				{ "Name", options.levelName },
				{ "LevelSettings", {
					{ "ImportMode", "IntegratedStageModel" },
					{ "SourceJson", options.sourceJsonPath },
					{ "StageModelPath", options.stageModelPath },
				} },
				{ "Actors", nlohmann::json::array({ stageActor }) },
				{ "Lighting", nlohmann::json::object() },
				{ "Camera", nlohmann::json::object() },
				{ "Environment", nlohmann::json::object() },
				{ "ImportSource", {
					{ "SchemaVersion", source.schemaVersion },
					{ "SceneName", source.name },
					{ "ObjectCount", result.sourceObjectCount },
					{ "MeshCount", result.sourceMeshCount },
					{ "Objects", std::move(sourceObjects) },
				} },
			};

			result.importedActorCount = result.levelJson["Actors"].size();
			result.succeeded = true;
			result.message = "BlenderSceneDataを一体型Stage ActorのKen4lowLevelデータへ変換しました。";
			return result;
		}

	private:
		static nlohmann::json BuildIntegratedStageActor(const Options& options)
		{
			// 現行Stageと同じく一体型GLTFを1回だけ描画し、28個のMESHごとの重複描画を防ぐ。
			const nlohmann::json modelComponent = {
				{ "Active", true },
				{ "Class", "ModelComponent" },
				{ "DrawOrder", 0 },
				{ "LocalPosition", { 0.0f, 0.0f, 0.0f } },
				{ "LocalRotation", { 0.0f, 0.0f, 0.0f } },
				{ "LocalScale", { 1.0f, 1.0f, 1.0f } },
				{ "ModelPath", options.stageModelPath },
				{ "Name", "Stage Model Component" },
				{ "Parent", "" },
				{ "Type", "SceneComponent" },
				{ "UpdateOrder", 0 },
				{ "Visible", true },
			};

			return {
				{ "Id", options.stageActorId },
				{ "ParentId", "" },
				{ "Editor", {
					{ "Visible", true },
					{ "Locked", false },
					{ "Folder", "Imported/Stage" },
				} },
				{ "Data", {
					{ "Active", true },
					{ "Class", "Actor" },
					{ "Components", nlohmann::json::array({ modelComponent }) },
					{ "Layer", "WorldStatic" },
					{ "Name", options.stageActorName },
					{ "Tags", nlohmann::json::array({ "BlenderImported", "StageVisual" }) },
				} },
			};
		}

		static void AppendSourceObject(
			const BlenderObjectData& object,
			const std::string& parentPath,
			std::size_t depth,
			nlohmann::json& outObjects,
			Result& result)
		{
			++result.sourceObjectCount;
			if (object.type == "MESH" || object.type == "StaticMesh") ++result.sourceMeshCount;

			const std::string objectPath = parentPath.empty() ? object.name : parentPath + "/" + object.name;
			nlohmann::json record = {
				{ "Path", objectPath },
				{ "ParentPath", parentPath },
				{ "Depth", depth },
				{ "Name", object.name },
				{ "Type", object.type },
				{ "Collection", object.collection },
				{ "Collections", object.collections },
				{ "Disabled", object.disabled },
				{ "SourceTransform", {
					{ "Translation", { object.transform.translation.x, object.transform.translation.y, object.transform.translation.z } },
					{ "Rotation", { object.transform.rotation.x, object.transform.rotation.y, object.transform.rotation.z } },
					{ "Scale", { object.transform.scaling.x, object.transform.scaling.y, object.transform.scaling.z } },
				} },
			};
			if (!object.properties.empty()) record["Properties"] = object.properties;
			if (!object.collider.empty()) record["Collider"] = object.collider;
			outObjects.push_back(std::move(record));

			for (const BlenderObjectData& child : object.children)
			{
				AppendSourceObject(child, objectPath, depth + 1, outObjects, result);
			}
		}
	};
} // namespace Ken4lowEngine
