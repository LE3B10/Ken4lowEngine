#include "Actor.h"

namespace Ken4lowEngine
{
	void Actor::Initialize()
	{
		for (auto& component : components_)
		{
			// 初期化処理は各Componentに移譲する
			component->Initialize();
		}
	}

	void Actor::Update(float deltaTime)
	{
		for (auto& component : components_)
		{
			// Actorは更新順だけ管理し、処理内容はComponent側に任せる
			component->Update(deltaTime);
		}
	}

	void Actor::Draw()
	{
		for (auto& component : components_)
		{
			// 描画を持つComponentだけがDrawを実装する
			component->Draw();
		}
	}

	void Actor::DrawShadow()
	{
		for (auto& component : components_)
		{
			// 影を落とすComponentだけがShadow描画を実装する
			component->DrawShadow();
		}
	}

	void Actor::DrawImGui()
	{
		for (auto& component : components_)
		{
			// Editor表示をComponent単位で拡張できるようにする
			component->DrawImGui();
		}
	}

	void Actor::Finalize()
	{
		for (auto& component : components_)
		{
			// Component破棄前に明示的な終了処理を流す
			component->Finalize();
		}

		components_.clear(); // ActorがComponentの寿命を管理するため、ここで破棄する
	}
}