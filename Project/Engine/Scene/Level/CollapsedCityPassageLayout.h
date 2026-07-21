#pragma once

#include "LevelData.h"

#include <vector>

namespace Ken4lowEngine
{
	/// 静的な仮設橋は生成せず、Stage4OpeningBridgeRuntimeの動的な展開橋へ置き換える。
	class CollapsedCityPassageLayout final
	{
	public:
		static std::vector<ObjectData> Build(const LevelData&)
		{
			return {}; // 起動前から渡れてしまう床とColliderを外し、序盤ギミック完了後だけ橋を通行可能にする。
		}
	};
} // namespace Ken4lowEngine
