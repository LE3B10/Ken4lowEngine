#include "Stage.h"
#include "LevelLoader.h"
#include "CollisionManager.h"
#include "CollisionTypeIdDef.h"

using namespace Ken4lowEngine;

void Ken4lowEngine::Stage::Initialize(const std::string& levelJsonPath, const std::string& defaultModelName)
{
	worldAABBs_.clear();
	worldColliders_.clear();

	/// ---------- レベルデータの読み込み ---------- ///
	std::unique_ptr<LevelData> loaded = LevelLoader::LoadLevel(levelJsonPath);
	if (!loaded)
	{
		// ロードに失敗した場合は空のステージを作る
		levelData_ = {};
		return;
	}
	// 読み込んだレベルデータをメンバ変数にコピー
	levelData_ = *loaded;

	/// ---------- ステージモデルの生成 ---------- ///
	std::string modelToLoad = defaultModelName;

	// Json 内に "MESH" タイプでモデル名が指定されていればそちらを優先して読み込む
	for (const ObjectData& data : levelData_.objects)
	{
		if (/*data.type == "MESH" &&*/ !data.modelName.empty())
		{
			modelToLoad = data.modelName;
			break; // 最初に見つけたモデル名を使用
		}
	}

	stageModel_ = std::make_unique<Object3D>();
	stageModel_->Initialize(modelToLoad);

	// 一先ず原点にそのまま置く
	stageModel_->SetTranslate(offset);
	stageModel_->SetRotate({ 0.0f, 0.0f, 0.0f });
	stageModel_->SetScale({ 1.0f, 1.0f, 1.0f });

	/// ---------- WorldCollisionResolver用AABBとColliderの生成 ---------- ///
	worldAABBs_.reserve(levelData_.objects.size());

	for (const ObjectData& data : levelData_.objects)
	{
		// Json の Collider が有効な BOX だけを使う
		if (!data.collider.enabled) continue;
		if (data.collider.type != "BOX") continue;

		// ローカル →ワールド（スケール反映）
		const Vector3 centerW = {
			data.position.x + data.collider.center.x * data.scale.x + offset.x,
			data.position.y + data.collider.center.y * data.scale.y + offset.y,
			data.position.z + data.collider.center.z * data.scale.z + offset.z
		};

		// size(フルサイズ) → half(半サイズ)
		const Vector3 halfW = {
			0.5f * data.collider.size.x * data.scale.x,
			0.5f * data.collider.size.y * data.scale.y,
			0.5f * data.collider.size.z * data.scale.z,
		};

		// AABB を生成してリストに追加
		AABB aabb{};
		aabb.min = centerW - halfW;
		aabb.max = centerW + halfW;
		worldAABBs_.push_back(aabb);

		// コライダーを生成してリストに追加
		auto up = std::make_unique<Collider>();
		Collider* raw = up.get(); // 先に生ポインタを保持

		raw->SetTypeID(static_cast<uint32_t>(CollisionTypeIdDef::kWorld));
		raw->SetCenterPosition(centerW);
		raw->SetOBBHalfSize(halfW);
		// owner_ が欲しければ raw->SetOwner(this); などしても良い

		worldColliders_.push_back(std::move(up));
	}

}

void Ken4lowEngine::Stage::Update()
{
	// ステージは今のところ特に更新処理はない
	// いずれステージギミック用のupdate処理などが入るかもしれない
	stageModel_->Update();
}

void Ken4lowEngine::Stage::Draw()
{
	if (stageModel_) stageModel_->Draw();
}

void Ken4lowEngine::Stage::RegisterColliders(CollisionManager* collisionManager)
{
	// コライダーを衝突マネージャーに登録
	if (!collisionManager) return; // 安全チェック

	// worldColliders_ の各コライダーを衝突マネージャーに登録
	for (auto& up : worldColliders_)
		collisionManager->AddCollider(up.get());
}
