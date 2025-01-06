#pragma once
#include "Obstacle.h"
#include <vector>
#include <random>


/// -------------------------------------------------------------
///				　	障害物を管理するクラス
/// -------------------------------------------------------------
class ObstacleManager
{
public: /// ---------- メンバ関数 ---------- ///

    // 初期化処理
    void Initialize(Object3DCommon* object3DCommon, int initialObstacleCount, float startZ, float spacing, float laneWidth);

    // 更新処理
    void Update(float scrollSpeed);

    // 描画処理
    void Draw(const Camera* camera);

    // 障害物を取得
    const std::vector<Obstacle>& GetObstacles() const { return obstacles_; }

private: /// ---------- メンバ関数 ---------- ///

    void SpawnObstacle(Object3DCommon* object3DCommon, float zPosition);  // 障害物を生成

private: /// ---------- メンバ変数 ---------- ///

    std::vector<Obstacle> obstacles_;  // 障害物のリスト
    std::default_random_engine randomEngine_;  // 乱数生成用エンジン
    std::uniform_int_distribution<int> laneDistribution_;  // レーンのランダム選択
    float laneWidth_;  // レーン間の幅
    float obstacleSpacing_;  // 障害物間の距離
};

