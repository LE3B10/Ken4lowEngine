#pragma once
#include <DX12Include.h>
#include "Vector3.h"
#include "Matrix4x4.h"

namespace Ken4lowEngine
{

	/// ---------- 前方宣言 ---------- ///
	class Camera;

	/// -------------------------------------------------------------
	///				　	ワールド変換データクラス
	/// -------------------------------------------------------------
	class WorldTransform
	{
	public: /// ---------- 構造体 ---------- ///

		/// <summary>
		/// シェーダーへ送る座標変換行列一式です。
		/// </summary>
		struct TransformationMatrix final
		{
			Matrix4x4 WVP;					  // ワールド・ビュー・プロジェクション行列
			Matrix4x4 World;				  // ワールド行列
			Matrix4x4 WorldInversedTranspose; // ワールド行列の逆転置行列
		};

	public: /// ---------- メンバ変数 ---------- ///

		// オブジェクト単体の拡大率。
		Vector3 scale_ = { 1.0f, 1.0f, 1.0f };

		// オブジェクト単体の回転角。
		Vector3 rotate_ = { 0.0f, 0.0f, 0.0f };

		// 親を持つ場合は親から見た位置、親がない場合はワールド上の位置。
		Vector3 translate_ = { 0.0f, 0.0f, 0.0f };

		// Update() 後に確定するワールド座標。
		Vector3 worldTranslate_ = { 0.0f, 0.0f, 0.0f };

		// 親の回転を含めたワールド回転角。
		Vector3 worldRotate_ = { 0.0f, 0.0f, 0.0f };

		// Update() 後に確定するワールド変換行列。
		Matrix4x4 matWorld_;

		// 親子付けを行う場合に参照する親のワールド変換。
		const WorldTransform* parent_ = nullptr;

	public: /// ---------- メンバ関数 ---------- ///

		/// <summary>
		/// WVP 用の定数バッファを作成し、行列データを単位行列で初期化します。
		/// </summary>
		void Initialize();

		/// <summary>
		/// ローカル変換と親のワールド行列から、GPU へ送る World / WVP / 逆転置行列を更新します。
		/// </summary>
		void Update();

		/// <summary>
		/// すでに合成済みのワールド行列をそのまま GPU へ反映します。
		/// Euler角では表しにくい親子関係やモデル補正を使いたい場合に使います。
		/// </summary>
		void UpdateWithWorldMatrix(const Matrix4x4& worldMatrix);

		/// <summary>
		/// 指定したルートパラメータへ WVP 定数バッファをバインドします。
		/// </summary>
		/// <param name="rootParameterIndex">WVP 定数バッファを設定するルートパラメータ番号。</param>
		void SetPipeline(UINT rootParameterIndex = 1);

		/// <summary>
		/// オブジェクトのワールド行列への定数参照を返します。
		/// </summary>
		/// <returns>Matrix4x4 型の、内部メンバ matWorld_ が保持するワールド行列への定数参照。</returns>
		const Matrix4x4& GetWorldMatrix() const { return matWorld_; }

	private: /// ---------- メンバ変数 ---------- ///

		// 座標変換行列データ
		ComPtr <ID3D12Resource> wvpResource;	 // WVP行列データ用リソース
		TransformationMatrix* wvpData = nullptr; // WVP行列データのマッピング先ポインタ
	};

} // namespace Ken4lowEngine
