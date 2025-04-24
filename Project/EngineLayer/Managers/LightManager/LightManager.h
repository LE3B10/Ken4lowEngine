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
private:
	// デフォルトコンストラクタ  
	LightManager() = default;
	// デフォルトデストラクタ  
	~LightManager() = default;
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

	static LightManager* GetInstance();

	// 初期化処理
	void Initialize(DirectXCommon* dxCommon);

	// 描画処理設定
	void PreDraw();

	// ImGuiを描画
	void DrawImGui();

private: /// ---------- メンバ関数 ---------- ///

	// 平行光源の生成
	void CreateDirectionalLight();

	// 点光源の生成
	void CreatePointLight();

	// スポットライトの生成
	void CreateSpotLight();

public: /// ---------- 平行光源の設定 ---------- ///

	/// <summary>
	/// 平行光源の設定
	/// </summary>
	/// <param name="direction">平行光源の方向</param>
	/// <param name="color">平行光源の色</param>
	/// <param name="intensity">平行光源の輝度</param>
	void SetDirectionalLight(const Vector3& direction, const Vector4& color, float intensity);

	// 平行光源の色を設定
	void SetDirectionalLightColor(const Vector4& color) { directionalLightData_->color = color; }

	// 平行光源の方向を設定
	void SetDirectionalLightDirection(const Vector3& direction) { directionalLightData_->direction = direction; }

	// 平行光源の輝度を設定
	void SetDirctionalLightIntensity(float intensity) { directionalLightData_->intensity = intensity; }

public: /// ---------- 点光源の設定 ---------- ///

	/// <summary>
	/// 点光源の設定
	/// </summary>
	/// <param name="position">点光源の座標</param>
	/// <param name="color">点光源の色</param>
	/// <param name="intensity">点光源の輝度</param>
	/// <param name="radius">点光源の有効範囲</param>
	/// <param name="decay">点光源の減衰率</param>
	void SetPointLight(const Vector3& position, const Vector4& color, float intensity, float radius, float decay);

	// 点光源の色を設定
	void SetPointLightColor(const Vector4& color) { pointLightData_->color = color; }

	// 点光源の座標を設定
	void SetPointLightPosition(const Vector3& position) { pointLightData_->position = position; }

	// 点光源の輝度を設定
	void SetPointLightIntensity(float intensity) { pointLightData_->intensity = intensity; }

	// 点光源の有効範囲を設定
	void SetPointLightRadius(float radius) { pointLightData_->radius = radius; }

	// 点光源の減衰率を設定
	void SetPointLightDecay(float decay) { pointLightData_->decay = decay; }

public: /// ---------- スポットライトの設定 ---------- ///

	/// <summary>
	/// スポットライトの設定
	/// </summary>
	/// <param name="position">スポットライトの座標</param>
	/// <param name="direction">スポットライトの方向</param>
	/// <param name="color">スポットライトの色</param>
	/// <param name="intensity">スポットライトの輝度</param>
	/// <param name="distance">スポットライトの届く最大距離</param>
	/// <param name="decay">スポットライトの減衰率</param>
	/// <param name="cosFalloffStart">スポットライトの開始角度の余弦値</param>
	/// <param name="cosAngle">スポットライトの余弦</param>
	void SetSpotLight(const Vector3& position, const Vector3& direction, const Vector4& color, float intensity, float distance, float decay, float cosFalloffStart, float cosAngle);

	// スポットライトの色を設定
	void SetSpotLightColor(const Vector4& color) { spotLightData_->color = color; }

	// スポットライトの座標を設定
	void SetSpotLightPosition(const Vector3& position) { spotLightData_->position = position; }

	// スポットライトの輝度を設定
	void SetSpotLightIntensity(float intensity) { spotLightData_->intensity = intensity; }

	// スポットライトの方向の設定
	void SetSpotLightDirection(const Vector3& direction) { spotLightData_->direction = direction; }

	// スポットライトの減衰率の設定
	void SetSpotLightDecay(float decay) { spotLightData_->decay = decay; }

	// スポットライトの開始角度の余弦値を設定
	void SetSpotLightCosFalloffStart(float cosFalloffStart) { spotLightData_->cosFalloffStart = cosFalloffStart; }

	// スポットライトの余弦を設定
	void SetSpotLightCosAngle(float cosAngle) { spotLightData_->cosAngle = cosAngle; }

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

private: /// ---------- コピー禁止 ---------- ///

	// コピーコンストラクタ
	LightManager(const LightManager&) = delete;
	// コピー代入演算子
	LightManager& operator=(const LightManager&) = delete;
	// ムーブコンストラクタ
	LightManager(LightManager&&) = delete;
	// ムーブ代入演算子
	LightManager& operator=(LightManager&&) = delete;

};

