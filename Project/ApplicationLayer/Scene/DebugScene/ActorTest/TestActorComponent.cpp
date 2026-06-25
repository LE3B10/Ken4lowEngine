#include "TestActorComponent.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

void TestActorComponent::Initialize()
{
	// Component生成時に経過時間を初期化する。
	elapsedTime_ = 0.0f;
}

void TestActorComponent::Update(float deltaTime)
{
	// Updateが毎フレーム呼ばれているか確認する。
	elapsedTime_ += deltaTime;
}

void TestActorComponent::Draw()
{
	// 現段階では通常描画は行わない。
}

void TestActorComponent::DrawShadow()
{
	// 現段階ではシャドウ描画は行わない。
}

void TestActorComponent::DrawImGui()
{
#ifdef USE_IMGUI
	// ActorComponent経由でImGui描画が呼ばれているか確認する。
	ImGui::Text("=== Test Actor Component ===");
	ImGui::Text("Elapsed Time : %.2f", elapsedTime_);
#endif // USE_IMGUI
}

void TestActorComponent::Finalize()
{
	// 現段階では終了処理は行わない。
}