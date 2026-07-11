#define NOMINMAX
#include "ActorWorldEditorBridge.h"
#include "EditorCommandHistory.h"

#include <SceneComponent.h>
#include <json.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

namespace Ken4lowEngine
{
	namespace
	{
		void ExecuteActiveCommand(const EditorObjectInfo& object, bool active)
		{
			bool before = active;
			if (!object.ReadActive(before) || before == active) return;
			EditorCommandHistory::GetInstance()->Execute(std::make_unique<EditorValueCommand<bool>>(
				"有効状態変更", before, active,
				[object](const bool& value)
				{
					object.WriteActive(value);
					EditorContext::GetInstance()->MarkLevelDirty();
				}));
		}

		void ExecuteRenameCommand(const EditorObjectInfo& object, std::string_view name)
		{
			if (name.empty() || object.displayName == name) return;
			EditorCommandHistory::GetInstance()->Execute(std::make_unique<EditorValueCommand<std::string>>(
				"名前変更", object.displayName, std::string(name),
				[object](const std::string& value)
				{
					object.Rename(value);
					EditorContext::GetInstance()->MarkLevelDirty();
				}));
		}
	}

	bool BuildInstancedModelInstanceEditorInfo(InstancedModelComponent* component, size_t instanceIndex, uint64_t componentId, std::string_view sceneName, EditorObjectInfo& outInfo)
	{
		if (!component || instanceIndex >= component->GetEditableInstanceCount()) return false;

		const std::string instanceKey = std::to_string(componentId) + "/Instance/" + std::to_string(instanceIndex);
		outInfo = {};
		outInfo.id = MakeStableEditorObjectId(instanceKey);
		outInfo.parentId = componentId;
		outInfo.sortOrder = static_cast<int>(std::min<size_t>(instanceIndex, static_cast<size_t>(std::numeric_limits<int>::max())));
		outInfo.displayName = "Instance " + std::to_string(instanceIndex);
		outInfo.typeName = "InstancedModelInstance";
		outInfo.sceneName = std::string(sceneName);
		outInfo.icon = "[I]";
		outInfo.objectKind = EditorObjectKind::Instance;
		outInfo.canEditTransform = true;
		outInfo.inspectorType = EditorInspectorType::Transform;
		outInfo.inspectorHint = "GPU Instancing内の個別Transform";
		outInfo.readTransform = [component, instanceIndex](EditorTransform& outTransform)
			{
				InstancedModelComponent::InstanceTransform transform{};
				if (!component->GetInstanceLocalTransform(instanceIndex, transform)) return false;
				outTransform.position = transform.position;
				outTransform.rotation = transform.rotation;
				outTransform.scale = transform.scale;
				return true;
			};
		outInfo.writeTransform = [component, instanceIndex](const EditorTransform& transform)
			{
				InstancedModelComponent::InstanceTransform instanceTransform{};
				if (!component->GetInstanceLocalTransform(instanceIndex, instanceTransform)) return;
				instanceTransform.position = transform.position;
				instanceTransform.rotation = transform.rotation;
				instanceTransform.scale = transform.scale;
				component->SetInstanceLocalTransform(instanceIndex, instanceTransform);
			};
		outInfo.readWorldTransform = [component, instanceIndex](EditorTransform& outTransform)
			{
				InstancedModelComponent::InstanceTransform transform{};
				if (!component->GetInstanceWorldTransform(instanceIndex, transform)) return false;
				outTransform.position = transform.position;
				outTransform.rotation = transform.rotation;
				outTransform.scale = transform.scale;
				return true;
			};
		outInfo.writeWorldTransform = [component, instanceIndex](const EditorTransform& transform)
			{
				InstancedModelComponent::InstanceTransform instanceTransform{};
				if (!component->GetInstanceWorldTransform(instanceIndex, instanceTransform)) return;
				instanceTransform.position = transform.position;
				instanceTransform.rotation = transform.rotation;
				instanceTransform.scale = transform.scale;
				component->SetInstanceWorldTransform(instanceIndex, instanceTransform);
			};
		outInfo.canCaptureState = true;
		outInfo.captureState = [component, instanceIndex]()
			{
				InstancedModelComponent::InstanceTransform transform{};
				if (!component->GetInstanceLocalTransform(instanceIndex, transform)) return std::string{};
				nlohmann::json state = {
					{ "Position", { transform.position.x, transform.position.y, transform.position.z } },
					{ "Rotation", { transform.rotation.x, transform.rotation.y, transform.rotation.z } },
					{ "Scale", { transform.scale.x, transform.scale.y, transform.scale.z } },
				};
				return state.dump();
			};
		outInfo.restoreState = [component, instanceIndex](std::string_view stateText)
			{
				const nlohmann::json state = nlohmann::json::parse(stateText, nullptr, false);
				if (state.is_discarded()) return;
				InstancedModelComponent::InstanceTransform transform{};
				if (!component->GetInstanceLocalTransform(instanceIndex, transform)) return;
				const auto position = state.value("Position", std::vector<float>{});
				const auto rotation = state.value("Rotation", std::vector<float>{});
				const auto scale = state.value("Scale", std::vector<float>{});
				if (position.size() == 3) transform.position = { position[0], position[1], position[2] };
				if (rotation.size() == 3) transform.rotation = { rotation[0], rotation[1], rotation[2] };
				if (scale.size() == 3) transform.scale = { scale[0], scale[1], scale[2] };
				component->SetInstanceLocalTransform(instanceIndex, transform); // 個別InstanceのUndoはGPU Buffer更新までComponentへ任せる。
			};
		outInfo.canDrawObjectId = false;
		return true;
	}

	void CollectActorWorldEditorObjects(ActorWorld& actorWorld, std::vector<EditorObjectInfo>& outObjects, std::string_view sceneName)
	{
		const std::string sceneNameText(sceneName);
		int actorSortOrder = 0;

		for (const auto& actorOwner : actorWorld.GetActors())
		{
			Actor* actor = actorOwner.get();
			if (!actor || actor->IsPendingDestroy()) continue;

			const std::string actorKey = sceneNameText + "/Actor/" + std::to_string(static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(actor)));
			const uint64_t actorId = MakeStableEditorObjectId(actorKey);

			EditorObjectInfo actorInfo{};
			actorInfo.id = actorId;
			actorInfo.sortOrder = actorSortOrder++;
			actorInfo.displayName = actor->GetName().empty() ? actor->GetClassTypeName() : actor->GetName();
			actorInfo.typeName = actor->GetClassTypeName();
			actorInfo.sceneName = sceneNameText;
			actorInfo.icon = "[A]";
			actorInfo.objectKind = EditorObjectKind::Actor;
			actorInfo.canToggleActive = true;
			actorInfo.readActive = [actor]() { return actor->IsActive(); };
			actorInfo.writeActive = [actor](bool active) { actor->SetActive(active); };
			actorInfo.canRename = true;
			actorInfo.rename = [actor](std::string_view name) { actor->SetName(name); };
			actorInfo.inspectorHint = "Actor / Component Details";
			actorInfo.canCaptureState = true;
			actorInfo.captureState = [actor]()
				{
					nlohmann::json state;
					actor->ToJson(state);
					state.erase("Components"); // Actor Inspectorの履歴ではComponent構成を作り直さずActor共通値だけ保持する。
					return state.dump();
				};
			actorInfo.restoreState = [actor](std::string_view stateText)
				{
					const nlohmann::json state = nlohmann::json::parse(stateText, nullptr, false);
					if (!state.is_discarded()) actor->FromJson(state);
				};

			if (SceneComponent* root = actor->GetRootComponent())
			{
				actorInfo.canEditTransform = true;
				actorInfo.inspectorType = EditorInspectorType::Transform;
				actorInfo.readTransform = [root](EditorTransform& outTransform)
					{
						outTransform.position = root->GetLocalPosition();
						outTransform.rotation = root->GetLocalRotation();
						outTransform.scale = root->GetLocalScale();
						return true;
					};
				actorInfo.writeTransform = [root](const EditorTransform& transform)
					{
						root->SetLocalPosition(transform.position);
						root->SetLocalRotation(transform.rotation);
						root->SetLocalScale(transform.scale);
						root->RefreshWorldTransform();
					};
				actorInfo.readWorldTransform = [root](EditorTransform& outTransform)
					{
						outTransform.position = root->GetWorldPosition();
						outTransform.rotation = root->GetWorldRotation();
						outTransform.scale = root->GetWorldScale();
						return true;
					};
				actorInfo.writeWorldTransform = actorInfo.writeTransform;
			}

			actorInfo.drawInspector = [&actorWorld, actor]()
				{
					actorWorld.SetSelectedEditorObject(actor, nullptr);
					actorWorld.DrawSelectedInspectorContent();
				};

			EditorObjectInfo actorCommandProxy = actorInfo;
			actorInfo.writeActive = [actorCommandProxy](bool active) { ExecuteActiveCommand(actorCommandProxy, active); };
			actorInfo.rename = [actorCommandProxy](std::string_view name) { ExecuteRenameCommand(actorCommandProxy, name); };
			outObjects.push_back(std::move(actorInfo));

			std::unordered_map<const ActorComponent*, uint64_t> componentIds;
			int componentSortOrder = 0;
			for (const auto& componentOwner : actor->GetComponents())
			{
				ActorComponent* component = componentOwner.get();
				if (!component) continue;
				const std::string componentKey = actorKey + "/Component/" + std::to_string(static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(component)));
				componentIds.emplace(component, MakeStableEditorObjectId(componentKey));
			}

			for (const auto& componentOwner : actor->GetComponents())
			{
				ActorComponent* component = componentOwner.get();
				if (!component) continue;

				EditorObjectInfo componentInfo{};
				componentInfo.id = componentIds.at(component);
				componentInfo.parentId = actorId;
				componentInfo.sortOrder = componentSortOrder++;
				componentInfo.displayName = component->GetName().empty() ? component->GetClassTypeName() : component->GetName();
				componentInfo.typeName = component->GetClassTypeName();
				componentInfo.sceneName = sceneNameText;
				componentInfo.icon = "[C]";
				componentInfo.objectKind = EditorObjectKind::Component;
				componentInfo.isRootComponent = component == actor->GetRootComponent();
				componentInfo.canToggleActive = true;
				componentInfo.readActive = [component]() { return component->IsActive(); };
				componentInfo.writeActive = [component](bool active) { component->SetActive(active); };
				componentInfo.canRename = true;
				componentInfo.rename = [component](std::string_view name) { component->SetName(name); };
				componentInfo.inspectorHint = componentInfo.isRootComponent ? "Root Component" : "Actor Component";
				componentInfo.canCaptureState = true;
				componentInfo.captureState = [component]()
					{
						nlohmann::json state;
						component->ToJson(state);
						return state.dump();
					};
				componentInfo.restoreState = [component](std::string_view stateText)
					{
						const nlohmann::json state = nlohmann::json::parse(stateText, nullptr, false);
						if (!state.is_discarded()) component->FromJson(state);
					};
				componentInfo.canDrawObjectId = actor->IsActive() && component->IsActiveInHierarchy() && component->SupportsEditorObjectId();
				componentInfo.drawObjectId = [component](uint32_t objectId) { component->DrawEditorObjectId(objectId); };

				if (auto* sceneComponent = dynamic_cast<SceneComponent*>(component))
				{
					if (SceneComponent* parent = sceneComponent->GetParent())
					{
						const auto parentId = componentIds.find(parent);
						if (parentId != componentIds.end()) componentInfo.parentId = parentId->second;
					}
					componentInfo.canEditTransform = true;
					componentInfo.inspectorType = EditorInspectorType::Transform;
					componentInfo.readTransform = [sceneComponent](EditorTransform& outTransform)
						{
							outTransform.position = sceneComponent->GetLocalPosition();
							outTransform.rotation = sceneComponent->GetLocalRotation();
							outTransform.scale = sceneComponent->GetLocalScale();
							return true;
						};
					componentInfo.writeTransform = [sceneComponent](const EditorTransform& transform)
						{
							sceneComponent->SetLocalPosition(transform.position);
							sceneComponent->SetLocalRotation(transform.rotation);
							sceneComponent->SetLocalScale(transform.scale);
							sceneComponent->RefreshWorldTransform();
						};
					componentInfo.readWorldTransform = [sceneComponent](EditorTransform& outTransform)
						{
							outTransform.position = sceneComponent->GetWorldPosition();
							outTransform.rotation = sceneComponent->GetWorldRotation();
							outTransform.scale = sceneComponent->GetWorldScale();
							return true;
						};
					componentInfo.writeWorldTransform = [sceneComponent](const EditorTransform& worldTransform)
						{
							EditorTransform localTransform = worldTransform;
							if (const SceneComponent* parent = sceneComponent->GetParent())
							{
								const Vector3& parentPosition = parent->GetWorldPosition();
								const Vector3& parentRotation = parent->GetWorldRotation();
								const Vector3& parentScale = parent->GetWorldScale();
								localTransform.position = worldTransform.position - parentPosition;
								localTransform.rotation = worldTransform.rotation - parentRotation;
								localTransform.scale = {
									std::abs(parentScale.x) > 0.0001f ? worldTransform.scale.x / parentScale.x : worldTransform.scale.x,
									std::abs(parentScale.y) > 0.0001f ? worldTransform.scale.y / parentScale.y : worldTransform.scale.y,
									std::abs(parentScale.z) > 0.0001f ? worldTransform.scale.z / parentScale.z : worldTransform.scale.z,
								};
							}
							sceneComponent->SetLocalPosition(localTransform.position);
							sceneComponent->SetLocalRotation(localTransform.rotation);
							sceneComponent->SetLocalScale(localTransform.scale);
							sceneComponent->RefreshWorldTransform();
						};
				}

				componentInfo.drawInspector = [&actorWorld, actor, component]()
					{
						actorWorld.SetSelectedEditorObject(actor, component);
						actorWorld.DrawSelectedInspectorContent();
					};

				EditorObjectInfo componentCommandProxy = componentInfo;
				componentInfo.writeActive = [componentCommandProxy](bool active) { ExecuteActiveCommand(componentCommandProxy, active); };
				componentInfo.rename = [componentCommandProxy](std::string_view name) { ExecuteRenameCommand(componentCommandProxy, name); };

				InstancedModelComponent* instancedComponent = dynamic_cast<InstancedModelComponent*>(component);
				if (instancedComponent && componentInfo.canDrawObjectId)
				{
					const size_t instanceCount = instancedComponent->GetEditableInstanceCount();
					componentInfo.objectIdSpan = static_cast<uint32_t>(std::min<size_t>(instanceCount, std::numeric_limits<uint32_t>::max()));
					const uint64_t componentId = componentInfo.id;
					componentInfo.buildObjectIdEntry = [instancedComponent, componentId, sceneNameText](uint32_t offset, EditorObjectInfo& outInfo)
						{
							return BuildInstancedModelInstanceEditorInfo(instancedComponent, offset, componentId, sceneNameText, outInfo);
						};
				}

				const uint64_t componentId = componentInfo.id;
				outObjects.push_back(std::move(componentInfo));

				if (instancedComponent)
				{
					constexpr size_t kOutlinerInstanceLimit = 512;
					const size_t instanceCount = instancedComponent->GetEditableInstanceCount();
					const size_t visibleCount = std::min(instanceCount, kOutlinerInstanceLimit);
					for (size_t instanceIndex = 0; instanceIndex < visibleCount; ++instanceIndex)
					{
						EditorObjectInfo instanceInfo{};
						if (BuildInstancedModelInstanceEditorInfo(instancedComponent, instanceIndex, componentId, sceneNameText, instanceInfo)) outObjects.push_back(std::move(instanceInfo));
					}

					const EditorSelection& selection = EditorContext::GetInstance()->GetSelection();
					if (instanceCount > visibleCount && selection.HasSelection())
					{
						const EditorObjectInfo& selected = selection.GetSelected();
						if (selected.objectKind == EditorObjectKind::Instance && selected.parentId == componentId && selected.sortOrder >= 0)
						{
							const size_t selectedIndex = static_cast<size_t>(selected.sortOrder);
							if (selectedIndex >= visibleCount && selectedIndex < instanceCount)
							{
								EditorObjectInfo selectedInfo{};
								if (BuildInstancedModelInstanceEditorInfo(instancedComponent, selectedIndex, componentId, sceneNameText, selectedInfo)) outObjects.push_back(std::move(selectedInfo));
							}
						}
					}
				}
			}
		}
	}
} // namespace Ken4lowEngine
