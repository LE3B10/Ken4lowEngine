#pragma once
#include "DX12Include.h"
#include "Vector3.h"
#include "Quaternion.h"
#include "ModelData.h"
#include "TransformationMatrix.h"
#include "WorldTransform.h"
#include "Material.h"
#include "AnimationMesh.h"
#include "Skeleton.h"
#include <SkinCluster.h>

#include <algorithm>
#include <string>
#include <vector>
#include <numbers>
#include <memory>
#include <map>

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
		Vector3 worldPosition;
	};

	struct SkinningSetting
	{
		bool isSkinning; // スキニングを行うかどうか
		Vector3 padding; // 16バイトアライメントを保つためのパディング
	};

public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize(const std::string& fileName);

	// 更新処理
	void Update();

	// 描画処理
	void Draw();

	// モデルデータを取得
	const ModelData& GetModelData() const { return modelData; }

	// ImGui描画処理
	void DrawImGui();

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

public: /// ---------- セッタ ---------- ///

	// 座標を設定
	void SetTranslate(const Vector3& translate) { worldTransform.translate_ = translate; }

	// スケールを設定
	void SetScale(const Vector3& scale) { worldTransform.scale_ = scale; }

	// 回転を設定
	void SetRotate(const Vector3& rotate) { worldTransform.rotate_ = rotate; }

	// 反射率を設定
	void SetReflectivity(float reflectivity) { material_.SetShininess(reflectivity); }

	// スキニングを有効にするか設定
	void SetSkinningEnabled(bool isSkinning) { skinningSetting_->isSkinning = isSkinning; }

private: /// ---------- メンバ関数 ---------- ///

	// アニメーションを更新
	void UpdateAnimation();

	// Animationを解析する
	Animation LoadAnimationFile(const std::string& directoryPath, const std::string& fileName);

	// スキニングリソースの作成
	void CreateSkinningSettingResource();

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
	Material material_;

	DirectXCommon* dxCommon_ = nullptr;
	Camera* camera_ = nullptr;

	// モデルデータ
	ModelData modelData;
	Animation animation;

	std::unique_ptr<AnimationMesh> animationMesh_;
	std::unique_ptr<Skeleton> skeleton_; // スケルトン
	std::unique_ptr<SkinCluster> skinCluster_; // スキンクラスター

	// バッファリソースの作成
	TransformationAnimationMatrix* wvpData_ = nullptr;
	CameraForGPU* cameraData = nullptr;


	ComPtr<ID3D12Resource> skinningSettingResource_; // スキニング設定用のリソース
	SkinningSetting* skinningSetting_; // スキニング設定


	ComPtr <ID3D12Resource> wvpResource;
	ComPtr <ID3D12Resource> cameraResource;

	// アニメーションタイム
	float animationTime_ = 0.0f;
};

