#define NOMINMAX
#include "ObstacleManager.h"
#include "Object3DCommon.h"
#include <ctime>


/// -------------------------------------------------------------
///				　			初期化処理
/// -------------------------------------------------------------
void ObstacleManager::Initialize(Object3DCommon* object3DCommon, int totalObstacleCount, float startZ, float spacing, float laneWidth)
{
	laneWidth_ = laneWidth;
	obstacleSpacing_ = spacing;
	initialZPosition_ = startZ;
	totalObstacleCount_ = totalObstacleCount;
	object3DCommon_ = object3DCommon;

	laneDistribution_ = std::uniform_int_distribution<int>(-1, 1);  // レーン：-1, 0, 1
	randomEngine_.seed(static_cast<unsigned>(std::time(nullptr)));  // 現在時刻をシードに使用

	// 初期の障害物を生成
	for (int i = 0; i < 1; ++i)
	{
		float zPosition = startZ - i * spacing;
		SpawnObstacle(object3DCommon, zPosition);
	}
}


/// -------------------------------------------------------------
///				　			　更新処理
/// -------------------------------------------------------------
void ObstacleManager::Update(float scrollSpeed)
{
	// 障害物の生成タイマーを更新
	spawnTimer_ += scrollSpeed * 0.1f; // スクロール速度に応じてタイマーを進める

	// 障害物を追加生成する条件
	if (spawnTimer_ >= spawnInterval_ && object3DCommon_)
	{
		spawnTimer_ = 0.0f; // タイマーをリセット

		// 最大生成数に達していない場合のみ生成
		if (obstacles_.size() < totalObstacleCount_)
		{
			// 固定の基準位置から生成
			float newZPsition = initialZPosition_;

			// 最も遠い障害物が位置を基準に新しい障害物を生成
			if (obstacles_.empty())
			{
				newZPsition = obstacles_.back().GetTransform().translate.z + obstacleSpacing_;
			}

			SpawnObstacle(object3DCommon_, newZPsition);
		}
	}

	// 既存の障害物を更新
	for (auto& obstacle : obstacles_)
	{
		obstacle.Update(scrollSpeed);

		// 障害物がカメラの後ろに到達したら再配置
		if (obstacle.GetTransform().translate.z < -50.0f)  // 再配置位置を適切に設定
		{
			obstacle.GetTransform().translate.z = initialZPosition_;
			obstacle.GetTransform().translate.x = laneDistribution_(randomEngine_) * laneWidth_;  // 新しいランダムなレーンに配置
		}
	}
}


/// -------------------------------------------------------------
///				　			　描画処理
/// -------------------------------------------------------------
void ObstacleManager::Draw(const Camera* camera)
{
	for (auto& obstacle : obstacles_)
	{
		obstacle.Draw(camera);
	}
}



void ObstacleManager::SpawnObstacle(Object3DCommon* object3DCommon, float zPosition)
{
	// 新しい障害物の z 座標を設定
	Transform transform;
	transform.translate.z = zPosition;

	// ランダムなレーンを選択
	int laneIndex = laneDistribution_(randomEngine_);  // -1, 0, 1の中からランダムに選択
	transform.translate.x = laneIndex * laneWidth_;
	transform.translate.y = 0.0f;	// y 座標は固定

	// 障害物を生成
	Obstacle obstacle;
	obstacle.Initialize(object3DCommon, "obstacle.obj", transform);  // モデルファイル名を適切に設定
	obstacles_.push_back(std::move(obstacle));
}
