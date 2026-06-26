#include "Actor.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

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

	void Actor::PostPhysicsUpdate(float deltaTime)
	{
		for (auto& component : components_)
		{
			// 物理更新後のTransform反映処理をComponent側へ流す。
			component->PostPhysicsUpdate(deltaTime);
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
#ifdef USE_IMGUI
		for (auto& component : components_)
		{
			if (ImGui::TreeNode(component->GetName().c_str()))
			{
				component->DrawImGui(); // Component単位でEditor表示を行う。
				ImGui::TreePop();
			}
		}
#endif // USE_IMGUI
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

	ActorComponent* Actor::FindComponentByName(std::string_view name)
	{
		for (auto& component : components_)
		{
			// Component名が一致した最初のComponentを返す
			if (component->GetName() == name)
			{
				return component.get();
			}
		}

		return nullptr; // 一致するComponentが見つからなかった場合はnullptrを返す
	}

	const ActorComponent* Actor::FindComponentByName(std::string_view name) const
	{
		for (auto& component : components_)
		{
			// Component名が一致した最初のComponentを返す
			if (component->GetName() == name)
			{
				return component.get();
			}
		}

		return nullptr; // 一致するComponentが見つからなかった場合はnullptrを返す
	}

}