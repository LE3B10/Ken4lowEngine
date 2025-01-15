#include "Floor.h"
#include <Camera.h>

/// -------------------------------------------------------------
///							初期化処理
/// -------------------------------------------------------------
void Floor::Initialize(Object3DCommon* object3DCommon, int tileCount, float startZ, float tileSpacing, float laneWidth)
{
    for (int laneIndex = -1; laneIndex <= 1; ++laneIndex) // 左、中央、右の3つのレーン
    {
        for (int i = 0; i < tileCount; ++i)
        {
            auto floorObject = std::make_unique<Object3D>();
            floorObject->Initialize(object3DCommon, "floor.obj");

            Tile tile;
            tile.transform.translate = { laneIndex * laneWidth, -1.0f, startZ - i * tileSpacing }; // レーンごとの位置
            tile.laneIndex = laneIndex; // レーンインデックスを保存
            floorObjects_.push_back(std::move(floorObject));
            tiles_.push_back(tile);
        }
    }
}


/// -------------------------------------------------------------
///							更新処理
/// -------------------------------------------------------------
void Floor::Update(float scrollSpeed, const Camera* camera)
{
    int tileCount = static_cast<int>(tiles_.size());

    for (int i = 0; i < tileCount; ++i)
    {
        // 床の移動（スクロール）
        tiles_[i].transform.translate.z -= scrollSpeed;

        // 再配置（カメラの後方に抜けた場合）
        if (tiles_[i].transform.translate.z < camera->GetTranslate().z - 50.0f) // カメラの後方一定距離
        {
            tiles_[i].transform.translate.z += tileCount / 3.0f * 10.0f; // 再配置（レーンごとの間隔調整）
        }

        // 位置をオブジェクトに反映
        floorObjects_[i]->SetTranslate(tiles_[i].transform.translate);
        floorObjects_[i]->Update();
    }
}


/// -------------------------------------------------------------
///							描画処理
/// -------------------------------------------------------------
void Floor::Draw(const Camera* camera)
{
    for (size_t i = 0; i < tiles_.size(); ++i)
    {
        // カメラの可視範囲内かどうかをチェック
        if (tiles_[i].transform.translate.z > camera->GetTranslate().z - 50.0f &&
            tiles_[i].transform.translate.z < camera->GetTranslate().z + 150.0f) // 可視範囲
        {
            floorObjects_[i]->Draw(); // 描画
        }
    }
}

float Floor::GetFloorHeightAt(float x, float z) const
{
    // レーンの幅を計算
    float laneWidth = 3.0f;

    for (const auto& tile : tiles_)
    {
        // X座標が同じレーン内にあるか確認
        if (std::abs(tile.transform.translate.x - x) < laneWidth / 2.0f)
        {
            // Z座標が近い床を探す
            if (std::abs(tile.transform.translate.z - z) < 10.0f)
            {
                return tile.transform.translate.y; // 床の高さを返す
            }
        }
    }

    // 該当する床が見つからない場合のデフォルト値（例: -1.0f）
    return -1.0f;
}
