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
#include <unordered_map>

using namespace Ken4lowEngine;

namespace
{
	std::string BuildBrownWireSourceLabel(
		bool showStageChunkBounds,
		bool showObjectBounds,
		bool showOldBounds,
		bool showColliderObb)
	{
		std::vector<std::string> sources{};
		if (showStageChunkBounds) { sources.emplace_back("StageChunkBounds"); }
		if (showObjectBounds) { sources.emplace_back("ObjectBounds"); }
		if (showOldBounds) { sources.emplace_back("Old AABB"); }
		if (showColliderObb) { sources.emplace_back("Collider OBB"); }
		if (sources.empty()) { return "None"; }

		std::string label{};
		for (size_t i = 0; i < sources.size(); ++i)
		{
			if (i > 0) { label += " + "; }
			label += sources[i];
		}
		return label;
	}

	void DrawObbCornersWire(const std::array<Vector3, 8>& corners, const Vector4& color)
	{
		static constexpr int kEdges[12][2] = {
			{0,1},{1,2},{2,3},{3,0}, // bottom 4
			{4,5},{5,6},{6,7},{7,4}, // top 4
			{0,4},{1,5},{2,6},{3,7}, // vertical 4
		};
		for (const auto& e : kEdges)
		{
			Wireframe::GetInstance()->DrawLine(corners[static_cast<size_t>(e[0])], corners[static_cast<size_t>(e[1])], color);
		}
	}

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

	debugMeleeEnemy_ = std::make_unique<MeleeEnemy>();
	debugMeleeEnemy_->Initialize();
	// ステージ床に確実に着地できるよう、初期位置は少し高めから開始する。
	debugMeleeEnemy_->SetCenterPosition({ 0.0f, 2.0f, 18.0f });

	meleeDummyTarget_.SetCenterPosition({ 0.0f, 2.0f, 24.0f });
	meleeDummyTarget_.SetOBBHalfSize({ 0.8f, 1.0f, 0.8f });
	debugMeleeEnemy_->SetTarget(&meleeDummyTarget_);
	collisionManager_->AddCollider(debugMeleeEnemy_.get());

	disintegrationDebug_ = std::make_unique<DisintegrationDebugController>();
	// Disintegration系の確認処理は専用コントローラへ委譲し、DebugScene本体の責務を絞る。
	disintegrationDebug_->Initialize();

	frustumCullingDebug_ = std::make_unique<FrustumCullingDebugController>();
	frustumCullingDebug_->Initialize(true);
	InitializeCullingTestObjects();

	stage_ = std::make_unique<K4E::Stage>();
	stage_->Initialize("Stages/hajimarinoheigen.json", "Stages/hajimarinoheigen.gltf");
	stage_->RegisterColliders(collisionManager_.get());
	stage_->Update();
	// DebugSceneでもステージAABBを共有し、EnemyBase系の敵が床と障害物に衝突できるようにする。
	EnemyBase::SetGlobalStageWorldAABBs(&stage_->GetWorldAABBs());
	EnemyBase::SetGlobalStageNavigationObstacleAABBs(&stage_->GetNavigationObstacleAABBs());
	debugMeleeEnemy_->SetFloorAABBs(&stage_->GetFloorAABBs());
	debugMeleeEnemy_->SetWallObstacleAABBs(&stage_->GetWallObstacleAABBs());
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

	if (debugMeleeEnemy_)
	{
		debugMeleeEnemy_->Update(deltaTime);
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

	for (auto& object : cullingTestObjects_)
	{
		object->Update();
	}

	collisionManager_->Update();
	collisionManager_->CheckAllCollisions();

	if (stage_)
	{
		stage_->Update();
	}

}

void DebugScene::Draw3DObjects()
{
	if (disintegrationDebug_)
	{
		disintegrationDebug_->Draw3DObjects();
	}

	for (auto& object : cullingTestObjects_)
	{
		object->Draw();
	}

	// ボス描画
	if (debugBoss_)
	{
		debugBoss_->Draw();
	}
	if (debugMeleeEnemy_)
	{
		debugMeleeEnemy_->Draw();
	}

	if (stage_)
	{
		stage_->Draw();
		if (showColliderObbWire_)
		{
			const Vector4 colliderObbColor = { 0.15f, 0.95f, 0.95f, 1.0f };
			// Collider OBBの8頂点だけを描画し、古いObjectBoundsと混同しないようにする
			for (const StageObstacleBox& box : stage_->GetObstacleBoxes())
			{
				DrawObbCornersWire(box.corners, colliderObbColor);
			}
		}
		if (showNavigationAabbWire_)
		{
			for (const AABB& aabb : stage_->GetNavigationObstacleAABBs())
			{
				Wireframe::GetInstance()->DrawAABB(aabb, { 0.90f, 0.45f, 0.12f, 1.0f });
			}
		}
		if (showWallAabbWire_)
		{
			for (const AABB& aabb : stage_->GetWallObstacleAABBs())
			{
				Wireframe::GetInstance()->DrawAABB(aabb, { 0.70f, 0.25f, 0.12f, 1.0f });
			}
		}
		if (showOldBoundsWire_)
		{
			for (const AABB& aabb : stage_->GetWorldAABBsLegacy())
			{
				Wireframe::GetInstance()->DrawAABB(aabb, { 0.55f, 0.35f, 0.20f, 1.0f });
			}
		}
		stage_->SetStageChunkBoundsVisible(showStageChunkBoundsWire_);
		stage_->SetStageChunkObjectBoundsVisible(showStageChunkObjectBoundsWire_);
		brownWireframeSource_ = BuildBrownWireSourceLabel(
			showStageChunkBoundsWire_,
			showStageChunkObjectBoundsWire_,
			showOldBoundsWire_,
			showColliderObbWire_);
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
	if (frustumCullingDebug_)
	{
		frustumCullingDebug_->DrawDebug();
	}

	collisionManager_->Draw();
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
	if (debugMeleeEnemy_)
	{
		debugMeleeEnemy_->DrawShadow();
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
	// 入力状態を必ず戻す（ロック/非表示のまま終了しない）
	input_->SetLockCursor(false);
	input_->SetCursorVisible(true);

	cullingTestObjects_.clear();
	frustumCullingDebug_.reset();
	disintegrationDebug_.reset();
	EnemyBase::SetGlobalStageWorldAABBs(nullptr);
	debugBoss_.reset();
	debugMeleeEnemy_.reset();
	collisionManager_.reset();
	stage_.reset();

	input_ = nullptr;
	dxCommon_ = nullptr;
}

void DebugScene::DrawImGui()
{
#ifdef USE_IMGUI

	LightManager::GetInstance()->DrawImGui();

	if (debugBoss_)
	{
		debugBoss_->DrawImGui();
	}
	if (debugMeleeEnemy_)
	{
		debugMeleeEnemy_->DrawImGui();
	}

	if (frustumCullingDebug_)
	{
		frustumCullingDebug_->DrawImGui();
	}

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
	ImGui::Checkbox("DummyTarget wire visible", &meleeDummyWireVisible_);
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
		const std::vector<AABB>& legacyWorldAABBs = stage_->GetWorldAABBsLegacy();
		const auto& obstacleBoxes = stage_->GetObstacleBoxes();
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
		Vector3 sampleOBBHalfSize{};
		Vector3 sampleOBBAxisX{ 1.0f, 0.0f, 0.0f };
		Vector3 sampleOBBAxisY{ 0.0f, 1.0f, 0.0f };
		Vector3 sampleOBBAxisZ{ 0.0f, 0.0f, 1.0f };
		Vector3 sampleRawSize{};
		Vector3 sampleRawColliderRotation{};
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
					sampleRawSize = object.collider.size;
					sampleRawColliderRotation = object.collider.rotation;
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
		if (!obstacleBoxes.empty())
		{
			const StageObstacleBox& sampleObb = obstacleBoxes.front();
			sampleColliderName = sampleObb.name.empty() ? sampleColliderName : sampleObb.name;
			sampleOBBCenter = sampleObb.center;
			sampleOBBHalfSize = sampleObb.halfSize;
			sampleOBBAxisX = sampleObb.axisX;
			sampleOBBAxisY = sampleObb.axisY;
			sampleOBBAxisZ = sampleObb.axisZ;
			sampleAABBMin = sampleObb.enclosingAABB.min;
			sampleAABBMax = sampleObb.enclosingAABB.max;
		}
		const size_t aabbFallbackCount = loadedColliders > worldColliders.size() ? loadedColliders - worldColliders.size() : 0;

		ImGui::Text("WorldAABBs: %zu", worldAABBs.size());
		ImGui::Text("FloorAABB count: %zu", floorAABBs.size());
		ImGui::Text("WallObstacleAABB count: %zu", wallObstacles.size());
		ImGui::Text("NavigationObstacleAABB count: %zu", navObstacles.size());
		ImGui::Text("Collider OBB count: %zu", obstacleBoxes.size());
		ImGui::Text("CollisionObstacle count: %zu", obstacleBoxes.size());
		ImGui::Text("Loaded Colliders: %zu", loadedColliders);
		ImGui::Text("Rotated collider count: %zu", rotatedColliderCount);
		ImGui::Text("AABB fallback count: %zu", aabbFallbackCount);
		ImGui::Text("collider_rotation read count: %zu", colliderRotationReadCount);
		ImGui::Text("sample collider name: %s", sampleColliderName.c_str());
		ImGui::Text("raw center: (%.2f, %.2f, %.2f)", sampleRawCenter.x, sampleRawCenter.y, sampleRawCenter.z);
		ImGui::Text("raw size: (%.2f, %.2f, %.2f)", sampleRawSize.x, sampleRawSize.y, sampleRawSize.z);
		ImGui::Text("raw collider_rotation(rad): (%.3f, %.3f, %.3f)",
			sampleRawColliderRotation.x, sampleRawColliderRotation.y, sampleRawColliderRotation.z);
		ImGui::Text("converted center: (%.2f, %.2f, %.2f)", sampleConvertedCenter.x, sampleConvertedCenter.y, sampleConvertedCenter.z);
		ImGui::Text("source rotation degree: (%.2f, %.2f, %.2f)",
			sampleSourceRotationDeg.x, sampleSourceRotationDeg.y, sampleSourceRotationDeg.z);
		ImGui::Text("converted rotation degree: (%.2f, %.2f, %.2f)",
			sampleConvertedRotationDeg.x, sampleConvertedRotationDeg.y, sampleConvertedRotationDeg.z);
		ImGui::Text("converted yaw sample: %.2f deg", sampleConvertedRotationDeg.y);
		ImGui::Text("final collider yaw degree: %.2f deg", sampleFinalYawDeg);
		ImGui::Text("OBB center: (%.2f, %.2f, %.2f)", sampleOBBCenter.x, sampleOBBCenter.y, sampleOBBCenter.z);
		ImGui::Text("OBB halfSize: (%.2f, %.2f, %.2f)", sampleOBBHalfSize.x, sampleOBBHalfSize.y, sampleOBBHalfSize.z);
		ImGui::Text("OBB axisX: (%.3f, %.3f, %.3f)", sampleOBBAxisX.x, sampleOBBAxisX.y, sampleOBBAxisX.z);
		ImGui::Text("OBB axisY: (%.3f, %.3f, %.3f)", sampleOBBAxisY.x, sampleOBBAxisY.y, sampleOBBAxisY.z);
		ImGui::Text("OBB axisZ: (%.3f, %.3f, %.3f)", sampleOBBAxisZ.x, sampleOBBAxisZ.y, sampleOBBAxisZ.z);
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
		const bool usingObbDerivedAabb = worldAABBs.size() == obstacleBoxes.size();
		ImGui::Text("Brown Wire Source: %s", brownWireframeSource_.c_str());
		ImGui::Text("Draw Source: StageObstacleBox OBB");
		ImGui::Text("Old Bounds source: StageChunkBounds/ObjectBounds/worldAABBsLegacy");
		ImGui::Text("Using OBB-derived AABB: %s", usingObbDerivedAabb ? "true" : "false");
		ImGui::Checkbox("Show Collider OBB Wire", &showColliderObbWire_);
		ImGui::Checkbox("Show Navigation AABB Wire", &showNavigationAabbWire_);
		ImGui::Checkbox("Show Wall AABB Wire", &showWallAabbWire_);
		ImGui::Checkbox("Show Old Bounds Wire", &showOldBoundsWire_);
		ImGui::Checkbox("Show StageChunk Bounds", &showStageChunkBoundsWire_);
		ImGui::Checkbox("Show Object Bounds", &showStageChunkObjectBoundsWire_);
		ImGui::Text("Legacy WorldAABB count: %zu", legacyWorldAABBs.size());
		if (!obstacleBoxes.empty())
		{
			ImGui::Text("corners[0]: (%.2f, %.2f, %.2f)", obstacleBoxes.front().corners[0].x, obstacleBoxes.front().corners[0].y, obstacleBoxes.front().corners[0].z);
			ImGui::Text("corners[1]: (%.2f, %.2f, %.2f)", obstacleBoxes.front().corners[1].x, obstacleBoxes.front().corners[1].y, obstacleBoxes.front().corners[1].z);
			ImGui::Text("corners[2]: (%.2f, %.2f, %.2f)", obstacleBoxes.front().corners[2].x, obstacleBoxes.front().corners[2].y, obstacleBoxes.front().corners[2].z);
			ImGui::Text("corners[3]: (%.2f, %.2f, %.2f)", obstacleBoxes.front().corners[3].x, obstacleBoxes.front().corners[3].y, obstacleBoxes.front().corners[3].z);
		}
		ImGui::Text("Floor: %d", colliderTypeCounts["Floor"]);
		ImGui::Text("Obstacle: %d", colliderTypeCounts["Obstacle"]);
		ImGui::Text("Pillar: %d", colliderTypeCounts["Pillar"]);
		ImGui::Text("Fence: %d", colliderTypeCounts["Fence"]);
		ImGui::Text("Tree: %d", colliderTypeCounts["Tree"]);

		if (debugMeleeEnemy_)
		{
			const Vector3 enemyPos = debugMeleeEnemy_->GetCenterPosition();
			ImGui::Text("MeleeEnemy Pos: (%.2f, %.2f, %.2f)", enemyPos.x, enemyPos.y, enemyPos.z);
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

void DebugScene::InitializeCullingTestObjects()
{
	cullingTestObjects_.clear();

	struct CullingTestObjectDesc
	{
		Vector3 position;
		Vector3 scale;
		Vector4 color;
	};

	const CullingTestObjectDesc objectDescs[] = {
		{ { 0.0f, 1.0f, 10.0f }, { 1.5f, 1.5f, 1.5f }, { 0.2f, 1.0f, 0.2f, 1.0f } },
		{ { -18.0f, 1.0f, 18.0f }, { 1.5f, 1.5f, 1.5f }, { 1.0f, 0.25f, 0.25f, 1.0f } },
		{ { 18.0f, 1.0f, 18.0f }, { 1.5f, 1.5f, 1.5f }, { 0.25f, 0.45f, 1.0f, 1.0f } },
		{ { 0.0f, 1.0f, 95.0f }, { 2.0f, 2.0f, 2.0f }, { 1.0f, 0.8f, 0.2f, 1.0f } },
		{ { 0.0f, 1.0f, -9.0f }, { 1.5f, 1.5f, 1.5f }, { 1.0f, 0.2f, 1.0f, 1.0f } },
		{ { 0.0f, 5.0f, 22.0f }, { 1.5f, 1.5f, 1.5f }, { 0.2f, 1.0f, 1.0f, 1.0f } },
	};

	cullingTestObjects_.reserve(sizeof(objectDescs) / sizeof(objectDescs[0]));
	for (const CullingTestObjectDesc& desc : objectDescs)
	{
		auto object = std::make_unique<Object3D>();
		object->Initialize("Test/cube.gltf");
		object->SetTranslate(desc.position);
		object->SetScale(desc.scale);
		object->SetColor(desc.color);
		// Object3D のデフォルト設定で共通カリング経路を確認する。
		object->Update();
		cullingTestObjects_.push_back(std::move(object));
	}
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
