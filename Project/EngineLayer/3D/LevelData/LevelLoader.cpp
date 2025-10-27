#include "LevelLoader.h"
#include <fstream>
#include <json.hpp>
#include <cassert>
#include <iostream>

/// ---------- jsonのエイリアス ---------- ///
using json = nlohmann::json;


/// --------------------------------------------------------------
///				　		レベルデータの読み込み
/// --------------------------------------------------------------
std::unique_ptr<LevelData> LevelLoader::LoadLevel(const std::string& filePath)
{
	// フルパスの作成
	std::string fullPath = fileDirectory_ + filePath;

	// ファイルストリームの作成
	std::ifstream file(fullPath);

	// ファイルが開けない場合のエラーチェック
	if (file.fail())
	{
		std::cerr << "ファイルを開くことができません: " << fullPath << std::endl;
		assert(false && "ファイルを開くことができません");
	}

	// JSONファイルの読み込み
	nlohmann::json deserialised;

	// JSONのパース処理
	try
	{
		// ファイルからJSONを読み込む
		file >> deserialised;
	}
	catch (const std::exception&)
	{
		// JSONのパースに失敗した場合の処理
		std::cerr << "JSONの読み込みに失敗しました: " << fullPath << std::endl;
		assert(false && "JSONの読み込みに失敗しました");
	}

	assert(deserialised.is_object() && "JSONの形式が正しくありません");
	assert(deserialised.contains("name") && "JSONに'name'キーが含まれていません");
	assert(deserialised["name"].is_string() && "'name'キーの値がオブジェクトではありません");

	// レベル名の取得と確認
	std::string name = deserialised["name"].get<std::string>();
	assert(name.compare("scene") == 0);

	// オブジェクトの配列が存在することを確認
	std::unique_ptr<LevelData> levelData = std::make_unique<LevelData>();

	// オブジェクトの配列が存在することを確認
	levelData->objects.reserve(deserialised["objects"].size());

	// オブジェクトの配列をループして処理
	for (nlohmann::json& object : deserialised["objects"])
	{
		// オブジェクトが正しい形式であることを確認
		assert(object.contains("type"));
		std::string type = object["type"].get<std::string>(); // オブジェクトのタイプを取得

		if (object.contains("disable"))
		{
			// オブジェクトが無効化されている場合はスキップ
			bool disable = object["disable"].get<bool>();
			if (disable) continue;
		}

		// オブジェクトのタイプに応じて処理を分岐
		if (type.compare("MESH") == 0)
		{
			// MESHタイプのオブジェクトを処理
			levelData->objects.emplace_back(ObjectData{});
			ObjectData& objectData = levelData->objects.back();

			objectData.type = type; // オブジェクトのタイプを設定

			// オブジェクトの名前の取得
			if (object.contains("name"))
			{
				objectData.name = object["name"];
			}

			// transform情報の取得
			nlohmann::json& transform = object["transform"];
			objectData.position.x = static_cast<float>(transform["translation"][0]);
			objectData.position.y = static_cast<float>(transform["translation"][2]);
			objectData.position.z = static_cast<float>(transform["translation"][1]);

			// 回転情報の取得
			objectData.rotation.x = -static_cast<float>(transform["rotation"][0]);
			objectData.rotation.y = -static_cast<float>(transform["rotation"][2]);
			objectData.rotation.z = -static_cast<float>(transform["rotation"][1]);

			// スケール情報の取得
			objectData.scale.x = static_cast<float>(transform["scaling"][0]);
			objectData.scale.y = static_cast<float>(transform["scaling"][2]);
			objectData.scale.z = static_cast<float>(transform["scaling"][1]);
		}

		// コライダー情報の有無を確認
		if (object.contains("collider"))
		{
			auto& jc = object["collider"];
			ObjectData& objectData = levelData->objects.back();
			objectData.collider.enabled = true;
			objectData.collider.type = jc["type"].get<std::string>();

			// center (X, Z→Y, Y→Z)
			objectData.collider.center.x = -(float)jc["center"][0];
			objectData.collider.center.y = (float)jc["center"][2];
			objectData.collider.center.z = -(float)jc["center"][1];

			if (jc.contains("size"))
			{
				objectData.collider.size.x = (float)jc["size"][0];
				objectData.collider.size.y = (float)jc["size"][2];
				objectData.collider.size.z = (float)jc["size"][1];
			}
		}
	}

	return levelData;
}
