#pragma once
#include <BaseScene.h>
#include <memory>
#include <DirectXCommon.h>
#include <Input.h>
#include <Object3D.h>
#include <Camera.h>
#include <LightManager.h>
#include <Matrix4x4.h>
#include <Vector3.h>

namespace K4E = ::Ken4lowEngine;

class ShadowTestScene : public BaseScene
{
public:

	void Initialize() override;

	void Update() override;

	void Draw3DObjects() override;

	void DrawShadowObjects() override;

	void Draw2DSprites() override;

	void Finalize() override;

	void DrawImGui() override;

private:

	void UpdateLightViewProjection();

	void UpdateShadowMatrices();

	bool TryGetDirectionalLightFromManager(K4E::Vector3& outDirection);

private:

	K4E::DirectXCommon* dxCommon_ = nullptr;
	K4E::Input* input_ = nullptr;

	std::unique_ptr<K4E::Object3D> floor_;
	std::unique_ptr<K4E::Object3D> box_;

	// 影用ライト行列
	K4E::Matrix4x4 lightViewProjection_;

	// 影用パラメータ
	K4E::Vector3 shadowCenter_ = { 0.0f, 0.0f, 0.0f };
	K4E::Vector3 lightDirection_ = { 0.5f, -1.0f, 0.3f };

	float shadowDistance_ = 40.0f;
	float orthoHalfWidth_ = 25.0f;
	float orthoHalfHeight_ = 25.0f;
	float nearZ_ = 0.1f;
	float farZ_ = 100.0f;
};