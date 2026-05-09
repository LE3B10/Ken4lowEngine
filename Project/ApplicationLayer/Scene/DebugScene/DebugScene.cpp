#define NOMINMAX
#include "DebugScene.h"
#include <DirectXCommon.h>
#include <Input.h>
#include <SpriteManager.h>
#include "CameraManager.h"
#include "Wireframe.h"
#include <GameTimer.h>
#ifdef _DEBUG
#include <DebugCamera.h>
#endif // _DEBUG

#ifdef USE_IMGUI
#include <ImGuiManager.h>
#endif // USE_IMGUI
#include <GpuParticleManager.h>

#include "WeaponMasterDataDatabase.h"
#include "WeaponMasterDataEditor.h"
#include "WeaponMasterDataWriter.h"
#include <algorithm>
#include <filesystem>
#include <cstdio>

using namespace Ken4lowEngine;

namespace
{
	/// -------------------------------------------------------------
	/// Visual Studio の「出力」ウィンドウへ文字列を出す
	/// 改行付きで送るための簡易ヘルパー
	/// -------------------------------------------------------------
	void DebugLog(const std::string& message)
	{
		std::string line = message + "\n";
		OutputDebugStringA(line.c_str());
	}

	GpuParticleType ToDebugParticleType(int index)
	{
		switch (index)
		{
		case 0: return GpuParticleType::Default;
		case 1: return GpuParticleType::Debris;
		case 2: return GpuParticleType::Spark;
		case 3: return GpuParticleType::Shockwave;
		case 4: return GpuParticleType::Smoke;
		case 5: return GpuParticleType::Heal;
		default: return GpuParticleType::Debris;
		}
	}

	GpuParticleEmitter* PrepareDebugMeshParticleEmitter(
		GpuParticleManager* manager,
		const std::string& emitterName,
		uint32_t meshId,
		GpuParticleType type,
		const Vector3& position,
		float radius)
	{
		if (!manager)
		{
			return nullptr;
		}

		if (GpuParticleEmitter* existing = manager->GetEmitter(emitterName))
		{
			auto& info = existing->GetInfoMutable();
			info.kind = GpuParticleKind::Mesh;
			info.spriteType = type;
			info.drawType = static_cast<uint32_t>(type);
			info.billboardFlags = BillboardMode::None;
			info.textureFilePath = "Mesh:" + std::to_string(meshId);
			info.radius = radius;
			info.loopCount = 0;
			info.loopFrequency = 0.0f;
			existing->SetPosition(position);
			return existing;
		}

		GpuParticleEmitter::EmitterInfo info{};
		info.kind = GpuParticleKind::Mesh;
		info.spriteType = type;
		info.drawType = static_cast<uint32_t>(type);
		info.billboardFlags = BillboardMode::None;
		info.textureFilePath = "Mesh:" + std::to_string(meshId);
		info.radius = radius;
		info.loopCount = 0;
		info.loopFrequency = 0.0f;

		GpuParticleEmitter* created = manager->CreateEmitter(emitterName, info);
		if (created)
		{
			created->SetPosition(position);
		}
		return created;
	}
}

void DebugScene::Initialize()
{
#ifdef _DEBUG
	// デバッグカメラの初期化
	DebugCamera::GetInstance()->Initialize();
#endif // _DEBUG

	dxCommon_ = DirectXCommon::GetInstance();
	input_ = Input::GetInstance();

	/*input_->SetLockCursor(true);
	input_->SetCursorVisible(false);*/

	collisionManager_ = std::make_unique<CollisionManager>();
	collisionManager_->Initialize();

	debugBoss_ = std::make_unique<GuardianBoss>();
	debugBoss_->Initialize();
	collisionManager_->AddCollider(debugBoss_.get());

	// 見やすい位置に置く
	debugBoss_->SetPosition({ 0.0f, 2.25f, 30.0f });
	debugBoss_->SetYaw(3.141592f); // 必要ならプレイヤー側へ向ける

	debugDisintegrationModel_ = std::make_unique<Object3D>();
	debugDisintegrationModel_->Initialize(debugDisintegrationModelPath_);
	debugDisintegrationModel_->SetTranslate(debugDisintegrationPosition_);
	debugDisintegrationModel_->SetRotate(debugDisintegrationRotation_);
	debugDisintegrationModel_->SetScale(debugDisintegrationScale_);
	debugDisintegrationModel_->Update();

	debugDisintegrationEffect_ = std::make_unique<ModelDisintegrationEffect>();
	debugDisintegrationEffect_->Initialize();

	debugReconstructionModel_ = std::make_unique<Object3D>();
	debugReconstructionModel_->Initialize(debugReconstructionModelPath_);
	debugReconstructionModel_->SetTranslate(debugReconstructionPosition_);
	debugReconstructionModel_->SetRotate(debugReconstructionRotation_);
	debugReconstructionModel_->SetScale(debugReconstructionScale_);
	debugReconstructionModel_->Update();

	debugReconstructionEffect_ = std::make_unique<ModelReconstructionEffect>();
	debugReconstructionEffect_->Initialize();

	debugModelBlockSequenceModel_ = std::make_unique<Object3D>();
	debugModelBlockSequenceModel_->Initialize(debugModelBlockSequenceModelPath_);
	debugModelBlockSequenceModel_->SetTranslate(debugModelBlockSequencePosition_);
	debugModelBlockSequenceModel_->SetRotate(debugModelBlockSequenceRotation_);
	debugModelBlockSequenceModel_->SetScale(debugModelBlockSequenceScale_);
	debugModelBlockSequenceModel_->Update();

	debugSequenceDisintegrationEffect_ = std::make_unique<ModelDisintegrationEffect>();
	debugSequenceDisintegrationEffect_->Initialize();
	debugSequenceReconstructionEffect_ = std::make_unique<ModelReconstructionEffect>();
	debugSequenceReconstructionEffect_->Initialize();
	debugModelBlockSequence_ = std::make_unique<ModelBlockEffectSequence>();
	debugModelBlockSequence_->Initialize(debugSequenceReconstructionEffect_.get(), debugSequenceDisintegrationEffect_.get());
}

void DebugScene::Update()
{
#ifdef _DEBUG
	UpdateDebug();
#endif // _DEBUG

	float deltaTime = K4E::GameTimer::GetInstance()->GetDeltaTime();

	static float animTime = 0.0f;
	animTime += deltaTime;

	// ボス更新
	if (debugBoss_)
	{
		// とりあえずプレイヤー位置をターゲットに渡す
		debugBoss_->SetTargetPosition({});
		debugBoss_->Update(deltaTime);
	}

	UpdateDebugBossHitTest();

	UpdateDebugParticleTest();

	UpdateDebugDisintegrationTest(deltaTime);

	UpdateDebugReconstructionTest(deltaTime);

	UpdateDebugModelBlockSequence(deltaTime);

	collisionManager_->Update();
	collisionManager_->CheckAllCollisions();

}

void DebugScene::Draw3DObjects()
{
	if (debugDisintegrationModelVisible_ && debugDisintegrationModel_)
	{
		debugDisintegrationModel_->Draw();
	}

	if (debugDisintegrationEffect_)
	{
		debugDisintegrationEffect_->Draw();
	}

	if (debugReconstructionModelVisible_ && debugReconstructionModel_)
	{
		debugReconstructionModel_->Draw();
	}

	if (debugReconstructionEffect_)
	{
		debugReconstructionEffect_->Draw();
	}

	if (debugModelBlockSequence_ && debugModelBlockSequence_->IsModelVisible() && debugModelBlockSequenceModel_)
	{
		debugModelBlockSequenceModel_->Draw();
	}

	if (debugSequenceReconstructionEffect_)
	{
		debugSequenceReconstructionEffect_->Draw();
	}

	if (debugSequenceDisintegrationEffect_)
	{
		debugSequenceDisintegrationEffect_->Draw();
	}

	// ボス描画
	if (debugBoss_)
	{
		debugBoss_->Draw();
	}

#ifdef _DEBUG
	// ワイヤーフレームの描画
	Wireframe::GetInstance()->DrawGrid(100.0f, 50.0f, { 0.25f, 0.25f, 0.25f,1.0f });

	collisionManager_->Draw();
#endif // _DEBUG
}

void DebugScene::DrawShadowObjects()
{
	if (debugDisintegrationModelVisible_ && debugDisintegrationModel_)
	{
		debugDisintegrationModel_->DrawShadow();
	}

	if (debugReconstructionModelVisible_ && debugReconstructionModel_)
	{
		debugReconstructionModel_->DrawShadow();
	}

	if (debugModelBlockSequence_ && debugModelBlockSequence_->IsModelVisible() && debugModelBlockSequenceModel_)
	{
		debugModelBlockSequenceModel_->DrawShadow();
	}

	if (debugBoss_)
	{
		debugBoss_->DrawShadow();
	}
}

void DebugScene::Draw2DSprites()
{
#pragma region スプライトの描画                    

	// 背景用の共通描画設定（後面）
	SpriteManager::GetInstance()->SetRenderSetting_Background();

#pragma endregion


#pragma region UIの描画

	// UI用の共通描画設定
	SpriteManager::GetInstance()->SetRenderSetting_UI();


#pragma endregion
}

void DebugScene::Finalize()
{
	// 入力状態を必ず戻す（ロック/非表示のまま終了しない）
	input_->SetLockCursor(false);
	input_->SetCursorVisible(true);

	debugModelBlockSequence_.reset();
	debugSequenceReconstructionEffect_.reset();
	debugSequenceDisintegrationEffect_.reset();
	debugModelBlockSequenceModel_.reset();
	debugReconstructionEffect_.reset();
	debugReconstructionModel_.reset();
	debugDisintegrationEffect_.reset();
	debugDisintegrationModel_.reset();
	debugBoss_.reset();
	collisionManager_.reset();

	input_ = nullptr;
	dxCommon_ = nullptr;
}

void DebugScene::DrawImGui()
{
#ifdef USE_IMGUI

	if (debugBoss_)
	{
		debugBoss_->DrawImGui();
	}

	ImGui::Begin("Debug Boss Hit Test");

	ImGui::Checkbox("Enable Hit Test", &debugBossHitTestEnabled_);
	ImGui::DragFloat("Hit Radius", &debugHitRadius_, 0.01f, 0.1f, 5.0f);
	ImGui::DragFloat("Base Damage", &debugBaseDamage_, 0.1f, 1.0f, 999.0f);

	ImGui::Separator();
	ImGui::Text("Press H to test hit.");
	ImGui::TextWrapped("%s", debugHitLog_.c_str());

	ImGui::End();

	/// ---------- GPUパーティクルデバッグ ---------- ///
	GpuParticleManager::GetInstance()->DrawImGui();

	/// ---------- Sprite / Mesh Particle 比較テスト ---------- ///
	{
		static char meshModelPath[256] = "Test/cube.gltf";
		static int meshId = 1000;
		static int particleTypeIndex = 1;
		static int spriteCount = 48;
		static int meshCount = 48;
		static float radius = 1.0f;
		static float position[3] = { 0.0f, 2.5f, 18.0f };
		static bool meshLoaded = false;
		static bool spawnSideBySide = true;

		const char* particleTypeNames[] = {
			"Default",
			"Debris",
			"Spark",
			"Shockwave",
			"Smoke",
			"Heal"
		};

		GpuParticleManager* gpuParticleManager = GpuParticleManager::GetInstance();
		const GpuParticleType selectedType = ToDebugParticleType(particleTypeIndex);

		ImGui::Begin("GPU Particle Sprite / Mesh Test");
		ImGui::TextWrapped("Sprite and Mesh particles can be spawned from this DebugScene-only panel. Mesh uses textureFilePath = Mesh:<MeshId> internally.");
		ImGui::Separator();

		ImGui::InputText("Mesh Model Path", meshModelPath, IM_ARRAYSIZE(meshModelPath));
		ImGui::InputInt("MeshId", &meshId);
		if (meshId < 0) { meshId = 0; }

		if (ImGui::Button("Load Mesh Asset"))
		{
			meshLoaded = gpuParticleManager->LoadMeshAssetsFromAssimp(static_cast<uint32_t>(meshId), meshModelPath, true);
			debugParticleLog_ = meshLoaded
				? "MeshParticle Debug: LoadMeshAssetsFromAssimp succeeded."
				: "MeshParticle Debug: LoadMeshAssetsFromAssimp failed.";
			DebugLog(debugParticleLog_);
		}

		ImGui::SameLine();
		ImGui::Text("Loaded: %s", meshLoaded ? "true" : "false");
		ImGui::Text("Registered MeshAssets: %zu", gpuParticleManager->GetMeshAssets().size());

		if (const auto* meshAsset = gpuParticleManager->FindMeshAsset(static_cast<uint32_t>(meshId)))
		{
			ImGui::Text("MeshId %d found. IndexCount: %u", meshId, meshAsset->indexCount);
			ImGui::Text("Mesh Texture: %s", meshAsset->textureFilePath.c_str());
		}
		else
		{
			ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.35f, 1.0f), "MeshId %d is not registered yet.", meshId);
		}

		ImGui::Separator();
		ImGui::Combo("Particle Type", &particleTypeIndex, particleTypeNames, IM_ARRAYSIZE(particleTypeNames));
		ImGui::DragInt("Sprite Count", &spriteCount, 1.0f, 0, 1000);
		ImGui::DragInt("Mesh Count", &meshCount, 1.0f, 0, 1000);
		ImGui::DragFloat("Emitter Radius", &radius, 0.01f, 0.0f, 30.0f);
		ImGui::DragFloat3("Center Position", position, 0.05f);
		ImGui::Checkbox("Side By Side", &spawnSideBySide);

		Vector3 center{ position[0], position[1], position[2] };
		Vector3 spritePos = center;
		Vector3 meshPos = center;
		if (spawnSideBySide)
		{
			spritePos.x -= 2.0f;
			meshPos.x += 2.0f;
		}

		if (ImGui::Button("Spawn Sprite"))
		{
			if (auto* spriteEmitter = gpuParticleManager->EmitBurst(
				"DebugScene_SpriteParticle",
				selectedType,
				spritePos,
				static_cast<uint32_t>(std::max(spriteCount, 0))))
			{
				spriteEmitter->GetInfoMutable().radius = radius;
				spriteEmitter->SetPosition(spritePos);
			}

			debugParticleLog_ = "Spawn: DebugScene Sprite Particle";
			DebugLog(debugParticleLog_);
		}

		ImGui::SameLine();
		if (ImGui::Button("Spawn Mesh"))
		{
			if (!gpuParticleManager->FindMeshAsset(static_cast<uint32_t>(meshId)))
			{
				meshLoaded = gpuParticleManager->LoadMeshAssetsFromAssimp(static_cast<uint32_t>(meshId), meshModelPath, true);
			}

			if (auto* meshEmitter = PrepareDebugMeshParticleEmitter(
				gpuParticleManager,
				"DebugScene_MeshParticle",
				static_cast<uint32_t>(meshId),
				selectedType,
				meshPos,
				radius))
			{
				meshEmitter->RequestEmit(static_cast<uint32_t>(std::max(meshCount, 0)));
				debugParticleLog_ = "Spawn: DebugScene Mesh Particle";
			}
			else
			{
				debugParticleLog_ = "Spawn failed: DebugScene Mesh Particle";
			}
			DebugLog(debugParticleLog_);
		}

		ImGui::SameLine();
		if (ImGui::Button("Spawn Both"))
		{
			if (auto* spriteEmitter = gpuParticleManager->EmitBurst(
				"DebugScene_SpriteParticle",
				selectedType,
				spritePos,
				static_cast<uint32_t>(std::max(spriteCount, 0))))
			{
				spriteEmitter->GetInfoMutable().radius = radius;
				spriteEmitter->SetPosition(spritePos);
			}

			if (!gpuParticleManager->FindMeshAsset(static_cast<uint32_t>(meshId)))
			{
				meshLoaded = gpuParticleManager->LoadMeshAssetsFromAssimp(static_cast<uint32_t>(meshId), meshModelPath, true);
			}

			if (auto* meshEmitter = PrepareDebugMeshParticleEmitter(
				gpuParticleManager,
				"DebugScene_MeshParticle",
				static_cast<uint32_t>(meshId),
				selectedType,
				meshPos,
				radius))
			{
				meshEmitter->RequestEmit(static_cast<uint32_t>(std::max(meshCount, 0)));
			}

			debugParticleLog_ = "Spawn: DebugScene Sprite + Mesh Particles";
			DebugLog(debugParticleLog_);
		}

		ImGui::Separator();
		ImGui::Text("Sprite position: %.2f, %.2f, %.2f", spritePos.x, spritePos.y, spritePos.z);
		ImGui::Text("Mesh position:   %.2f, %.2f, %.2f", meshPos.x, meshPos.y, meshPos.z);
		ImGui::TextWrapped("%s", debugParticleLog_.c_str());
		ImGui::End();
	}


	// エフェクト調整時に迷わないよう、崩壊・再構築・シーケンス系ImGuiの表示文言は日本語で統一する。
	/// ---------- モデル崩壊エフェクト単体テスト ---------- ///
	{
		static char modelPathBuffer[256] = "Characters/body.gltf";

		ImGui::Begin("モデル崩壊エフェクト確認");
		ImGui::TextWrapped("DebugScene専用テストです。F9またはボタンで表示中モデルの表面をサンプリングし、崩壊エフェクトを再生します。");
		ImGui::InputText("モデルパス", modelPathBuffer, IM_ARRAYSIZE(modelPathBuffer));
		ImGui::DragFloat3("位置", &debugDisintegrationPosition_.x, 0.05f);
		ImGui::DragFloat3("回転", &debugDisintegrationRotation_.x, 0.01f);
		ImGui::DragFloat3("スケール", &debugDisintegrationScale_.x, 0.01f, 0.01f, 10.0f);

		if (ImGui::Button("テストモデル再読み込み"))
		{
			pendingDebugDisintegrationPath_ = modelPathBuffer;
			pendingDebugDisintegrationReload_ = true;
			debugDisintegrationLog_ = "テストモデル再読み込みを予約: " + pendingDebugDisintegrationPath_;
		}

		ImGui::SameLine();
		if (ImGui::Button("崩壊再生"))
		{
			pendingDebugDisintegrationPath_ = modelPathBuffer;
			pendingDebugDisintegrationPlay_ = true;
			debugDisintegrationLog_ = "再生を予約: " + pendingDebugDisintegrationPath_;
		}
		ImGui::SameLine();
		if (ImGui::Button("リセット"))
		{
			pendingDebugDisintegrationReset_ = true;
			debugDisintegrationLog_ = "崩壊リセットを予約しました。";
		}

		if (ImGui::Button(debugDisintegrationPaused_ ? "再開" : "一時停止"))
		{
			debugDisintegrationPaused_ = !debugDisintegrationPaused_;
		}
		ImGui::SameLine();
		if (ImGui::Button("1フレーム送り"))
		{
			debugDisintegrationStepFrame_ = true;
			debugDisintegrationPaused_ = true;
		}

		ImGui::Text("モデル表示: %s", debugDisintegrationModelVisible_ ? "はい" : "いいえ");
		ImGui::TextWrapped("%s", debugDisintegrationLog_.c_str());
		ImGui::End();
	}

	if (debugDisintegrationEffect_)
	{
		debugDisintegrationEffect_->DrawImGui();
	}


	/// ---------- モデル再構築エフェクト単体テスト ---------- ///
	{
		static char reconstructionModelPathBuffer[256] = "Characters/body.gltf";

		ImGui::Begin("モデル再構築エフェクト確認");
		ImGui::TextWrapped("DebugScene専用テストです。F10またはボタンで散らばったCPUキューブブロックをモデル表面のサンプル位置へ集めます。");
		ImGui::InputText("モデルパス", reconstructionModelPathBuffer, IM_ARRAYSIZE(reconstructionModelPathBuffer));
		ImGui::DragFloat3("位置", &debugReconstructionPosition_.x, 0.05f);
		ImGui::DragFloat3("回転", &debugReconstructionRotation_.x, 0.01f);
		ImGui::DragFloat3("スケール", &debugReconstructionScale_.x, 0.01f, 0.01f, 10.0f);

		if (ImGui::Button("テストモデル再読み込み##Reconstruction"))
		{
			pendingDebugReconstructionPath_ = reconstructionModelPathBuffer;
			pendingDebugReconstructionReload_ = true;
			debugReconstructionLog_ = "再構築テストモデル再読み込みを予約: " + pendingDebugReconstructionPath_;
		}

		ImGui::SameLine();
		if (ImGui::Button("再構築再生"))
		{
			pendingDebugReconstructionPath_ = reconstructionModelPathBuffer;
			pendingDebugReconstructionPlay_ = true;
			debugReconstructionLog_ = "再構築再生を予約: " + pendingDebugReconstructionPath_;
		}

		ImGui::Text("モデル表示: %s", debugReconstructionModelVisible_ ? "はい" : "いいえ");
		ImGui::TextWrapped("%s", debugReconstructionLog_.c_str());
		ImGui::End();
	}

	if (debugReconstructionEffect_)
	{
		debugReconstructionEffect_->DrawImGui();
	}

	/// ---------- モデルブロック演出シーケンステスト ---------- ///
	{
		static char sequenceModelPathBuffer[256] = "Characters/body.gltf";

		ImGui::Begin("ブロック演出シーケンス確認");
		ImGui::TextWrapped("DebugScene専用のシーケンステストです。再構築 → モデル表示 → 崩壊、および崩壊 → 再構築の流れを確認します。");
		ImGui::InputText("モデルパス##BlockSequence", sequenceModelPathBuffer, IM_ARRAYSIZE(sequenceModelPathBuffer));
		ImGui::DragFloat3("位置##BlockSequence", &debugModelBlockSequencePosition_.x, 0.05f);
		ImGui::DragFloat3("回転##BlockSequence", &debugModelBlockSequenceRotation_.x, 0.01f);
		ImGui::DragFloat3("スケール##BlockSequence", &debugModelBlockSequenceScale_.x, 0.01f, 0.01f, 10.0f);

		if (debugModelBlockSequence_)
		{
			auto& sequenceParams = debugModelBlockSequence_->GetParameters();
			ImGui::DragFloat("通常モデル表示時間", &sequenceParams.showDuration, 0.05f, 0.0f, 10.0f);
			ImGui::Checkbox("モデルブレンドを使う", &sequenceParams.useModelBlend);
			ImGui::DragFloat("モデル切り替えブレンド時間", &sequenceParams.modelBlendDuration, 0.01f, 0.0f, 3.0f);
			ImGui::DragFloat("ブロック完全体保持時間", &sequenceParams.blockHoldDuration, 0.05f, 0.0f, 10.0f);
			ImGui::Checkbox("崩壊までブロックを保持", &sequenceParams.keepBlocksUntilDisintegration);
			ImGui::Checkbox("通常モデルを挟まない", &sequenceParams.skipNormalModelInSequence);
			ImGui::Checkbox("ブロックから通常モデルへフェード", &sequenceParams.useBlockToModelFade);
			ImGui::Checkbox("通常モデルからブロックへフェード", &sequenceParams.useModelToBlockFade);
			ImGui::DragFloat("待機時間", &sequenceParams.waitDuration, 0.05f, 0.0f, 10.0f);
			ImGui::Checkbox("ループ有効", &sequenceParams.loopEnabled);
			ImGui::SliderInt("ブロック数##BlockSequence", &sequenceParams.blockCount, 32, 8000);
			if (ImGui::SliderFloat("ブロックサイズ##BlockSequence", &sequenceParams.blockSize, 0.005f, 0.30f))
			{
				sequenceParams.blockSize = std::max(sequenceParams.blockSize, 0.005f);
			}
			ImGui::Checkbox("表面サンプリング##BlockSequence", &sequenceParams.surfaceSampling);
			const char* sequencePlacementModeLabels[] = { "ランダム表面配置", "均一表面配置", "整列表面配置" };
			int sequencePlacementModeIndex = sequenceParams.placementMode == DisintegrationPlacementMode::AlignedSurfaceGrid ? 2 : (sequenceParams.placementMode == DisintegrationPlacementMode::UniformSurface ? 1 : 0);
			if (ImGui::Combo("配置モード##BlockSequence", &sequencePlacementModeIndex, sequencePlacementModeLabels, IM_ARRAYSIZE(sequencePlacementModeLabels)))
			{
				sequenceParams.placementMode = sequencePlacementModeIndex == 2 ? DisintegrationPlacementMode::AlignedSurfaceGrid : (sequencePlacementModeIndex == 1 ? DisintegrationPlacementMode::UniformSurface : DisintegrationPlacementMode::RandomSurface);
				if (sequenceParams.placementMode != DisintegrationPlacementMode::RandomSurface)
				{
					sequenceParams.useRandomScale = false;
					sequenceParams.useRandomRotation = false;
					sequenceParams.useSurfaceInset = true;
				}
			}
			ImGui::Checkbox("ランダムサイズを使う##BlockSequence", &sequenceParams.useRandomScale);
			ImGui::SliderFloat("サイズばらつき##BlockSequence", &sequenceParams.scaleVariation, 0.0f, 0.75f);
			ImGui::Checkbox("ランダム回転を使う##BlockSequence", &sequenceParams.useRandomRotation);
			ImGui::SliderFloat("回転ばらつき##BlockSequence", &sequenceParams.rotationRandomness, 0.0f, 8.0f);
			int sequencePlacementSeed = static_cast<int>(sequenceParams.placementSeed);
			if (ImGui::InputInt("配置シード##BlockSequence", &sequencePlacementSeed))
			{
				sequenceParams.placementSeed = static_cast<uint32_t>(std::max(sequencePlacementSeed, 0));
			}
			ImGui::SliderFloat("配置間隔##BlockSequence", &sequenceParams.placementSpacing, 0.0f, 0.5f);
			ImGui::Checkbox("表面内側オフセットを使う##BlockSequence", &sequenceParams.useSurfaceInset);
			ImGui::Checkbox("ブロックサイズから自動計算##BlockSequence", &sequenceParams.autoSurfaceInsetFromBlockSize);
			if (!sequenceParams.autoSurfaceInsetFromBlockSize)
			{
				ImGui::SliderFloat("表面内側オフセット量##BlockSequence", &sequenceParams.surfaceInset, 0.0f, 0.30f);
			}
			else
			{
				const float effectiveSurfaceInset = sequenceParams.blockSize * 0.5f;
				ImGui::Text("表面内側オフセット量: %.3f", effectiveSurfaceInset);
			}
			if (sequenceParams.placementMode == DisintegrationPlacementMode::UniformSurface)
			{
				ImGui::TextWrapped("均一表面配置はSweep Erosionで形を保って蝕む表現に推奨です。");
			}
			else if (sequenceParams.placementMode == DisintegrationPlacementMode::AlignedSurfaceGrid)
			{
				ImGui::TextWrapped("整列表面配置はAABBではなくモデル表面上の格子候補へ配置します。");
			}
			ImGui::SeparatorText("方向侵食崩壊##BlockSequence");
			ImGui::Checkbox("方向侵食を使う##BlockSequence", &sequenceParams.useSweepErosion);
			ImGui::DragFloat3("侵食方向##BlockSequence", &sequenceParams.sweepDirection.x, 0.01f, -1.0f, 1.0f);
			ImGui::SliderFloat("侵食時間##BlockSequence", &sequenceParams.sweepDuration, 0.05f, 8.0f);
			ImGui::SliderFloat("侵食ノイズ強度##BlockSequence", &sequenceParams.erosionNoisePower, 0.0f, 4.0f);
			ImGui::SliderFloat("侵食境界幅##BlockSequence", &sequenceParams.erosionBandWidth, 0.0f, 2.0f);
			ImGui::SliderFloat("侵食前の発光幅##BlockSequence", &sequenceParams.preGlowWidth, 0.0f, 2.0f);
			ImGui::SliderFloat("侵食後の発光幅##BlockSequence", &sequenceParams.postGlowWidth, 0.0f, 2.0f);
			ImGui::SliderFloat("発光エッジ幅##BlockSequence", &sequenceParams.glowEdgeWidth, 0.001f, 2.0f);
			ImGui::SliderFloat("発光強度##BlockSequence", &sequenceParams.glowIntensity, 0.0f, 8.0f);
			ImGui::SliderFloat("発光の鋭さ##BlockSequence", &sequenceParams.glowSharpness, 0.1f, 8.0f);
			ImGui::ColorEdit4("発光色##BlockSequence", &sequenceParams.glowColor.x);

			if (ImGui::Button("モデル再読み込み##BlockSequence"))
			{
				pendingDebugModelBlockSequencePath_ = sequenceModelPathBuffer;
				debugModelBlockSequenceRequest_ = DebugModelBlockSequenceRequest::Reload;
				debugModelBlockSequenceLog_ = "シーケンスモデル再読み込みを予約: " + pendingDebugModelBlockSequencePath_;
			}

			if (ImGui::Button("再構築 → 崩壊 を再生"))
			{
				pendingDebugModelBlockSequencePath_ = sequenceModelPathBuffer;
				debugModelBlockSequenceRequest_ = DebugModelBlockSequenceRequest::PlaySpawnThenDisintegrate;
				debugModelBlockSequenceLog_ = "再構築 → 崩壊 の再生を予約: " + pendingDebugModelBlockSequencePath_;
			}

			if (ImGui::Button("崩壊 → 再構築 を再生"))
			{
				pendingDebugModelBlockSequencePath_ = sequenceModelPathBuffer;
				debugModelBlockSequenceRequest_ = DebugModelBlockSequenceRequest::PlayDisintegrateThenReconstruct;
				debugModelBlockSequenceLog_ = "崩壊 → 再構築 の再生を予約: " + pendingDebugModelBlockSequencePath_;
			}

			if (ImGui::Button("ループ再生"))
			{
				pendingDebugModelBlockSequencePath_ = sequenceModelPathBuffer;
				debugModelBlockSequenceRequest_ = DebugModelBlockSequenceRequest::PlayLoop;
				debugModelBlockSequenceLog_ = "ループ再生を予約: " + pendingDebugModelBlockSequencePath_;
			}

			ImGui::SameLine();
			if (ImGui::Button("停止"))
			{
				debugModelBlockSequenceRequest_ = DebugModelBlockSequenceRequest::Stop;
				debugModelBlockSequenceLog_ = "シーケンス停止を予約しました。";
			}

			ImGui::SameLine();
			if (ImGui::Button("リセット"))
			{
				debugModelBlockSequenceRequest_ = DebugModelBlockSequenceRequest::Reset;
				debugModelBlockSequenceLog_ = "シーケンスリセットを予約しました。";
			}

			ImGui::Separator();
			ImGui::Text("現在の状態: %s", debugModelBlockSequence_->GetStateName());
			ImGui::Text("シーケンス経過時間: %.2f", debugModelBlockSequence_->GetSequenceElapsed());
			ImGui::Text("再構築完了: %s", debugModelBlockSequence_->IsReconstructionCompleted() ? "はい" : "いいえ");
			ImGui::Text("崩壊完了: %s", debugModelBlockSequence_->IsDisintegrationCompleted() ? "はい" : "いいえ");
			ImGui::Text("モデル表示: %s", debugModelBlockSequence_->IsModelVisible() ? "はい" : "いいえ");
		}
		ImGui::TextWrapped("%s", debugModelBlockSequenceLog_.c_str());
		ImGui::End();
	}

	/// ---------- 武器マスターデータエディタ ---------- ///
	static WeaponMasterDataDatabase weaponDB;
	static WeaponMasterDataEditor weaponEditor;
	static WeaponEditorHooks hooks;
	static bool initialized = false;
	static int32_t lastAppliedID = 0;

	if (!initialized)
	{
		initialized = true;

		// まだ保存/再読込はしないので一旦空実装でOK
		hooks.SaveAll = [&]()
			{
				std::string err;
				const std::filesystem::path outRoot = "Resources/JSON/weapons";
				WeaponMasterDataWriter::SaveAllByCategory(weaponDB, outRoot, &err);
			};
		hooks.RequestReloadFocus = [](int32_t) {};
		hooks.RebuildLoadout = []() {};

		// Applyされたら「最後のID」を更新（動作確認）
		hooks.ApplyToRuntimeIfCurrent =
			[&](int32_t weaponID, const FWeaponMasterData&)
			{
				lastAppliedID = weaponID;
			};

		// 削除はDBから消すだけ（ファイル削除は後で）
		hooks.RequestDelete =
			[&](int32_t weaponID)
			{
				std::string err;
				const std::filesystem::path outRoot = "Resources/JSON/weapons";

				// まずはディスク上のjsonファイルを削除
				WeaponMasterDataWriter::DeleteFilesByWeaponID(outRoot, weaponID, &err);

				// DBから削除
				weaponDB.RemoveByID(weaponID);
			};

		// 追加予約は今は使わないなら空でOK
		hooks.RequestAdd = [](const std::string&, int32_t) {};

		// 初期データを2つだけ作る（任意）
		weaponDB.Clear();
	}

	ImGui::Begin("武器マスターデータエディタ");
	if (ImGui::CollapsingHeader("Weapon Master Editor", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Text("Count: %zu", weaponDB.Size());
		ImGui::Text("Last Applied ID: %d", lastAppliedID);
		ImGui::Separator();

		weaponEditor.DrawImGui(weaponDB, hooks);
	}

	ImGui::Separator();
	ImGui::Text("Press H to test hit.");
	ImGui::TextWrapped("%s", debugHitLog_.c_str());

	ImGui::Separator();
	ImGui::Text("Particle Test");
	ImGui::Text("1 : HitSpark");
	ImGui::Text("2 : Heal_Effect");
	ImGui::Text("3 : Boss_Appear_Dust");
	ImGui::TextWrapped("%s", debugParticleLog_.c_str());

	ImGui::End();

#endif // USE_IMGUI

}

void DebugScene::UpdateDebug()
{
	if (input_->TriggerKey(DIK_F12))
	{
		CameraManager::GetInstance()->SetUseDebugCamera(!CameraManager::GetInstance()->IsUsingDebugCamera());
		Wireframe::GetInstance()->SetDebugCamera(!Wireframe::GetInstance()->GetDebugCamera());
		GpuParticleManager::GetInstance()->SetDebugCameraEnabled(!isDebugCamera_);
		isDebugCamera_ = !isDebugCamera_;

		/*input_->SetLockCursor(!isDebugCamera_);
		input_->SetCursorVisible(isDebugCamera_);*/
	}
}

void DebugScene::UpdateDebugReconstructionTest(float deltaTime)
{
	if (input_ && input_->TriggerKey(DIK_F10))
	{
		pendingDebugReconstructionPlay_ = true;
	}

	ProcessDebugReconstructionRequest();

	if (debugReconstructionModel_)
	{
		debugReconstructionModel_->SetTranslate(debugReconstructionPosition_);
		debugReconstructionModel_->SetRotate(debugReconstructionRotation_);
		debugReconstructionModel_->SetScale(debugReconstructionScale_);
		debugReconstructionModel_->Update();
	}

	if (debugReconstructionEffect_)
	{
		debugReconstructionEffect_->Update(deltaTime);
		if (debugReconstructionEffect_->IsActive())
		{
			debugReconstructionModelVisible_ = debugReconstructionEffect_->ShouldShowFinalModel();
		}
		else if (debugReconstructionEffect_->IsComplete())
		{
			debugReconstructionModelVisible_ = debugReconstructionEffect_->ShouldShowFinalModel();
		}
	}
}

void DebugScene::PlayDebugReconstructionEffect()
{
	if (!debugReconstructionEffect_)
	{
		return;
	}

	const Matrix4x4 worldMatrix = Matrix4x4::MakeAffineMatrix(
		debugReconstructionScale_,
		debugReconstructionRotation_,
		debugReconstructionPosition_);

	// 表面サンプルを最終到達位置にして、散らばったブロックをモデル形状へ集める。
	debugReconstructionEffect_->PlayFromModel(debugReconstructionModelPath_, worldMatrix);
	debugReconstructionModelVisible_ = debugReconstructionEffect_->ShouldShowFinalModel();
	debugReconstructionLog_ = "再構築再生: " + debugReconstructionModelPath_;
	DebugLog(debugReconstructionLog_);
}

void DebugScene::ReloadDebugReconstructionModel()
{
	debugReconstructionModel_ = std::make_unique<Object3D>();
	debugReconstructionModel_->Initialize(debugReconstructionModelPath_);
	debugReconstructionModel_->SetTranslate(debugReconstructionPosition_);
	debugReconstructionModel_->SetRotate(debugReconstructionRotation_);
	debugReconstructionModel_->SetScale(debugReconstructionScale_);
	debugReconstructionModel_->Update();
	debugReconstructionModelVisible_ = true;
	debugReconstructionLog_ = "再構築テストモデルを再読み込み: " + debugReconstructionModelPath_;
	DebugLog(debugReconstructionLog_);
}

void DebugScene::ProcessDebugReconstructionRequest()
{
	if (!pendingDebugReconstructionReload_ && !pendingDebugReconstructionPlay_)
	{
		return;
	}

	// ImGui からのGPUリソース更新要求は、コマンドリスト記録前のUpdateでだけ実行する。
	if (!pendingDebugReconstructionPath_.empty())
	{
		debugReconstructionModelPath_ = pendingDebugReconstructionPath_;
	}

	if (pendingDebugReconstructionReload_)
	{
		ReloadDebugReconstructionModel();
	}

	if (pendingDebugReconstructionPlay_)
	{
		PlayDebugReconstructionEffect();
	}

	pendingDebugReconstructionReload_ = false;
	pendingDebugReconstructionPlay_ = false;
	pendingDebugReconstructionPath_.clear();
}

void DebugScene::UpdateDebugModelBlockSequence(float deltaTime)
{
	ProcessDebugModelBlockSequenceRequest();

	if (debugModelBlockSequenceModel_)
	{
		debugModelBlockSequenceModel_->SetTranslate(debugModelBlockSequencePosition_);
		debugModelBlockSequenceModel_->SetRotate(debugModelBlockSequenceRotation_);
		debugModelBlockSequenceModel_->SetScale(debugModelBlockSequenceScale_);
		debugModelBlockSequenceModel_->Update();
	}

	if (debugModelBlockSequence_)
	{
		debugModelBlockSequence_->Update(deltaTime);
	}
}

void DebugScene::ReloadDebugModelBlockSequenceModel()
{
	debugModelBlockSequenceModel_ = std::make_unique<Object3D>();
	debugModelBlockSequenceModel_->Initialize(debugModelBlockSequenceModelPath_);
	debugModelBlockSequenceModel_->SetTranslate(debugModelBlockSequencePosition_);
	debugModelBlockSequenceModel_->SetRotate(debugModelBlockSequenceRotation_);
	debugModelBlockSequenceModel_->SetScale(debugModelBlockSequenceScale_);
	debugModelBlockSequenceModel_->Update();
	debugModelBlockSequenceLog_ = "シーケンステストモデルを再読み込み: " + debugModelBlockSequenceModelPath_;
	DebugLog(debugModelBlockSequenceLog_);
}

void DebugScene::ProcessDebugModelBlockSequenceRequest()
{
	if (debugModelBlockSequenceRequest_ == DebugModelBlockSequenceRequest::None)
	{
		return;
	}

	if (!pendingDebugModelBlockSequencePath_.empty())
	{
		debugModelBlockSequenceModelPath_ = pendingDebugModelBlockSequencePath_;
	}

	const DebugModelBlockSequenceRequest request = debugModelBlockSequenceRequest_;
	debugModelBlockSequenceRequest_ = DebugModelBlockSequenceRequest::None;
	pendingDebugModelBlockSequencePath_.clear();

	if (!debugModelBlockSequence_)
	{
		return;
	}

	switch (request)
	{
	case DebugModelBlockSequenceRequest::Reload:
		ReloadDebugModelBlockSequenceModel();
		break;
	case DebugModelBlockSequenceRequest::PlaySpawnThenDisintegrate:
		ReloadDebugModelBlockSequenceModel();
		debugModelBlockSequence_->PlaySpawnThenDisintegrate(debugModelBlockSequenceModelPath_, MakeDebugModelBlockSequenceWorldMatrix());
		debugModelBlockSequenceLog_ = "再構築 → 崩壊 を再生: " + debugModelBlockSequenceModelPath_;
		DebugLog(debugModelBlockSequenceLog_);
		break;
	case DebugModelBlockSequenceRequest::PlayDisintegrateThenReconstruct:
		ReloadDebugModelBlockSequenceModel();
		debugModelBlockSequence_->PlayDisintegrateThenReconstruct(debugModelBlockSequenceModelPath_, MakeDebugModelBlockSequenceWorldMatrix());
		debugModelBlockSequenceLog_ = "崩壊 → 再構築 を再生: " + debugModelBlockSequenceModelPath_;
		DebugLog(debugModelBlockSequenceLog_);
		break;
	case DebugModelBlockSequenceRequest::PlayLoop:
		ReloadDebugModelBlockSequenceModel();
		debugModelBlockSequence_->PlayLoop(debugModelBlockSequenceModelPath_, MakeDebugModelBlockSequenceWorldMatrix());
		debugModelBlockSequenceLog_ = "ループ再生: " + debugModelBlockSequenceModelPath_;
		DebugLog(debugModelBlockSequenceLog_);
		break;
	case DebugModelBlockSequenceRequest::Stop:
		debugModelBlockSequence_->Stop(false);
		debugModelBlockSequenceLog_ = "シーケンスを停止しました。";
		DebugLog(debugModelBlockSequenceLog_);
		break;
	case DebugModelBlockSequenceRequest::Reset:
		debugModelBlockSequence_->Reset();
		debugModelBlockSequenceLog_ = "シーケンスをリセットしました。";
		DebugLog(debugModelBlockSequenceLog_);
		break;
	case DebugModelBlockSequenceRequest::None:
	default:
		break;
	}
}

Matrix4x4 DebugScene::MakeDebugModelBlockSequenceWorldMatrix() const
{
	return Matrix4x4::MakeAffineMatrix(
		debugModelBlockSequenceScale_,
		debugModelBlockSequenceRotation_,
		debugModelBlockSequencePosition_);
}

/// -------------------------------------------------------------
/// BossHitPart をログ用文字列へ変換
/// -------------------------------------------------------------
const char* DebugScene::ToString(BossHitPart part) const
{
	switch (part)
	{
	case BossHitPart::Head:     return "Head";
	case BossHitPart::Body:     return "Body";
	case BossHitPart::LeftArm:  return "LeftArm";
	case BossHitPart::RightArm: return "RightArm";
	case BossHitPart::LeftLeg:  return "LeftLeg";
	case BossHitPart::RightLeg: return "RightLeg";
	default:                    return "None";
	}
}

/// -------------------------------------------------------------
/// DebugScene での仮ヒット確認
///
/// Hキーを押した瞬間に簡易球判定を飛ばし、
/// 結果を OutputDebugStringA で出力する
/// -------------------------------------------------------------
void DebugScene::UpdateDebugBossHitTest()
{
	if (!debugBossHitTestEnabled_)
	{
		return;
	}

	if (!debugBoss_)
	{
		debugHitLog_ = "Boss or Player is null.";
		DebugLog(debugHitLog_);
		return;
	}

	// Hキーを押した瞬間だけ判定
	if (!input_->TriggerKey(DIK_H))
	{
		return;
	}

	// ---------------------------------------------------------
	// 仮の攻撃位置
	// 本来は弾のヒット位置や近接武器先端などを使うが、
	// 今回はデバッグ用としてボス中心より少し上を狙う
	// ---------------------------------------------------------
	Vector3 attackCenter = debugBoss_->GetCenterPosition();
	attackCenter.y += 1.0f; // 頭寄りを狙いやすくする

	// BossBase 側の簡易球判定
	const BossHitResult hitResult =
		debugBoss_->CheckDebugHitSphere(attackCenter, debugHitRadius_);

	if (hitResult.isHit)
	{
		// 倍率込みダメージを適用
		debugBoss_->ApplyDebugHitResult(hitResult, debugBaseDamage_);

		Vector3 effectPos = attackCenter;
		effectPos.y += 0.15f;

		GpuParticleManager::GetInstance()->EmitBurst(
			"Debug_HitSpark_OnHit",
			GpuParticleType::Spark,
			effectPos,
			18);

		// 画面表示用にも保持
		debugHitLog_ =
			std::string("HIT  Part: ") + ToString(hitResult.part) +
			"  Damage: " + std::to_string(debugBaseDamage_ * hitResult.damageMultiplier) +
			"  HP: " + std::to_string(debugBoss_->GetHP()) +
			" / " + std::to_string(debugBoss_->GetMaxHP());

		// Visual Studio の出力ウィンドウへ送る
		DebugLog(debugHitLog_);
	}
	else
	{
		debugHitLog_ = "MISS";
		DebugLog(debugHitLog_);
	}
}

void DebugScene::UpdateDebugParticleTest()
{
	if (!debugBoss_)
	{
		return;
	}

	GpuParticleManager* gpuParticleManager = GpuParticleManager::GetInstance();
	if (!gpuParticleManager)
	{
		debugParticleLog_ = "GpuParticleManager is null.";
		return;
	}

	const Vector3 bossCenter = debugBoss_->GetCenterPosition();

	// ---------------------------------------------------------
	// 1キー: ヒット火花
	// ---------------------------------------------------------
	if (input_->TriggerKey(DIK_1))
	{
		Vector3 pos = bossCenter;
		pos.y += 1.0f;

		gpuParticleManager->EmitBurst(
			"Debug_HitSpark",
			GpuParticleType::Spark,
			pos,
			20);

		debugParticleLog_ = "Spawn: HitSpark";
		DebugLog(debugParticleLog_);
	}

	// ---------------------------------------------------------
	// 2キー: 回復エフェクト
	// ---------------------------------------------------------
	if (input_->TriggerKey(DIK_2))
	{
		Vector3 pos = bossCenter;
		pos.y += 1.5f;

		gpuParticleManager->EmitBurst(
			"Debug_Heal",
			GpuParticleType::Heal,
			pos,
			24);

		debugParticleLog_ = "Spawn: Heal_Effect";
		DebugLog(debugParticleLog_);
	}

	// ---------------------------------------------------------
	// 3キー: ボス登場砂埃
	// ---------------------------------------------------------
	if (input_->TriggerKey(DIK_3))
	{
		Vector3 pos = bossCenter;

		gpuParticleManager->EmitBurst(
			"Debug_BossAppear",
			GpuParticleType::Default,
			pos,
			48);

		debugParticleLog_ = "Spawn: Boss_Appear_Dust";
		DebugLog(debugParticleLog_);
	}
}


void DebugScene::UpdateDebugDisintegrationTest(float deltaTime)
{
	if (input_ && input_->TriggerKey(DIK_F9))
	{
		pendingDebugDisintegrationPlay_ = true;
	}

	ProcessDebugDisintegrationRequest();

	if (debugDisintegrationModel_)
	{
		debugDisintegrationModel_->SetTranslate(debugDisintegrationPosition_);
		debugDisintegrationModel_->SetRotate(debugDisintegrationRotation_);
		debugDisintegrationModel_->SetScale(debugDisintegrationScale_);
		debugDisintegrationModel_->Update();
	}

	if (debugDisintegrationEffect_)
	{
		const bool shouldAdvanceEffect = !debugDisintegrationPaused_ || debugDisintegrationStepFrame_;
		debugDisintegrationEffect_->Update(shouldAdvanceEffect ? deltaTime : 0.0f);
		debugDisintegrationStepFrame_ = false;
		debugDisintegrationModelVisible_ = !debugDisintegrationEffect_->IsActive();
	}
}

void DebugScene::ReloadDebugDisintegrationModel()
{
	debugDisintegrationModel_ = std::make_unique<Object3D>();
	debugDisintegrationModel_->Initialize(debugDisintegrationModelPath_);
	debugDisintegrationModel_->SetTranslate(debugDisintegrationPosition_);
	debugDisintegrationModel_->SetRotate(debugDisintegrationRotation_);
	debugDisintegrationModel_->SetScale(debugDisintegrationScale_);
	debugDisintegrationModel_->Update();
	debugDisintegrationModelVisible_ = true;
	debugDisintegrationLog_ = "テストモデルを再読み込み: " + debugDisintegrationModelPath_;
	DebugLog(debugDisintegrationLog_);
}

void DebugScene::ProcessDebugDisintegrationRequest()
{
	if (!pendingDebugDisintegrationReload_ && !pendingDebugDisintegrationPlay_ && !pendingDebugDisintegrationReset_)
	{
		return;
	}

	if (!pendingDebugDisintegrationPath_.empty())
	{
		debugDisintegrationModelPath_ = pendingDebugDisintegrationPath_;
	}

	if (pendingDebugDisintegrationReload_)
	{
		ReloadDebugDisintegrationModel();
	}

	if (pendingDebugDisintegrationReset_ && debugDisintegrationEffect_)
	{
		debugDisintegrationEffect_->Initialize();
		debugDisintegrationModelVisible_ = true;
		debugDisintegrationPaused_ = false;
		debugDisintegrationStepFrame_ = false;
	}

	if (pendingDebugDisintegrationPlay_)
	{
		PlayDebugDisintegrationEffect();
	}

	pendingDebugDisintegrationReload_ = false;
	pendingDebugDisintegrationPlay_ = false;
	pendingDebugDisintegrationReset_ = false;
	pendingDebugDisintegrationPath_.clear();
}

void DebugScene::PlayDebugDisintegrationEffect()
{
	if (!debugDisintegrationEffect_)
	{
		return;
	}

	const Matrix4x4 worldMatrix = Matrix4x4::MakeAffineMatrix(
		debugDisintegrationScale_,
		debugDisintegrationRotation_,
		debugDisintegrationPosition_);

	// 表示中のテストモデルと同じ行列から生成し、形状サンプリングのずれを確認しやすくする。
	debugDisintegrationEffect_->PlayFromModel(debugDisintegrationModelPath_, worldMatrix);
	debugDisintegrationPaused_ = false;
	debugDisintegrationStepFrame_ = false;
	debugDisintegrationModelVisible_ = !debugDisintegrationEffect_->IsActive();
	debugDisintegrationLog_ = "崩壊再生: " + debugDisintegrationModelPath_;
	DebugLog(debugDisintegrationLog_);
}
