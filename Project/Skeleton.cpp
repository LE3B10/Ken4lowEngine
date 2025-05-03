#include "Skeleton.h"
#include <functional>
#include "Wireframe.h"
#include "DirectXCommon.h"
#include <numeric>

void Skeleton::CreateFromNode(const Node& rootNode)
{
	joints_.clear();
	jointMap_.clear();
	rootIndex_ = CreateJointRecursive(rootNode, std::nullopt);

	// 名前とインデックスのマップを作成
	for (const Joint& joint : joints_)
	{
		jointMap_.emplace(joint.name, joint.index);
	}
}

std::unique_ptr<Skeleton> Skeleton::CreateFromRootNode(const Node& rootNode)
{
	auto skeleton = std::make_unique<Skeleton>();
	skeleton->CreateFromNode(rootNode); // 再利用！
	skeleton->UpdateSkeleton(); // スケルトンの更新
	return skeleton;
}

void Skeleton::UpdateSkeleton()
{
	if (joints_.empty()) return;

	std::function<void(uint32_t)> updateJoint = [&](uint32_t index)
		{
			Joint& joint = joints_[index];

			// ローカル行列を取得
			joint.localMatrix = Matrix4x4::MakeAffineMatrix(joint.transform.scale, joint.transform.rotate, joint.transform.translate);

			if (joint.parent.has_value())
			{
				// 親の行列を取得してスケルトンスペース行列を更新
				joint.skeletonSpaceMatrix = joint.localMatrix * joints_[*joint.parent].skeletonSpaceMatrix;
			}
			else
			{
				// 親の行列を取得できなかったら
				joint.skeletonSpaceMatrix = joint.localMatrix;

			}
			// 子どもの行列を更新
			for (int32_t childIndex : joint.children)
			{
				updateJoint(childIndex);
			}
		};

	updateJoint(rootIndex_);
}

void Skeleton::Draw()
{
	for (const auto& joint : joints_) {
		// 位置取得
		Vector3 pos = {
			-joint.skeletonSpaceMatrix.m[3][0],
			joint.skeletonSpaceMatrix.m[3][1],
			joint.skeletonSpaceMatrix.m[3][2],
		};

		// 球を描画
		//Wireframe::GetInstance()->DrawSphere(pos, 0.05f, { 1.0f, 0.0f, 0.0f, 1.0f });

		// 親がいるなら線を描画
		if (joint.parent.has_value()) {
			const auto& parentJoint = joints_[joint.parent.value()];
			Vector3 parentPos = {
				-parentJoint.skeletonSpaceMatrix.m[3][0],
				parentJoint.skeletonSpaceMatrix.m[3][1],
				parentJoint.skeletonSpaceMatrix.m[3][2],
			};

			Wireframe::GetInstance()->DrawLine(pos, parentPos, { 0.0f, 1.0f, 0.0f, 1.0f });
		}
	}
}

uint32_t Skeleton::CreateJointRecursive(const Node& node, const std::optional<int32_t>& parent)
{
	int32_t currentIndex = static_cast<int32_t>(joints_.size());

	Joint joint;
	joint.name = node.name;
	joint.localMatrix = node.localMatrix;
	joint.skeletonSpaceMatrix = Matrix4x4::MakeIdentity(); // 初期値
	joint.transform = node.transform;
	joint.index = currentIndex;
	joint.parent = parent;
	joints_.push_back(joint);
	jointMap_[joint.name] = currentIndex;

	for (const auto& childNode : node.children) {
		int32_t childIndex = CreateJointRecursive(childNode, currentIndex);
		joints_[currentIndex].children.push_back(childIndex);
	}

	return currentIndex;
}
