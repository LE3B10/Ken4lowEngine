#pragma once

#include <Actor.h>
#include <ColliderComponent.h>
#include <Object3D.h>
#include <PhysicsCollisionLayer.h>
#include <RigidbodyComponent.h>
#include <SceneComponent.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>

namespace K4E = ::Ken4lowEngine;

/// 隠し通路を塞ぐ岩壁と状態表示灯を描画するStage 2専用Component。
class Stage2HiddenPassageVisualComponent final : public K4E::SceneComponent
{
public:
	std::string GetClassTypeName() const override { return "Stage2HiddenPassageVisualComponent"; }

	void Initialize() override
	{
		K4E::SceneComponent::Initialize();
		gateObject_ = CreateCube();
		leftSignalObject_ = CreateCube();
		rightSignalObject_ = CreateCube();
		SyncVisuals();
	}

	void Update(float deltaTime) override
	{
		K4E::SceneComponent::Update(deltaTime);
		visualTimer_ += std::max(0.0f, deltaTime);
		SyncVisuals();
	}

	void UpdateEditor(float deltaTime) override
	{
		K4E::SceneComponent::UpdateEditor(deltaTime);
		visualTimer_ += std::max(0.0f, deltaTime);
		SyncVisuals();
	}

	void Draw() override
	{
		if (gateObject_) gateObject_->Draw();
		if (leftSignalObject_) leftSignalObject_->Draw();
		if (rightSignalObject_) rightSignalObject_->Draw();
	}

	void DrawShadow() override
	{
		if (gateObject_) gateObject_->DrawShadow();
	}

	void Finalize() override
	{
		rightSignalObject_.reset();
		leftSignalObject_.reset();
		gateObject_.reset();
		K4E::SceneComponent::Finalize();
	}

	void SetOpenProgress(float progress)
	{
		openProgress_ = std::clamp(progress, 0.0f, 1.0f);
	}

	void UpdateShadowMatrix(const K4E::Matrix4x4& lightViewProjection)
	{
		if (gateObject_) gateObject_->UpdateShadowMatrix(lightViewProjection);
	}

private:
	static std::unique_ptr<K4E::Object3D> CreateCube()
	{
		try
		{
			auto object = std::make_unique<K4E::Object3D>();
			object->Initialize("Sample/cube.gltf");
			object->SetPbrEnabled(true);
			object->SetMetallic(0.18f);
			object->SetRoughness(0.78f);
			object->SetReflectivity(0.16f);
			object->SetFrustumCullingEnabled(false);
			return object;
		}
		catch (...)
		{
			return nullptr; // Gate描画の生成に失敗してもObjective進行とCollider制御は継続する。
		}
	}

	void SyncVisuals()
	{
		const K4E::Vector3 position = GetWorldPosition();
		const float pulse = 0.5f + 0.5f * std::sin(visualTimer_ * (openProgress_ > 0.0f ? 7.5f : 3.4f));
		if (gateObject_)
		{
			gateObject_->SetTranslate(position);
			gateObject_->SetScale({ 8.0f, 4.0f, 1.0f });
			gateObject_->SetColor({ 0.19f, 0.18f, 0.17f, 1.0f });
			gateObject_->SetEmissiveFactor({ 0.015f, 0.012f, 0.008f, 1.0f });
			gateObject_->Update();
		}

		const K4E::Vector4 signalColor = openProgress_ > 0.98f
			? K4E::Vector4{ 0.18f, 1.0f, 0.38f, 1.0f }
			: K4E::Vector4{ 1.0f, 0.20f + pulse * 0.12f, 0.08f, 1.0f };
		const K4E::Vector4 signalEmissive = openProgress_ > 0.98f
			? K4E::Vector4{ 0.25f, 3.4f, 0.60f, 1.0f }
			: K4E::Vector4{ 3.2f + pulse * 1.4f, 0.18f, 0.04f, 1.0f };
		for (int side = -1; side <= 1; side += 2)
		{
			K4E::Object3D* signal = side < 0 ? leftSignalObject_.get() : rightSignalObject_.get();
			if (!signal) continue;
			signal->SetTranslate(position + K4E::Vector3{ static_cast<float>(side) * 6.5f, 0.0f, -1.08f });
			signal->SetScale({ 0.18f, 2.8f, 0.12f });
			signal->SetColor(signalColor);
			signal->SetEmissiveFactor(signalEmissive);
			signal->Update();
		}
	}

	std::unique_ptr<K4E::Object3D> gateObject_;
	std::unique_ptr<K4E::Object3D> leftSignalObject_;
	std::unique_ptr<K4E::Object3D> rightSignalObject_;
	float visualTimer_ = 0.0f;
	float openProgress_ = 0.0f;
};

/// 3基目の装置起動後に上昇し、隠し坑道への入口を開くActor。
class Stage2HiddenPassageActor final : public K4E::Actor
{
public:
	std::string GetClassTypeName() const override { return "Stage2HiddenPassageActor"; }

	void Initialize() override
	{
		if (!GetComponents().empty())
		{
			K4E::Actor::Initialize();
			visualComponent_ = GetComponent<Stage2HiddenPassageVisualComponent>();
			colliderComponent_ = GetComponent<K4E::ColliderComponent>();
			return;
		}

		auto& visual = CreateRootComponent<Stage2HiddenPassageVisualComponent>();
		visual.SetName("Stage 2 Hidden Gate Visual");
		visual.SetUpdateOrder(-10);
		visual.SetDrawOrder(1);
		visualComponent_ = &visual;

		auto& collider = AddComponent<K4E::ColliderComponent>();
		collider.SetName("Stage 2 Hidden Gate Collider");
		collider.SetUpdateOrder(-20);
		collider.SetShapeType(K4E::ECollisionShapeType::OBB);
		collider.SetHalfSize({ 8.0f, 4.0f, 1.0f });
		collider.SetCollisionLayer(K4E::PhysicsCollisionLayer::WorldStatic);
		collider.SetCollisionTag("Obstacle");
		collider.SetIsTrigger(false);
		collider.AttachTo(&visual);
		colliderComponent_ = &collider;

		auto& rigidbody = AddComponent<K4E::RigidbodyComponent>();
		rigidbody.SetName("Stage 2 Hidden Gate Rigidbody");
		rigidbody.SetUpdateOrder(-30);
		rigidbody.SetBodyType(K4E::BodyType::Static);
		rigidbody.SetUseGravity(false);
		rigidbody.SetSleepEnabled(false);

		K4E::Actor::Initialize();
	}

	void Configure(const K4E::Vector3& closedPosition, float openOffsetY = 10.5f)
	{
		closedPosition_ = closedPosition;
		openPosition_ = closedPosition + K4E::Vector3{ 0.0f, std::max(8.5f, openOffsetY), 0.0f };
		SetName("Stage2HiddenPassageGate");
		SetLayer("StageObjective");
		AddTag("Stage2HiddenPassage");
		ApplyPosition(closedPosition_);
		if (visualComponent_) visualComponent_->SetOpenProgress(0.0f);
	}

	void Update(float deltaTime) override
	{
		if (opening_ && openProgress_ < 1.0f)
		{
			openProgress_ = std::min(1.0f, openProgress_ + std::max(0.0f, deltaTime) / std::max(0.05f, openDuration_));
			const float eased = openProgress_ * openProgress_ * (3.0f - 2.0f * openProgress_);
			ApplyPosition(closedPosition_ + (openPosition_ - closedPosition_) * eased); // 岩壁とColliderを同じ補間値で上昇させ、見た目と通行可能時刻を一致させる。
			if (visualComponent_) visualComponent_->SetOpenProgress(openProgress_);
			if (openProgress_ >= 1.0f) DisableBlockingCollider();
		}
		K4E::Actor::Update(deltaTime);
	}

	void RequestOpen()
	{
		if (opening_ || openProgress_ >= 1.0f) return;
		opening_ = true;
	}

	void UpdateShadowMatrix(const K4E::Matrix4x4& lightViewProjection)
	{
		if (visualComponent_) visualComponent_->UpdateShadowMatrix(lightViewProjection);
	}

	bool IsOpening() const { return opening_ && openProgress_ < 1.0f; }
	bool IsOpen() const { return openProgress_ >= 1.0f; }
	float GetOpenProgress() const { return openProgress_; }

private:
	void ApplyPosition(const K4E::Vector3& position)
	{
		if (K4E::SceneComponent* root = GetRootComponent())
		{
			root->SetLocalPosition(position);
			root->RefreshWorldTransform();
		}
	}

	void DisableBlockingCollider()
	{
		if (!colliderComponent_) return;
		colliderComponent_->SetActive(false);
		if (K4E::Collider* collider = colliderComponent_->GetCollider()) collider->SetEnabled(false);
	}

	Stage2HiddenPassageVisualComponent* visualComponent_ = nullptr;
	K4E::ColliderComponent* colliderComponent_ = nullptr;
	K4E::Vector3 closedPosition_{ 0.0f, 6.0f, 119.0f };
	K4E::Vector3 openPosition_{ 0.0f, 16.5f, 119.0f };
	float openDuration_ = 2.15f;
	float openProgress_ = 0.0f;
	bool opening_ = false;
};
