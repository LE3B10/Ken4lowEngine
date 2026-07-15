#pragma once
#include "BaseScene.h"
#include "Validation/EnemyMigrationValidation.h"
#include "Validation/BossMigrationValidation.h"
#include "Validation/LevelDataValidation.h"
#include "Validation/LevelImportValidation.h"
#include "Validation/PerformancePhaseValidation.h"

#include <ActorWorld.h>
#include <Editor/ActorWorldEditorBridge.h>
#include <PhysicsWorld.h>
#include <PhysicsDebugDraw.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace Ken4lowEngine
{
	class Actor;
	class Input;
}

namespace K4E = ::Ken4lowEngine;

class DebugScene : public K4E::BaseScene
{
public:
	DebugScene();
	~DebugScene() override;

	void Initialize() override;
	void Update() override;
	void UpdateEditor(float deltaTime) override;

	/// Play開始時はDebugPlayerを操作するFPS入力へ自動キャプチャする。
	void BeginEditorPlay() override;

	/// Stop時はEditorへ入力を返し、カーソル固定を解除する。
	void EndEditorPlay() override;

	/// DebugSceneはPlayerActorのFPS操作を検証するため、Play中はMain Viewport入力をキャプチャする。
	K4E::EditorInputPolicy GetEditorInputPolicy() const override { return K4E::EditorInputPolicy::FpsCapture; }

	void CollectEditorObjects(std::vector<K4E::EditorObjectInfo>& outObjects) override
	{
		const std::uint64_t fingerprint = BuildEditorObjectFingerprint();
		if (!editorObjectCacheValid_ || fingerprint != editorObjectCacheFingerprint_)
		{
			editorObjectCache_.clear();
			K4E::CollectActorWorldEditorObjects(actorWorld_, editorObjectCache_, "DebugScene");
			editorObjectCacheFingerprint_ = fingerprint;
			editorObjectCacheValid_ = true; // Actor/Component構造が変わらないフレームでは重いEditor情報生成を再利用する。
		}

		outObjects.reserve(outObjects.size() + editorObjectCache_.size());
		outObjects.insert(outObjects.end(), editorObjectCache_.begin(), editorObjectCache_.end());
	}

	K4E::ActorWorld* GetEditorActorWorld() override { return &actorWorld_; }

	void PrepareShadowPass() override
	{
		actorWorld_.PrepareRenderState();
	}

	void Draw3DObjects() override;
	void DrawShadowObjects() override;
	void Draw2DSprites() override;
	void Finalize() override;
	void DrawImGui() override;

private:
	void UpdateDebug();
	void ProcessActorWorldValidationRequests();
	void RunActorWorldValidation();
	void DrawActorWorldValidationImGui();
	K4E::Actor* FindActorWorldValidationTarget() const;

	std::uint64_t BuildEditorObjectFingerprint() const
	{
		std::uint64_t hash = 1469598103934665603ull;
		auto mix = [&hash](std::uint64_t value)
			{
				hash ^= value;
				hash *= 1099511628211ull;
			};

		const std::hash<std::string_view> stringHasher{};
		for (const auto& actor : actorWorld_.GetActors())
		{
			if (!actor) continue;
			mix(reinterpret_cast<std::uintptr_t>(actor.get()));
			mix(stringHasher(actor->GetName()));
			mix(actor->IsActive() ? 1ull : 0ull);
			for (const auto& component : actor->GetComponents())
			{
				if (!component) continue;
				mix(reinterpret_cast<std::uintptr_t>(component.get()));
				mix(stringHasher(component->GetName()));
				mix(component->IsActive() ? 1ull : 0ull);
			}
		}
		return hash;
	}

private:
	K4E::Input* input_ = nullptr;
	bool isDebugCamera_ = false;

	K4E::ActorWorld actorWorld_;
	K4E::PhysicsWorld actorPhysicsWorld_;
	K4E::PhysicsDebugDraw actorPhysicsDebugDraw_;

	struct ActorWorldValidationState
	{
		std::string targetActorName = "DebugPlayer";
		std::string jsonPath = "../Generated/Intermediate/ActorWorldValidation.json";
		std::string lastMessage = "未検証";
		bool lastSucceeded = false;
		bool requestSpawn = false;
		bool requestToggleActive = false;
		bool requestSave = false;
		bool requestReload = false;
		bool requestSpawnFromJson = false;
		bool requestDestroy = false;
		bool requestValidation = true;
		bool pendingDestroyCheck = false;
	} actorWorldValidation_;

	std::vector<K4E::EditorObjectInfo> editorObjectCache_;
	std::uint64_t editorObjectCacheFingerprint_ = 0;
	bool editorObjectCacheValid_ = false;

	LevelDataValidation levelDataValidation_;
	LevelImportValidation levelImportValidation_{ &actorWorld_, &actorPhysicsWorld_ };
	PerformancePhaseValidation performancePhaseValidation_{ &actorWorld_, &actorPhysicsWorld_ };
	EnemyMigrationValidation enemyMigrationValidation_;
	BossMigrationValidation bossMigrationValidation_;
};
