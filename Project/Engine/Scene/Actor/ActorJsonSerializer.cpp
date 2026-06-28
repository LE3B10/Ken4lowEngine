#include "ActorJsonSerializer.h"
#include "Actor.h"

#include <fstream>
#include <filesystem>
#include <json.hpp>

namespace Ken4lowEngine
{
	bool ActorJsonSerializer::SaveActorToFile(const Actor& actor, std::string_view filePath)
	{
		nlohmann::json actorJson;

		actor.ToJson(actorJson); // Actorの情報をJSONへ保存する

		const std::filesystem::path path(filePath);

		if (path.has_parent_path())
		{
			std::filesystem::create_directories(path.parent_path()); // 親ディレクトリが存在しない場合は作成する
		}

		std::ofstream file(path);
		if (!file.is_open())
		{
			return false; // ファイルが開けなかった場合は失敗を返す
		}

		file << actorJson.dump(4); // JSONデータをフォーマットしてファイルに書き込む
		return true;
	}
}