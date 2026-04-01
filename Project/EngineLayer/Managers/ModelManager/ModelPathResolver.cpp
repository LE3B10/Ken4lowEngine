#include "ModelPathResolver.h"

namespace Ken4lowEngine
{
	std::filesystem::path ModelPathResolver::ToSourcesPath(const std::string& logicalPath)
	{
		// Sources 側は元データ置き場
		// 例:
		//   "Characters/body.gltf"
		//    -> "Resources/Models/Sources/Characters/body.gltf"
		return std::filesystem::path("Resources/Models/Sources") / logicalPath;
	}

	std::filesystem::path ModelPathResolver::ToCompiledPath(const std::string& logicalPath)
	{
		// Compiled 側は変換後データ置き場
		// 論理パスの拡張子は .kmesh に置き換える
		// 例:
		//   "Characters/body.gltf"
		//    -> "Resources/Models/Compiled/Characters/body.kmesh"
		std::filesystem::path path = std::filesystem::path("Resources/Models/Compiled") / logicalPath;
		path.replace_extension(".kmesh");
		return path;
	}

	bool ModelPathResolver::ExistsCompiled(const std::string& logicalPath)
	{
		// Compiled 側に .kmesh があるかを調べる
		return std::filesystem::exists(ToCompiledPath(logicalPath));
	}

	bool ModelPathResolver::ExistsSource(const std::string& logicalPath)
	{
		// Sources 側に元モデルがあるかを調べる
		return std::filesystem::exists(ToSourcesPath(logicalPath));
	}
}