void BossBattleController::EnsureMineArenaInitialized(const Dependencies& deps)
{
	if (!mineArenaEnabled_ || mineArenaInitialized_) return;
	if (deps.crystalManager) deps.crystalManager->Finalize(); // ステージ2では破壊用クリスタルを生成物ごと撤去する。
	BuildMinePassageAndArena(deps);
	ApplyMineLighting();
	mineArenaInitialized_ = true;
	K4E::Log("[MineArena] Hidden passage and dome arena initialized.\n");
}

void BossBattleController::FinalizeMineArena(const Dependencies& deps)
{
	if (!mineArenaEnabled_) return;
	for (MineArenaBlock& block : mineArenaBlocks_)
	{
		if (!block.collider) continue;
		if (deps.characters) deps.characters->GetPhysicsWorld().UnregisterCollider(block.collider.get());
		if (deps.collisionManager) deps.collisionManager->RemoveCollider(block.collider.get());
	}
	mineArenaBlocks_.clear();
	mineDevices_.clear();
	mineCorridorLightIndices_.clear();
	mineArenaLightIndices_.clear();
	RestoreMineLighting();
	mineArenaInitialized_ = false;
}

void BossBattleController::BuildMinePassageAndArena(const Dependencies& deps)
{
	const K4E::Vector4 rock{ 0.115f, 0.105f, 0.095f, 1.0f };
	const K4E::Vector4 rockLight{ 0.17f, 0.15f, 0.13f, 1.0f };
	const K4E::Vector4 floorColor{ 0.14f, 0.125f, 0.11f, 1.0f };
	const K4E::Vector4 metal{ 0.19f, 0.205f, 0.215f, 1.0f };

	BuildMineDevices(deps);

	const float corridorX = mineArenaCenter_.x;
	const float corridorCenterZ = (minePassageDoorCenter_.z + mineArenaGateCenter_.z) * 0.5f;
	const float corridorLength = mineArenaGateCenter_.z - minePassageDoorCenter_.z + 4.0f;
	AddMineArenaBlock(deps, { corridorX, -0.5f, corridorCenterZ }, { 13.0f, 1.0f, corridorLength }, {}, floorColor, true, "MineHiddenPassageFloor");
	AddMineArenaBlock(deps, { corridorX - 6.4f, 4.2f, corridorCenterZ }, { 1.4f, 9.5f, corridorLength }, {}, rockLight, true, "MineHiddenPassageWallLeft");
	AddMineArenaBlock(deps, { corridorX + 6.4f, 4.2f, corridorCenterZ }, { 1.4f, 9.5f, corridorLength }, {}, rockLight, true, "MineHiddenPassageWallRight");
	AddMineArenaBlock(deps, { corridorX, 8.7f, corridorCenterZ }, { 13.0f, 1.2f, corridorLength }, {}, rock, true, "MineHiddenPassageCeiling");

	AddMineArenaBlock(deps, { corridorX - 9.0f, 4.2f, minePassageDoorCenter_.z }, { 7.2f, 9.5f, 3.0f }, {}, rockLight, true, "MineSecretDoorFrameLeft");
	AddMineArenaBlock(deps, { corridorX + 9.0f, 4.2f, minePassageDoorCenter_.z }, { 7.2f, 9.5f, 3.0f }, {}, rockLight, true, "MineSecretDoorFrameRight");
	AddMineArenaBlock(deps, { corridorX, 8.3f, minePassageDoorCenter_.z }, { 18.0f, 2.0f, 3.0f }, {}, rock, true, "MineSecretDoorFrameTop");

	minePassageDoorLeftIndex_ = AddMineArenaBlock(
		deps, { corridorX - 2.65f, minePassageDoorCenter_.y, minePassageDoorCenter_.z }, { 5.5f, 7.2f, 1.6f }, {}, metal, true, "MineSecretDoorLeft");
	minePassageDoorRightIndex_ = AddMineArenaBlock(
		deps, { corridorX + 2.65f, minePassageDoorCenter_.y, minePassageDoorCenter_.z }, { 5.5f, 7.2f, 1.6f }, {}, metal, true, "MineSecretDoorRight");
	SetMineMovableBlock(minePassageDoorLeftIndex_, { corridorX - 2.65f, minePassageDoorCenter_.y, minePassageDoorCenter_.z }, { corridorX - 7.4f, minePassageDoorCenter_.y, minePassageDoorCenter_.z });
	SetMineMovableBlock(minePassageDoorRightIndex_, { corridorX + 2.65f, minePassageDoorCenter_.y, minePassageDoorCenter_.z }, { corridorX + 7.4f, minePassageDoorCenter_.y, minePassageDoorCenter_.z });

	AddMineArenaBlock(deps, { mineArenaCenter_.x, -0.5f, mineArenaCenter_.z }, { 50.0f, 1.0f, 50.0f }, {}, floorColor, true, "MineBossArenaFloor");

	constexpr int lowerSegmentCount = 24;
	const float lowerRadius = 23.5f;
	const float lowerWidth = (kTwoPi * lowerRadius / static_cast<float>(lowerSegmentCount)) * 1.16f;
	for (int i = 0; i < lowerSegmentCount; ++i)
	{
		const float angle = kTwoPi * static_cast<float>(i) / static_cast<float>(lowerSegmentCount);
		const bool entranceOpening = std::abs(std::cos(angle) + 1.0f) < 0.10f && std::abs(std::sin(angle)) < 0.42f;
		if (entranceOpening) continue;
		const K4E::Vector3 position{
			mineArenaCenter_.x + std::sin(angle) * lowerRadius,
			5.0f,
			mineArenaCenter_.z + std::cos(angle) * lowerRadius
		};
		AddMineArenaBlock(deps, position, { lowerWidth, 11.0f, 3.0f }, { 0.0f, angle, 0.0f }, rockLight, true, "MineBossArenaLowerWall");
	}

	constexpr int upperSegmentCount = 20;
	const float upperRadius = 18.6f;
	const float upperWidth = (kTwoPi * upperRadius / static_cast<float>(upperSegmentCount)) * 1.18f;
	for (int i = 0; i < upperSegmentCount; ++i)
	{
		const float angle = kTwoPi * static_cast<float>(i) / static_cast<float>(upperSegmentCount);
		const K4E::Vector3 position{
			mineArenaCenter_.x + std::sin(angle) * upperRadius,
			13.0f,
			mineArenaCenter_.z + std::cos(angle) * upperRadius
		};
		AddMineArenaBlock(deps, position, { upperWidth, 6.5f, 3.2f }, { 0.0f, angle, 0.0f }, rock, true, "MineBossArenaUpperDome");
	}
	AddMineArenaBlock(deps, { mineArenaCenter_.x, 16.9f, mineArenaCenter_.z }, { 38.0f, 2.4f, 38.0f }, {}, rock, true, "MineBossArenaDomeCap");

	AddMineArenaBlock(deps, { corridorX - 9.0f, 4.2f, mineArenaGateCenter_.z }, { 7.2f, 9.5f, 3.0f }, {}, rockLight, true, "MineArenaGateFrameLeft");
	AddMineArenaBlock(deps, { corridorX + 9.0f, 4.2f, mineArenaGateCenter_.z }, { 7.2f, 9.5f, 3.0f }, {}, rockLight, true, "MineArenaGateFrameRight");
	AddMineArenaBlock(deps, { corridorX, 8.3f, mineArenaGateCenter_.z }, { 18.0f, 2.0f, 3.0f }, {}, rock, true, "MineArenaGateFrameTop");

	mineArenaGateLeftIndex_ = AddMineArenaBlock(
		deps, { corridorX - 7.4f, mineArenaGateCenter_.y, mineArenaGateCenter_.z }, { 5.5f, 7.2f, 1.6f }, {}, metal, true, "MineArenaGateLeft");
	mineArenaGateRightIndex_ = AddMineArenaBlock(
		deps, { corridorX + 7.4f, mineArenaGateCenter_.y, mineArenaGateCenter_.z }, { 5.5f, 7.2f, 1.6f }, {}, metal, true, "MineArenaGateRight");
	SetMineMovableBlock(mineArenaGateLeftIndex_, { corridorX - 2.65f, mineArenaGateCenter_.y, mineArenaGateCenter_.z }, { corridorX - 7.4f, mineArenaGateCenter_.y, mineArenaGateCenter_.z });
	SetMineMovableBlock(mineArenaGateRightIndex_, { corridorX + 2.65f, mineArenaGateCenter_.y, mineArenaGateCenter_.z }, { corridorX + 7.4f, mineArenaGateCenter_.y, mineArenaGateCenter_.z });
	UpdateMineMovableBlock(mineArenaGateLeftIndex_, 1.0f);
	UpdateMineMovableBlock(mineArenaGateRightIndex_, 1.0f);

	for (int i = 0; i < 8; ++i)
	{
		const float angle = kTwoPi * static_cast<float>(i) / 8.0f;
		const K4E::Vector3 position{
			mineArenaCenter_.x + std::sin(angle) * 19.0f,
			2.0f,
			mineArenaCenter_.z + std::cos(angle) * 19.0f
		};
		AddMineArenaBlock(deps, position, { 2.2f, 5.0f, 2.2f }, { 0.0f, angle, 0.0f }, metal, true, "MineBossArenaSupport");
	}
}
