#include "Floor.h"
#include "Object3DCommon.h"
#include "CollisionTypeIdDef.h"
#include "Player.h"

Floor::Floor()
{
	// シリアルナンバーを振る
	serialNumber_ = nextSerialNumber_;
	// 次のシリアルナンバーに１を足す
	++nextSerialNumber_;
}


/// -------------------------------------------------------------
///							初期化処理
/// -------------------------------------------------------------
void Floor::Initialize(Object3DCommon* object3DCommon)
{
	// コライダーの初期化
	Collider::Initialize(object3DCommon);
	Collider::SetTranslate(worldTransform_.translate);

	// フロアオブジェクトの初期化
	object3D_ = std::make_unique<Object3D>();
	object3D_->Initialize(object3DCommon, "floorBlock.gltf");
	worldTransform_ = { { 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, -2.0f, 0.0f } };
	object3D_->SetTranslate(worldTransform_.translate);

	// 識別番号（ID）の設定
	Collider::SetTypeID(static_cast<uint32_t>(CollisionTypeIdDef::kFloorBlock));
}


/// -------------------------------------------------------------
///							更新処理
/// -------------------------------------------------------------
void Floor::Update()
{
	Collider::SetTranslate(worldTransform_.translate);
	Collider::Update();

	// オブジェクトの更新処理
	object3D_->Update();
}


/// -------------------------------------------------------------
///							描画処理
/// -------------------------------------------------------------
void Floor::Draw()
{
	// モデルの描画処理
	object3D_->Draw();
}


/// -------------------------------------------------------------
///						接触記録を削除
/// -------------------------------------------------------------
void Floor::ClearContactRecord()
{
	// 接触記録を削除
	contactRecord_.Clear();
}


/// -------------------------------------------------------------
///							衝突判定
/// -------------------------------------------------------------
void Floor::OnCollision(Collider* other)
{
	// 衝突相手の識別番号（ID）を取得
	uint32_t typeID = other->GetTypeID();

	// 衝突相手がプレイヤーなら
	if (typeID == static_cast<uint32_t>(CollisionTypeIdDef::kPlayer))
	{
		// プレイヤーの変数
		Player* player = static_cast<Player*>(other);

		// シリアルナンバー
		uint32_t serialNumber = player->GetSerialNumber(); // 仮

		// 接触記録があれば何もせずに抜ける
		if (contactRecord_.Check(serialNumber))
		{
			return;
		}

		// 接触記録に登録
		contactRecord_.Add(serialNumber);

		// 必要であればエフェクトを追加

	}
}


/// -------------------------------------------------------------
///                     衝突判定が呼ばれる処理
/// -------------------------------------------------------------
Vector3 Floor::GetOrientation(int index) const
{
	return Vector3();
}
