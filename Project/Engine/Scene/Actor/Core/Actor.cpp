#include "Actor.h"

#ifdef USE_IMGUI
#include <imgui.h>
#include <typeinfo>
#endif // USE_IMGUI

namespace Ken4lowEngine
{
	void Actor::Initialize()
	{
		InitializeComponents();
	}

	void Actor::InitializeComponents()
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
			if (!component)
			{
				continue; // nullptrのComponentは無視する
			}

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
		DrawInspectorImGui(); // 既存のDrawImGui呼び出しはDetails表示として扱う
#endif // USE_IMGUI
	}

	void Actor::DrawHierarchyImGui(Actor*& selectedActor, ActorComponent*& selectedComponent)
	{
#ifdef USE_IMGUI
		const std::string actorLabel =
			GetName().empty()
			? std::string(typeid(*this).name())
			: GetName();

		const std::string treeLabel = actorLabel + "##ActorHierarchy";

		ImGuiTreeNodeFlags flags =
			ImGuiTreeNodeFlags_OpenOnArrow |
			ImGuiTreeNodeFlags_OpenOnDoubleClick |
			ImGuiTreeNodeFlags_SpanAvailWidth;

		if (selectedActor == this && selectedComponent == nullptr)
		{
			flags |= ImGuiTreeNodeFlags_Selected; // 選択中のActorはハイライト表示する
		}

		const bool opened = ImGui::TreeNodeEx(treeLabel.c_str(), flags);

		if (ImGui::IsItemClicked())
		{
			selectedComponent = nullptr; // Actor自体が選択された場合はComponent選択を解除する
			selectedActor = this;		 // 閉じたTreeNodeをクリックした場合もActorを選択する
		}

		if (opened)
		{
			DrawComponentHierarchyImGui(selectedActor, selectedComponent);
			ImGui::TreePop();
		}
#endif // USE_IMGUI
	}

	void Actor::DrawComponentHierarchyImGui(Actor*& selectedActor, ActorComponent*& selectedComponent)
	{
#ifdef USE_IMGUI
		SceneComponent* root = GetRootComponent();

		if (root)
		{
			ImGui::PushID(root); // RootComponentのポインタを使ってImGui IDの衝突を防ぐ
			root->DrawComponentHierarchyImGui(selectedActor, selectedComponent);
			ImGui::PopID();
		}

		for (auto& component : components_)
		{
			if (!component)
			{
				continue; // nullptrのComponentは無視する
			}

			SceneComponent* sceneComponent = dynamic_cast<SceneComponent*>(component.get());
			if (sceneComponent)
			{
				continue; // SceneComponentはRootComponentの階層描画に含まれるため、ここでは描画しない
			}

			ImGui::PushID(component.get()); // Componentのポインタを使ってImGui IDの衝突を防ぐ

			const std::string label = component->GetName().empty()
				? std::string(typeid(*component).name())
				: component->GetName();

			const std::string treeLabel = label + "##ActorComponentHierarchy";

			ImGuiTreeNodeFlags flags =
				ImGuiTreeNodeFlags_Leaf |
				ImGuiTreeNodeFlags_NoTreePushOnOpen |
				ImGuiTreeNodeFlags_SpanAvailWidth;

			if (selectedComponent == component.get())
			{
				flags |= ImGuiTreeNodeFlags_Selected; // 選択中のComponentはハイライト表示する
			}

			ImGui::TreeNodeEx(treeLabel.c_str(), flags);

			if (ImGui::IsItemClicked())
			{
				selectedActor = nullptr;			 // 選択されたComponentの所有者をDetails表示対象として選択する
				selectedComponent = component.get(); // ActorComponentをDetails表示対象として選択する
			}

			ImGui::PopID();
		}
#endif // USE_IMGUI
	}

	void Actor::DrawInspectorImGui()
	{
#ifdef USE_IMGUI
		ImGui::SeparatorText("Actor");
		ImGui::Text("Component Count : %zu", components_.size());
		ImGui::Text("Root Component : %s", rootComponent_ ? rootComponent_->GetName().c_str() : "None");
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

	void Actor::ToJson(nlohmann::json& outJson) const
	{
		outJson["Name"] = GetName();		// Actor名を保存する
		outJson["Class"] = GetClassTypeName();  // Actorの種類を保存する

		outJson["Components"] = nlohmann::json::array(); // Component一覧を保存するための配列を作成する

		for (const auto& component : components_)
		{
			if (!component)
			{
				continue; // nullptrのComponentは無視する
			}

			nlohmann::json componentJson;
			component->ToJson(componentJson);				// Componentの情報をJSONへ保存する
			outJson["Components"].push_back(componentJson); // Component情報を配列へ追加する
		}
	}

	void Actor::FromJson(const nlohmann::json& inJson)
	{
		if (inJson.contains("Name") && inJson["Name"].is_string())
		{
			SetName(inJson["Name"].get<std::string>()); // JSONに保存されたActor名を復元する。
		}
	}

	void Actor::ClearComponents()
	{
		for (auto& component : components_)
		{
			if (component)
			{
				component->Finalize(); // Component破棄前に明示的な終了処理を流す
			}
		}

		components_.clear();		  // ActorがComponentの寿命を管理するため、ここで破棄する
		rootComponent_ = nullptr;	  // RootComponentをリセットする
		isPhysicsRegistered_ = false; // PhysicsWorldへの登録状態をリセットする
	}

	bool Actor::RemoveComponent(ActorComponent* component)
	{
		if (!component)
		{
			return false; // 無効なComponentは削除できない
		}

		if (component == rootComponent_)
		{
			return false; // RootComponent削除は今回は禁止する
		}

		// SceneComponentの場合は、親子関係を整理してから削除する
		if (SceneComponent* sceneComponent = dynamic_cast<SceneComponent*>(component))
		{
			SceneComponent* parent = sceneComponent->GetParent();

			// 子Component一覧はAttachTo中に変化するのでコピーしてから処理する
			std::vector<SceneComponent*> children = sceneComponent->GetChildren();

			for (SceneComponent* child : children)
			{
				if (!child)
				{
					continue; // nullptrの子Componentは無視する
				}

				if (parent)
				{
					child->AttachTo(parent); // 親がいる場合は、子Componentを親に接続し直す
				}
				else if (rootComponent_ && rootComponent_ != child)
				{
					child->AttachTo(rootComponent_); // 親がいない場合は、子ComponentをRootComponentに接続する
				}
				else
				{
					child->Detach(); // 親もRootComponentもいない場合は、子Componentを切り離す
				}
			}

			sceneComponent->Detach(); // 削除するSceneComponentを親から切り離す
		}

		component->Finalize(); // Component破棄前に明示的な終了処理を流す

		const auto removeIt = std::remove_if(components_.begin(), components_.end(),
			[component](const std::unique_ptr<ActorComponent>& ownedComponent)
			{
				return ownedComponent.get() == component; // 指定されたComponentと一致する場合に削除対象とする
			});

		if (removeIt == components_.end())
		{
			return false; // 指定されたComponentが見つからなかった場合は削除できない
		}

		components_.erase(removeIt, components_.end());
		return true;
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

} // namespace Ken4lowEngine