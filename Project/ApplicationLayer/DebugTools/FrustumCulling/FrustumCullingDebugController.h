#pragma once
#include "Vector4.h"

namespace K4E = ::Ken4lowEngine;

/// <summary>
/// Scene から Frustum Culling 確認用 UI / Wireframe 表示を分離する Controller。
/// </summary>
class FrustumCullingDebugController
{
public:
	void Initialize(bool resetMainCamera = false);
	void Update(float deltaTime);
	void DrawDebug();
	void DrawImGui();
	void DrawImGuiContent();

	void SetWireframeVisible(bool visible) { showFrustumWireframe_ = visible; }
	bool IsWireframeVisible() const { return showFrustumWireframe_; }

private:
	void DrawMainCameraImGui();
	void CopyDebugCameraToMainCamera();
	void ResetMainCamera();
	void GetCullingCameraClipDistances(float& nearDistance, float& farDistance) const;
	float GetCurrentWindowAspectRatio() const;

	K4E::Vector4 frustumWireColor_ = { 0.1f, 1.0f, 0.2f, 1.0f };
	bool showFrustumWireframe_ = true;
};
