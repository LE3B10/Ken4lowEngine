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

	// 初期化処理
	void Initialize(const std::string& fileName, bool isSkinning = true);

	// 複数 LOD を直接渡すオーバーロード
	void Initialize(const std::string& fileName, const std::vector<std::string>& lodFiles, bool isSkinning = true);

	// 更新処理
	void Update();

	// 描画処理
	void Draw();

	// 単一のモデルをまとめて描画
	static void DrawBatched(const std::unique_ptr<AnimationModel>& models);

	// これ1行でOK：可視チェックも含めてまとめて描画
	static void DrawBatched(const std::vector<std::unique_ptr<AnimationModel>>& models);

	// ポインタ配列版も欲しければオーバーロード
	static void DrawBatched(const std::vector<AnimationModel*>& models);

	// モデルデータを取得
	const ModelData& GetModelData() const { return modelData; }

	// ImGui描画処理
	void DrawImGui();
	int  GetLOD() const { return lodIndex_; }

	// 削除処理
	void Clear();

	// ワイヤーフレーム描画
	void DrawSkeletonWireframe();

	void DrawBodyPartColliders();

public: /// ---------- ゲッタ ---------- ///

	const WorldTransform& GetWorldTransform() const { return worldTransform; }

	// 座標を取得
	const Vector3& GetTranslate() const { return worldTransform.translate_; }

	// スケールを取得
	const Vector3& GetScale() const { return worldTransform.scale_; }

	// 回転を取得
	const Vector3& GetRotate() const { return worldTransform.rotate_; }

	// メッシュを取得
	AnimationMesh* GetAnimationMesh() { return animationMesh_.get(); }

	float GetDeltaTime() const { return deltaTime; }

	// アニメーション時間を取得
	float GetAnimationTime() const { return animationTime_; }

	// 反射率を取得
	void SetLodFiles(const std::vector<std::string>& files) { lodSourceFiles_ = files; }

	// LODファイルリストをクリア
	void ClearLodFiles() { lodSourceFiles_.clear(); }

	// LODファイルリストを取得
	const std::vector<std::string>& GetLodFiles() const { return lodSourceFiles_; }

public: /// ---------- セッタ ---------- ///

	// 座標を設定
	void SetTranslate(const Vector3& translate) { worldTransform.translate_ = translate; }

	// スケールを設定
	void SetScale(const Vector3& scale) { worldTransform.scale_ = scale; }

	// 回転を設定
	void SetRotate(const Vector3& rotate) { worldTransform.rotate_ = rotate; }

	// 反射率を設定
	void SetReflectivity(float reflectivity) { material_.SetShininess(reflectivity); }

	// ワールド空間からボディパーツのカプセルを取得
	std::vector<std::pair<std::string, Capsule>> GetBodyPartCapsulesWorld() const;

	// ワールド空間からボディパーツのスフィアを取得
	std::vector<std::pair<std::string, Sphere>> GetBodyPartSpheresWorld() const;

	// 頭を消すかどうか
	void SetHideHead(bool hide) { hideHead_ = hide; }

	// スケールファクターを設定
	void SetScaleFactor(float factor) { scaleFactor = factor; }

	// スケールファクターを取得
	float GetScaleFactor() const { return scaleFactor; }

	// アニメーションの再生/停止
	void SetIsPlaying(bool isPlaying) { isAnimationPlaying_ = isPlaying; }

	// アニメーション時間を設定
	void SetAnimationTime(float time) { animationTime_ = time; }

	// 遠距離カリングの余裕距離を設定
	void  SetFarCullExtra(float v) { farCullExtra_ = v; }

	// 遠距離カリングされているか
	bool  IsVisible() const { return !culledByDistance_; }

	// LODごとの更新間引き（例: {1,1,2,4} = LOD2は隔フレ、LOD3は4フレに1回）
	void SetLodUpdateEvery(const std::vector<uint32_t>& v) { lodUpdateEvery_ = v; }

private: /// ---------- メンバ関数 ---------- ///

	// LODの初期化
	void InitializeLODs();

	// アニメーションを更新
	void UpdateAnimation();

	// Animationを解析する
	Animation LoadAnimationFile(const std::string& fileName);

public: /// ---------- ボーン情報の初期化 ---------- ///

	// ボーン情報の初期化
	void InitializeBones();

	// LOD選択
	void SetLODByDistance(float dist);     // LOD選択（ヒステリシス含む）

	// CS側：t0 t1 t2 u0 b0
	void DispatchSkinningCS();

	// Graphics
	void DrawSkinned();

private: /// ---------- メンバ関数・テンプレート関数 ---------- ///

	// 任意の時刻の値を取得する
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

