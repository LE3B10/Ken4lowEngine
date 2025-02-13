#pragma once
#include "DX12Include.h"
#include "Vector3.h" 
#include "Vector4.h"
#include "Matrix4x4.h"

/// ---------- 前方宣言 ---------- ///
class DirectXCommon;


/// -------------------------------------------------------------
///				　		ライトの管理クラス
/// -------------------------------------------------------------
class LightManager
{
public: /// ---------- 構造体 ---------- ///

	// 平行光源の構造体
	struct DirectionalLight final
	{
		Vector4 color;	   // ライトの色
		Vector3 direction; // ライトの向き
		float intensity;   // 輝度
	};

	// 点光源の構造体
	struct PointLight
	{
		Vector4 color;	  // ライトの色
		Vector3 position; // ライトの位置
		float intensity;  // 輝度
		float radius;	  // 有効範囲
		float decay;	  // 減衰率
		float padding[2]; // パディング
	};

	// スポットライトの構造体
	struct SpotLight
	{
		Vector4 color;		   // ライトの色
		Vector3 position;	   // ライトの位置
		float intensity;	   // スポットライトの輝度
		Vector3 direction;	   // スポットライトの方向
		float distance;		   // ライトの届く最大距離
		float decay;		   // 減衰率
		float cosFalloffStart; // 開始角度の余弦値
		float cosAngle;		   // スポットライトの余弦
		float padding[2];	   // パディング
	};

public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize(DirectXCommon* dxCommon);

	// パイプライン設定
	void ApplyToPipeline();

	void DrawImGui();

private: /// ---------- メンバ変数 ---------- ///

	DirectXCommon* dxCommon_ = nullptr;

	// 平行光源
	DirectionalLight* directionalLightData_;
	ComPtr<ID3D12Resource> directionalLightResource_;

	// 点光源
	PointLight* pointLightData_;
	ComPtr<ID3D12Resource> pointLightResource_;

	// スポットライト
	SpotLight* spotLightData_;
	ComPtr<ID3D12Resource> spotLightResource_;
};

