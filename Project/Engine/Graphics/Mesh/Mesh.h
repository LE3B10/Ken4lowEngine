#pragma once
#include "DX12Include.h"
#include "VertexData.h"

#include <vector>

namespace Ken4lowEngine
{

	/// -------------------------------------------------------------
	///				　	　メッシュデータクラス
	/// -------------------------------------------------------------
	class Mesh
	{
	public: /// ---------- メンバ関数 ---------- ///

		/// <summary>
		/// モデルの頂点データとインデックスを使って初期化を行います。
		/// </summary>
		/// <param name="modelVertices">初期化に使用する頂点データの配列（読み取り専用の参照）。</param>
		/// <param name="modelIndices">頂点の順序やプリミティブを示すインデックスの配列（読み取り専用の参照）。</param>
		void Initialize(const std::vector<VertexData>& modelVertices, const std::vector<uint32_t>& modelIndices);

		/// <summary>
		/// 描画処理
		/// </summary>
		void Draw();

		/// <summary>
		/// 同じメッシュを指定数まとめてインスタンシング描画します。
		/// </summary>
		void DrawInstanced(UINT instanceCount);

	public: /// ---------- アクセッサ ---------- ///

		/// <summary>
		/// メッシュのインデックス数を取得します。
		/// </summary>
		uint32_t GetIndexCount() const { return static_cast<uint32_t>(indices.size()); }

		/// <summary>
		/// メッシュの頂点数を取得します。
		/// </summary>
		uint32_t GetVertexCount() const { return static_cast<uint32_t>(vertices.size()); }

	private: /// ---------- メンバ変数 ---------- ///

		// 頂点バッファ
		ComPtr<ID3D12Resource> vertexResource;

		// 頂点リソースにデータを書き込む
		D3D12_VERTEX_BUFFER_VIEW vertexBufferView = {};

		// 頂点リソース内のデータを指すポインタ
		std::vector<VertexData> vertices = {};

		// インデックスバッファ
		ComPtr<ID3D12Resource> indexResource = nullptr;
		D3D12_INDEX_BUFFER_VIEW indexBufferView = {};
		std::vector<uint32_t> indices = {};
	};


} // namespace Ken4lowEngine
