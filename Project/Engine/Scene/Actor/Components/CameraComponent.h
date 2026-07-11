#pragma once
#include "SceneComponent.h"

namespace Ken4lowEngine
{
	class Camera;

	/// -------------------------------------------------------------
	///		  Actorにカメラ機能を追加するSceneComponentクラス
	/// -------------------------------------------------------------
	class CameraComponent : public SceneComponent
	{
	public:
		void Initialize() override;
		void Update(float deltaTime) override;
		void UpdateEditor(float deltaTime) override;
		void PostPhysicsUpdate(float deltaTime) override;
		void DrawImGui() override;
		void Finalize() override;

		std::string GetClassTypeName() const override { return "CameraComponent"; }
		void ToJson(nlohmann::json& outJson) const override;
		void FromJson(const nlohmann::json& inJson) override;

		Camera* GetCamera() const { return camera_; }

		void SetAutoRegisterMainCamera(bool enabled);
		void SetInheritParentRotation(bool enabled) { inheritParentRotation_ = enabled; }
		bool IsInheritParentRotationEnabled() const { return inheritParentRotation_; }

	private:
		void SyncTransformToCamera();

		Camera* camera_ = nullptr;
		bool autoRegisterMainCamera_ = false;
		bool inheritParentRotation_ = true; // falseならActorやModelの回転変更をゲームカメラへ伝播させない。
	};
} // namespace Ken4lowEngine
