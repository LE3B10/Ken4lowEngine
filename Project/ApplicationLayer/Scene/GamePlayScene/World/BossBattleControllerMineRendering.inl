size_t BossBattleController::AddMineArenaBlock(
	const Dependencies& deps,
	const K4E::Vector3& position,
	const K4E::Vector3& scale,
	const K4E::Vector3& rotation,
	const K4E::Vector4& color,
	bool collisionEnabled,
	const char* debugName)
{
	MineArenaBlock block{};
	block.closedPosition = position;
	block.openPosition = position;
	block.visual = std::make_unique<K4E::Object3D>();
	block.visual->Initialize(kMineBlockModelPath);
	block.visual->SetScale(scale);
	block.visual->SetRotate(rotation);
	block.visual->SetTranslate(position);
	block.visual->SetColor(color);
	block.visual->SetMetallic(0.05f);
	block.visual->SetRoughness(0.88f);
	block.visual->SetFrustumCullingEnabled(false);
	block.visual->SetIgnoreStageChunkCulling(true);
	block.visual->Update();

	if (collisionEnabled)
	{
		block.collider = std::make_unique<K4E::Collider>();
		ApplyCollisionPreset(*block.collider, ECollisionPresetId::WorldStatic);
		block.collider->SetCenterPosition(position);
		block.collider->SetOBBHalfSize(scale * 0.5f);
		block.collider->SetOrientation(rotation);
		block.collider->SetDebugName(debugName ? debugName : "MineArenaBlock");
		if (deps.characters) deps.characters->GetPhysicsWorld().RegisterCollider(block.collider.get());
		if (deps.collisionManager) deps.collisionManager->AddCollider(block.collider.get());
	}
	mineArenaBlocks_.push_back(std::move(block));
	return mineArenaBlocks_.size() - 1;
}

void BossBattleController::SetMineMovableBlock(size_t blockIndex, const K4E::Vector3& closedPosition, const K4E::Vector3& openPosition)
{
	if (blockIndex >= mineArenaBlocks_.size()) return;
	mineArenaBlocks_[blockIndex].closedPosition = closedPosition;
	mineArenaBlocks_[blockIndex].openPosition = openPosition;
}

void BossBattleController::UpdateMineMovableBlock(size_t blockIndex, float openAmount)
{
	if (blockIndex >= mineArenaBlocks_.size()) return;
	MineArenaBlock& block = mineArenaBlocks_[blockIndex];
	const K4E::Vector3 position = LerpVector(block.closedPosition, block.openPosition, std::clamp(openAmount, 0.0f, 1.0f));
	if (block.visual) block.visual->SetTranslate(position);
	if (block.collider) block.collider->SetCenterPosition(position);
}

bool BossBattleController::IsPlayerInsideMineArena(const IPlayerRuntime& player) const
{
	const K4E::Vector3 position = player.GetWorldPosition();
	const K4E::Vector3 delta = position - mineArenaCenter_;
	return std::abs(delta.x) <= 17.5f && delta.z >= -18.0f && delta.z <= 19.5f && position.y >= -2.0f && position.y <= 12.0f;
}

void BossBattleController::ApplyMineLighting()
{
	if (!mineArenaEnabled_ || mineLightingSaved_) return;
	K4E::LightManager* lightManager = K4E::LightManager::GetInstance();
	auto& lights = lightManager->GetMutablePunctualLightsForEditor();
	auto& settings = lightManager->GetMutableLightingSettingsForEditor();
	savedMineLights_ = lights;
	savedMineLightingSettings_ = settings;
	mineLightingSaved_ = true;

	// ステージ全体の既存ライトは維持し、隠し坑道用の局所ライトだけを追加する。
	settings = savedMineLightingSettings_;

	auto addPoint = [&lights](const K4E::Vector3& position, const K4E::Vector4& color, float intensity, float radius)
	{
		K4E::LightManager::PunctualLightGPU light{};
		light.lightType = 2;
		light.color = color;
		light.intensity = intensity;
		light.position = position;
		light.radius = radius;
		light.decay = 2.0f;
		light.direction = { 0.0f, -1.0f, 0.0f };
		light.distance = radius;
		light.cosFalloffStart = 0.86f;
		light.cosAngle = 0.72f;
		light.areaSize = { 0.4f, 0.4f, 0.0f };
		light.enabled = 1;
		lights.push_back(light);
		return lights.size() - 1;
	};
	auto addSpot = [&lights](const K4E::Vector3& position, const K4E::Vector3& direction, const K4E::Vector4& color, float intensity, float distance)
	{
		K4E::LightManager::PunctualLightGPU light{};
		light.lightType = 3;
		light.color = color;
		light.intensity = intensity;
		light.position = position;
		light.radius = distance;
		light.decay = 1.5f;
		light.direction = direction;
		light.distance = distance;
		light.cosFalloffStart = 0.82f;
		light.cosAngle = 0.52f;
		light.areaSize = { 1.0f, 1.0f, 0.0f };
		light.enabled = 1;
		lights.push_back(light);
		return lights.size() - 1;
	};

	mineCorridorLightIndices_.push_back(addPoint({ mineArenaCenter_.x, 6.4f, minePassageDoorCenter_.z + 7.0f }, { 1.0f, 0.58f, 0.25f, 1.0f }, 0.7f, 13.0f));
	mineCorridorLightIndices_.push_back(addPoint({ mineArenaCenter_.x, 6.4f, minePassageDoorCenter_.z + 19.0f }, { 1.0f, 0.52f, 0.20f, 1.0f }, 0.7f, 13.0f));
	mineCorridorLightIndices_.push_back(addPoint({ mineArenaCenter_.x, 6.4f, minePassageDoorCenter_.z + 31.0f }, { 0.72f, 0.82f, 1.0f, 1.0f }, 0.7f, 14.0f));

	mineArenaLightIndices_.push_back(addSpot(
		{ mineArenaCenter_.x, 16.5f, mineArenaCenter_.z - 2.0f },
		{ 0.0f, -1.0f, 0.08f },
		{ 0.58f, 0.72f, 1.0f, 1.0f },
		1.4f,
		32.0f));
	for (int i = 0; i < 4; ++i)
	{
		const float angle = kTwoPi * static_cast<float>(i) / 4.0f + kPi * 0.25f;
		mineArenaLightIndices_.push_back(addPoint(
			{ mineArenaCenter_.x + std::sin(angle) * 16.5f, 5.0f, mineArenaCenter_.z + std::cos(angle) * 16.5f },
			{ 1.0f, 0.36f, 0.16f, 1.0f },
			0.8f,
			18.0f));
	}
}

void BossBattleController::UpdateMineLighting()
{
	if (!mineArenaEnabled_ || !mineLightingSaved_) return;
	K4E::LightManager* lightManager = K4E::LightManager::GetInstance();
	auto& lights = lightManager->GetMutablePunctualLightsForEditor();
	auto& settings = lightManager->GetMutableLightingSettingsForEditor();

	settings = savedMineLightingSettings_;
	settings.ambientColor.x = std::max(settings.ambientColor.x, 0.075f);
	settings.ambientColor.y = std::max(settings.ambientColor.y, 0.080f);
	settings.ambientColor.z = std::max(settings.ambientColor.z, 0.090f);
	settings.exposure = std::max(settings.exposure, mineArenaEntered_ ? 1.02f : 0.95f);
	settings.diffuseStrength = std::max(settings.diffuseStrength, 0.95f);
	if (minePassageUnlocked_)
	{
		settings.fogColor = { 0.075f, 0.082f, 0.095f, 1.0f };
		settings.fogStart = std::max(28.0f, savedMineLightingSettings_.fogStart);
		settings.fogEnd = std::max(mineArenaEntered_ ? 150.0f : 125.0f, savedMineLightingSettings_.fogEnd);
		settings.enableFog = savedMineLightingSettings_.enableFog;
	}

	const float corridorIntensity = minePassageUnlocked_ ? (2.8f + minePassageDoorOpenAmount_ * 3.4f) : 0.0f;
	for (size_t index : mineCorridorLightIndices_)
	{
		if (index < lights.size()) lights[index].intensity = corridorIntensity;
	}
	const float arenaFactor = mineArenaEntered_ ? 1.0f : (minePassageUnlocked_ ? 0.28f : 0.0f);
	for (size_t i = 0; i < mineArenaLightIndices_.size(); ++i)
	{
		const size_t index = mineArenaLightIndices_[i];
		if (index >= lights.size()) continue;
		lights[index].intensity = (i == 0 ? 11.0f : 6.8f) * arenaFactor;
	}
}

void BossBattleController::RestoreMineLighting()
{
	if (!mineLightingSaved_) return;
	K4E::LightManager* lightManager = K4E::LightManager::GetInstance();
	lightManager->GetMutablePunctualLightsForEditor() = savedMineLights_;
	lightManager->GetMutableLightingSettingsForEditor() = savedMineLightingSettings_;
	savedMineLights_.clear();
	mineLightingSaved_ = false;
}

K4E::Vector3 BossBattleController::LerpVector(const K4E::Vector3& a, const K4E::Vector3& b, float t)
{
	return a + (b - a) * std::clamp(t, 0.0f, 1.0f);
}
