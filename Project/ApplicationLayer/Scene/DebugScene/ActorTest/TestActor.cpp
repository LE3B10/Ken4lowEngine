#include "TestActor.h"

#include <ActorJsonSerializer.h>
#include <CameraManager.h>
#include <Input.h>
#include <InputSnapshot.h>

namespace
{
	constexpr float kMouseLookSensitivity = 0.0025f;
	constexpr const char* kPlayerPrefabPath = "Resources/ActorPrefabs/DebugPlayer.json";
}

void TestActor::Initialize()
{
	const bool hadSavedComposition = !GetComponents().empty();
	bool loadedFromPrefab = false;
	if (!hadSavedComposition)
	{
		loadedFromPrefab = Ken4lowEngine::ActorJsonSerializer::LoadActorFromFile(*this, kPlayerPrefabPath);
	}

	Ken4lowEngine::PlayerActor::Initialize();
	SetName("DebugPlayer");
	wasControllingPlayer_ = false;
	if (!hadSavedComposition && !loadedFromPrefab) ResetForValidation({ 0.0f, 1.5f, 0.0f });
}

void TestActor::Update(float deltaTime)
{
	auto* input = Ken4lowEngine::Input::GetInstance();
	auto* playerInput = GetPlayerInputComponent();
	auto* cameraManager = Ken4lowEngine::CameraManager::GetInstance();

	if (input && input->IsGameInputEnabled() && cameraManager->IsUsingDebugCamera()) cameraManager->SetUseDebugCamera(false);

	const bool useDebugCamera = cameraManager->IsUsingDebugCamera();
	const bool canControlPlayer = input && playerInput && input->IsGameInputEnabled() && !useDebugCamera;
	if (canControlPlayer)
	{
		const InputSnapshot inputSnapshot = Ken4lowEngine::BuildInputSnapshot(*input);
		playerInput->ApplyInputSnapshot(inputSnapshot, kMouseLookSensitivity, wasControllingPlayer_);
		wasControllingPlayer_ = true;

		if (input->TriggerKey(DIK_F6)) ApplyPlayerDamage(25.0f);
		if (input->TriggerKey(DIK_F7)) HealPlayer(25.0f);
		if (input->TriggerKey(DIK_F8))
		{
			const Ken4lowEngine::SceneComponent* root = GetRootComponent();
			ResetForValidation(root ? root->GetLocalPosition() : Ken4lowEngine::Vector3{}); // その場で新Player全機能を初期状態へ戻して再検証する。
		}
	}
	else
	{
		wasControllingPlayer_ = false;
		if (playerInput) playerInput->ResetInputState();
	}

	Ken4lowEngine::PlayerActor::Update(deltaTime);
}

void TestActor::PostPhysicsUpdate(float deltaTime)
{
	Ken4lowEngine::PlayerActor::PostPhysicsUpdate(deltaTime);
	if (!Ken4lowEngine::CameraManager::GetInstance()->IsUsingDebugCamera())
	{
		if (Ken4lowEngine::PlayerCameraComponent* camera = GetPlayerCameraComponent()) camera->SyncToMainCameraNow();
	}
}
