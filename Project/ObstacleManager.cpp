#include "ObstacleManager.h"
#include "Object3DCommon.h"
#include <ctime>

/// -------------------------------------------------------------
///				　			初期化処理
/// -------------------------------------------------------------
void ObstacleManager::Initialize(Object3DCommon* object3DCommon, int initialObstacleCount, float startZ, float spacing, float laneWidth)
{
    laneWidth_ = laneWidth;
    obstacleSpacing_ = spacing;

    laneDistribution_ = std::uniform_int_distribution<int>(-1, 1);  // レーン：-1, 0, 1
    randomEngine_.seed(static_cast<unsigned>(std::time(nullptr)));  // 現在時刻をシードに使用

    // 初期の障害物を生成
    for (int i = 0; i < initialObstacleCount; ++i)
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
    for (auto& obstacle : obstacles_)
    {
        obstacle.Update(scrollSpeed);

        // 障害物がカメラの後ろに到達したら再配置
        if (obstacle.GetTransform().translate.z < -50.0f)  // 再配置位置を適切に設定
        {
            obstacle.GetTransform().translate.z += obstacles_.size() * obstacleSpacing_;  // 後方に再配置
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
    Transform transform;
    int laneIndex = laneDistribution_(randomEngine_);  // -1, 0, 1の中からランダムに選択
    transform.translate = { laneIndex * laneWidth_, 0.0f, zPosition };

    Obstacle obstacle;
    obstacle.Initialize(object3DCommon, "plane.obj", transform);  // モデルファイル名を適切に設定
    obstacles_.push_back(std::move(obstacle));
}
