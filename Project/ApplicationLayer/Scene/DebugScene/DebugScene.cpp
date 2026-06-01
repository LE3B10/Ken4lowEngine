#define NOMINMAX
#include "DebugScene.h"
#include "DisintegrationDebugController.h"
#include "ApplicationLayer/DebugTools/FrustumCulling/FrustumCullingDebugController.h"
#include <DirectXCommon.h>
#include <Input.h>
#include <SpriteManager.h>
#include "CameraManager.h"
#include "Wireframe.h"
#include "Object3D.h"
#include "Camera.h"
#include "SkyBox.h"
#include "JsonAssetEntry.h"
#include "JsonDataManager.h"
#include "WinApp.h"
#include <LightManager.h>
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
#include <array>
#include <numbers>
#include <new>
#include <exception>
#include <unordered_map>
#include <cmath>
#include <chrono>

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

DebugScene::~DebugScene() = default;

void DebugScene::Initialize()
{
#ifdef _DEBUG
	// デバッグカメラの初期化
	DebugCamera::GetInstance()->Initialize();
#endif // _DEBUG

	dxCommon_ = DirectXCommon::GetInstance();
	input_ = Input::GetInstance();
	InitializeSkyBox();

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

	meleeDummyTarget_.SetCenterPosition({ 0.0f, 2.0f, 24.0f });
	meleeDummyTarget_.SetOBBHalfSize({ 0.8f, 1.0f, 0.8f });
	// 複数体同時検証のため、DebugScene起動時に近接敵を2〜3体まとめて生成する。
	debugMeleeEnemy_ = std::make_unique<MeleeEnemy>();
	debugMeleeEnemy_->Initialize();
	debugMeleeEnemy_->SetCenterPosition({ -3.0f, 2.5f, 18.0f });
	debugMeleeEnemy_->SetTarget(&meleeDummyTarget_);
	collisionManager_->AddCollider(debugMeleeEnemy_.get());
	// 中距離敵は近接敵と独立したunique_ptrで管理する。
	debugMidRangeEnemy_ = std::make_unique<MidRangeEnemy>();
	debugMidRangeEnemy_->Initialize();
	debugMidRangeEnemy_->SetCenterPosition({ 3.0f, 2.5f, 18.0f });
	debugMidRangeEnemy_->SetTarget(meleeDummyTarget_.GetCenterPosition());
	disintegrationDebug_ = std::make_unique<DisintegrationDebugController>();
	// Disintegration系の確認処理は専用コントローラへ委譲し、DebugScene本体の責務を絞る。
	disintegrationDebug_->Initialize();

	frustumCullingDebug_ = std::make_unique<FrustumCullingDebugController>();
	frustumCullingDebug_->Initialize(true);

	stage_ = std::make_unique<K4E::Stage>();
	stage_->Initialize("Stages/hajimarinoheigen.json", "Stages/hajimarinoheigen.gltf");
	stage_->RegisterColliders(collisionManager_.get());
	stage_->Update();
	// DebugSceneでもステージAABBを共有し、EnemyBase系の敵が床と障害物に衝突できるようにする。
	EnemyBase::SetGlobalStageWorldAABBs(&stage_->GetWorldAABBs());
	EnemyBase::SetGlobalStageNavigationObstacleAABBs(&stage_->GetNavigationObstacleAABBs());
	if (debugMeleeEnemy_)
	{
		debugMeleeEnemy_->SetFloorAABBs(&stage_->GetFloorAABBs());
		debugMeleeEnemy_->SetWallObstacleAABBs(&stage_->GetWallObstacleAABBs());
	}
	if (debugMidRangeEnemy_)
	{
		debugMidRangeEnemy_->SetFloorAABBs(&stage_->GetFloorAABBs());
		debugMidRangeEnemy_->SetWallObstacleAABBs(&stage_->GetWallObstacleAABBs());
	}
}

void DebugScene::InitializeSkyBox()
{
	LoadSkyBoxPresets();
	K4E::SkyBoxPreset* preset = skyBoxPresets_.FindActivePreset();
	if (!preset)
	{
		return;
	}

	skyBox_ = std::make_unique<K4E::SkyBox>();
	skyBox_->Initialize(preset->texturePath);
	ApplyActiveSkyBoxPreset();
}

void DebugScene::ApplyActiveSkyBoxPreset(bool reloadTexture)
{
	K4E::SkyBoxPreset* preset = skyBoxPresets_.FindActivePreset();
	if (!skyBox_ || !preset)
	{
		return;
	}

	// 読み込んだプリセットを優先し、毎フレーム既定値で上書きしない。
	K4E::ApplySkyBoxPreset(*skyBox_, *preset, reloadTexture);
	std::snprintf(skyBoxTexturePathBuffer_.data(), skyBoxTexturePathBuffer_.size(), "%s", preset->texturePath.c_str());
	std::snprintf(cloudTexturePathBuffer_.data(), cloudTexturePathBuffer_.size(), "%s", preset->cloud.texturePath.c_str());
}

bool DebugScene::LoadSkyBoxPresets()
{
	K4E::JsonAssetEntry entry;
	if (!K4E::JsonDataManager::SafeLoad("Resources/DataAssets/SkyBoxPresets/debug_skybox.json", entry))
	{
		skyBoxPresetLog_ = "設定ファイルがないため、デフォルト設定で表示します。";
		return false;
	}

	skyBoxPresets_.FromJson(entry.data);
	skyBoxPresetLog_ = "SkyBox設定を読み込みました。";
	return true;
}

bool DebugScene::SaveSkyBoxPresets()
{
	K4E::JsonAssetEntry entry;
	entry.id = "debug_skybox";
	entry.displayName = "Debug Scene SkyBox";
	entry.type = "SkyBoxPresetCollection";
	entry.path = "Resources/DataAssets/SkyBoxPresets/debug_skybox.json";
	skyBoxPresets_.ToJson(entry.data);
	const bool saved = K4E::JsonDataManager::SafeSave(entry);
	skyBoxPresetLog_ = saved ? "SkyBox設定を保存しました。" : "SkyBox設定の保存に失敗しました。";
	return saved;
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

	EnemyBase::SetPerformanceDebugDrawEnabled(enableEnemyDebugDraw_);
	EnemyBase::SetPerformanceCollisionEnabled(enableEnemyCollision_);
	EnemyBase::SetPerformanceAIEnabled(enableEnemyAI_);
	EnemyBase::SetPerformanceAttackEnabled(enableEnemyAttack_);
	EnemyBase::SetPerformanceMovementEnabled(enableEnemyMovement_);
	EnemyBase::SetPerformanceNavigationEnabled(enableEnemyNavigation_);
	EnemyBase::SetPerformanceTransformUpdateEnabled(enableEnemyTransformUpdate_);
	if (collisionManager_) { collisionManager_->SetEnemyCollisionEnabled(enableEnemyCollision_); }
	lastEnemyUpdateCount_ = 0;
	EnemyPerformanceProfiler::BeginFrame();
	auto updateMeleeEnemy = [deltaTime, this](MeleeEnemy* enemy)
		{
			if (!enemy) return;
			EnemyPerformanceProfiler::EnemyUpdateScope timer(EnemyPerformanceProfiler::EnemyType::Melee);
			enemy->Update(deltaTime);
			++lastEnemyUpdateCount_;
		};
	auto updateMidRangeEnemy = [deltaTime, this](MidRangeEnemy* enemy)
		{
			if (!enemy) return;
			enemy->SetTarget(meleeDummyTarget_.GetCenterPosition());
			EnemyPerformanceProfiler::EnemyUpdateScope timer(EnemyPerformanceProfiler::EnemyType::MidRange);
			enemy->Update(deltaTime);
			++lastEnemyUpdateCount_;
		};
	if (enableEnemyUpdate_)
	{
		if (debugMeleeEnemy_)
		{
			updateMeleeEnemy(debugMeleeEnemy_.get());
		}
		if (debugMidRangeEnemy_)
		{
			debugMidRangeEnemy_->SetTarget(meleeDummyTarget_.GetCenterPosition());
			// 中距離敵にも床/障害物AABBを近接敵と同じ参照元で渡す。
			debugMidRangeEnemy_->SetFloorAABBs(&stage_->GetFloorAABBs());
			debugMidRangeEnemy_->SetWallObstacleAABBs(&stage_->GetWallObstacleAABBs());
			updateMidRangeEnemy(debugMidRangeEnemy_.get());
		}
		for (auto& enemy : stressTestMeleeEnemies_)
		{
			updateMeleeEnemy(enemy.get());
		}
		for (auto& enemy : stressTestMidRangeEnemies_)
		{
			if (enemy)
			{
				updateMidRangeEnemy(enemy.get());
			}
		}
	}

	UpdateDebugBossHitTest();

	UpdateDebugParticleTest();

	if (disintegrationDebug_)
	{
		disintegrationDebug_->Update(deltaTime);
	}

	if (frustumCullingDebug_)
	{
		frustumCullingDebug_->Update(deltaTime);
	}

	collisionManager_->Update();
#ifdef _DEBUG
	const auto enemyCollisionStart = std::chrono::steady_clock::now();
#endif
	collisionManager_->CheckAllCollisions();
#ifdef _DEBUG
	const double enemyCollisionMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - enemyCollisionStart).count();
	EnemyPerformanceProfiler::AddFrameSectionMs(EnemyPerformanceProfiler::Section::Collision, enemyCollisionMs);
#endif

	if (stage_)
	{
		stage_->Update();
	}
	if (skyBox_)
	{
		skyBox_->Update();
		skyBox_->AdvanceCloudLayer(deltaTime);
		if (K4E::SkyBoxPreset* skyPreset = skyBoxPresets_.FindActivePreset()) skyPreset->cloud.uvOffset = skyBox_->GetCloudUvOffset();
	}

}

void DebugScene::ResolveMeleeEnemySeparation(float deltaTime)
{
	(void)deltaTime;
	// 近接敵は単体管理のため分離処理は不要。
}

void DebugScene::Draw3DObjects()
{
	const K4E::SkyBoxPreset* skyBoxPreset = skyBoxPresets_.FindActivePreset();
	if (skyBox_ && skyBoxPreset && skyBoxPreset->enabled)
	{
		// SkyBox は深度読み取り専用 PSO で先に描画し、ステージ描画を妨げない。
		skyBox_->Draw();
		skyBox_->DrawCloudLayer();
	}

	if (disintegrationDebug_)
	{
		disintegrationDebug_->Draw3DObjects();
	}

	// ボス描画
	if (debugBoss_)
	{
		debugBoss_->Draw();
	}
	lastEnemyDrawCount_ = 0;
	lastEnemyDebugDrawCount_ = 0;
	if (enableEnemyDraw_)
	{
		auto drawEnemy = [this](EnemyBase* enemy)
		{
			if (!enemy) { return; }
			enemy->Draw();
			++lastEnemyDrawCount_;
			if (enableEnemyDebugDraw_) { ++lastEnemyDebugDrawCount_; }
		};
		drawEnemy(debugMeleeEnemy_.get());
		drawEnemy(debugMidRangeEnemy_.get());
		for (const auto& enemy : stressTestMeleeEnemies_) { drawEnemy(enemy.get()); }
		for (const auto& enemy : stressTestMidRangeEnemies_) { drawEnemy(enemy.get()); }
	}

	if (stage_)
	{
		stage_->Draw();
		// Stage Debugの茶色ワイヤーはNavigation/Wall用AABBではなくCollider由来OBBで描画する。
		if (enableStageBoundsDebugDraw_)
		{
			for (const auto& obstacleObb : stage_->GetWallObstacleOBBs())
			{
				Wireframe::GetInstance()->DrawOBB(obstacleObb, { 0.60f, 0.35f, 0.12f, 0.90f });
			}
		}
	}

#ifdef _DEBUG
	// ワイヤーフレームの描画
	Wireframe::GetInstance()->DrawGrid(100.0f, 50.0f, { 0.25f, 0.25f, 0.25f,1.0f });
	if (meleeDummyWireVisible_)
	{
		const Vector3 c = meleeDummyTarget_.GetCenterPosition();
		Wireframe::GetInstance()->DrawSphere(c, meleeDummyWireRadius_, { 0.1f, 1.0f, 0.2f, 1.0f });
		Wireframe::GetInstance()->DrawLine(c + Vector3{ -0.2f, 0.0f, 0.0f }, c + Vector3{ 0.2f, 0.0f, 0.0f }, { 0.8f, 1.0f, 0.2f, 1.0f });
		Wireframe::GetInstance()->DrawLine(c + Vector3{ 0.0f, -0.2f, 0.0f }, c + Vector3{ 0.0f, 0.2f, 0.0f }, { 0.8f, 1.0f, 0.2f, 1.0f });
		Wireframe::GetInstance()->DrawLine(c + Vector3{ 0.0f, 0.0f, -0.2f }, c + Vector3{ 0.0f, 0.0f, 0.2f }, { 0.8f, 1.0f, 0.2f, 1.0f });
	}
	if (enableFrustumCullingDebugDraw_ && frustumCullingDebug_)
	{
		frustumCullingDebug_->DrawDebug();
	}

	collisionManager_->Draw(enableEnemyDebugDraw_);
#endif // _DEBUG
}

void DebugScene::DrawShadowObjects()
{
	if (disintegrationDebug_)
	{
		disintegrationDebug_->DrawShadowObjects();
	}

	if (debugBoss_)
	{
		debugBoss_->DrawShadow();
	}
	if (enableEnemyShadow_)
	{
		if (debugMeleeEnemy_) { debugMeleeEnemy_->DrawShadow(); }
		if (debugMidRangeEnemy_) { debugMidRangeEnemy_->DrawShadow(); }
		for (const auto& enemy : stressTestMeleeEnemies_) { if (enemy) { enemy->DrawShadow(); } }
		for (const auto& enemy : stressTestMidRangeEnemies_) { if (enemy) { enemy->DrawShadow(); } }
	}
	if (stage_)
	{
		stage_->DrawShadow();
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
	EnemyBase::SetPerformanceDebugDrawEnabled(true);
	EnemyBase::SetPerformanceCollisionEnabled(true);
	EnemyBase::SetPerformanceAIEnabled(true);
	EnemyBase::SetPerformanceAttackEnabled(true);
	EnemyBase::SetPerformanceMovementEnabled(true);
	EnemyBase::SetPerformanceNavigationEnabled(true);
	EnemyBase::SetPerformanceTransformUpdateEnabled(true);
	// 入力状態を必ず戻す（ロック/非表示のまま終了しない）
	input_->SetLockCursor(false);
	input_->SetCursorVisible(true);

	frustumCullingDebug_.reset();
	disintegrationDebug_.reset();
	skyBox_.reset();
	EnemyBase::SetGlobalStageWorldAABBs(nullptr);
	ClearStressTestEnemies();
	debugBoss_.reset();
	debugMidRangeEnemy_.reset();
	debugMeleeEnemy_.reset();
	collisionManager_.reset();
	stage_.reset();

	input_ = nullptr;
	dxCommon_ = nullptr;
}


void DebugScene::ApplyEnemyStressTestCounts()
{
	constexpr size_t kMaxEnemyCountPerType = 5000;
	const size_t meleeTarget = enemyStressTestEnabled_ ? static_cast<size_t>(std::clamp(requestedStressTestMeleeCount_, 0, static_cast<int>(kMaxEnemyCountPerType))) : 0;
	const size_t midRangeTarget = enemyStressTestEnabled_ ? static_cast<size_t>(std::clamp(requestedStressTestMidRangeCount_, 0, static_cast<int>(kMaxEnemyCountPerType))) : 0;

	auto shrinkMelee = [this](size_t target)
		{
			while (stressTestMeleeEnemies_.size() > target)
			{
				if (collisionManager_ && stressTestMeleeEnemies_.back())
				{
					collisionManager_->RemoveCollider(stressTestMeleeEnemies_.back().get());
				}
				stressTestMeleeEnemies_.pop_back();
			}
		};
	shrinkMelee(meleeTarget);
	if (stressTestMidRangeEnemies_.size() > midRangeTarget) { stressTestMidRangeEnemies_.resize(midRangeTarget); }

	try
	{
		stressTestMeleeEnemies_.reserve(meleeTarget);
		stressTestMidRangeEnemies_.reserve(midRangeTarget);
		while (stressTestMeleeEnemies_.size() < meleeTarget)
		{
			auto enemy = std::make_unique<MeleeEnemy>();
			if (!enemy) { break; }
			enemy->Initialize();
			enemy->SetCenterPosition(GetStressTestEnemyPosition(stressTestMeleeEnemies_.size(), false));
			enemy->SetTarget(&meleeDummyTarget_);
			if (stage_)
			{
				enemy->SetFloorAABBs(&stage_->GetFloorAABBs());
				enemy->SetWallObstacleAABBs(&stage_->GetWallObstacleAABBs());
			}
			stressTestMeleeEnemies_.push_back(std::move(enemy));
			if (collisionManager_) { collisionManager_->AddCollider(stressTestMeleeEnemies_.back().get()); }
		}
		while (stressTestMidRangeEnemies_.size() < midRangeTarget)
		{
			auto enemy = std::make_unique<MidRangeEnemy>();
			if (!enemy) { break; }
			enemy->Initialize();
			enemy->SetCenterPosition(GetStressTestEnemyPosition(stressTestMidRangeEnemies_.size(), true));
			enemy->SetTarget(meleeDummyTarget_.GetCenterPosition());
			if (stage_)
			{
				enemy->SetFloorAABBs(&stage_->GetFloorAABBs());
				enemy->SetWallObstacleAABBs(&stage_->GetWallObstacleAABBs());
			}
			stressTestMidRangeEnemies_.push_back(std::move(enemy));
		}
		enemyStressTestLog_ = "Applied stress test enemy counts.";
	}
	catch (const std::bad_alloc&)
	{
		enemyStressTestLog_ = "Stopped spawning: memory allocation failed.";
	}
	catch (const std::exception& e)
	{
		enemyStressTestLog_ = std::string("Stopped spawning: ") + e.what();
	}
}

void DebugScene::ClearStressTestEnemies()
{
	if (collisionManager_)
	{
		for (const auto& enemy : stressTestMeleeEnemies_)
		{
			if (enemy) { collisionManager_->RemoveCollider(enemy.get()); }
		}
	}
	stressTestMeleeEnemies_.clear();
	stressTestMidRangeEnemies_.clear();
	enemyStressTestLog_ = "Cleared stress test enemies.";
}

K4E::Vector3 DebugScene::GetStressTestEnemyPosition(size_t index, bool isMidRange) const
{
	constexpr size_t kGridColumns = 100;
	constexpr float kGridSpacing = 3.5f;
	const float baseX = isMidRange ? 20.0f : -360.0f;
	const float baseZ = -220.0f;
	return {
		baseX + static_cast<float>(index % kGridColumns) * kGridSpacing,
		2.5f,
		baseZ + static_cast<float>(index / kGridColumns) * kGridSpacing
	};
}

void DebugScene::DrawImGui()
{
#ifdef USE_IMGUI

	LightManager::GetInstance()->DrawImGui();
	DrawSkyBoxImGui();

	if (debugBoss_)
	{
		debugBoss_->DrawImGui();
	}
	if (debugMeleeEnemy_)
	{
		debugMeleeEnemy_->DrawImGui();
	}
	if (debugMidRangeEnemy_)
	{
		debugMidRangeEnemy_->DrawImGui();
	}

	if (frustumCullingDebug_)
	{
		frustumCullingDebug_->DrawImGui();
	}

	ImGui::Begin("Enemy Stress Test");
	ImGui::Checkbox("Enable Enemy Stress Test", &enemyStressTestEnabled_);
	ImGui::SliderInt("Melee Enemy Count", &requestedStressTestMeleeCount_, 0, 5000);
	ImGui::SliderInt("MidRange Enemy Count", &requestedStressTestMidRangeCount_, 0, 5000);
	if (ImGui::Button("Apply Enemy Count")) { ApplyEnemyStressTestCounts(); }
	if (ImGui::Button("Clear Stress Test Enemies")) { ClearStressTestEnemies(); }
	ImGui::SeparatorText("Enemy Performance Debug");
	ImGui::Checkbox("Enable Enemy Update", &enableEnemyUpdate_);
	ImGui::Checkbox("Enable Enemy Draw", &enableEnemyDraw_);
	ImGui::Checkbox("Enable Enemy Debug Draw", &enableEnemyDebugDraw_);
	ImGui::Checkbox("Enable Enemy Collision", &enableEnemyCollision_);
	ImGui::Checkbox("Enable Enemy AI", &enableEnemyAI_);
	ImGui::Checkbox("Enable Enemy Attack", &enableEnemyAttack_);
	ImGui::Checkbox("Enable Enemy Movement", &enableEnemyMovement_);
	ImGui::Checkbox("Enable Enemy Navigation", &enableEnemyNavigation_);
	ImGui::Checkbox("Enable Enemy Transform Update", &enableEnemyTransformUpdate_);
	ImGui::Checkbox("Enable Enemy Shadow", &enableEnemyShadow_);
	ImGui::Checkbox("Enable Stage Bounds Debug Draw", &enableStageBoundsDebugDraw_);
	ImGui::Checkbox("Enable Frustum Culling Debug Draw", &enableFrustumCullingDebugDraw_);
	ImGui::Checkbox("Enable Dummy Target Wire Draw", &meleeDummyWireVisible_);
	ImGui::Separator();
	const size_t meleeEnemyCount = stressTestMeleeEnemies_.size() + (debugMeleeEnemy_ ? 1u : 0u);
	const size_t midRangeEnemyCount = stressTestMidRangeEnemies_.size() + (debugMidRangeEnemy_ ? 1u : 0u);
	ImGui::Text("Melee Enemy Count: %zu", meleeEnemyCount);
	ImGui::Text("MidRange Enemy Count: %zu", midRangeEnemyCount);
	ImGui::Text("Total Enemy Count: %zu", meleeEnemyCount + midRangeEnemyCount);
	ImGui::Text("Enemy Update Count: %d", lastEnemyUpdateCount_);
	ImGui::Text("Enemy Draw Count: %d", lastEnemyDrawCount_);
	ImGui::Text("Enemy DebugDraw Count: %d", lastEnemyDebugDrawCount_);
	ImGui::Text("Enemy Collision Count: %d", collisionManager_ ? collisionManager_->GetLastEnemyCollisionCount() : 0);
	ImGui::Text("  Enemy vs Player: %d", collisionManager_ ? collisionManager_->GetLastEnemyPlayerCollisionCount() : 0);
	ImGui::Text("  Bullet vs Enemy: %d", collisionManager_ ? collisionManager_->GetLastBulletEnemyCollisionCount() : 0);
	ImGui::Text("  Enemy vs World: %d", collisionManager_ ? collisionManager_->GetLastEnemyWorldCollisionCount() : 0);
	const auto& enemyPerf = EnemyPerformanceProfiler::GetFrameStats();
	const auto& meleePerf = enemyPerf.types[static_cast<size_t>(EnemyPerformanceProfiler::EnemyType::Melee)];
	const auto& midRangePerf = enemyPerf.types[static_cast<size_t>(EnemyPerformanceProfiler::EnemyType::MidRange)];
	auto sectionMs = [&enemyPerf](EnemyPerformanceProfiler::Section section) { return enemyPerf.GetSectionMs(section); };
	ImGui::SeparatorText("Enemy Update Timing (Debug Build)");
	ImGui::Text("Total Enemy Update Time: %.3f ms", enemyPerf.GetTotalMs());
	ImGui::Text("Melee Enemy Update Time: %.3f ms", meleePerf.totalMs);
	ImGui::Text("MidRange Enemy Update Time: %.3f ms", midRangePerf.totalMs);
	ImGui::Text("Enemy AI Time: %.3f ms", sectionMs(EnemyPerformanceProfiler::Section::AI));
	ImGui::Text("Enemy Move Time: %.3f ms", sectionMs(EnemyPerformanceProfiler::Section::Move));
	ImGui::Text("Enemy Attack Time: %.3f ms", sectionMs(EnemyPerformanceProfiler::Section::Attack));
	ImGui::Text("Enemy Collision Time: %.3f ms", sectionMs(EnemyPerformanceProfiler::Section::Collision));
	ImGui::Text("Enemy Navigation Time: %.3f ms", sectionMs(EnemyPerformanceProfiler::Section::Navigation));
	ImGui::Text("Enemy Transform Update Time: %.3f ms", sectionMs(EnemyPerformanceProfiler::Section::Transform));
	ImGui::Text("Enemy Bullet Spawn / Attack Check Time: %.3f ms", sectionMs(EnemyPerformanceProfiler::Section::BulletSpawnAttackCheck));
	ImGui::Text("Average Update Time Per Enemy: %.3f ms", enemyPerf.GetEnemyCount() > 0 ? enemyPerf.GetTotalMs() / enemyPerf.GetEnemyCount() : 0.0);
	ImGui::Text("Melee AI / Collision: %.3f / %.3f ms", meleePerf.sectionMs[static_cast<size_t>(EnemyPerformanceProfiler::Section::AI)], meleePerf.sectionMs[static_cast<size_t>(EnemyPerformanceProfiler::Section::Collision)]);
	ImGui::Text("MidRange AI / Collision: %.3f / %.3f ms", midRangePerf.sectionMs[static_cast<size_t>(EnemyPerformanceProfiler::Section::AI)], midRangePerf.sectionMs[static_cast<size_t>(EnemyPerformanceProfiler::Section::Collision)]);
	const auto* timer = K4E::GameTimer::GetInstance();
	ImGui::Text("FPS: %.1f", timer ? timer->GetFPS() : 0.0f);
	ImGui::Text("FrameTime: %.3f ms", timer ? timer->GetDeltaTime() * 1000.0f : 0.0f);
	ImGui::TextWrapped("%s", enemyStressTestLog_.c_str());
	ImGui::End();

	ImGui::Begin("Debug Boss Hit Test");

	ImGui::Checkbox("Enable Hit Test", &debugBossHitTestEnabled_);
	ImGui::DragFloat("Hit Radius", &debugHitRadius_, 0.01f, 0.1f, 5.0f);
	ImGui::DragFloat("Base Damage", &debugBaseDamage_, 0.1f, 1.0f, 999.0f);

	ImGui::Separator();
	ImGui::Text("Press H to test hit.");
	ImGui::TextWrapped("%s", debugHitLog_.c_str());

	ImGui::End();

	ImGui::Begin("MeleeEnemy Debug Target");
	Vector3 targetPos = meleeDummyTarget_.GetCenterPosition();
	float targetPosArray[3] = { targetPos.x, targetPos.y, targetPos.z };
	if (ImGui::DragFloat3("Dummy Target Position", targetPosArray, 0.05f))
	{
		meleeDummyTarget_.SetCenterPosition({ targetPosArray[0], targetPosArray[1], targetPosArray[2] });
	}
	ImGui::SliderFloat("DummyTarget wire radius", &meleeDummyWireRadius_, 0.1f, 2.0f);
	ImGui::Text("MeleeEnemy and dummy target are for BT behavior verification.");
	ImGui::End();

	if (stage_)
	{
		ImGui::Begin("Stage Debug");
		const std::vector<AABB>& worldAABBs = stage_->GetWorldAABBs();
		const std::vector<AABB>& floorAABBs = stage_->GetFloorAABBs();
		const std::vector<AABB>& wallObstacles = stage_->GetWallObstacleAABBs();
		const std::vector<AABB>& navObstacles = stage_->GetNavigationObstacleAABBs();
		const std::vector<OBB>& wallObstacleOBBs = stage_->GetWallObstacleOBBs();
		const std::vector<OBB>& navObstacleOBBs = stage_->GetNavigationObstacleOBBs();
		const auto& worldColliders = stage_->GetWorldColliders();
		const LevelData* levelData = stage_->GetLevelData();
		size_t loadedColliders = 0;
		size_t rotatedColliderCount = 0;
		size_t colliderRotationReadCount = 0;
		std::string sampleColliderName = "(none)";
		Vector3 sampleRawCenter{};
		Vector3 sampleConvertedCenter{};
		Vector3 sampleSourceRotationDeg{};
		Vector3 sampleConvertedRotationDeg{};
		Vector3 sampleOBBCenter{};
		Vector3 sampleAABBMin{};
		Vector3 sampleAABBMax{};
		float sampleFinalYawDeg = 0.0f;
		bool sampleUsedByNavigationObstacle = false;
		bool sampleUsedByWallObstacle = false;
		bool sampleUsedByCollisionObstacle = false;
		std::unordered_map<std::string, int> colliderTypeCounts{};
		if (levelData)
		{
			for (const auto& object : levelData->objects)
			{
				if (!object.collider.enabled)
				{
					continue;
				}
				++loadedColliders;
				const std::string typeName = object.collider.collisionType.empty() ? "Unspecified" : object.collider.collisionType;
				++colliderTypeCounts[typeName];
				if (object.collider.hasRotation)
				{
					++colliderRotationReadCount;
				}
				if ((object.collider.hasRotation && Vector3::Length(object.collider.rotation) > 0.0001f) ||
					Vector3::Length(object.rotation) > 0.0001f)
				{
					++rotatedColliderCount;
				}
				if (sampleColliderName == "(none)" && object.collider.hasRotation)
				{
					sampleColliderName = object.name;
					sampleRawCenter = object.collider.center;
					sampleConvertedCenter = {
						object.position.x + object.collider.center.x * object.scale.x,
						object.position.y + object.collider.center.y * object.scale.y,
						object.position.z + object.collider.center.z * object.scale.z,
					};
					sampleSourceRotationDeg = object.collider.sourceRotationDeg;
					sampleConvertedRotationDeg = object.collider.convertedRotationDeg;
					constexpr float kRadToDeg = 180.0f / 3.14159265358979323846f;
					sampleFinalYawDeg = (object.rotation.y + object.collider.rotation.y) * kRadToDeg;
					sampleOBBCenter = sampleConvertedCenter;
					const Vector3 half = {
						0.5f * object.collider.size.x * object.scale.x,
						0.5f * object.collider.size.y * object.scale.y,
						0.5f * object.collider.size.z * object.scale.z
					};
					sampleAABBMin = sampleConvertedCenter - half;
					sampleAABBMax = sampleConvertedCenter + half;
					sampleUsedByNavigationObstacle =
						(object.collider.collisionType == "Obstacle" || object.collider.collisionType == "Pillar" ||
							object.collider.collisionType == "Fence" || object.collider.collisionType == "Tree");
					sampleUsedByWallObstacle = sampleUsedByNavigationObstacle;
					sampleUsedByCollisionObstacle = sampleUsedByNavigationObstacle;
				}
			}
		}
		const size_t aabbFallbackCount = loadedColliders > worldColliders.size() ? loadedColliders - worldColliders.size() : 0;

		ImGui::Text("WorldAABBs: %zu", worldAABBs.size());
		ImGui::Text("FloorAABB count: %zu", floorAABBs.size());
		ImGui::Text("WallObstacleAABB count: %zu", wallObstacles.size());
		ImGui::Text("NavigationObstacleAABB count: %zu", navObstacles.size());
		ImGui::Text("WallObstacleOBB count: %zu", wallObstacleOBBs.size());
		ImGui::Text("NavigationObstacleOBB count: %zu", navObstacleOBBs.size());
		ImGui::Text("Brown wire source: OBB");
		ImGui::Text("Loaded Colliders: %zu", loadedColliders);
		ImGui::Text("Rotated collider count: %zu", rotatedColliderCount);
		ImGui::Text("AABB fallback count: %zu", aabbFallbackCount);
		ImGui::Text("collider_rotation read count: %zu", colliderRotationReadCount);
		ImGui::Text("sample collider name: %s", sampleColliderName.c_str());
		ImGui::Text("raw center: (%.2f, %.2f, %.2f)", sampleRawCenter.x, sampleRawCenter.y, sampleRawCenter.z);
		ImGui::Text("converted center: (%.2f, %.2f, %.2f)", sampleConvertedCenter.x, sampleConvertedCenter.y, sampleConvertedCenter.z);
		ImGui::Text("source rotation degree: (%.2f, %.2f, %.2f)",
			sampleSourceRotationDeg.x, sampleSourceRotationDeg.y, sampleSourceRotationDeg.z);
		ImGui::Text("converted rotation degree: (%.2f, %.2f, %.2f)",
			sampleConvertedRotationDeg.x, sampleConvertedRotationDeg.y, sampleConvertedRotationDeg.z);
		ImGui::Text("converted yaw sample: %.2f deg", sampleConvertedRotationDeg.y);
		ImGui::Text("final collider yaw degree: %.2f deg", sampleFinalYawDeg);
		ImGui::Text("OBB center: (%.2f, %.2f, %.2f)", sampleOBBCenter.x, sampleOBBCenter.y, sampleOBBCenter.z);
		ImGui::Text("AABB min: (%.2f, %.2f, %.2f)", sampleAABBMin.x, sampleAABBMin.y, sampleAABBMin.z);
		ImGui::Text("AABB max: (%.2f, %.2f, %.2f)", sampleAABBMax.x, sampleAABBMax.y, sampleAABBMax.z);
		ImGui::Text("used by NavigationObstacle: %s", sampleUsedByNavigationObstacle ? "true" : "false");
		ImGui::Text("used by WallObstacle: %s", sampleUsedByWallObstacle ? "true" : "false");
		ImGui::Text("used by CollisionObstacle: %s", sampleUsedByCollisionObstacle ? "true" : "false");
		const int floorCount = colliderTypeCounts["Floor"];
		const int wallObstacleCount = colliderTypeCounts["Obstacle"] + colliderTypeCounts["Pillar"] + colliderTypeCounts["Fence"] + colliderTypeCounts["Tree"];
		ImGui::Text("Floor AABB count: %d", floorCount);
		ImGui::Text("Wall/Obstacle AABB count: %d", wallObstacleCount);
		ImGui::Text("Navigation obstacle count: %zu", navObstacles.size());
		ImGui::Text("Collision obstacle count: %d", wallObstacleCount);
		ImGui::Text("Floor: %d", colliderTypeCounts["Floor"]);
		ImGui::Text("Obstacle: %d", colliderTypeCounts["Obstacle"]);
		ImGui::Text("Pillar: %d", colliderTypeCounts["Pillar"]);
		ImGui::Text("Fence: %d", colliderTypeCounts["Fence"]);
		ImGui::Text("Tree: %d", colliderTypeCounts["Tree"]);

		if (debugMeleeEnemy_)
		{
			const Vector3 enemyPos = debugMeleeEnemy_->GetCenterPosition();
			ImGui::Text("MeleeEnemy Count: 1");
			ImGui::Text("Selected Pos: (%.2f, %.2f, %.2f)", enemyPos.x, enemyPos.y, enemyPos.z);
			ImGui::Text("Selected Action: %s", debugMeleeEnemy_->GetCurrentBehaviorName());
			ImGui::Text("Grounded: %s", debugMeleeEnemy_->GetVelocity().y == 0.0f ? "Likely grounded" : "Falling/Moving");
		}

		ImGui::End();
	}

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


	if (disintegrationDebug_)
	{
		disintegrationDebug_->DrawImGui();
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

void DebugScene::DrawSkyBoxImGui()
{
#ifdef USE_IMGUI
	K4E::SkyBoxPreset* preset = skyBoxPresets_.FindActivePreset();
	ImGui::Begin("SkyBox Settings");
	ImGui::TextWrapped("DebugSceneの背景を、写真ではなくシンプルな空と独立した雲レイヤーとして調整します。");
	ImGui::Separator();

	if (ImGui::BeginCombo("Active Preset", skyBoxPresets_.activePresetName.c_str()))
	{
		for (const K4E::SkyBoxPreset& candidate : skyBoxPresets_.presets)
		{
			const bool selected = candidate.name == skyBoxPresets_.activePresetName;
			if (ImGui::Selectable(candidate.name.c_str(), selected)) { skyBoxPresets_.activePresetName = candidate.name; ApplyActiveSkyBoxPreset(); preset = skyBoxPresets_.FindActivePreset(); }
			if (selected) ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}
	ImGui::TextWrapped("Active Preset: 現在使用する名前付きSkyBox設定を選択します。");

	if (preset)
	{
		if (ImGui::Checkbox("Enabled", &preset->enabled)) ApplyActiveSkyBoxPreset();
		ImGui::TextWrapped("Enabled: SkyBox背景の表示ON/OFFを切り替えます。");
		const char* skyTypes[] = { "ColorOnly", "Gradient", "Texture" };
		int skyTypeIndex = preset->skyType == "ColorOnly" ? 0 : preset->skyType == "Texture" ? 2 : 1;
		if (ImGui::Combo("Sky Type", &skyTypeIndex, skyTypes, IM_ARRAYSIZE(skyTypes))) { preset->skyType = skyTypes[skyTypeIndex]; ApplyActiveSkyBoxPreset(); }
		ImGui::TextWrapped("Sky Type: 空の描画方式を切り替えます。Gradientは上下の色と地平線色からシンプルな空を作ります。");
		ImGui::InputText("Texture Path", skyBoxTexturePathBuffer_.data(), skyBoxTexturePathBuffer_.size());
		if (ImGui::Button("Apply Texture Path")) { preset->texturePath = skyBoxTexturePathBuffer_.data(); ApplyActiveSkyBoxPreset(); }
		ImGui::SameLine();
		if (ImGui::Button("Reload Texture")) { preset->texturePath = skyBoxTexturePathBuffer_.data(); ApplyActiveSkyBoxPreset(true); }
		ImGui::TextWrapped("Texture Path / Reload Texture: Texture方式で使う従来SkyBoxテクスチャを変更、再読み込みします。");
		if (ImGui::ColorEdit4("Top Color", &preset->topColor.x)) ApplyActiveSkyBoxPreset();
		ImGui::TextWrapped("Top Color: 空の上側の色を調整します。ColorOnlyではこの色を使います。");
		if (ImGui::ColorEdit4("Bottom Color", &preset->bottomColor.x)) ApplyActiveSkyBoxPreset();
		ImGui::TextWrapped("Bottom Color: 空の下側の色を調整します。");
		if (ImGui::ColorEdit4("Horizon Color", &preset->horizonColor.x)) ApplyActiveSkyBoxPreset();
		ImGui::TextWrapped("Horizon Color: 地平線付近の色を調整します。");
		if (ImGui::DragFloat("Brightness", &preset->brightness, 0.01f, 0.0f, 10.0f)) ApplyActiveSkyBoxPreset();
		ImGui::TextWrapped("Brightness: SkyBoxの明るさ倍率を調整します。");
		if (ImGui::DragFloat3("Rotation", &preset->rotation.x, 0.01f)) ApplyActiveSkyBoxPreset();
		ImGui::TextWrapped("Rotation: SkyBoxの向きをラジアン単位で調整します。");
		if (ImGui::DragFloat3("Scale", &preset->scale.x, 10.0f, 1.0f, 50000.0f)) ApplyActiveSkyBoxPreset();
		ImGui::TextWrapped("Scale: 背景キューブの大きさを調整します。");

		ImGui::SeparatorText("Cloud Layer");
		ImGui::TextWrapped("Cloud Layer: SkyBoxとは別に、上空の雲レイヤーを背景寄りに描画します。");
		if (ImGui::Checkbox("Cloud Enabled", &preset->cloud.enabled)) ApplyActiveSkyBoxPreset();
		ImGui::TextWrapped("Cloud Enabled: 雲レイヤーの表示ON/OFFを切り替えます。テクスチャがない場合は安全のため描画しません。");
		ImGui::InputText("Cloud Texture Path", cloudTexturePathBuffer_.data(), cloudTexturePathBuffer_.size());
		if (ImGui::Button("Apply Cloud Texture Path")) { preset->cloud.texturePath = cloudTexturePathBuffer_.data(); ApplyActiveSkyBoxPreset(); }
		ImGui::SameLine();
		if (ImGui::Button("Reload Cloud Texture")) { preset->cloud.texturePath = cloudTexturePathBuffer_.data(); ApplyActiveSkyBoxPreset(true); }
		ImGui::TextWrapped("Cloud Texture Path / Reload: 透過を持つシンプルな雲用2Dテクスチャを変更、再読み込みします。");
		ImGui::Text("Actual Cloud DDS: Resources/Textures/Compiled/%s", skyBox_->GetCloudTexturePath().c_str());
		ImGui::Text("Cloud Load Result: enabled=%s available=%s srvIndex=%u", skyBox_->IsCloudEnabled() ? "true" : "false", skyBox_->IsCloudTextureAvailable() ? "true" : "false", skyBox_->GetCloudTextureIndex());
		if (ImGui::DragFloat("Cloud Height", &preset->cloud.height, 1.0f, 0.0f, 10000.0f)) ApplyActiveSkyBoxPreset();
		ImGui::TextWrapped("Cloud Height: 雲レイヤーの見かけ上の高さを調整します。");
		if (ImGui::DragFloat("Cloud Scale", &preset->cloud.scale, 0.01f, 0.01f, 20.0f)) ApplyActiveSkyBoxPreset();
		ImGui::TextWrapped("Cloud Scale: 雲模様の広がりを調整します。");
		if (ImGui::DragFloat2("Cloud Scroll Speed", &preset->cloud.scrollSpeed.x, 0.0001f, -1.0f, 1.0f)) ApplyActiveSkyBoxPreset();
		ImGui::TextWrapped("Cloud Scroll Speed: UVスクロールで雲が流れる速度を調整します。");
		if (ImGui::DragFloat("Cloud Alpha", &preset->cloud.alpha, 0.01f, 0.0f, 1.0f)) ApplyActiveSkyBoxPreset();
		ImGui::TextWrapped("Cloud Alpha: 雲の透明度を調整します。");
		if (ImGui::ColorEdit4("Cloud Tint Color", &preset->cloud.tintColor.x)) ApplyActiveSkyBoxPreset();
		ImGui::TextWrapped("Cloud Tint Color: 雲へ重ねる色味を調整します。");
		if (ImGui::Button("Reset Cloud UV Offset")) { preset->cloud.uvOffset = {}; ApplyActiveSkyBoxPreset(); }
		ImGui::TextWrapped("Reset Cloud UV Offset: 流れた雲のUV位置を初期位置へ戻します。");
	}

	ImGui::Separator();
	if (ImGui::Button("Save Settings")) SaveSkyBoxPresets();
	ImGui::SameLine();
	if (ImGui::Button("Load Settings")) { LoadSkyBoxPresets(); ApplyActiveSkyBoxPreset(); }
	ImGui::TextWrapped("Save / Load Settings: SkyBoxとCloudLayer設定をJsonへ保存、またはJsonから復元します。");
	ImGui::TextWrapped("%s", skyBoxPresetLog_.c_str());
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
