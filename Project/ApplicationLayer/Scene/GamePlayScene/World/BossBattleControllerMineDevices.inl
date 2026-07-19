void BossBattleController::BuildMineDevices(const Dependencies& deps)
{
	mineDevices_.clear();
	if (deps.stage && deps.stage->GetLevelData())
	{
		for (const K4E::ObjectData& object : deps.stage->GetLevelData()->objects)
		{
			if (object.type != "DeviceObjective" && object.type != "DevicePoint") continue;
			MineDeviceRuntime device{};
			device.name = object.name;
			device.position = object.position;
			if (object.hasDeviceObjectiveProps)
			{
				if (!object.deviceObjectiveProps.uiName.empty()) device.name = object.deviceObjectiveProps.uiName;
				device.activateTime = object.deviceObjectiveProps.activateTime > 0.0f ? std::max(0.25f, object.deviceObjectiveProps.activateTime) : 1.5f;
			}
			mineDevices_.push_back(std::move(device));
		}
	}

	if (mineDevices_.empty())
	{
		const IPlayerRuntime* player = deps.characters ? deps.characters->GetPlayerRuntime() : nullptr;
		const K4E::Vector3 base = player ? player->GetWorldPosition() : K4E::Vector3{};
		// 旧JSONにDeviceObjectiveが無い場合も探索フローを検証できる仮配置を用意する。
		for (int i = 0; i < mineRequiredDeviceCount_; ++i)
		{
			MineDeviceRuntime device{};
			device.name = "Mine Device " + std::to_string(i + 1);
			device.position = base + K4E::Vector3{ (static_cast<float>(i) - 1.0f) * 12.0f, 0.0f, 18.0f + static_cast<float>(i) * 10.0f };
			device.activateTime = 1.5f;
			mineDevices_.push_back(std::move(device));
		}
	}

	for (MineDeviceRuntime& device : mineDevices_)
	{
		device.visual = std::make_unique<K4E::Object3D>();
		device.visual->Initialize(kMineBlockModelPath);
		device.visual->SetScale({ 1.25f, 2.1f, 1.25f });
		device.visual->SetTranslate(device.position + K4E::Vector3{ 0.0f, 1.05f, 0.0f });
		device.visual->SetColor({ 0.10f, 0.72f, 0.92f, 1.0f });
		device.visual->SetEmissiveFactor({ 0.05f, 0.75f, 1.0f, 1.0f });
		device.visual->SetMetallic(0.45f);
		device.visual->SetRoughness(0.28f);
		device.visual->SetFrustumCullingEnabled(false);
		device.visual->SetIgnoreStageChunkCulling(true);
		device.visual->Update();
	}
}

void BossBattleController::UpdateMineDevices(const Dependencies& deps, float deltaTime)
{
	mineFocusedDeviceIndex_ = -1;
	IPlayerRuntime* player = deps.characters ? deps.characters->GetPlayerRuntime() : nullptr;
	if (!player) return;

	const K4E::Vector3 playerPosition = player->GetWorldPosition();
	float nearestDistanceSq = 3.25f * 3.25f;
	for (size_t i = 0; i < mineDevices_.size(); ++i)
	{
		const MineDeviceRuntime& device = mineDevices_[i];
		if (device.activated) continue;
		const K4E::Vector3 delta = device.position - playerPosition;
		const float distanceSq = delta.x * delta.x + delta.y * delta.y + delta.z * delta.z;
		if (distanceSq <= nearestDistanceSq)
		{
			nearestDistanceSq = distanceSq;
			mineFocusedDeviceIndex_ = static_cast<int>(i);
		}
	}

	K4E::Input* input = K4E::Input::GetInstance();
	const bool interactHeld = input && (input->PushKey(DIK_E) || input->PushButton(K4E::XButtons.A));
	for (size_t i = 0; i < mineDevices_.size(); ++i)
	{
		MineDeviceRuntime& device = mineDevices_[i];
		device.pulseTimer += deltaTime;
		const bool focused = mineFocusedDeviceIndex_ == static_cast<int>(i);
		if (!device.activated)
		{
			if (focused && interactHeld)
			{
				device.activationProgress += deltaTime;
			}
			else
			{
				device.activationProgress = std::max(0.0f, device.activationProgress - deltaTime * 0.7f);
			}
			if (device.activationProgress >= device.activateTime)
			{
				device.activationProgress = device.activateTime;
				device.activated = true;
				++mineActivatedDeviceCount_;
				K4E::Log("[MineArena] Search device activated.\n");
			}
		}

		if (!device.visual) continue;
		const float pulse = 0.5f + std::sin(device.pulseTimer * 3.5f) * 0.5f;
		if (device.activated)
		{
			device.visual->SetColor({ 0.20f, 0.95f, 0.48f, 1.0f });
			device.visual->SetEmissiveFactor({ 0.05f, 1.0f, 0.34f, 1.0f });
		}
		else if (focused)
		{
			const float progress = std::clamp(device.activationProgress / std::max(0.01f, device.activateTime), 0.0f, 1.0f);
			device.visual->SetColor({ 0.20f + progress * 0.25f, 0.78f, 1.0f, 1.0f });
			device.visual->SetEmissiveFactor({ 0.15f + progress * 0.55f, 0.85f, 1.0f, 1.0f });
		}
		else
		{
			device.visual->SetColor({ 0.08f, 0.48f + pulse * 0.16f, 0.78f + pulse * 0.16f, 1.0f });
			device.visual->SetEmissiveFactor({ 0.02f, 0.38f + pulse * 0.22f, 0.68f + pulse * 0.25f, 1.0f });
		}
		if (deps.shadowLightViewProjection) device.visual->UpdateShadowMatrix(*deps.shadowLightViewProjection);
		device.visual->Update();
	}

	minePassageUnlocked_ = mineActivatedDeviceCount_ >= mineRequiredDeviceCount_;
}

void BossBattleController::UpdateMineArena(const Dependencies& deps, float deltaTime)
{
	if (!mineArenaEnabled_ || !mineArenaInitialized_) return;
	UpdateMineDevices(deps, deltaTime);
	const float passageTarget = minePassageUnlocked_ ? 1.0f : 0.0f;
	minePassageDoorOpenAmount_ = Approach(minePassageDoorOpenAmount_, passageTarget, 0.48f, deltaTime);
	UpdateMineMovableBlock(minePassageDoorLeftIndex_, minePassageDoorOpenAmount_);
	UpdateMineMovableBlock(minePassageDoorRightIndex_, minePassageDoorOpenAmount_);

	IPlayerRuntime* player = deps.characters ? deps.characters->GetPlayerRuntime() : nullptr;
	if (!mineArenaEntered_ && minePassageDoorOpenAmount_ >= 0.92f && player && IsPlayerInsideMineArena(*player))
	{
		mineArenaEntered_ = true;
		StartCameraShake(0.55f, 0.24f, 18.0f);
		K4E::Log("[MineArena] Player entered dome; entrance gate closing.\n");
	}
	const float gateTarget = mineArenaEntered_ ? 0.0f : 1.0f;
	mineArenaGateOpenAmount_ = Approach(mineArenaGateOpenAmount_, gateTarget, 0.70f, deltaTime);
	UpdateMineMovableBlock(mineArenaGateLeftIndex_, mineArenaGateOpenAmount_);
	UpdateMineMovableBlock(mineArenaGateRightIndex_, mineArenaGateOpenAmount_);

	UpdateMineLighting();
	for (MineArenaBlock& block : mineArenaBlocks_)
	{
		if (!block.visual) continue;
		if (deps.shadowLightViewProjection) block.visual->UpdateShadowMatrix(*deps.shadowLightViewProjection);
		block.visual->Update();
	}
}

void BossBattleController::DrawMineArena()
{
	if (!mineArenaEnabled_ || !mineArenaInitialized_) return;
	for (MineDeviceRuntime& device : mineDevices_)
	{
		if (device.visual) device.visual->Draw();
	}
	for (MineArenaBlock& block : mineArenaBlocks_)
	{
		if (block.visual) block.visual->Draw();
	}
}

void BossBattleController::DrawMineArenaShadow()
{
	if (!mineArenaEnabled_ || !mineArenaInitialized_) return;
	for (MineDeviceRuntime& device : mineDevices_)
	{
		if (device.visual) device.visual->DrawShadow();
	}
	for (MineArenaBlock& block : mineArenaBlocks_)
	{
		if (block.visual) block.visual->DrawShadow();
	}
}
