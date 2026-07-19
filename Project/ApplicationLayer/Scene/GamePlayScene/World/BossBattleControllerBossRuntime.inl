void BossBattleController::SpawnBossActor(const Dependencies& deps, bool enableBattleImmediately)
{
	if (bossSpawned_ || !deps.characters) return;
	RegisterApplicationActorTypes();
	K4E::ActorWorld& world = deps.characters->GetActorWorld();
	K4E::BossActor* actor = nullptr;
	if (K4E::Actor* prefabActor = world.SpawnActorFromJson(kBossPrefabPath))
	{
		actor = dynamic_cast<K4E::BossActor*>(prefabActor);
		if (!actor) world.DestroyActor(prefabActor);
	}
	if (!actor) actor = &world.SpawnActor<K4E::BossActor>();
	else actor->Initialize();

	bossActor_ = actor;
	bossActor_->SetName("GameplayBossActor");
	bossActor_->SetLayer("Boss");
	bossActor_->AddTag("Boss");
	bossActor_->AddTag("GameplayBoss");
	bossActor_->SetTargetActor(deps.characters->GetPlayer());
	if (stage1BeginnerBalanceEnabled_)
	{
		if (auto* health = bossActor_->GetHealthComponent()) health->ResetHealth(kBeginnerBossMaxHp);
	}
	bossDeathPositionCaptured_ = false;
	bossDeathPosition_ = bossSpawnPosition_;
	bossActor_->SetPosition(enableBattleImmediately ? bossSpawnPosition_ : bossIntroController_.GetBossStartPosition());
	bossActor_->SetYaw(kPi);
	bossActor_->SetBattleEnabled(enableBattleImmediately);
	bossActor_->SetHealthHudVisible(enableBattleImmediately);
	bossActor_->ForceSyncWorldTransform();
	bossSpawned_ = true;
	lastPresentedPhaseRevision_ = bossActor_->GetPhaseRevision();
	if (enableBattleImmediately) RegisterBossCollider(deps);
}

void BossBattleController::RegisterBossCollider(const Dependencies& deps)
{
	if (!bossActor_ || bossColliderRegistered_) return;
	bossActor_->ClearRootParentKeepingWorldPosition();
	bossActor_->SetPosition(bossIntroController_.GetBossAppearPosition());
	bossActor_->SetYaw(kPi);
	bossActor_->SetBattleEnabled(true);
	bossActor_->SetHealthHudVisible(true);
	bossActor_->ForceSyncWorldTransform();
	if (deps.collisionManager && bossActor_->GetCollisionPrimitive()) deps.collisionManager->AddCollider(bossActor_->GetCollisionPrimitive());
	bossColliderRegistered_ = true;
	K4E::Log("[BossActor] Legacy query collider registered.\n");
}

void BossBattleController::DestroyBossActor(const Dependencies& deps)
{
	if (!bossActor_) return;
	if (deps.collisionManager && bossColliderRegistered_ && bossActor_->GetCollisionPrimitive()) deps.collisionManager->RemoveCollider(bossActor_->GetCollisionPrimitive());
	bossActor_->SetBattleEnabled(false);
	bossActor_->SetHealthHudVisible(false);
	bossActor_->SetActive(false);
	if (deps.characters) deps.characters->GetActorWorld().DestroyActor(bossActor_);
	bossActor_ = nullptr;
	bossColliderRegistered_ = false;
}

void BossBattleController::AlignPlayerViewToBossAfterIntro(IPlayerRuntime& player) const
{
	K4E::Camera* camera = player.GetCamera();
	if (!camera) return;
	float pitch = 0.0f;
	float yaw = 0.0f;
	if (CalcLookAnglesToTarget(camera->GetTranslate(), bossIntroController_.GetBossLookTarget(), pitch, yaw)) player.SetViewLookAngles(pitch, yaw);
	K4E::CameraManager::GetInstance()->SetMainCamera(camera);
	camera->Update();
}

void BossBattleController::UpdateBossClearProgress(const Dependencies& deps, float deltaTime)
{
	if (bossActor_ && bossActor_->IsDead() && !bossDefeated_)
	{
		bossDefeated_ = true;
		if (!bossDeathPositionCaptured_)
		{
			bossDeathPosition_ = bossActor_->GetDeathWorldPosition();
			bossDeathPositionCaptured_ = true;
		}
		if (deps.setBossDefeated) deps.setBossDefeated(false);
	}
	if (bossDefeated_ && bossActor_ && bossActor_->IsDeathPresentationComplete() && !clearItemSpawned_ && bossDeathPositionCaptured_) SpawnClearItem(deps, bossDeathPosition_);
	if (clearItem_ && !clearItemCollected_)
	{
		clearItem_->Update(deltaTime);
		IPlayerRuntime* player = deps.characters ? deps.characters->GetPlayerRuntime() : nullptr;
		if (player && clearItem_->CheckPickup(*player)) CollectClearItem(deps);
	}
}

void BossBattleController::SpawnClearItem(const Dependencies& deps, const K4E::Vector3& deathPosition)
{
	if (clearItemSpawned_) return;
	K4E::Vector3 position = deathPosition;
	if (deps.stage)
	{
		float floorY = -std::numeric_limits<float>::infinity();
		float nearest = std::numeric_limits<float>::infinity();
		for (const K4E::AABB& floor : deps.stage->GetFloorAABBs())
		{
			if (position.x < floor.min.x || position.x > floor.max.x || position.z < floor.min.z || position.z > floor.max.z) continue;
			const float distance = std::abs(floor.max.y - position.y);
			if (distance < nearest){ nearest = distance; floorY = floor.max.y; }
		}
		if (std::isfinite(floorY)) position.y = floorY;
	}
	position.y = std::max(position.y, 0.0f);
	clearItem_ = std::make_unique<BossClearItem>();
	clearItem_->Initialize(position);
	if (deps.collisionManager) deps.collisionManager->AddCollider(clearItem_.get());
	clearItemSpawned_ = true;
	K4E::Log("[GameClear] BossClearItem spawned at BossActor death position.\n");
}

void BossBattleController::CollectClearItem(const Dependencies& deps)
{
	if (clearItemCollected_ || isGameClear_) return;
	clearItemCollected_ = true;
	isGameClear_ = true;
	if (clearItem_)
	{
		clearItem_->MarkCollected();
		if (deps.collisionManager) deps.collisionManager->RemoveCollider(clearItem_.get());
	}
	if (deps.setBossDefeated) deps.setBossDefeated(true);
}

void BossBattleController::HandleBossPhasePresentation(const Dependencies& deps)
{
	(void)deps;
	if (!bossActor_) return;
	const unsigned int revision = bossActor_->GetPhaseRevision();
	if (revision == lastPresentedPhaseRevision_) return;
	lastPresentedPhaseRevision_ = revision;
	const int phase = bossActor_->GetCurrentPhase();
	if (phase >= 3) StartCameraShake(0.85f, 0.40f, 25.0f);
	else if (phase >= 2) StartCameraShake(0.55f, 0.24f, 20.0f);
}

void BossBattleController::StartCameraShake(float duration, float amplitude, float frequency)
{
	if (duration <= 0.0f || amplitude <= 0.0f) return;
	cameraShakeDuration_ = cameraShakeTimer_ = duration;
	cameraShakeAmplitude_ = amplitude;
	cameraShakeFrequency_ = std::max(1.0f, frequency);
	cameraShakeSeed_ += 2.31f;
}

void BossBattleController::UpdateCameraShake(float deltaTime, IPlayerRuntime* player)
{
	if (cameraShakeTimer_ <= 0.0f) return;
	cameraShakeTimer_ = std::max(0.0f, cameraShakeTimer_ - deltaTime);
	K4E::Camera* camera = player ? player->GetCamera() : nullptr;
	if (!camera) return;
	camera->SetTranslate(camera->GetTranslate() + BuildCameraShakeOffset());
	camera->Update();
}

K4E::Vector3 BossBattleController::BuildCameraShakeOffset() const
{
	if (cameraShakeTimer_ <= 0.0f || cameraShakeDuration_ <= 0.0f) return {};
	const float rate = std::clamp(cameraShakeTimer_ / cameraShakeDuration_, 0.0f, 1.0f);
	const float t = (cameraShakeDuration_ - cameraShakeTimer_) * cameraShakeFrequency_ + cameraShakeSeed_;
	const float amplitude = cameraShakeAmplitude_ * rate * rate;
	return { std::sin(t * 1.51f) * amplitude, std::cos(t * 1.13f) * amplitude * 0.50f, std::sin(t * 0.83f) * amplitude * 0.30f };
}

bool BossBattleController::CalcLookAnglesToTarget(const K4E::Vector3& from, const K4E::Vector3& target, float& pitch, float& yaw)
{
	K4E::Vector3 direction = target - from;
	if (K4E::Vector3::LengthSquared(direction) <= 0.000001f) return false;
	direction = K4E::Vector3::Normalize(direction);
	yaw = std::atan2(-direction.x, direction.z);
	pitch = std::atan2(-direction.y, std::sqrt(direction.x * direction.x + direction.z * direction.z));
	return true;
}
