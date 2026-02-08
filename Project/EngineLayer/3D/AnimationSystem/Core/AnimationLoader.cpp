#include "AnimationLoader.h"

// Assimp
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

using namespace Ken4lowEngine;

namespace Ken4lowEngine
{
	namespace
	{
		Animation ParseAiAnimation(const aiAnimation* animationAssimp, const AnimationLoader::Settings& settings)
		{
			Animation animation;

			// アニメーションが無い場合は空のアニメーションを返す
			if (!animationAssimp) return animation;

			// Assimp は ticksPerSecond が 0 のことがあるので保険
			const double ticksPerSecond = (animationAssimp->mTicksPerSecond != 0.0) ? animationAssimp->mTicksPerSecond : 25.0;
			animation.duration = static_cast<float>(animationAssimp->mDuration / ticksPerSecond); // 時間の単位を秒に変換

			for (uint32_t channelIndex = 0; channelIndex < animationAssimp->mNumChannels; ++channelIndex)
			{
				const aiNodeAnim* nodeAnimationAssimp = animationAssimp->mChannels[channelIndex];
				if (!nodeAnimationAssimp) continue;

				NodeAnimation& nodeAnimation = animation.nodeAnimations[nodeAnimationAssimp->mNodeName.C_Str()];

				// 位置（translate）のキーフレームを追加
				for (uint32_t keyIndex = 0; keyIndex < nodeAnimationAssimp->mNumPositionKeys; ++keyIndex)
				{
					const aiVectorKey& keyAssimp = nodeAnimationAssimp->mPositionKeys[keyIndex];
					KeyframeVector3 keyframe;
					keyframe.time = static_cast<float>(keyAssimp.mTime / ticksPerSecond); // ここも秒に変換
					if (settings.rightHandToLeftHand)
					{
						keyframe.value = { -keyAssimp.mValue.x, keyAssimp.mValue.y, keyAssimp.mValue.z }; // 右手 → 左手
					}
					else
					{
						keyframe.value = { keyAssimp.mValue.x, keyAssimp.mValue.y, keyAssimp.mValue.z };
					}
					nodeAnimation.translate.push_back(keyframe);
				}

				// 回転（rotate）のキーフレームを追加
				for (uint32_t keyIndex = 0; keyIndex < nodeAnimationAssimp->mNumRotationKeys; ++keyIndex)
				{
					const aiQuatKey& keyAssimp = nodeAnimationAssimp->mRotationKeys[keyIndex];
					KeyframeQuaternion keyframe;
					keyframe.time = static_cast<float>(keyAssimp.mTime / ticksPerSecond); // ここも秒に変換
					if (settings.rightHandToLeftHand)
					{
						keyframe.value = { keyAssimp.mValue.x, -keyAssimp.mValue.y, -keyAssimp.mValue.z, keyAssimp.mValue.w }; // 右手 → 左手
					}
					else
					{
						keyframe.value = { keyAssimp.mValue.x, keyAssimp.mValue.y, keyAssimp.mValue.z, keyAssimp.mValue.w };
					}
					nodeAnimation.rotate.push_back(keyframe);
				}

				// スケール（scale）のキーフレームを追加
				for (uint32_t keyIndex = 0; keyIndex < nodeAnimationAssimp->mNumScalingKeys; ++keyIndex)
				{
					const aiVectorKey& keyAssimp = nodeAnimationAssimp->mScalingKeys[keyIndex];
					KeyframeVector3 keyframe;
					keyframe.time = static_cast<float>(keyAssimp.mTime / ticksPerSecond); // ここも秒に変換
					if (settings.rightHandToLeftHand)
					{
						keyframe.value = { keyAssimp.mValue.x, keyAssimp.mValue.y, keyAssimp.mValue.z }; // スケールは座標系変換不要
					}
					else
					{
						keyframe.value = { keyAssimp.mValue.x, keyAssimp.mValue.y, keyAssimp.mValue.z };
					}
					nodeAnimation.scale.push_back(keyframe);
				}
			}

			return animation;
		}
	}
}

/// -------------------------------------------------------------
///				　アニメーションファイルを読み込む
/// -------------------------------------------------------------
Animation Ken4lowEngine::AnimationLoader::LoadFirstAnimation(const std::string& filePath, const Settings& settings)
{
	return LoadByIndexAnimation(filePath, 0, settings);
}

/// -------------------------------------------------------------
///				複数アニメーションファイルを読み込む
/// -------------------------------------------------------------
Animation Ken4lowEngine::AnimationLoader::LoadByIndexAnimation(const std::string& filePath, uint32_t animationIndex, const Settings& settings)
{
	Assimp::Importer importer;
	const std::string path = settings.animationFilePath + filePath;
	const aiScene* scene = importer.ReadFile(path.c_str(), 0);

	if (!scene || scene->mNumAnimations == 0) { return Animation{}; }
	if (animationIndex >= scene->mNumAnimations) { return Animation{}; }

	return ParseAiAnimation(scene->mAnimations[animationIndex], settings);
}
