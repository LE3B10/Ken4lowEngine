#pragma once
#include "DX12Include.h"
#include <vector>
#include <map>
#include <optional>
#include "VertexData.h"
#include "MaterialData.h"
#include "Quaternion.h"
#include "Matrix4x4.h"
#include <span>
#include <array>

// EulerTransformの構造体
struct EulerTransform
{
	Vector3 scale{};
	Vector3 rotate{};	// Eulerでの回転
	Vector3 trnaslate{};
};

// QuaternionTransformの構造体
struct QuaternionTransform
{
	Vector3 scale{};
	Quaternion rotate{};
	Vector3 translate{};
};

// キーフレームの構造体
template <typename T>
struct Keyframe
{
	float time;
	T value;
};

using KeyframeVector3 = Keyframe<Vector3>;
using KeyframeQuaternion = Keyframe<Quaternion>;

// NodeのAnimationの構造体
struct NodeAnimation
{
	std::vector<KeyframeVector3> translate;
	std::vector<KeyframeQuaternion> rotate;
	std::vector<KeyframeVector3> scale;
};

// 拡張する場合のテンプレート
template <typename T>
struct AnimationCurve
{
	std::vector<T> keyflames;
};

// アニメーションを表現する構造体
struct Animation
{
	float duration; // アニメーション全体の尺（単位は秒）
	// NodeAnimationの集合、Node名でひけるようにしておく
	std::map<std::string, NodeAnimation> nodeAnimations;
};

// jointの構造体
struct Joint
{
	QuaternionTransform transform; // Transform情報
	Matrix4x4 localMatrix;		   // localMatrix
	Matrix4x4 skeletonSpaceMatrix; // skeletonSpaceでの変換行列
	std::string name;			   // 名前
	std::vector<int32_t> children; // 子JointのIndexのリスト。居なければ空
	int32_t index;				   // 自身のindex
	std::optional<int32_t> parent; // 親JointのIndex。いなければnull
};

// Skeletonの構造体
struct Skeleton
{
	int32_t root; // RootJointのIndex
	std::map<std::string, int32_t> jointMap; // Joint名とIndexとの辞書
	std::vector<Joint> joints; // 所属しているジョイント
};

// ノード
struct Node
{
	QuaternionTransform transform;
	Matrix4x4 localMatrix{};
	std::string name;
	std::vector<Node> children;
};

// VertexWeightDataの構造体
struct VertexWeightData
{
	float weight;
	uint32_t vertexIndex;
};

// JointWeightDataの構造体
struct JointWeightData
{
	Matrix4x4 inverseBindPoseMatrix;
	std::vector<VertexWeightData> vertexWeights;
};

// ModelData構造体
struct ModelData
{
	std::map<std::string, JointWeightData> skinClusterData;
	std::vector<VertexData> vertices;
	std::vector<uint32_t> indices;
	MaterialData material;
	Node rootNode;
};

// 最大4Jointの影響を受ける
const uint32_t kNumMaxInfluence = 4;

// インフルエンスの構造体
struct VertexInfluence
{
	std::array<float, kNumMaxInfluence> weights;
	std::array<int32_t, kNumMaxInfluence> jointIndices;
};

// WellForGPUの構造体
struct WellForGPU
{
	Matrix4x4 skeletonSpaceMatrix; // 位置用
	Matrix4x4 skeletonSpaceInverceTransposeMatrix; // 法線用
};

// SkinClusterの構造体
struct SkinCluster
{
	std::vector<Matrix4x4> inverseBindPoseMatrices;
	ComPtr<ID3D12Resource> influenceResource;
	D3D12_VERTEX_BUFFER_VIEW influenceBufferView;
	std::span<VertexInfluence> mappedInfluence;
	ComPtr<ID3D12Resource> paletteResource;
	std::span<WellForGPU> mappedPalette;
	std::pair<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE> paletteSrvHandle;
};