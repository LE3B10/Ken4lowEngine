#pragma once
#include "DX12Include.h"
#include "Vector3.h"
#include "Quaternion.h"
#include "ModelData.h"
#include "TransformationMatrix.h"
#include "WorldTransform.h"

#include <string>
#include <vector>
#include <numbers>
#include <map>

/// ---------- 前方宣言 ---------- ///
class DirectXCommon;
class Camera;
class Wireframe;

/// -------------------------------------------------------------
///				　アニメーションを管理するクラス
/// -------------------------------------------------------------
class AnimationModel
{
private: /// ---------- 構造体 ---------- ///

	// シェーダー側のカメラ構造体
	struct CameraForGPU
	{
		Vector3 worldPosition;
	};

public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize(const std::string& fileName, bool isAnimation = false, bool hasSkeleton = false);

	// 更新処理
	void Update();

	// 描画処理
	void Draw();

	// モデルデータを取得
	const ModelData& GetModelData() const { return modelData; }

private: /// ---------- メンバ関数 ---------- ///

	// アニメーションを更新
	void UpdateAnimation(float deltaTime);

	// Animationを解析する
	Animation LoadAnimationFile(const std::string& directoryPath, const std::string& fileName);

	// ノードの階層構造からSkeltonを作成
	Skeleton CreateSkelton(const Node& rootNode);

	// ノードからJointを作成
	int32_t CreateJoint(const Node& node, const std::optional<int32_t>& parent, std::vector<Joint>& joints);

	// Skeletonの更新処理
	void UpdateSkeleton(Skeleton& skeleton);

	// アニメーションを適用する処理
	void ApplyAnimation(float animationTime);

	// SkinClusterの生成
	SkinCluster CreateSkinCluster( const Skeleton& skeleton, const ModelData& modelData, uint32_t descriptorSize);

	// SkinClusterの更新
	void UpdateSkinCluster(SkinCluster& skinCluster, const Skeleton& skeleton);

private: /// ---------- メンバ関数・テンプレート関数 ---------- ///

	// 任意の時刻の値を取得する
	template <typename T>
	inline T CalculateValue(const std::vector<Keyframe<T>>& keyframes, float time)
	{
		assert(!keyframes.empty()); // キーがないものは返す値が分からないのでダメ
		if (keyframes.size() == 1 || time <= keyframes[0].time) // キーが１つか、時刻がキーフレーム前なら最初の値とする
		{
			return keyframes[0].value;
		}

		// 
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
					return Vector3::Lerp(keyframes[index].value, keyframes[nextIndex].value, t);
				}
				else if constexpr (std::is_same_v<T, Quaternion>)
				{
					// T が Quaternion の場合のみ Slerp を使用
					return Quaternion::Slerp(keyframes[index].value, keyframes[nextIndex].value, t);
				}
				else
				{
					static_assert(false, "Unsupported type for interpolation");
				}
			}
		}
		// ここまでできた場合は一番後の時刻よりも後ろなので最後の値を返すことにする
		return (*keyframes.rbegin()).value;
	}

private: /// ---------- メンバ変数 ---------- ///

	WorldTransform worldTransform;

	DirectXCommon* dxCommon_ = nullptr;

	Camera* camera_ = nullptr;

	Wireframe* wireframe_ = nullptr;
	
	// モデルデータ
	ModelData modelData;
	Animation animation;
	Skeleton skeleton;
	SkinCluster skinCluster;

	bool isAnimation_ = false;
	bool hasSkeleton_ = false;

	// バッファリソースの作成
	TransformationMatrix* wvpData = nullptr;
	CameraForGPU* cameraData = nullptr;

	ComPtr <ID3D12Resource> wvpResource;
	ComPtr <ID3D12Resource> vertexResource;
	ComPtr <ID3D12Resource> materialResource;
	ComPtr <ID3D12Resource> cameraResource;
	ComPtr <ID3D12Resource> indexResource;

	// バッファリソースの使い道を補足するバッファビュー
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
	D3D12_INDEX_BUFFER_VIEW indexBufferView{};

	// アニメーションタイム
	float animationTime_ = 0.0f;
};
