#include "SkeletonAnimator.h"

namespace Ken4lowEngine
{
	void SkeletonAnimator::Update(Skeleton& skeleton, const Animation& animation, float timeSeconds, SkinCluster* skinCluster) const
	{
		auto& joints = skeleton.GetJoints();
		if (joints.empty())
		{
			return;
		}

		auto& nodeAnimations = animation.nodeAnimations;

		// ノードアニメーションの適用
		for (Joint& joint : joints)
		{
			auto it = nodeAnimations.find(joint.name);
			if (it == nodeAnimations.end())
			{
				continue;
			}

			const NodeAnimation& nodeAnim = it->second;

			// AnimationSampler は keyframes が空だと assert するので、空チェックしてから呼ぶ
			if (!nodeAnim.translate.empty())
			{
				joint.transform.translate = AnimationSampler::CalculateValue(nodeAnim.translate, timeSeconds);
			}
			if (!nodeAnim.rotate.empty())
			{
				joint.transform.rotate = AnimationSampler::CalculateValue(nodeAnim.rotate, timeSeconds);
			}
			if (!nodeAnim.scale.empty())
			{
				joint.transform.scale = AnimationSampler::CalculateValue(nodeAnim.scale, timeSeconds);
			}

			// ローカル行列を更新（AnimationModel の現挙動を維持）
			joint.localMatrix = Matrix4x4::MakeAffineMatrix(
				joint.transform.scale,
				joint.transform.rotate,
				joint.transform.translate
			);
		}

		// スケルトン更新
		skeleton.UpdateSkeleton();

		// パレット更新（必要なときだけ）
		if (skinCluster)
		{
			skinCluster->UpdatePaletteMatrix(skeleton);
		}
	}
}
