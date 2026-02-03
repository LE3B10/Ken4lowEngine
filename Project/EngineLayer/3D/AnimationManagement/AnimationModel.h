#pragma once
#include "DX12Include.h"
#include "Vector3.h"
#include "Quaternion.h"
#include "ModelData.h"
#include "WorldTransform.h"
#include "Material.h"
#include "AnimationMesh.h"
#include "Skeleton.h"
#include <SkinCluster.h>
#include <Sphere.h>
#include "Capsule.h"
#include "TransformationMatrix.h"
#include "LinearInterpolation.h"

#include <algorithm>
#include <string>
#include <vector>
#include <numbers>
#include <memory>
#include <filesystem>
#include <regex>

namespace Ken4lowEngine
{

/// ---------- 前方宣言 ---------- ///
class DirectXCommon;
class Camera;

/// -------------------------------------------------------------
///				　アニメーションを描画するクラス
/// -------------------------------------------------------------
class AnimationModel
{
private: /// ---------- 構造体 ---------- ///

	// シェーダー側のカメラ構造体
	struct CameraForGPU
	{
		Vector3 worldPosition; // ワールド座標系でのカメラ位置
	};

	// シェーダー側のライト構造体
	struct BodyPartCollider
	{
		std::string name;         // 名前（"LeftArm", "RightLeg", ...）
		int startJointIndex = -1; // 始点となるジョイント
		int endJointIndex = -1;   // 終点となるジョイント（カプセル用）
		Vector3 offset;           // 単一ジョイント用のオフセット（sphere 描画等に使える）
		float radius = 0.1f;      // カプセルまたはスフィアの半径
		float height = 0.0f;      // offset を使う Capsule 用（レガシー用途 or fallback）
	};

	// LODごとのスキンクラスタ情報
	std::vector<BodyPartCollider> bodyPartColliders_;

public: /// ---------- LOD構造体 ---------- ///

	// LODごとの情報
	struct LODEntry
	{
		// 入力（共有候補）：DEFAULTの頂点SRV、Influence SRV、IB
		ComPtr<ID3D12Resource> staticVBDefault; // t1
		D3D12_VERTEX_BUFFER_VIEW influenceVBV = {};  // VSで使わないならなくても可

		// インデックスバッファの実体を保持（解放されないように）
		ComPtr<ID3D12Resource> indexBuffer;     //インデックスバッファ

		D3D12_INDEX_BUFFER_VIEW  ibv{};
		uint32_t vertexCount = 0;
		uint32_t indexCount = 0;

		// 出力（インスタンス固有）：スキン結果u0とVBV、UAVディスクリプタ
		ComPtr<ID3D12Resource>  skinnedVB;     // u0
		D3D12_VERTEX_BUFFER_VIEW skinnedVBV = {};
		uint32_t uavIndex = UINT32_MAX; // UAVヒープのu0
		uint32_t srvInputVerticesOnUavHeap = UINT32_MAX; // t1 SRV on UAV heap

		D3D12_GPU_DESCRIPTOR_HANDLE influenceSrvGpuOnUavHeap = {}; // t2
		// 出力VBのリソース状態
		D3D12_RESOURCE_STATES skinnedState = D3D12_RESOURCE_STATE_COMMON;

		// サブメッシュごとの描画範囲とマテリアルSRV
		struct SubMeshRange
		{
			uint32_t startIndex = 0;
			uint32_t indexCount = 0;
			D3D12_GPU_DESCRIPTOR_HANDLE baseColorSrvGpuHandle{}; // t2用
		};
		std::vector<SubMeshRange> subMeshRanges; // subMeshごとに分割
	};
	std::vector<LODEntry> lods_; // LOD0～3まで
	int lodIndex_ = 0;

public: /// ---------- メンバ関数 ---------- ///

	/// <summary>
	/// 指定したモデルファイルを読み込み、アニメーションメッシュやスケルトン、
	/// スキンクラスタなどのリソースを初期化します。
	/// </summary>
	/// <param name="fileName">読み込むモデルファイル名（相対パス）。</param>
	/// <param name="isSkinning">true の場合はスキニング用リソースも初期化します。</param>
	void Initialize(const std::string& fileName, bool isSkinning = true);

	/// <summary>
	/// 複数の LOD 用モデルファイルを指定して初期化します。
	/// </summary>
	/// <param name="fileName">基準となるモデルファイル名（LOD0 など）。</param>
	/// <param name="lodFiles">LOD1 以降のモデルファイルパスの配列。</param>
	/// <param name="isSkinning">true の場合はスキニング用リソースも初期化します。</param>
	void Initialize(const std::string& fileName, const std::vector<std::string>& lodFiles, bool isSkinning = true);

	/// <summary>
	/// アニメーション時間やスケルトン、スキンクラスタ、マテリアルなどを更新します。
	/// 距離によるカリングや LOD の更新間引きにも対応しています。
	/// </summary>
	void Update();

	/// <summary>
	/// 単体のアニメーションモデルを描画します。
	/// Compute スキニング → Graphics の順にパイプラインを設定して描画します。
	/// </summary>
	void Draw();

	/// <summary>
	/// 単一の AnimationModel をまとめて描画するユーティリティ関数です。
	/// 可視チェックも行います。
	/// </summary>
	/// <param name="models">描画対象の AnimationModel（unique_ptr）。</param>
	static void DrawBatched(const std::unique_ptr<AnimationModel>& models);

	/// <summary>
	/// AnimationModel の配列（unique_ptr）を一括で描画します。
	/// Compute パスと Graphics パスをそれぞれ 1 回ずつだけセットし、
	/// 各モデルの可視チェックを行いながら描画します。
	/// </summary>
	/// <param name="models">描画対象の AnimationModel の配列。</param>
	static void DrawBatched(const std::vector<std::unique_ptr<AnimationModel>>& models);

	/// <summary>
	/// AnimationModel の生ポインタ配列版を一括で描画します。
	/// </summary>
	/// <param name="models">描画対象の AnimationModel のポインタ配列。</param>
	static void DrawBatched(const std::vector<AnimationModel*>& models);

	/// <summary>
	/// 読み込まれたモデルデータ（メッシュ階層情報など）を取得します。
	/// </summary>
	const ModelData& GetModelData() const { return modelData; }

	/// <summary>
	/// AnimationModel 用のデバッグ ImGui ウィンドウを描画します。
	/// LOD やスキニング状態などを確認・切り替えできます。
	/// </summary>
	void DrawImGui();

	/// <summary>
	/// 現在選択されている LOD インデックスを取得します。
	/// </summary>
	int GetLOD() const { return lodIndex_; }

	/// <summary>
	/// モデルやアニメーション、関連する GPU リソースを解放し、状態を初期化します。
	/// </summary>
	void Clear();

	/// <summary>
	/// スケルトン情報をワイヤーフレームで可視化します。
	/// デバッグ用途でジョイント位置や階層を確認するときに使用します。
	/// </summary>
	void DrawSkeletonWireframe();

	/// <summary>
	/// ボディパートごとのコライダーをワイヤーフレームで描画します。
	/// デバッグ用途で当たり判定の位置・サイズを確認します。
	/// </summary>
	void DrawBodyPartColliders();

public: /// ---------- ゲッタ ---------- ///

	/// <summary>
	/// このモデルのワールド変換情報を取得します。
	/// </summary>
	/// <returns>スケール・回転・平行移動を含む WorldTransform への const 参照。</returns>
	const WorldTransform& GetWorldTransform() const { return worldTransform; }

	/// <summary>
	/// ワールド座標系での位置（平行移動成分）を取得します。
	/// </summary>
	/// <returns>XYZ 各軸の座標。</returns>
	const Vector3& GetTranslate() const { return worldTransform.translate_; }

	/// <summary>
	/// ワールド座標系でのスケールを取得します。
	/// </summary>
	/// <returns>XYZ 各軸の拡大率。</returns>
	const Vector3& GetScale() const { return worldTransform.scale_; }

	/// <summary>
	/// ワールド座標系での回転量を取得します。（ラジアン指定想定）
	/// </summary>
	/// <returns>XYZ 各軸の回転量。</returns>
	const Vector3& GetRotate() const { return worldTransform.rotate_; }

	/// <summary>
	/// このモデルが保持している AnimationMesh へのポインタを取得します。
	/// </summary>
	/// <returns>AnimationMesh へのポインタ。未初期化時は nullptr。</returns>
	AnimationMesh* GetAnimationMesh() { return animationMesh_.get(); }

	/// <summary>
	/// 前フレームからの経過時間（秒）を取得します。
	/// </summary>
	/// <returns>deltaTime（秒単位）。</returns>
	float GetDeltaTime() const { return deltaTime; }

	/// <summary>
	/// 現在のアニメーション再生時刻（秒）を取得します。
	/// </summary>
	/// <returns>アニメーションの再生位置（秒）。</returns>
	float GetAnimationTime() const { return animationTime_; }

	/// <summary>
	/// 使用する LOD モデルファイルのリストを設定します。<br/>
	/// Initialize 前に呼び出しておくことで、LOD0/1/2…に対応するファイルを差し替えることができます。
	/// </summary>
	/// <param name="files">LOD 用モデルファイルパスの配列。</param>
	void SetLodFiles(const std::vector<std::string>& files) { lodSourceFiles_ = files; }

	/// <summary>
	/// 設定されている LOD モデルファイルリストをクリアします。
	/// </summary>
	void ClearLodFiles() { lodSourceFiles_.clear(); }

	/// <summary>
	/// 設定済みの LOD モデルファイルリストを取得します。
	/// </summary>
	/// <returns>LOD 用モデルファイルパスの配列。</returns>
	const std::vector<std::string>& GetLodFiles() const { return lodSourceFiles_; }

public: /// ---------- セッタ ---------- ///

	/// <summary>
	/// ワールド座標系での位置（平行移動成分）を設定します。
	/// </summary>
	/// <param name="translate">XYZ 各軸の座標。</param>
	void SetTranslate(const Vector3& translate) { worldTransform.translate_ = translate; }

	/// <summary>
	/// ワールド座標系でのスケールを設定します。
	/// </summary>
	/// <param name="scale">XYZ 各軸の拡大率。</param>
	void SetScale(const Vector3& scale) { worldTransform.scale_ = scale; }

	/// <summary>
	/// ワールド座標系での回転量を設定します。（ラジアン指定想定）
	/// </summary>
	/// <param name="rotate">XYZ 各軸の回転量。</param>
	void SetRotate(const Vector3& rotate) { worldTransform.rotate_ = rotate; }

	/// <summary>
	/// マテリアルの反射率（スペキュラ強度）を設定します。
	/// </summary>
	void SetReflectivity(float reflectivity) { material_.SetShininess(reflectivity); }

	/// <summary>
	/// 現在のワールド行列を用いて、ボディパートごとのカプセルコライダーを
	/// ワールド空間に変換した結果を取得します。
	/// </summary>
	std::vector<std::pair<std::string, Capsule>> GetBodyPartCapsulesWorld() const;

	/// <summary>
	/// 現在のワールド行列を用いて、ボディパートごとのスフィアコライダーを
	/// ワールド空間に変換した結果を取得します。
	/// </summary>
	std::vector<std::pair<std::string, Sphere>> GetBodyPartSpheresWorld() const;

	/// <summary>
	/// 頭部の描画を非表示にするかどうかを設定します（FPS視点などで使用）。
	/// </summary>
	void SetHideHead(bool hide) { hideHead_ = hide; }

	/// <summary>
	/// モデル全体に掛けるスケールファクターを設定します。
	/// </summary>
	void SetScaleFactor(float factor) { scaleFactor = factor; }

	/// <summary>
	/// モデル全体に掛かっているスケールファクターを取得します。
	/// </summary>
	float GetScaleFactor() const { return scaleFactor; }

	/// <summary>
	/// アニメーションを再生中にするか停止するかを設定します。
	/// </summary>
	void SetIsPlaying(bool isPlaying) { isAnimationPlaying_ = isPlaying; }

	/// <summary>
	/// アニメーション再生時刻を直接指定します（シーク用）。
	/// </summary>
	void SetAnimationTime(float time) { animationTime_ = time; }

	/// <summary>
	/// 遠距離カリングを行う際の、最終 LOD からの余裕距離を設定します。
	/// </summary>
	void SetFarCullExtra(float v) { farCullExtra_ = v; }

	/// <summary>
	/// 遠距離カリングされているかどうかを取得します。
	/// true の場合は描画対象外です。
	/// </summary>
	bool IsVisible() const { return !culledByDistance_; }

	/// <summary>
	/// LOD ごとの更新頻度（フレーム間引き）を設定します。
	/// 例: {1,1,2,4} → LOD2 は隔フレ、LOD3 は 4 フレに 1 回更新。
	/// </summary>
	void SetLodUpdateEvery(const std::vector<uint32_t>& v) { lodUpdateEvery_ = v; }

private: /// ---------- メンバ関数 ---------- ///

	/// <summary>
	/// モデルデータから LOD エントリ（VB/IB/UAV など）を初期化します。
	/// </summary>
	void InitializeLODs();

	/// <summary>
	/// 現在のアニメーション時刻に基づいて、ジョイント変換などを更新します。
	/// </summary>
	void UpdateAnimation();

	/// <summary>
	/// アニメーションファイルを読み込み、Animation 構造体に変換します。
	/// </summary>
	/// <param name="fileName">読み込むアニメーションファイル名。</param>
	Animation LoadAnimationFile(const std::string& fileName);

public: /// ---------- ボーン情報の初期化 ---------- ///

	/// <summary>
	/// アニメーションメッシュ／スケルトンからボーン情報を初期化し、
	/// スキニング用データ（スキンクラスタなど）を構築します。
	/// </summary>
	void InitializeBones();

	/// <summary>
	/// カメラとの距離をもとに LOD を選択し、ヒステリシス込みで切り替えます。
	/// </summary>
	/// <param name="dist">カメラとオブジェクトの距離。</param>
	void SetLODByDistance(float dist);     // LOD選択（ヒステリシス含む）

	/// <summary>
	/// Compute Shader を使用してスキニングを実行します。
	/// t0, t1, t2, u0, b0 などのバインドを行い、頂点データを書き換えます。
	/// </summary>
	void DispatchSkinningCS();

	/// <summary>
	/// スキン済み頂点バッファを用いてモデルを描画します。
	/// </summary>
	void DrawSkinned();

private: /// ---------- メンバ関数・テンプレート関数 ---------- ///

	/// <summary>
	/// キーフレーム列から任意の時刻の値を補間して取得します。
	/// Vector3 の場合は線形補間、Quaternion の場合は球面線形補間を行います。
	/// </summary>
	/// <typeparam name="T">Vector3 または Quaternion を想定。</typeparam>
	/// <param name="keyframes">補間対象のキーフレーム配列。</param>
	/// <param name="time">取得したい時刻（秒）。</param>
	/// <returns>補間された値。</returns>
	template <typename T>
	inline T CalculateValue(const std::vector<Keyframe<T>>& keyframes, float time)
	{
		assert(!keyframes.empty()); // キーがないものは返す値が分からないのでダメ
		if (keyframes.size() == 1 || time <= keyframes[0].time) // キーが１つか、時刻がキーフレーム前なら最初の値とする
		{
			return keyframes[0].value; // 最初の値を返す
		}

		// それ以外は線形補間で求める
		for (size_t index = 0; index < keyframes.size() - 1; ++index)
		{
			size_t nextIndex = index + 1;
			// indexとnextIndexの2つのkeyframeを取得して範囲内に自国があるかを判定
			if (keyframes[index].time <= time && time <= keyframes[nextIndex].time)
			{
				// 範囲内を保管する
				float t = (time - keyframes[index].time) / (keyframes[nextIndex].time - keyframes[index].time);
				if constexpr (std::is_same_v<T, Vector3>)
				{
					// T が Vector3 の場合のみ Lerp を使用
					return Lerp(keyframes[index].value, keyframes[nextIndex].value, t);
				}
				else if constexpr (std::is_same_v<T, Quaternion>)
				{
					// T が Quaternion の場合のみ Slerp を使用
					return Quaternion::Slerp(keyframes[index].value, keyframes[nextIndex].value, t);
				}
				else
				{
					// それ以外の型はサポートされていない
					static_assert(false, "Unsupported type for interpolation");
				}
			}
		}
		// ここまでできた場合は一番後の時刻よりも後ろなので最後の値を返すことにする
		return (*keyframes.rbegin()).value;
	}

private: /// ---------- メンバ変数 ---------- ///

	WorldTransform worldTransform; // ワールド変換情報
	Material material_;			   // マテリアル情報

	DirectXCommon* dxCommon_ = nullptr;  // DirectX共通クラス
	Camera* camera_ = nullptr;			 // カメラ

	// 環境マップのテクスチャ
	D3D12_GPU_DESCRIPTOR_HANDLE environmentMapHandle_{};

	ModelData modelData;	// モデルデータ
	std::string fileName_;  // 読み込んだファイル名を保持

	Animation animation; // アニメーションデータ

	std::unique_ptr<AnimationMesh> animationMesh_; // アニメーションメッシュ
	std::unique_ptr<Skeleton> skeleton_; // スケルトン
	std::vector<std::unique_ptr<SkinCluster>> skinClusterLOD_; // LOD別

	// 表示/デバッグ用に LOD ファイル名を保持
	std::vector<std::string> lodFileName_;

	// 外部から注入された LOD リスト（空なら単一扱い）
	std::vector<std::string> lodSourceFiles_;

	// バッファリソースの作成
	TransformationAnimationMatrix* wvpData_ = nullptr;
	CameraForGPU* cameraData = nullptr;

	ComPtr <ID3D12Resource> wvpResource;	// 定数バッファ : ワールド変換行列
	ComPtr <ID3D12Resource> cameraResource; // 定数バッファ : カメラ情報

	// アニメーションタイム
	float animationTime_ = 0.0f;

	// フレーム間の経過時間
	float deltaTime = 0.0f;

	bool hideHead_ = false; // デフォルトは表示
	float scaleFactor = 1.0f; // スケールファクター

	bool isAnimationPlaying_ = true; // アニメーションが再生中かどうか

private: /// ---------- コンピュートシェーダーによるスキニング用 ---------- ///

	ComPtr<ID3D12Resource> staticVBDefault_;		  // CS入力用の頂点（Deviceローカル）
	ComPtr<ID3D12Resource> skinnedVB_;				  // 描画用スキン頂点バッファ
	D3D12_VERTEX_BUFFER_VIEW skinnedVBV_{};			  // 描画用VBV
	uint32_t srvInputVerticesOnUavHeap_ = UINT32_MAX; // t1
	uint32_t uavOutIndex_ = UINT32_MAX;               // u0
	ComPtr<ID3D12Resource> csCB_;                     // b0
	SkinningInformationForGPU* csCBMapped_ = nullptr; // b0マッピングデータ
	bool useComputeSkinning_ = true;				  // 切替 : コンピュートシェーダースキニングを使うかどうか

	// スキン頂点バッファのリソース状態
	D3D12_RESOURCE_STATES skinnedVBState_ = D3D12_RESOURCE_STATE_COMMON;

private: /// ---------- LOD・カリング関連 ---------- ///

	bool  culledByDistance_ = false;   // 遠距離で非表示にするフラグ
	float farCullExtra_ = 20.0f;   // 最終LODの“出しきい値”から更に何m離れたらカリングするか
	bool forceLOD_ = false;                    // ← 手動LOD固定トグル（デバッグ用）
	int  forcedLODIndex_ = 0;                  // ← 固定するLOD

	// フレームカウンタ（LODごとの更新間引きに使用）
	uint32_t frame_ = 0;
	std::vector<uint32_t> lodUpdateEvery_{ 1, 1, 2, 4 }; // 既定: LOD0/1=毎フレ, LOD2=隔フレ, LOD3=4フレ
};


} // namespace Ken4lowEngine
