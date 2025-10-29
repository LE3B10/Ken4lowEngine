#include "BaseCharacter.h"
#include "Object3DCommon.h"

/// -------------------------------------------------------------
///				　			　 更新処理
/// -------------------------------------------------------------
void BaseCharacter::Initialize()
{
	// 体幹部位の初期化
	body_.object = std::make_unique<Object3D>();
	body_.object->Initialize("Character/body.gltf");
	body_.transform.translate_ = { 0.0f, 2.5f, 0.0f };	// 初期位置

	// 子オブジェクト（頭、腕、脚）をリストに追加
	std::vector<std::pair<std::string, Vector3>> partData =
	{
		{"Character/head.gltf",		 { 0.0f,   0.75f, 0.0f } },	// 頭   : 0
		{"Character/left_arm.gltf",  {-0.75f,  0.75f, 0.0f } },	// 左腕 : 1
		{"Character/right_arm.gltf", { 0.75f,  0.75f, 0.0f } }, // 右腕 : 2
		{"Character/left_leg.gltf",  {-0.25f, -0.75f, 0.0f } }, // 左脚 : 3
		{"Character/right_leg.gltf", { 0.25f, -0.75f, 0.0f } }  // 右脚 : 4
	};

	// 部位データをもとに部位オブジェクトを生成
	for (const auto& [modelPath, position] : partData)
	{
		// ローカル変数で部位データを作成
		BodyPart part = {};
		part.object = std::make_unique<Object3D>();			  // オブジェクト生成
		part.object->Initialize(modelPath); 				  // モデル読み込み
		part.transform.translate_ = position;				  // 位置設定
		part.object->SetTranslate(part.transform.translate_); // オブジェクトにも位置設定
		part.transform.parent_ = &body_.transform;			  // 親を設定
		parts_.push_back(std::move(part));					  // リストに追加
	}
}

/// -------------------------------------------------------------
///				　			　 更新処理
/// -------------------------------------------------------------
void BaseCharacter::Update(float deltaTime)
{
	// デルタタイム未使用
	(void)deltaTime;

	// 階層更新
	UpdateHierarchy();
}

/// -------------------------------------------------------------
///				　			　 描画処理
/// -------------------------------------------------------------
void BaseCharacter::Draw()
{
	// 体幹部位の描画
	if (body_.active && body_.object)
	{
		body_.object->Draw();
	}
	// 各部位の描画
	for (const auto& part : parts_)
	{
		if (part.active && part.object)
		{
			part.object->Draw();
		}
	}
}

/// -------------------------------------------------------------
///				　			階層更新
/// -------------------------------------------------------------
void BaseCharacter::UpdateHierarchy()
{
	// 体のワールド変換を更新
	body_.transform.Update();
	body_.object->SetTranslate(body_.transform.translate_); // 位置を適用
	body_.object->SetRotate(body_.transform.rotate_);		// 回転を適用
	body_.object->Update();									// オブジェクト更新

	// 各部位のワールド変換を更新
	for (auto& part : parts_)
	{
		part.transform.worldRotate_ = body_.transform.worldRotate_; // 親の回転を適用
		part.transform.Update(); // 親の影響を受ける

		part.object->SetTranslate(part.transform.worldTranslate_); // ワールド座標を適用
		part.object->SetRotate(part.transform.worldRotate_); // ワールド回転を適用
		part.object->Update();
	}
}

/// -------------------------------------------------------------
///					　	中心座標を取得
/// -------------------------------------------------------------
Vector3 BaseCharacter::GetCenterPosition() const
{
	// ローカル座標でのオフセット
	const Vector3 offset = { 0.0f,1.0f,0.0f };
	// ワールド座標に変換
	Vector3 worldPosition = body_.transform.translate_ + offset;
	return worldPosition;
}
