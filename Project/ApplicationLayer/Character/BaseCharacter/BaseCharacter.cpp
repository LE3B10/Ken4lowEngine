#include "BaseCharacter.h"

#include <memory>
#include <utility>

using namespace Ken4lowEngine;

/// -------------------------------------------------------------
///				　			　 更新処理
/// -------------------------------------------------------------
void BaseCharacter::Initialize()
{
	// 体幹部位の初期化
	body_.object = std::make_unique<Object3D>();	// オブジェクト生成
	body_.object->Initialize("Characters/body.gltf");	// モデル読み込み
	body_.transform.translate_ = { 0.0f, 2.25f, 0.0f };	// 初期位置

	// 子オブジェクト（頭、腕、脚）をリストに追加
	std::vector<std::pair<std::string, Vector3>> partData =
	{
		{ "Characters/head.gltf", { 0.0f, 0.75f, 0.0f } },		  // 頭   : 0
		{ "Characters/left_arm.gltf", { -0.75f, 0.75f, 0.0f } },  // 左腕 : 1
		{ "Characters/right_arm.gltf", { 0.75f, 0.75f, 0.0f } },  // 右腕 : 2
		{ "Characters/left_leg.gltf", { -0.25f, -0.75f, 0.0f } }, // 左脚 : 3
		{ "Characters/right_leg.gltf", { 0.25f, -0.75f, 0.0f } }  // 右脚 : 4
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
	(void)deltaTime; // 未使用

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
///				　	シャドウマトリクスの更新
/// -------------------------------------------------------------
void BaseCharacter::UpdateShadowMatrix(const Matrix4x4& lightViewProjection)
{
	body_.object->UpdateShadowMatrix(lightViewProjection);
	for (const auto& part : parts_)
	{
		if (part.active && part.object)
		{
			part.object->UpdateShadowMatrix(lightViewProjection);
		}
	}
}

/// -------------------------------------------------------------
///				　		シャドウ描画処理
/// -------------------------------------------------------------
void BaseCharacter::DrawShadow()
{
	// 体幹部位のシャドウ描画
	body_.object->DrawShadow();

	// 各部位のシャドウ描画
	for (const auto& part : parts_)
	{
		if (part.active && part.object)
		{
			part.object->DrawShadow();
		}
	}
}

/// -------------------------------------------------------------
///				　			スキン適用
/// -------------------------------------------------------------
void BaseCharacter::ApplySkinToAllParts(const std::string& texPath)
{
	// 体幹部位
	if (body_.object) ApplySkinTo(body_.object.get(), texPath);

	// 各部位
	for (auto& part : parts_)
	{
		// 描画されている部位にのみスキンを適用
		if (part.object && part.active)
		{
			ApplySkinTo(part.object.get(), texPath);
		}
	}
}

/// -------------------------------------------------------------
///				　	スキン適用（静的関数）
/// -------------------------------------------------------------
void BaseCharacter::ApplySkinTo(Object3D* obj, const std::string& texPath)
{
	if (!obj) return; // nullチェック

	// 全サブメッシュを同じテクスチャに差し替える
	obj->SetTextureForAll(texPath);
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
		// 親の回転を適用
		part.transform.worldRotate_ = body_.transform.worldRotate_;

		// 親の影響を受ける
		part.transform.Update();

		// ワールド変換をオブジェクトに適用
		if (part.transform.useQuaternionRotation_)
		{
			// ワールド行列を直接オブジェクトに適用
			part.object->UpdateWithWorldMatrix(part.transform.worldMatrix_);
		}
		else
		{
			part.object->SetTranslate(part.transform.worldTranslate_); // ワールド座標を適用
			part.object->SetRotate(part.transform.worldRotate_);	   // ワールド回転を適用
			part.object->Update();									   // オブジェクト更新
		}
	}
}

/// -------------------------------------------------------------
///					　	中心座標を取得
/// -------------------------------------------------------------
Vector3 BaseCharacter::GetCenterPosition() const
{
	// ローカル座標でのオフセット
	const Vector3 offset = { 0.0f, 0.0f, 0.0f };

	// ワールド座標に変換
	Vector3 worldPosition = body_.transform.translate_ + offset;

	// ワールド回転を考慮してオフセットを回転させる
	return worldPosition;
}
