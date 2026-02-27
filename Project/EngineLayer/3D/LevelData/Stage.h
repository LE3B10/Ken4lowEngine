#pragma once
#include "LevelData.h"
#include "AABB.h"
#include "Collider.h"
#include "Object3D.h"

#include <memory>
#include <vector>

/// ---------- 前方宣言 ---------- ///
class CollisionManager;

namespace Ken4lowEngine
{

	/// -------------------------------------------------------------
	///				ステージ（地形＋ワールドコリジョン）クラス
	///	--------------------------------------------------------------
	class Stage
	{
	public: /// ---------- メンバ関数 ---------- ///

		// コンストラクタ・デストラクタ
		Stage() = default;
		~Stage() = default;

		// 初期化処理
		void Initialize(const std::string& levelJsonPath, const std::string& defaultModelName);

		// 更新処理
		void Update();

		// 描画処理
		void Draw();

	public: /// ---------- アクセサ関数 ---------- ///

		// ワールドコリジョンのAABBリストを取得
		const std::vector<AABB>& GetWorldAABBs() const { return worldAABBs_; }

		// コライダーを衝突マネージャーに登録
		void RegisterColliders(CollisionManager* collisionManager);

	private: /// ---------- メンバ変数 ---------- ///

		LevelData levelData_; // レベルデータ

		// ステージモデル（今は1つのモデルのみ）
		std::unique_ptr<Object3D> stageModel_; // ステージの3Dモデル

		std::vector<AABB> worldAABBs_; // ワールドコリジョンのAABBリスト

		std::vector<std::unique_ptr<Collider>> worldColliders_; // コライダーのリスト

		Vector3 offset = { 50.0f, 0.0f, -50.0f }; // ステージモデルのオフセット（原点からの位置）
	};
}