#pragma once
#include "DX12Include.h"
#include "WorldTransform.h"
#include "TextureManager.h"
#include "Material.h"
#include "Mesh.h"
#include "VertexData.h"
#include "ModelData.h"
#include "Camera.h"
#include "LightManager.h"

#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <numbers>

namespace Ken4lowEngine
{

/// ---------- 前方宣言 ---------- ///
class DirectXCommon;
class Model;
class Object3DCommon;
class SkyBox;


/// -------------------------------------------------------------
///						オブジェクト3Dクラス
/// -------------------------------------------------------------
class Object3D
{
public: /// ---------- 構造体 ---------- ///

	// シェーダー側のカメラ構造体
	struct CameraForGPU
	{
		Vector3 worldPosition;
	};

	// ディゾルブの設定
	struct DissolveSetting
	{
		float threshold;        // 閾値
		float edgeThickness;    // エッジの太さ
		float padding0[2];      // パディング
		Vector4 edgeColor;      // 色
	};

public: /// ---------- メンバ関数 ---------- ///

	/// <summary>
	/// 3D オブジェクトの初期化処理を行います。<br/>
	/// ・DirectXCommon / デフォルトカメラの取得<br/>
	/// ・AssimpLoader による ModelData の読み込み<br/>
	/// ・SubMesh ごとの Mesh 生成とテクスチャ SRV の登録<br/>
	/// ・環境マップとディゾルブマスクテクスチャの読み込み<br/>
	/// ・WorldTransform / Material の初期化<br/>
	/// ・カメラ用 CBV / ディゾルブ用 CBV の生成とマップ<br/>
	/// をまとめて行います。
	/// </summary>
	/// <param name="fileName">読み込むモデルファイル名（パス）。</param>
	void Initialize(const std::string& fileName);

	/// <summary>
	/// 毎フレームの更新処理を行います。<br/>
	/// ・Object3DCommon からデフォルトカメラを取得し直す<br/>
	/// ・Material / WorldTransform の更新<br/>
	/// ・CameraForGPU にアクティブカメラのワールド位置を書き込み<br/>
	/// などを行い、描画に必要な CBV を更新します。
	/// </summary>
	void Update();

	/// <summary>
	/// ImGui を使ったデバッグ用 UI の描画を行います。<br/>
	/// ・Position / Rotation / Scale の編集<br/>
	/// ・カメラ位置の簡易調整（Slider）<br/>
	/// ・Material 側の ImGui 表示<br/>
	/// などを行います（USE_IMGUI 定義時のみ有効）。
	/// </summary>
	void DrawImGui();

	/// <summary>
	/// 3D オブジェクトの描画処理を行います。<br/>
	/// ・Object3DCommon で共通 PSO / ルートシグネチャ設定<br/>
	/// ・Material / WorldTransform のパイプラインセット<br/>
	/// ・カメラ CBV / 環境マップ / ディゾルブ用 CBV / マスク SRV をルートにバインド<br/>
	/// ・SubMesh ごとにテクスチャ SRV をセットして Mesh::Draw() を呼び出し<br/>
	/// を行います。
	/// </summary>
	void Draw();

public: /// ---------- 設定処理 ---------- ///

	/// <summary>
	/// モデルを外部の ModelManager から検索して設定します。<br/>
	/// FindModel(filePath) で共有ポインタとして取得し、必要に応じて Model 側の Initialize を呼び出します。
	/// </summary>
	/// <param name="filePath">モデルを探す際のキーとなるパス。</param>
	void SetModel(const std::string& filePath);

	/// <summary>
	/// ワールド座標系でのスケールを設定します。
	/// </summary>
	/// <param name="scale">XYZ 各軸の拡大率。</param>
	void SetScale(const Vector3& scale) { worldTransform.scale_ = scale; }

	/// <summary>
	/// 現在のスケールを取得します。
	/// </summary>
	/// <returns>XYZ 各軸のスケール。</returns>
	Vector3 GetScale() const { return worldTransform.scale_; }

	/// <summary>
	/// ワールド座標系での回転を設定します（ラジアン指定想定）。
	/// </summary>
	/// <param name="rotate">XYZ 各軸の回転量。</param>
	void SetRotate(const Vector3& rotate) { worldTransform.rotate_ = rotate; }

	/// <summary>
	/// 現在の回転値を取得します。
	/// </summary>
	/// <returns>XYZ 各軸の回転量。</returns>
	Vector3 GetRotate() const { return worldTransform.rotate_; }

	/// <summary>
	/// ワールド座標系での位置を設定します。
	/// </summary>
	/// <param name="translate">XYZ 各軸の位置。</param>
	void SetTranslate(const Vector3& translate) { worldTransform.translate_ = translate; }

	/// <summary>
	/// 現在のワールド座標を取得します。
	/// </summary>
	/// <returns>XYZ 各軸の位置。</returns>
	Vector3 GetTranslate() const { return worldTransform.translate_; }

	/// <summary>
	/// マテリアルのベースカラーを設定します。
	/// </summary>
	/// <param name="color">RGBA 形式の色。</param>
	void SetColor(const Vector4& color) { material_.SetColor(color); }

	/// <summary>
	/// このオブジェクトが使用するカメラを明示的に設定します。<br/>
	/// 特別なカメラで描画したい場合などに使用します。
	/// </summary>
	/// <param name="camera">使用したいカメラのポインタ。</param>
	void SetCamera(Camera* camera) { camera_ = camera; }

	/// <summary>
	/// マテリアルの反射率を設定します。<br/>
	/// シェーダ側でリフレクションの強さとして利用されます。
	/// </summary>
	/// <param name="reflectivity">反射率（0.0 ～ 1.0 想定）。</param>
	void SetReflectivity(float reflectivity) { material_.SetReflection(reflectivity); }

	/// <summary>
	/// 全てのサブメッシュのテクスチャを同じものに差し替えます。<br/>
	/// UI から一括でテクスチャを変えたい場合などに使用します。
	/// </summary>
	/// <param name="texturePath">読み込むテクスチャファイルのパス。</param>
	void SetTextureForAll(const std::string& texturePath);

	/// <summary>
	/// 指定したサブメッシュのテクスチャだけを変更します。<br/>
	/// メッシュごとに異なるテクスチャを貼りたい場合に使用します。
	/// </summary>
	/// <param name="index">対象となるサブメッシュのインデックス。</param>
	/// <param name="texturePath">差し替えるテクスチャファイルのパス。</param>
	void SetTextureForSubmesh(size_t index, const std::string& texturePath);

	/// <summary>
	/// 管理しているサブメッシュの数を取得します。<br/>
	/// ImGui などで「サブメッシュ単位の設定」を行う際に、UI 側でループ回数として利用できます。
	/// </summary>
	/// <returns>サブメッシュ数。</returns>
	size_t GetSubmeshCount() const { return meshes_.size(); }

public: /// ---------- ディゾルブの設定 ---------- ///

	/// <summary>
	/// ディゾルブの閾値を設定します。<br/>
	/// 閾値を小さくしていくことで、モデルがだんだん消えていくような表現に使用します。
	/// </summary>
	/// <param name="threshold">ディゾルブの閾値（0.0 ～ 1.0 想定）。</param>
	void SetDissolveThreshold(float threshold) { dissolveSetting_->threshold = threshold; }

	/// <summary>
	/// ディゾルブ時のエッジの太さを設定します。
	/// </summary>
	/// <param name="thickness">エッジの太さ。</param>
	void SetDissolveEdgeThickness(float thickness) { dissolveSetting_->edgeThickness = thickness; }

	/// <summary>
	/// ディゾルブ時のエッジカラーを設定します。
	/// </summary>
	/// <param name="color">エッジに使用する RGBA カラー。</param>
	void SetDissolveEdgeColor(const Vector4& color) { dissolveSetting_->edgeColor = color; }

private: /// ---------- 内部メンバ関数 ---------- ///

	/// <summary>
	/// カメラ情報用の定数バッファを生成・初期化します。<br/>
	/// ・CameraForGPU 用のバッファを作成<br/>
	/// ・Map して cameraData ポインタを取得<br/>
	/// ・初期値として現在のカメラ位置を worldPosition に書き込む<br/>
	/// を行います。
	/// </summary>
	void InitializeCameraResource();

	/// <summary>
	/// ディゾルブ用定数バッファを生成・初期化します。<br/>
	/// ・DissolveSetting 用のバッファを作成<br/>
	/// ・Map して dissolveSetting_ ポインタを取得<br/>
	/// ・threshold / edgeThickness / edgeColor の初期値を設定<br/>
	/// を行います。
	/// </summary>
	void InitializeDissolveResource();

private: /// ---------- メンバ変数 ---------- ///

	DirectXCommon* dxCommon_ = nullptr;
	Camera* camera_ = nullptr;
	SkyBox* skyBox_ = nullptr;

	std::shared_ptr<Object3D> model_;

	// マテリアルデータ
	Material material_;

	// ワールドトランスフォーム
	WorldTransform worldTransform;

	// メッシュ
	std::vector<Mesh> meshes_;
	std::vector<D3D12_GPU_DESCRIPTOR_HANDLE> materialSRVs_;

	// バッファリソースの作成
	ComPtr <ID3D12Resource> cameraResource;

	// カメラにデータを書き込む
	CameraForGPU* cameraData = nullptr;

	// モデルデータ（subMeshes を想定）
	ModelData modelData;

	float alpha = 1.0f; // α値

	// 環境マップのテクスチャ
	D3D12_GPU_DESCRIPTOR_HANDLE environmentMapHandle_{};

	// ディゾルブマスクのテクスチャ
	D3D12_GPU_DESCRIPTOR_HANDLE dissolveMaskHandle_{};
	// ディゾルブの設定
	Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_;
	DissolveSetting* dissolveSetting_ = nullptr;
};

} // namespace Ken4lowEngine
