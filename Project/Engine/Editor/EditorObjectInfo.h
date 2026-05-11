#pragma once

#include <cstdint>
#include <string>

namespace Ken4lowEngine
{

	/// <summary>
	/// World OutlinerとDetailsへ安全に渡すための軽量なエディタ表示用オブジェクト情報です。
	/// </summary>
	struct EditorObjectInfo
	{
		uint64_t id = 0;
		std::string displayName;
		std::string typeName;
		std::string sceneName;
	};

} // namespace Ken4lowEngine
