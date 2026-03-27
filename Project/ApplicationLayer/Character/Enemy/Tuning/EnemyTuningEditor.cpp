#include "EnemyTuningEditor.h"

#include "EnemyTuningRepository.h"
#include "imgui.h"
#include <cstring>
#include <string>

void EnemyTuningEditor::Initialize()
{
	if (initialized_)
	{
		return;
	}

	EnemyTuningRepository::Initialize();
	LoadFromRepository();
	initialized_ = true;
}

void EnemyTuningEditor::EnsureInitialized()
{
	if (!initialized_)
	{
		Initialize();
	}
}

void EnemyTuningEditor::LoadFromRepository()
{
	workingCopy_ = EnemyTuningRepository::Get(selectedArchetype_);
	dirty_ = false;
	std::snprintf(
		statusText_,
		sizeof(statusText_),
		"Loaded: %s",
		EnemyTuningRepository::ToString(selectedArchetype_)
	);
}

void EnemyTuningEditor::SaveToRepository()
{
	EnemyTuningRepository::GetMutable(selectedArchetype_) = workingCopy_;
	dirty_ = true;
}

void EnemyTuningEditor::Draw(const EnemyTuningEditorHooks& hooks)
{
#ifdef USE_IMGUI
	EnsureInitialized();

	if (!ImGui::Begin("Enemy Tuning Editor"))
	{
		ImGui::End();
		return;
	}

	DrawArchetypeSelector();
	DrawToolbar(hooks);
	ImGui::Separator();
	DrawTuningFields();

	ImGui::Separator();
	ImGui::TextUnformatted(statusText_);

	ImGui::End();
#endif // USE_IMGUI
}

void EnemyTuningEditor::DrawArchetypeSelector()
{
#ifdef USE_IMGUI

	const char* preview = EnemyTuningRepository::ToString(selectedArchetype_);

	if (ImGui::BeginCombo("Archetype", preview))
	{
		for (EnemyArchetype type : EnemyTuningRepository::GetAllArchetypes())
		{
			const bool isSelected = (type == selectedArchetype_);
			if (ImGui::Selectable(EnemyTuningRepository::ToString(type), isSelected))
			{
				selectedArchetype_ = type;
				LoadFromRepository();
			}

			if (isSelected)
			{
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}

	ImGui::SameLine();
	const bool exists = EnemyTuningRepository::ExistsJson(selectedArchetype_);
	ImGui::Text("%s", exists ? "[JSON: YES]" : "[JSON: NO]");
#endif // USE_IMGUI
}

void EnemyTuningEditor::DrawToolbar(const EnemyTuningEditorHooks& hooks)
{
#ifdef USE_IMGUI
	// --------------------------------------------------------
	// Create JSON
	// --------------------------------------------------------
	// 今の workingCopy を repository に反映して、
	// そのまま新規 JSON として保存する
	if (ImGui::Button("Create JSON"))
	{
		SaveToRepository();

		if (EnemyTuningRepository::SaveOne(selectedArchetype_))
		{
			dirty_ = false;
			std::snprintf(
				statusText_,
				sizeof(statusText_),
				"Created JSON: %s",
				EnemyTuningRepository::ToString(selectedArchetype_)
			);

			if (hooks.onSaved)
			{
				hooks.onSaved(selectedArchetype_);
			}
		}
		else
		{
			std::snprintf(
				statusText_,
				sizeof(statusText_),
				"Create failed: %s",
				EnemyTuningRepository::ToString(selectedArchetype_)
			);
		}
	}

	ImGui::SameLine();

	// --------------------------------------------------------
	// Save
	// --------------------------------------------------------
	// 今の workingCopy を repository に反映して JSON 保存
	if (ImGui::Button("Save"))
	{
		SaveToRepository();

		if (EnemyTuningRepository::SaveOne(selectedArchetype_))
		{
			dirty_ = false;
			std::snprintf(
				statusText_,
				sizeof(statusText_),
				"Saved: %s",
				EnemyTuningRepository::ToString(selectedArchetype_)
			);

			if (hooks.onSaved)
			{
				hooks.onSaved(selectedArchetype_);
			}
		}
		else
		{
			std::snprintf(
				statusText_,
				sizeof(statusText_),
				"Save failed: %s",
				EnemyTuningRepository::ToString(selectedArchetype_)
			);
		}
	}

	ImGui::SameLine();

	// --------------------------------------------------------
	// Load
	// --------------------------------------------------------
	// JSON を 1 体分だけ読み込み直して workingCopy に反映
	if (ImGui::Button("Load"))
	{
		const std::string path = EnemyTuningRepository::GetDefaultJsonPath(selectedArchetype_);

		if (EnemyTuningRepository::LoadOneFromJson(selectedArchetype_, path))
		{
			LoadFromRepository();

			std::snprintf(
				statusText_,
				sizeof(statusText_),
				"Loaded JSON: %s",
				EnemyTuningRepository::ToString(selectedArchetype_)
			);
		}
		else
		{
			std::snprintf(
				statusText_,
				sizeof(statusText_),
				"Load failed: %s",
				EnemyTuningRepository::ToString(selectedArchetype_)
			);
		}
	}

	ImGui::SameLine();

	// --------------------------------------------------------
	// Reload All
	// --------------------------------------------------------
	// 全 enemy tuning を再読込
	if (ImGui::Button("Reload All"))
	{
		EnemyTuningRepository::Reload();
		LoadFromRepository();

		std::snprintf(
			statusText_,
			sizeof(statusText_),
			"Reloaded all enemy tunings"
		);

		if (hooks.onReloaded)
		{
			hooks.onReloaded();
		}
	}

	ImGui::SameLine();

	// --------------------------------------------------------
	// Delete JSON
	// --------------------------------------------------------
	// JSON ファイルだけ消して、repository は既定値ベースで再構築する
	if (ImGui::Button("Delete JSON"))
	{
		if (EnemyTuningRepository::DeleteOne(selectedArchetype_))
		{
			EnemyTuningRepository::Reload();
			LoadFromRepository();

			std::snprintf(
				statusText_,
				sizeof(statusText_),
				"Deleted JSON: %s",
				EnemyTuningRepository::ToString(selectedArchetype_)
			);

			if (hooks.onDeleted)
			{
				hooks.onDeleted(selectedArchetype_);
			}
		}
		else
		{
			std::snprintf(
				statusText_,
				sizeof(statusText_),
				"Delete failed: %s",
				EnemyTuningRepository::ToString(selectedArchetype_)
			);
		}
	}

	ImGui::SameLine();
	ImGui::Text("%s", dirty_ ? "[Modified]" : "[Saved]");
#endif // USE_IMGUI
}

void EnemyTuningEditor::DrawTuningFields()
{
#ifdef USE_IMGUI

	bool changed = false;

	changed |= ImGui::DragFloat("moveSpeed", &workingCopy_.moveSpeed, 0.05f, 0.0f, 100.0f);
	changed |= ImGui::DragFloat("attackRange", &workingCopy_.attackRange, 0.1f, 0.0f, 500.0f);
	changed |= ImGui::DragFloat("viewRange", &workingCopy_.viewRange, 0.1f, 0.0f, 500.0f);

	ImGui::SeparatorText("Fire");
	changed |= ImGui::DragFloat("fireInterval", &workingCopy_.fireInterval, 0.01f, 0.01f, 10.0f);
	changed |= ImGui::DragInt("burstMin", &workingCopy_.burstMin, 1.0f, 1, 100);
	changed |= ImGui::DragInt("burstMax", &workingCopy_.burstMax, 1.0f, 1, 100);

	ImGui::SeparatorText("GunAI");
	changed |= ImGui::DragFloat("preferredMinRatio", &workingCopy_.preferredMinRatio, 0.01f, 0.0f, 2.0f);
	changed |= ImGui::DragFloat("preferredMaxRatio", &workingCopy_.preferredMaxRatio, 0.01f, 0.0f, 2.0f);
	changed |= ImGui::DragFloat("strafeSpeedMul", &workingCopy_.strafeSpeedMul, 0.01f, 0.0f, 5.0f);
	changed |= ImGui::DragFloat("aimMoveMul", &workingCopy_.aimMoveMul, 0.01f, 0.0f, 5.0f);
	changed |= ImGui::DragFloat("burstMoveMul", &workingCopy_.burstMoveMul, 0.01f, 0.0f, 5.0f);

	ImGui::SeparatorText("Spread");
	changed |= ImGui::DragFloat("spreadNearDeg", &workingCopy_.spreadNearDeg, 0.01f, 0.0f, 45.0f);
	changed |= ImGui::DragFloat("spreadFarDeg", &workingCopy_.spreadFarDeg, 0.01f, 0.0f, 45.0f);

	ImGui::SeparatorText("Reaction");
	changed |= ImGui::DragFloat("reactionDelaySec", &workingCopy_.reactionDelaySec, 0.01f, 0.0f, 10.0f);

	ImGui::SeparatorText("Bullet");
	changed |= ImGui::DragFloat("bulletSpeed", &workingCopy_.bulletSpeed, 0.1f, 0.0f, 1000.0f);
	changed |= ImGui::DragFloat("bulletLifeSec", &workingCopy_.bulletLifeSec, 0.01f, 0.0f, 30.0f);
	changed |= ImGui::DragInt("bulletDamage", &workingCopy_.bulletDamage, 1.0f, 0, 999);

	ImGui::SeparatorText("Durability");
	changed |= ImGui::DragInt("maxHp", &workingCopy_.maxHp, 1.0f, 1, 9999);

	if (changed)
	{
		dirty_ = true;
	}
#endif // USE_IMGUI
}