#include "ShadowTestScene.h"
#include <Object3DCommon.h>
#include <Wireframe.h>
#include <SpriteManager.h>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

using namespace Ken4lowEngine;

void ShadowTestScene::Initialize()
{
	dxCommon_ = DirectXCommon::GetInstance();
	input_ = Input::GetInstance();

	// 影確認用オブジェクト
	floor_ = std::make_unique<Object3D>();
	floor_->Initialize("terrain.gltf");
	floor_->SetScale({ 20.0f, 1.0f, 20.0f });
	floor_->SetTranslate({ 0.0f, -1.0f, 0.0f });
	floor_->SetColor({ 0.8f, 0.8f, 0.8f, 1.0f });

	box_ = std::make_unique<Object3D>();
	box_->Initialize("cube.gltf");
	box_->SetScale({ 2.0f, 2.0f, 2.0f });
	box_->SetTranslate({ 0.0f, 1.0f, 0.0f });
	box_->SetColor({ 0.4f, 0.7f, 1.0f, 1.0f });

	// ライト方向はとりあえず固定
	lightDirection_ = Vector3::Normalize(lightDirection_);

	Vector3 managerDir{};
	if (TryGetDirectionalLightFromManager(managerDir))
	{
		lightDirection_ = managerDir;
	}

	UpdateLightViewProjection();

	// 初回更新
	floor_->Update();
	box_->Update();
	UpdateShadowMatrices();
}

void ShadowTestScene::Update()
{
	if (floor_) { floor_->Update(); }
	if (box_) { box_->Update(); }

	Vector3 managerDir{};
	if (TryGetDirectionalLightFromManager(managerDir))
	{
		lightDirection_ = managerDir;
	}
	else
	{
		// fallback: ImGui で調整した値を反映
		lightDirection_ = Vector3::Normalize(lightDirection_);
	}

	UpdateLightViewProjection();
	UpdateShadowMatrices();
}

void ShadowTestScene::UpdateLightViewProjection()
{
	// 今は新規シーンで最小確認したいので、中心は原点付近固定でOK
	shadowCenter_ = { 0.0f, 1.0f, 0.0f };

	lightViewProjection_ = Matrix4x4::MakeLightViewProjection(
		lightDirection_,
		shadowCenter_,
		shadowDistance_,
		orthoHalfWidth_,
		orthoHalfHeight_,
		nearZ_,
		farZ_
	);
}

void ShadowTestScene::UpdateShadowMatrices()
{
	if (floor_) { floor_->UpdateShadowMatrix(lightViewProjection_); }
	if (box_) { box_->UpdateShadowMatrix(lightViewProjection_); }
}

bool ShadowTestScene::TryGetDirectionalLightFromManager(K4E::Vector3& outDirection)
{
	const auto& lights = LightManager::GetInstance()->GetPunctualLights();

	for (const auto& L : lights)
	{
		// 1 = Directional
		if (L.lightType == 1)
		{
			outDirection = Vector3::Normalize(L.direction);
			return true;
		}
	}

	return false;
}

void ShadowTestScene::DrawShadowObjects()
{
	if (floor_) { floor_->DrawShadow(); }
	if (box_) { box_->DrawShadow(); }
}

void ShadowTestScene::Draw3DObjects()
{
	if (floor_) { floor_->Draw(); }
	if (box_) { box_->Draw(); }

	Wireframe::GetInstance()->DrawGrid(100.0f, 20.0f, { 0.3f, 0.3f, 0.3f, 1.0f });
}

void ShadowTestScene::Draw2DSprites()
{
	SpriteManager::GetInstance()->SetRenderSetting_UI();
}

void ShadowTestScene::Finalize()
{
	box_.reset();
	floor_.reset();
}

void ShadowTestScene::DrawImGui()
{
#ifdef USE_IMGUI
	LightManager::GetInstance()->DrawImGui();

	box_->DrawImGui();
	floor_->DrawImGui();

	if (ImGui::Begin("ShadowTestScene"))
	{
		ImGui::Text("Shadow uses Directional Light only.");
		ImGui::Text("Point / Spot lights do not drive shadow map yet.");

		Vector3 dirFromManager{};
		const bool hasDirectional = TryGetDirectionalLightFromManager(dirFromManager);

		if (hasDirectional)
		{
			ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "Using Directional Light from LightManager");
			ImGui::Text("Manager Direction: %.3f, %.3f, %.3f", dirFromManager.x, dirFromManager.y, dirFromManager.z);
		}
		else
		{
			ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "No Directional Light in LightManager. Using local fallback.");
			ImGui::DragFloat3("LightDirection", &lightDirection_.x, 0.01f, -1.0f, 1.0f);
			if (ImGui::Button("Normalize Light"))
			{
				lightDirection_ = Vector3::Normalize(lightDirection_);
			}
		}

		ImGui::DragFloat("ShadowDistance", &shadowDistance_, 0.1f, 1.0f, 200.0f);
		ImGui::DragFloat("OrthoHalfWidth", &orthoHalfWidth_, 0.1f, 1.0f, 200.0f);
		ImGui::DragFloat("OrthoHalfHeight", &orthoHalfHeight_, 0.1f, 1.0f, 200.0f);
		ImGui::DragFloat("NearZ", &nearZ_, 0.01f, 0.01f, 10.0f);
		ImGui::DragFloat("FarZ", &farZ_, 0.1f, 1.0f, 500.0f);
	}
	ImGui::End();
#endif
}