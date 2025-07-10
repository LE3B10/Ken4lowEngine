#pragma once
#include <DX12Include.h>
#include "Vector3.h"
#include "Matrix4x4.h"

/// ---------- 前方宣言 ---------- ///
class Camera;


/// -------------------------------------------------------------
///				　	ワールド変換データクラス
/// -------------------------------------------------------------
class WorldTransform
{
public: /// ---------- 構造体 ---------- ///

	struct TransformationMatrix final
	{
		Matrix4x4 WVP;
		Matrix4x4 World;
		Matrix4x4 WorldInversedTranspose;
	};

public: /// ---------- メンバ変数 ---------- ///

	// ローカルスケール
	Vector3 scale_ = { 1.0f, 1.0f, 1.0f };
	// ローカル回転角
	Vector3 rotate_ = { 0.0f, 0.0f, 0.0f };
	// ローカル座標
	Vector3 translate_ = { 0.0f, 0.0f, 0.0f };
	// ワールド座標
	Vector3 worldTranslate_ = { 0.0f, 0.0f, 0.0f };
	// ワールド回転
	Vector3 worldRotate_ = { 0.0f, 0.0f, 0.0f };
	// ワールド変換行列
	Matrix4x4 matWorld_;
	// 親となるワールド変換ポインタ
	const WorldTransform* parent_ = nullptr;

public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize();

	// 更新処理
	void Update();

	// パイプラインを設定
	void SetPipeline(UINT rootParameterIndex = 1);

	// ワールド変換行列を取得
	const Matrix4x4& GetWorldMatrix() const { return matWorld_; }

private: /// ---------- メンバ変数 ---------- ///

	Camera* camera_ = nullptr;
	TransformationMatrix* wvpData = nullptr;
	ComPtr <ID3D12Resource> wvpResource;

};
