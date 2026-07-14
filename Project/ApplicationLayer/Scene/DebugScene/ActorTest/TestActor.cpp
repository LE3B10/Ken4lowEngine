#include "TestActor.h"

#include <CameraManager.h>
#include <Input.h>

namespace
{
	constexpr float kMouseLookSensitivity = 0.0025f;
}

void TestActor::Initialize()
{
	const bool hadSavedComposition = !GetComponents().empty();
	Ken4lowEngine::PlayerActor::Initialize();
	wasControllingPlayer_ = false;

	if (!hadSavedComposition)
	{
		ResetForValidation({ 0.0f, 1.5f, 0.0f }); // 床へ落下して接地するまでをPhysicsWorldで確認できる高さから開始する。
	}
}

void TestActor::Update(float deltaTime)
{
	auto* input = Ken4lowEngine::Input::GetInstance();
	auto* playerInput = GetPlayerInputComponent();
	auto* cameraManager = Ken4lowEngine::CameraManager::GetInstance();

	// PIEでゲーム入力を取得している間は、作業用DebugCameraではなくPlayerのゲームCameraを必ず描画に使う。
	if (input && input->IsGameInputEnabled() && cameraManager->IsUsingDebugCamera())
	{
		cameraManager->SetUseDebugCamera(false);
	}

	const bool useDebugCamera = cameraManager->IsUsingDebugCamera();
	const bool canControlPlayer = input && playerInput && input->IsGameInputEnabled() && !useDebugCamera;

	if (canControlPlayer)
	{
		float moveX = 0.0f;
		float moveZ = 0.0f;
		if (input->PushKey(DIK_A)) moveX -= 1.0f;
		if (input->PushKey(DIK_D)) moveX += 1.0f;
		if (input->PushKey(DIK_S)) moveZ -= 1.0f;
		if (input->PushKey(DIK_W)) moveZ += 1.0f;
		playerInput->RequestMove(moveX, moveZ);

		// Camera座標系ではYaw正方向が左、Pitch正方向が下なので、マウス操作は画面上の直感方向へ符号を合わせる。
		if (wasControllingPlayer_)
		{
			const float yawDelta = static_cast<float>(-input->GetMouseMoveX()) * kMouseLookSensitivity;
			const float pitchDelta = static_cast<float>(input->GetMouseMoveY()) * kMouseLookSensitivity;
			if (yawDelta != 0.0f || pitchDelta != 0.0f) playerInput->RequestLook(yawDelta, pitchDelta);
		}
		wasControllingPlayer_ = true;

		if (input->TriggerKey(DIK_SPACE)) playerInput->RequestJump();
		if (input->PushMouse(0)) playerInput->RequestFire();
		if (input->TriggerKey(DIK_R)) playerInput->RequestReload();
		if (input->TriggerKey(DIK_1)) playerInput->RequestInventorySlot(0);
		if (input->TriggerKey(DIK_2)) playerInput->RequestInventorySlot(1);
		if (input->TriggerKey(DIK_3)) playerInput->RequestInventorySlot(2);
		if (input->TriggerKey(DIK_4)) playerInput->RequestInventorySlot(3);
		if (input->TriggerKey(DIK_5)) playerInput->RequestInventorySlot(4);
	}
	else
	{
		wasControllingPlayer_ = false;
		if (playerInput)
		{
			playerInput->RequestMove(0.0f, 0.0f); // Editor操作やDebugCamera中は以前の移動要求を残さない。
		}
	}

	Ken4lowEngine::PlayerActor::Update(deltaTime);
}

void TestActor::PostPhysicsUpdate(float deltaTime)
{
	Ken4lowEngine::PlayerActor::PostPhysicsUpdate(deltaTime);

	// Collider補正後のPlayer Root位置を反映した後で、Player Cameraをそのフレーム最後のMain Camera状態へ確定する。
	if (!Ken4lowEngine::CameraManager::GetInstance()->IsUsingDebugCamera())
	{
		if (Ken4lowEngine::PlayerCameraComponent* camera = GetPlayerCameraComponent())
		{
			camera->SyncToMainCameraNow();
		}
	}
}
