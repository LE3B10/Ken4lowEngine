#pragma once
#include "AnimationModel.h"
#include <Collider.h>

#include <memory>

class CollisionManager;

class DummyModel : public Collider
{
	// ★各部位ごとの Collider を保持
	struct PartCol
	{
		std::string name;
		std::unique_ptr<Collider> col;
	};
	std::vector<PartCol> bodyCols_;

public:

	// 初期化処理
	void Initialize();

	// 更新処理
	void Update();

	// 描画処理
	void Draw();

	// ImGuiの描画処理
	void DrawImGui();

	// すべての部位コライダーを CollisionManager へ登録
	void RegisterColliders(CollisionManager* collisionManager) const;

	void OnCollision(Collider* other) override;

private:

	std::unique_ptr<AnimationModel> model_;
};

