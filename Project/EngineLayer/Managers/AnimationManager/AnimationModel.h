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

#include "Capsule.h"

#include <algorithm>
#include <string>
#include <vector>
#include <numbers>
#include <memory>
#include <map>
#include <Sphere.h>

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

	struct DissolveSetting
	{
		float threshold; // 閾値
		float edgeThickness; // エッジの範囲（0.05など）
		float padding[2];
		Vector4 edgeColor; // エッジ部分の色
	};

	struct BodyPartCollider
	{
		std::string name;         // 名前（"LeftArm", "RightLeg", ...）
		int startJointIndex = -1; // 始点となるジョイント
		int endJointIndex = -1;   // 終点となるジョイント（カプセル用）
		Vector3 offset;           // 単一ジョイント用のオフセット（sphere 描画等に使える）
		float radius = 0.1f;      // カプセルまたはスフィアの半径
		float height = 0.0f;      // offset を使う Capsule 用（レガシー用途 or fallback）
	};
	std::vector<BodyPartCollider> bodyPartColliders_;

public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize(const std::string& fileName, bool isSkinning = true);

	// 更新処理
	void Update();

	// 描画処理
	void Draw();

	// モデルデータを取得
	const ModelData& GetModelData() const { return modelData; }

	// ImGui描画処理
	void DrawImGui();

	// 削除処理
	void Clear();

	// ワイヤーフレーム描画
	void DrawSkeletonWireframe();

	void DrawBodyPartColliders();

	// AnimationModel.h に追加
	void SetDissolveThreshold(float threshold) { dissolveSetting_->threshold = threshold; }
	float GetDissolveThreshold() const { return dissolveSetting_->threshold; }

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

	// ワールド空間からボディパーツのカプセルを取得
	std::vector<std::pair<std::string, Capsule>> GetBodyPartCapsulesWorld() const;

	std::vector<std::pair<std::string, Sphere>> GetBodyPartSpheresWorld() const;

	// 頭を消すかどうか
	void SetHideHead(bool hide) { hideHead_ = hide; }

	void SetScaleFactor(float factor) { scaleFactor = factor; }

	void SetIsPlaying(bool isPlaying) { isAnimationPlaying_ = isPlaying; }

	void SetAnimationTime(float time) { animationTime_ = time; }

private: /// ---------- メンバ関数 ---------- ///

	// アニメーションを更新
	void UpdateAnimation();

	// Animationを解析する
	Animation LoadAnimationFile(const std::string& directoryPath, const std::string& fileName);

	// スキニングリソースの作成
	void CreateSkinningSettingResource();

	// Dissolve設定リソースの作成
	void CreateDissolveSettingResource();

	// ボーン情報の初期化
	void InitializeBones();

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
	SkinningSetting* skinningSetting_ = nullptr; // スキニング設定

	ComPtr<ID3D12Resource> dissolveSettingResource_; // Dissolve設定用のリソース
	DissolveSetting* dissolveSetting_ = nullptr; // Dissolve設定
	uint32_t dissolveMaskSrvIndex_ = 0; // SRV index for mask

	ComPtr <ID3D12Resource> wvpResource;
	ComPtr <ID3D12Resource> cameraResource;

	// アニメーションタイム
	float animationTime_ = 0.0f;

	float deltaTime = 0.0f;

	bool hideHead_ = false; // デフォルトは表示
	float scaleFactor = 1.0f;

	bool isAnimationPlaying_ = true; // アニメーションが再生中かどうか
};

