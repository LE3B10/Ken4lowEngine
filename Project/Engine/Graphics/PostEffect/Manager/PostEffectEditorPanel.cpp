#include "PostEffectEditorPanel.h"

#include "PostEffectManager.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

namespace Ken4lowEngine
{
	void PostEffectEditorPanel::Draw(PostEffectManager& postEffectManager, bool* pOpen)
	{
#ifdef USE_IMGUI
		// WindowメニューのPost Effect Settings表示フラグが閉じている間は、既存通りUIを生成しない。
		if (pOpen != nullptr && !*pOpen)
		{
			return;
		}

		ImGui::Begin("Post Effect Settings", pOpen);

		for (const auto& [name, category] : postEffectManager.effectCategory_)
		{
			(void)category;
			ImGui::Checkbox(name.c_str(), &postEffectManager.effectEnabled_[name]);
			if (postEffectManager.effectEnabled_[name])
			{
				// 各エフェクトのパラメータUIは既存Effect実装に残し、Panelは表示順と開閉だけを担当する。
				postEffectManager.postEffects_[name]->DrawImGui();
			}
		}

		ImGui::End();
#else
		(void)postEffectManager;
		(void)pOpen;
#endif // USE_IMGUI
	}
}
