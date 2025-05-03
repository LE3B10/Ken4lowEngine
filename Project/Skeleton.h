#pragma once
#include "Matrix4x4.h"
#include "Quaternion.h"
#include "Vector3.h"
#include "ModelData.h"

#include <string>
#include <map>
#include <vector>
#include <optional>
#include <algorithm>


/// -------------------------------------------------------------
///		　				　スケルトンクラス
/// -------------------------------------------------------------
class Skeleton
{
public: /// ---------- メンバ関数 ---------- ///

	// コンストラクタ
	Skeleton() = default;

	// スケルトンをノードから生成
	void CreateFromNode(const Node& rootNode);

	// ルートノードからスケルトンを生成する
	static std::unique_ptr<Skeleton> CreateFromRootNode(const Node& rootNode);

	// スケルトンの更新処理（ローカル→スケルトンスペース行列の更新）
	void UpdateSkeleton();

	// 描画処理
	void Draw();

public: /// ---------- ゲッタ ---------- ///

	// ジョイントを取得
	std::vector<Joint>& GetJoints() { return joints_; }

	// 名前とインデックスのマップを取得
	const std::map<std::string, int32_t>& GetJointMap() const { return jointMap_; }

	int32_t GetRootIndex() const { return rootIndex_; }

private: /// ---------- メンバ関数 ---------- ///

	// 再帰的にジョイントを作成
	uint32_t CreateJointRecursive(const Node& node, const std::optional<int32_t>& parent);

private: /// ---------- メンバ変数 ---------- ///

	int32_t rootIndex_ = -1; // ルートジョイントのIndex
	std::map<std::string, int32_t> jointMap_; // Joint名とIndexとの辞書
	std::vector<Joint> joints_; // 所属しているジョイント
};

