#include "Floor.h"
#include "Object3DCommon.h"
#include "CollisionTypeIdDef.h"
#include "Player.h"
#include "Windows.h"


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
		uint32_t serialNumber = player->GetSerialNumber();

		// 接触記録があれば何もせずに抜ける
		if (contactRecord_.Check(serialNumber))
		{
			return;
		}

		// 接触記録に登録
		contactRecord_.Add(serialNumber);

		// 必要であればエフェクトを追加

		OutputDebugStringA("Hit !!!\n");

	}
}


/// -------------------------------------------------------------
///                     衝突判定が呼ばれる処理
/// -------------------------------------------------------------
Vector3 Floor::GetOrientation(int index) const
{
	// 回転角 (オイラー角)
	float roll = worldTransform_.rotate.x; // Z軸回転
	float pitch = worldTransform_.rotate.y; // X軸回転
	float yaw = worldTransform_.rotate.z; // Y軸回転

	// 回転行列の各要素を計算
	float cosYaw = cos(yaw);
	float sinYaw = sin(yaw);
	float cosPitch = cos(pitch);
	float sinPitch = sin(pitch);
	float cosRoll = cos(roll);
	float sinRoll = sin(roll);

	// 各軸の方向ベクトルを求める
	Vector3 xAxis(
		cosYaw * cosRoll + sinYaw * sinPitch * sinRoll,
		cosPitch * sinRoll,
		sinYaw * cosRoll - cosYaw * sinPitch * sinRoll
	);

	Vector3 yAxis(
		-cosYaw * sinRoll + sinYaw * sinPitch * cosRoll,
		cosPitch * cosRoll,
		-sinYaw * sinRoll - cosYaw * sinPitch * cosRoll
	);

	Vector3 zAxis(
		sinYaw * cosPitch,
		-sinPitch,
		cosYaw * cosPitch
	);

	// 指定された軸を返す
	switch (index)
	{
	case 0: return xAxis; // X軸方向
	case 1: return yAxis; // Y軸方向
	case 2: return zAxis; // Z軸方向
	default: return Vector3(0, 0, 0);
	}
}
