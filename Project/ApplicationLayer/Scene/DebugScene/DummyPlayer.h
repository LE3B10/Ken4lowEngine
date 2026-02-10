#pragma once
#include "Collider.h"
#include "ContactRecord.h"
#include "Object3D.h"
#include <Vector3.h>
#include <Vector4.h>

#include <memory>

namespace K4E = ::Ken4lowEngine;

namespace Ken4lowEngine { class Input; }

/// -------------------------------------------------------------
///                     ダミープレイヤークラス
/// -------------------------------------------------------------
class DummyPlayer : public K4E::Collider
{
public:
	DummyPlayer() = default;

	void Initialize();
	void Update();
	void Draw();
	void DrawImGui();

	// 衝突状態（Enter/Exit）
	void OnCollisionEnter(K4E::Collider* other) override;
	void OnCollisionExit(K4E::Collider* other) override;

private:
	K4E::Input* input_ = nullptr;

	K4E::Vector3 moveVelocity_ = { 0.0f, 0.0f, 0.0f };
	K4E::Vector4 debugColor_ = { 0.0f, 1.0f, 0.0f, 1.0f };

	std::unique_ptr<K4E::Object3D> model_ = nullptr;

	// 接触中の相手
	K4E::ContactRecord contactRecord_{};
};
