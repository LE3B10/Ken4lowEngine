#pragma once

namespace Ken4lowEngine
{
	class PostEffectManager;

	/// <summary>
	/// PostEffectManagerが保持するポストエフェクト設定をImGuiで表示・編集するエディタ専用パネルです。<br/>
	/// RenderTarget、BackBuffer出力、ResourceBarrier、CommandListには触れず、描画結果に影響しないUI責務だけを
	/// PostEffectManagerから分離します。
	/// </summary>
	class PostEffectEditorPanel
	{
	public:
		/// <summary>
		/// Post Effect Settingsウィンドウを描画します。<br/>
		/// 既存のPostEffectManager::ImGuiRender互換入口から呼ばれ、エフェクトON/OFFと各エフェクト固有UIだけを扱います。
		/// </summary>
		void Draw(PostEffectManager& postEffectManager, bool* pOpen = nullptr);
	};
}
