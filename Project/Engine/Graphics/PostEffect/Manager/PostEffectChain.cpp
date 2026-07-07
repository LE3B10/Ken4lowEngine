#include "PostEffectChain.h"

#include <algorithm>

namespace Ken4lowEngine
{
	void PostEffectChain::Clear()
	{
		effectOrder_.clear();
	}

	void PostEffectChain::RegisterEffect(const std::string& name, int order)
	{
		// effectTableのorder値をそのまま登録し、既存のPostEffect適用順を保持する。
		effectOrder_.emplace_back(name, order);
	}

	std::vector<std::string> PostEffectChain::BuildActiveEffectNames(
		const std::unordered_map<std::string, bool>& editorEnabledFlags,
		const std::unordered_map<std::string, bool>& runtimeEnabledFlags) const
	{
		std::vector<std::pair<std::string, int>> sortedEffects = effectOrder_;
		std::sort(sortedEffects.begin(), sortedEffects.end(),
			[](const auto& lhs, const auto& rhs) { return lhs.second < rhs.second; });

		std::vector<std::string> activeNames;
		activeNames.reserve(sortedEffects.size());
		for (const auto& [name, order] : sortedEffects)
		{
			(void)order;
			const auto editorIt = editorEnabledFlags.find(name);
			const auto runtimeIt = runtimeEnabledFlags.find(name);
			const bool editorEnabled = (editorIt != editorEnabledFlags.end()) ? editorIt->second : false;
			const bool runtimeEnabled = (runtimeIt != runtimeEnabledFlags.end()) ? runtimeIt->second : false;
			if (!(editorEnabled || runtimeEnabled))
			{
				continue;
			}

			// ここでは名前だけを返し、実際のApplyやBarrier順序はPostEffectManager側に残す。
			activeNames.push_back(name);
		}
		return activeNames;
	}
}
