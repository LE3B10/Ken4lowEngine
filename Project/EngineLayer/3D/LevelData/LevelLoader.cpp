#include "LevelLoader.h"
#include <fstream>
#include <json.hpp>
#include <cassert>
#include <iostream>

using json = nlohmann::json;

std::unique_ptr<LevelData> LevelLoader::Load(const std::string& filepath)
{
	std::ifstream file(filepath);
	if (file.fail())
	{
		std::cerr << "ファイルを開くことができません: " << filepath << std::endl;
		assert(false && "ファイルを開くことができません");
	}

	// JSONファイルの読み込み
	nlohmann::json deserialised;
	try
	{
		file >> deserialised;
	}
	catch (const std::exception&)
	{
		std::cerr << "JSONの読み込みに失敗しました: " << filepath << std::endl;
		assert(false && "JSONの読み込みに失敗しました");
	}

	assert(deserialised.is_object() && "JSONの形式が正しくありません");
	assert(deserialised.contains("name") && "JSONに'name'キーが含まれていません");
	assert(deserialised["name"].is_string() && "'name'キーの値がオブジェクトではありません");

	std::string name = deserialised["name"].get<std::string>();
	assert(name.compare("scene") == 0);

	std::unique_ptr<LevelData> levelData = std::make_unique<LevelData>();
	levelData->objects.reserve(deserialised["objects"].size());
	for (nlohmann::json& object : deserialised["objects"])
	{
		assert(object.contains("type"));
		std::string type = object["type"].get<std::string>();

		if (type.compare("MESH") == 0)
		{
			levelData->objects.emplace_back(ObjectData{});
			ObjectData& objectData = levelData->objects.back();

			if (object.contains("name"))
			{
				objectData.fileName = object["name"];
			}

			nlohmann::json& transform = object["transform"];
			objectData.position.x = static_cast<float>(transform["translation"][0]);
			objectData.position.y = static_cast<float>(transform["translation"][2]);
			objectData.position.z = static_cast<float>(transform["translation"][1]);

			objectData.rotation.x = -static_cast<float>(transform["rotation"][0]);
			objectData.rotation.y = -static_cast<float>(transform["rotation"][2]);
			objectData.rotation.z = -static_cast<float>(transform["rotation"][1]);

			objectData.scale.x = static_cast<float>(transform["scaling"][0]);
			objectData.scale.y = static_cast<float>(transform["scaling"][2]);
			objectData.scale.z = static_cast<float>(transform["scaling"][1]);
		}
	}

	return levelData;
}
