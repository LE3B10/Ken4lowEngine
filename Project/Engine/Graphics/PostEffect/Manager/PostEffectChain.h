#pragma once

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Ken4lowEngine
{
	/// <summary>
	/// PostEffectの実行順序と有効判定だけを管理する薄いチェーン定義です。<br/>
	/// RenderTarget、BackBuffer、ResourceBarrier、CommandListには触れず、将来Bloom/ToneMapping/SSRなどを
	/// 順序付きで追加しやすくするためにPostEffectManagerから順序管理責務だけを分離します。
	/// </summary>
	class PostEffectChain
	{
	public:
		/// <summary>
		/// 登録済みEffect順序を初期化します。<br/>
		/// PostEffectManager::Finalizeや再初期化時に、古い順序情報を残さないために呼び出します。
		/// </summary>
		void Clear();

		/// <summary>
		/// Effect名と実行順序を登録します。<br/>
		/// 既存のeffectTableに書かれているorder値をそのまま受け取り、従来の適用順を維持します。
		/// </summary>
		void RegisterEffect(const std::string& name, int order);

		/// <summary>
		/// 現在有効なEffect名だけを、登録順序に従って返します。<br/>
		/// UI用の有効フラグと外部API用の強制有効フラグを従来通りOR判定し、描画結果を変えないようにします。
		/// </summary>
		std::vector<std::string> BuildActiveEffectNames(
			const std::unordered_map<std::string, bool>& editorEnabledFlags,
			const std::unordered_map<std::string, bool>& runtimeEnabledFlags) const;

	private:
		std::vector<std::pair<std::string, int>> effectOrder_; // Effect名と適用順だけを保持し、描画リソースは所有しない。
	};
}
