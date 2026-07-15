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

	// DebugScene初回生成時は保存済みPlayer構成を自動復元し、毎回Editorから読込操作を行わなくてよい状態にする。
	if (!hadSavedComposition)
	{
		loadedFromPrefab = Ken4lowEngine::ActorJsonSerializer::LoadActorFromFile(*this, kPlayerPrefabPath);
	}

	Ken4lowEngine::PlayerActor::Initialize(); // 古いPrefabに無い武器ViewModelやHUD Componentは不足分だけ補完する。
	SetName("DebugPlayer");
	wasControllingPlayer_ = false;

	if (!hadSavedComposition && !loadedFromPrefab)
	{
		ResetForValidation({ 0.0f, 1.5f, 0.0f }); // Prefabが無い場合だけコード既定値へフォールバックする。
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
		// 旧Playerと同じInputSnapshot生成経路を使い、DebugSceneだけ別キー割り当てになる状態を解消する。
		const InputSnapshot inputSnapshot = Ken4lowEngine::BuildInputSnapshot(*input);
		playerInput->ApplyInputSnapshot(inputSnapshot, kMouseLookSensitivity, wasControllingPlayer_);
		wasControllingPlayer_ = true;
	}
	else
	{
		wasControllingPlayer_ = false;
		if (playerInput)
		{
			playerInput->ResetInputState(); // Editor操作やDebugCamera中は以前の移動・Action要求を残さない。
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
