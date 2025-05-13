#include "LightManager.h"
#include "DirectXCommon.h"
#include <ResourceManager.h>
#include <numbers>
#include "ImGuiManager.h"

LightManager* LightManager::GetInstance()
{
	static LightManager instance;
	return &instance;
}

/// -------------------------------------------------------------
///				　		初期化処理
/// -------------------------------------------------------------
void LightManager::Initialize(DirectXCommon* dxCommon)
{
	dxCommon_ = dxCommon;
	CreateDirectionalLight();
	CreatePointLight();
	CreateSpotLight();
}


/// -------------------------------------------------------------
///				　		　描画処理設定
/// -------------------------------------------------------------
void LightManager::PreDraw()
{
	auto commandList = dxCommon_->GetCommandManager()->GetCommandList();

	// 平行光源のCBufferの設定
	commandList->SetGraphicsRootConstantBufferView(4, directionalLightResource_->GetGPUVirtualAddress());

	// 点光源のCBufferの設定
	commandList->SetGraphicsRootConstantBufferView(5, pointLightResource_->GetGPUVirtualAddress());

	// スポットライトのCBufferの設定
	commandList->SetGraphicsRootConstantBufferView(6, spotLightResource_->GetGPUVirtualAddress());
}

/// -------------------------------------------------------------
///				　		　平行光源の生成
/// -------------------------------------------------------------
void LightManager::CreateDirectionalLight()
{
	//平行光源用のリソースを作る
	directionalLightResource_ = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(DirectionalLight));

	//書き込むためのアドレスを取得
	directionalLightResource_->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData_));

	directionalLightData_->color = { 1.0f,1.0f,1.0f ,1.0f };					 // 平行光源の色
	directionalLightData_->direction = Vector3::Normalize({ 0.0f, 1.0f, 0.0f }); // 平行光源の向き
	directionalLightData_->intensity = 1.0f;									 // 平行光源の輝度
	directionalLightResource_->Unmap(0, nullptr);
}


/// -------------------------------------------------------------
///				　		　点光源の生成
/// -------------------------------------------------------------
void LightManager::CreatePointLight()
{
	// ポイントライト用のリソースを作る
	pointLightResource_ = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(PointLight));

	// 書き込むためのアドレスを取得
	pointLightResource_->Map(0, nullptr, reinterpret_cast<void**>(&pointLightData_));

	pointLightData_->position = { 0.0f, 10.0f, 0.0f };	 // ポイントライトの初期位置
	pointLightData_->color = { 1.0f, 1.0f, 1.0f, 1.0f }; // ポイントライトの色
	pointLightData_->intensity = 1.0f;					 // ポイントライトの輝度
	pointLightData_->radius = 10.0f;					 // ポイントライトの有効範囲
	pointLightData_->decay = 1.0f;						 // ポイントライトの減衰率
	pointLightResource_->Unmap(0, nullptr);
}


/// -------------------------------------------------------------
///				　	　スポットライトの生成
/// -------------------------------------------------------------
void LightManager::CreateSpotLight()
{
	// スポットライト用のリソースを作る
	spotLightResource_ = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(SpotLight));

	// 書き込むためのアドレスを取得
	spotLightResource_->Map(0, nullptr, reinterpret_cast<void**>(&spotLightData_));

	spotLightData_->color = { 1.0f,1.0f,1.0f,1.0f };							  // スポットライトの色
	spotLightData_->position = { 2.0f,2.0f,0.0f };								  // スポットライトの位置
	spotLightData_->distance = 7.0f;											  // スポットライトの距離
	spotLightData_->direction = Vector3::Normalize({ -1.0f, -1.0f,0.0f });		  // スポットライトの方向
	spotLightData_->intensity = 4.0f;											  // スポットライトの輝度
	spotLightData_->decay = 2.0f;												  // スポットライトの減衰率
	spotLightData_->cosFalloffStart = std::cos(std::numbers::pi_v<float> / 6.0f); // スポットライトの開始角度の余弦値
	spotLightData_->cosAngle = std::cos(std::numbers::pi_v<float> / 3.0f);		  // スポットライトの余弦
	spotLightResource_->Unmap(0, nullptr);
}


/// -------------------------------------------------------------
///				　		　平行光源の設定
/// -------------------------------------------------------------
void LightManager::SetDirectionalLight(const Vector3& direction, const Vector4& color, float intensity)
{
	directionalLightData_->color = color;							  // 平行光源の色
	directionalLightData_->direction = Vector3::Normalize(direction); // 平行光源の向き
	directionalLightData_->intensity = intensity;					  // 平行光源の輝度
}


/// -------------------------------------------------------------
///				　		　点光源の設定
/// -------------------------------------------------------------
void LightManager::SetPointLight(const Vector3& position, const Vector4& color, float intensity, float radius, float decay)
{
	pointLightData_->position = position;	// ポイントライトの初期位置
	pointLightData_->color = color;			// ポイントライトの色
	pointLightData_->intensity = intensity;	// ポイントライトの輝度
	pointLightData_->radius = radius;		// ポイントライトの有効範囲
	pointLightData_->decay = decay;			// ポイントライトの減衰率
}

/// -------------------------------------------------------------
///				　		スポットライトの設定
/// -------------------------------------------------------------
void LightManager::SetSpotLight(const Vector3& position, const Vector3& direction, const Vector4& color, float intensity, float distance, float decay, float cosFalloffStart, float cosAngle)
{
	spotLightData_->color = color;							   // スポットライトの色
	spotLightData_->position = position;					   // スポットライトの位置
	spotLightData_->distance = distance;					   // スポットライトの距離
	spotLightData_->direction = Vector3::Normalize(direction); // スポットライトの方向
	spotLightData_->intensity = intensity;					   // スポットライトの輝度
	spotLightData_->decay = decay;							   // スポットライトの減衰率
	spotLightData_->cosFalloffStart = cosFalloffStart;		   // スポットライトの開始角度の余弦値
	spotLightData_->cosAngle = cosAngle;					   // スポットライトの余弦
}


/// -------------------------------------------------------------
///				　		　	ImGui
/// -------------------------------------------------------------
void LightManager::DrawImGui()
{
	// 平行光源の設定
	if (ImGui::CollapsingHeader("Directional Light Settings"))
	{
		if (ImGui::SliderFloat3("Directional Light Direction", &directionalLightData_->direction.x, -1.0f, 1.0f))
		{
			directionalLightData_->direction = Vector3::Normalize(directionalLightData_->direction);
		}
		ImGui::SliderFloat("Directional Light Intensity", &directionalLightData_->intensity, 0.0f, 10.0f);
	}

	// 点光源の設定
	if (ImGui::CollapsingHeader("Point Light Settings"))
	{
		static float pointPosition[3] = { pointLightData_->position.x, pointLightData_->position.y, pointLightData_->position.z };
		static float pointIntensity = pointLightData_->intensity;
		static float pointRadius = pointLightData_->radius;
		static float pointDecay = pointLightData_->decay;

		// 点光源の位置
		if (ImGui::SliderFloat3("Point Light Position", pointPosition, -10.0f, 10.0f))
		{
			pointLightData_->position = { pointPosition[0], pointPosition[1], pointPosition[2] };
		}

		// 点光源の輝度
		if (ImGui::SliderFloat("Point Light Intensity", &pointIntensity, 0.0f, 5.0f))
		{
			pointLightData_->intensity = pointIntensity;
		}

		// 点光源の半径
		if (ImGui::SliderFloat("Point Light Radius", &pointRadius, 0.0f, 20.0f))
		{
			pointLightData_->radius = pointRadius;
		}

		// 点光源の減衰率
		if (ImGui::SliderFloat("Point Light Decay", &pointDecay, 0.1f, 5.0f))
		{
			pointLightData_->decay = pointDecay;
		}
	}

	// スポットライトの設定
	if (ImGui::CollapsingHeader("Spot Light Settings"))
	{
		static float spotPosition[3] = { spotLightData_->position.x, spotLightData_->position.y, spotLightData_->position.z };
		static float spotDirection[3] = { spotLightData_->direction.x, spotLightData_->direction.y, spotLightData_->direction.z };
		static float spotIntensity = spotLightData_->intensity;
		static float spotDistance = spotLightData_->distance;
		static float spotDecay = spotLightData_->decay;

		// スポットライトの開始角度と終了角度（度単位で操作）
		static float spotFalloffStartAngle = std::acos(spotLightData_->cosFalloffStart) * 180.0f / std::numbers::pi_v<float>;
		static float spotConeAngle = std::acos(spotLightData_->cosAngle) * 180.0f / std::numbers::pi_v<float>;


		// スポットライトの位置
		if (ImGui::SliderFloat3("Spot Light Position", spotPosition, -10.0f, 10.0f))
		{
			spotLightData_->position = { spotPosition[0], spotPosition[1], spotPosition[2] };
		}

		// スポットライトの方向
		if (ImGui::SliderFloat3("Spot Light Direction", spotDirection, -1.0f, 1.0f))
		{
			spotLightData_->direction = Vector3::Normalize({ spotDirection[0], spotDirection[1], spotDirection[2] });
		}

		// スポットライトの輝度
		if (ImGui::SliderFloat("Spot Light Intensity", &spotIntensity, 0.0f, 10.0f))
		{
			spotLightData_->intensity = spotIntensity;
		}

		// スポットライトの距離
		if (ImGui::SliderFloat("Spot Light Distance", &spotDistance, 0.0f, 50.0f))
		{
			spotLightData_->distance = spotDistance;
		}

		// スポットライトの減衰率
		if (ImGui::SliderFloat("Spot Light Decay", &spotDecay, 0.0f, 5.0f))
		{
			spotLightData_->decay = spotDecay;
		}

		// スポットライトの開始角度（Falloff Start Angle）
		if (ImGui::SliderFloat("Spot Light Falloff Start Angle (Degrees)", &spotFalloffStartAngle, 0.0f, 90.0f))
		{
			// 度をラジアンに変換し、余弦値を計算
			spotLightData_->cosFalloffStart = std::cos(spotFalloffStartAngle * std::numbers::pi_v<float> / 180.0f);
		}

		// スポットライトの終了角度（Cone Angle）
		if (ImGui::SliderFloat("Spot Light Cone Angle (Degrees)", &spotConeAngle, 0.0f, 90.0f))
		{
			// 度をラジアンに変換し、余弦値を計算
			spotLightData_->cosAngle = std::cos(spotConeAngle * std::numbers::pi_v<float> / 180.0f);
		}

		// 開始角度が終了角度より大きくならないように調整
		if (spotLightData_->cosFalloffStart < spotLightData_->cosAngle)
		{
			spotLightData_->cosFalloffStart = spotLightData_->cosAngle;
			spotFalloffStartAngle = std::acos(spotLightData_->cosFalloffStart) * 180.0f / std::numbers::pi_v<float>;
		}
	}
}
