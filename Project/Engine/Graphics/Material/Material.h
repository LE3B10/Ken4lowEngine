#pragma once
#include "DX12Include.h"
#include "Matrix4x4.h"
#include "Vector4.h"
#include <cstdint>

namespace Ken4lowEngine
{


/// -------------------------------------------------------------
///					　	マテリアルクラス
/// -------------------------------------------------------------
class Material
{
public: /// ---------- 構造体 ---------- ///

	// マテリアルデータ 定数バッファで送るデータ
	struct MaterialCBData
	{
		Vector4 color;			// 色 : bytes 16
		float shininess;		// シェーディングの強さ : bytes 4
		float padding[3];		// パディング : bytes 12
		Matrix4x4 uvTransform;  // UV変換行列 : bytes 64
		float reflection;		// 反射率 : bytes 4
		float roughness;		// 粗さ : bytes 4
		float usePointSampling; // 1.0f で Point Sampler を使用 : bytes 4
		float padding2;			// パディング : bytes 4
	};

public: /// ---------- メンバ変数 ---------- ///

	// テクスチャ系データ
	std::string textureFilePath;			 // テクスチャファイルパス
	D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle{}; // GPUハンドル

public: /// ---------- メンバ関数 ---------- ///

	/// <summary>
	/// デフォルトコンストラクタ。
	/// メンバはゼロ初期化されますが、GPU リソースの確保は行いません。
	/// </summary>
	Material() = default;

	/// <summary>
	/// マテリアル用定数バッファの GPU リソースを作成し、
	/// CPU から書き込むためにマップします。
	/// また、色や光沢度などのパラメータをデフォルト値で初期化します。
	/// </summary>
	void Initialize();

	/// <summary>
	/// マテリアルのパラメータを更新するための処理。
	/// マップ済みの定数バッファに対して、現在の materialData_ の内容を書き戻します。
	/// </summary>
	void Update();

	/// <summary>
	/// 指定したルートパラメータインデックスに、このマテリアルの定数バッファをバインドします。
	/// 描画前に呼び出すことで、シェーダからマテリアル情報へアクセスできるようにします。
	/// </summary>
	/// <param name="rootParameterIndex">
	/// ルートシグネチャ内でマテリアル定数バッファをバインドするスロット番号。
	/// デフォルトは 0 番。
	/// </param>
	void SetPipeline(UINT rootParameterIndex = 0) const;

	/// <summary>
	/// ImGui を用いてマテリアルパラメータを編集する UI を描画します。
	/// 色、光沢度、反射率などをリアルタイムに調整できます。
	/// USE_IMGUI が有効なときのみ機能します。
	/// </summary>
	void DrawImGui();

public: /// ---------- ゲッタ ---------- ///

	/// <summary>
	/// マテリアル用定数バッファのリソースを取得します。
	/// </summary>
	/// <returns>定数バッファを表す ID3D12Resource のスマートポインタ。</returns>
	ComPtr<ID3D12Resource> GetMaterialResource() { return materialResource_; }

	/// <summary>
	/// マテリアルデータへのポインタを取得します。
	/// ImGui やゲーム側から直接パラメータを書き換える際に使用します。
	/// </summary>
	/// <returns>マップ済みの MaterialCBData へのポインタ。</returns>
	MaterialCBData* GetMaterialData() { return materialData_; }

public: /// ---------- セッタ ---------- ///

	/// <summary>
	/// マテリアルのベースカラーを設定します。
	/// </summary>
	/// <param name="color">設定する RGBA カラー。</param>
	void SetColor(const Vector4& color) { materialData_->color = color; }

	/// <summary>
	/// マテリアルの光沢度（スペキュラの鋭さ）を設定します。
	/// </summary>
	/// <param name="shininess">設定する光沢度。</param>
	void SetShininess(float shininess) { materialData_->reflection = shininess; }

	/// <summary>
	/// マテリアルの輝度（明るさ）を設定します。
	/// 光沢度とは別に、全体の明るさパラメータとして利用できます。
	/// </summary>
	/// <param name="shininess">設定する輝度。</param>
	void SetIntensity(float shininess) { materialData_->shininess = shininess; }

	/// <summary>
	/// マテリアルの反射率を設定します。
	/// 環境マップ等を用いた反射表現の強さを調整するための係数です。
	/// </summary>
	/// <param name="reflection">設定する反射率（0.0 ～ 1.0 を推奨）。</param>
	void SetReflection(float reflection) { materialData_->reflection = reflection; }

	/// <summary>
	/// UV 変換行列を設定します。
	/// タイルやスクロール、回転などの UV アニメーションを行う際に使用します。
	/// </summary>
	/// <param name="uvTransform">UV 座標に適用する 4x4 行列。</param>
	void SetUVTransform(const Matrix4x4& uvTransform) { materialData_->uvTransform = uvTransform; }
	void SetUsePointSampling(bool enabled) { materialData_->usePointSampling = enabled ? 1.0f : 0.0f; }

private: /// ---------- メンバ変数 ---------- ///

	// マテリアル用のリソース
	ComPtr<ID3D12Resource> materialResource_{};

	// マテリアルデータ
	MaterialCBData* materialData_{};

};
} // namespace Ken4lowEngine
