#include "BossEnemy.h"
#include "BossBehaviorTreeBuilder.h"
#include "BossChaseState.h"
#include "BossRushState.h"
#include "BossSpinState.h"
#include "BossDeadState.h"
#include <CollisionTypeIdDef.h>
#include <LinearInterpolation.h>
#include <LevelObjectManager.h>
#include "WorldCollisionResolver.h"
#include <Player.h>

#include <random>
#include <cmath>

/// -------------------------------------------------------------
///					　		初期化処理
/// -------------------------------------------------------------
void BossEnemy::Initialize()
{
	// ベースキャラクター初期化
	BaseCharacter::Initialize();

	// テクスチャの設定
	BaseCharacter::ApplySkinToAllParts(skinTexturePath_);

	// ID登録
	Collider::SetTypeID(static_cast<uint32_t>(CollisionTypeIdDef::kBoss));
	Collider::SetOwner<BossEnemy>(this); //	オーナー設定
	Collider::SetOBBHalfSize({}); // コライダーサイズ設定（後で調整）

	// GPUパーティクルマネージャー取得
	gpuParticleManager_ = GpuParticleManager::GetInstance();

	// VFX初期化
	vfx_ = std::make_unique<BossEnemyVfx>();
	vfx_->Initialize(gpuParticleManager_, "");

	// HP 初期化
	currentHP_ = maxHP_;

	// ステート初期化（登場からスタート）
	state_ = State::Appear;

	// 登場演出セットアップ
	appear_.timer = 0.0f;
	appear_.duration = 2.0f;
	appear_.finished = false;
	appear_.active = true;

	// シーンで配置した位置を「最終位置」として保存
	appear_.endPosition = body_.transform.translate_;
	appear_.startPosition = appear_.endPosition;
	appear_.startPosition.y -= 6.0f;            // ちょっと下からせり上がってくる感じ
	body_.transform.translate_ = appear_.startPosition;

	// ボス用ビヘイビアツリー生成
	behaviorTree_ = BossBehaviorTreeBuilder::BuildBehaviorTree(BossType::BossA);
}

/// -------------------------------------------------------------
///					　		更新処理
/// -------------------------------------------------------------
void BossEnemy::Update(float deltaTime)
{
	if (state_ == State::Dead)
	{
		attackState_->Update(this, deltaTime);
		return;
	}

	// 移動前の位置を保存
	Vector3 oldPos = body_.transform.translate_;

	// ビヘイビアツリー更新（死亡チェック・登場演出・フェーズ行動）
	if (behaviorTree_) behaviorTree_->Tick(*this, deltaTime);

	// ダメージフラッシュ更新
	UpdateDamageFlash(deltaTime);

	// 移動後に必ずワールドとの衝突を解決
	if (state_ != State::Appear)
	{
		if (levelObjectManager_)
		{
			WorldCollisionSettings s{};
			s.half = { 0.8f, 2.0f, 0.8f };
			s.centerOffset = { 0.0f, 0.0f, 0.0f };

			const auto worldAABBs = levelObjectManager_->GetWorldAABBs();
			auto res = WorldCollisionResolver::Resolve(worldAABBs, s, oldPos, body_.transform.translate_, false);

			body_.transform.translate_ = res.fixedCenter + s.centerOffset;
			Collider::SetCenterPosition(res.fixedCenter);
		}
	}

	if (appear_.finished)
	{
		vfx_->UpdateAura(GetCenterPosition(), 2); // 常時オーラエフェクト更新

		// クールダウン更新
		vfx_->Tick(deltaTime);

		const bool isRush =
			(currentAttackKind_ == AttackKind::kRush) ||
			(currentAttackKind_ == AttackKind::kBackstepRush) ||
			(currentAttackKind_ == AttackKind::kMultiRush);

		const bool isSpin = (currentAttackKind_ == AttackKind::kSpinAttack);

		// 攻撃系VFX更新
		vfx_->UpdateRushTrail(deltaTime, GetCenterPosition(), oldPos, isRush);
		vfx_->UpdateSpinAttack(GetCenterPosition(), isSpin);

		// コライダー半サイズ更新（仮の固定値、後で調整）
		Collider::SetOBBHalfSize({ 0.8f, 2.0f, 0.8f });
	}

	// コライダー中心座標更新
	Collider::SetCenterPosition(GetCenterPosition());

	// ベースキャラクター更新
	BaseCharacter::Update(deltaTime);
}

/// -------------------------------------------------------------
///					　		描画処理
/// -------------------------------------------------------------
void BossEnemy::Draw()
{
	BaseCharacter::Draw();
}

/// -------------------------------------------------------------
///					　		ImGui描画処理
/// -------------------------------------------------------------
void BossEnemy::DrawImGui()
{
#ifdef USE_IMGUI

#endif // USE_IMGUI
}

/// -------------------------------------------------------------
///					　		衝突判定処理
/// -------------------------------------------------------------
void BossEnemy::OnCollision(Collider* other)
{
	if (state_ == State::Dead || death_.active || death_.finished) {
		return; // 死亡中は当たり判定処理しない
	}

	uint32_t serialNumber = other->GetUniqueID(); // 相手のシリアルナンバー取得

	// 弾丸と衝突したときの処理
	if (other->GetTypeID() == static_cast<uint32_t>(CollisionTypeIdDef::kBullet))
	{
		// 接触記録があれば何もせず抜ける
		if (contactRecord_.Check(serialNumber)) return;

		// 接触記録に登録
		contactRecord_.Add(serialNumber);

		// ダメージフラッシュ開始
		flashInfo_.timer = flashInfo_.duration;

		// 弾丸と衝突したときの処理
		OutputDebugStringA("Boss hit by bullet!\n");

		// 仮のダメージ処理（武器ごとに後で調整）
		const float damage = player_->GetWeaponManager()->GetCurrentConfig().damage;
		currentHP_ -= damage;
		if (currentHP_ < 0.0f)
		{
			currentHP_ = 0.0f;
		}
	}

	// ----- プレイヤーとの衝突（プレイヤーがダメージを受ける） ----- //
	if (other->GetTypeID() == static_cast<uint32_t>(CollisionTypeIdDef::kPlayer))
	{
		// 例: Rush / Spin 攻撃中のみ有効
		if (currentAttackKind_ == AttackKind::kRush ||
			currentAttackKind_ == AttackKind::kSpinAttack)
		{
			if (Player* player = other->GetOwner<Player>())
			{
				// ダメージ
				player->TakeDamage(attackPower_);

				// --- ノックバック方向と強さを決める ---

				// ボス中心 → プレイヤー中心 方向（押し飛ばす方向）
				Vector3 bossCenter = GetCenterPosition();
				Vector3 playerCenter = player->GetCenterPosition();
				Vector3 toPlayer{
					playerCenter.x - bossCenter.x,
					playerCenter.y - bossCenter.y,
					playerCenter.z - bossCenter.z
				};

				// 攻撃種類ごとにノックバックの強さを変える
				float horizontalPow = 0.0f;
				float upPow = 0.0f;

				switch (currentAttackKind_)
				{
				case AttackKind::kRush:
					// 突進は強めに後ろへ & ちょい浮く
					horizontalPow = 0.6f;   // 水平方向の押し飛ばし
					upPow = 0.20f;  // 少し浮かす
					break;

				case AttackKind::kSpinAttack:
					// スピンはちょっと吹き飛ばすくらい
					horizontalPow = 0.35f;
					upPow = 0.15f;
					break;

				default:
					break;
				}

				if (horizontalPow > 0.0f || upPow > 0.0f)
				{
					// 実際にノックバックを適用
					player->ApplyDamageImpulse(toPlayer, horizontalPow, upPow);
				}

				if (currentAttackKind_ == AttackKind::kRush)
				{
					if (vfx_) vfx_->UpdateRushHit(player->GetCenterPosition(), GetCenterPosition());
				}
			}
		}
	}
}

/// -------------------------------------------------------------
///					　		中心座標取得
/// -------------------------------------------------------------
Vector3 BossEnemy::GetCenterPosition() const
{
	const Vector3 offset = { 0.0f,0.0f,0.0f };
	return body_.transform.translate_ + offset;
}

/// -------------------------------------------------------------
///					ダメージフラッシュ更新
/// -------------------------------------------------------------
void BossEnemy::UpdateDamageFlash(float deltaTime)
{
	//  フラッシュ演出
	if (flashInfo_.timer > 0.0f)
	{
		flashInfo_.timer -= deltaTime;
		if (flashInfo_.timer < 0.0f) flashInfo_.timer = 0.0f;

		float t = 1.0f - std::clamp(flashInfo_.timer / flashInfo_.duration, 0.0f, 1.0f); // 0→1
		Vector4 c = {
			Lerp(flashInfo_.hitColor.x,  flashInfo_.baseColor.x,  t),
			Lerp(flashInfo_.hitColor.y,  flashInfo_.baseColor.y,  t),
			Lerp(flashInfo_.hitColor.z,  flashInfo_.baseColor.z,  t),
			Lerp(flashInfo_.hitColor.w,  flashInfo_.baseColor.w,  t),
		};
		ApplyColorToAll(c);
	}
	else
	{
		// 念のため最後に元の色に戻す
		ApplyColorToAll(flashInfo_.baseColor);
	}
}

/// -------------------------------------------------------------
///					全部位に色を適用
/// -------------------------------------------------------------
void BossEnemy::ApplyColorToAll(const Vector4& color)
{
	// 全パーツに色を適用
	flashInfo_.colorModulate = color;

	// 本体
	if (body_.object) { body_.object->SetColor(flashInfo_.colorModulate); }

	// 部位群
	for (auto& part : parts_) {
		if (part.object) { part.object->SetColor(flashInfo_.colorModulate); }
	}
}

/// -------------------------------------------------------------
///					移動方向に体の向きを合わせる
/// -------------------------------------------------------------
void BossEnemy::UpdateFacingDirection(const Vector3& moveDir, float deltaTime)
{
	// ほぼ動いていないなら回転しない
	float lenSq = moveDir.x * moveDir.x + moveDir.z * moveDir.z;
	if (lenSq < 0.0001f) {
		return;
	}

	// XZ 平面の向きからヨー角を計算
	// ※前方向が +Z のモデルなら atan2(x, z) でOK
	float targetYaw = std::atan2f(-moveDir.x, moveDir.z);

	float currentYaw = body_.transform.rotate_.y;

	// 一番近い回転方向になるように差分を -π～+π に正規化
	const float pi = std::numbers::pi_v<float>;
	float diff = targetYaw - currentYaw;
	while (diff > pi) diff -= 2.0f * pi;
	while (diff < -pi) diff += 2.0f * pi;

	// 1フレームあたりの最大回転量（ラジアン）
	const float rotateSpeed = 10.0f; // 素早めに向きを変える
	float maxStep = rotateSpeed * deltaTime;

	// 回転量をクランプ
	if (diff > maxStep) diff = maxStep;
	if (diff < -maxStep) diff = -maxStep;

	body_.transform.rotate_.y = currentYaw + diff;
}

/// -------------------------------------------------------------
///					次の攻撃種類を選択
/// -------------------------------------------------------------
BossEnemy::AttackKind BossEnemy::DecideNextAttackKind()
{
	if (!player_) return AttackKind::kNone;
	if (attackCooldown_ > 0.0f) return AttackKind::kNone;

	const auto& turning = GetTurning();

	// ChaseState と同じ閾値を参照
	const float rushDistance = turning.chase.rushDistance;
	const float rushFarTime = turning.chase.rushFarTime;

	// 「Spinを出していい距離」：ひとまず hitRadius を流用（※後で分離推奨）
	const float spinRange = turning.spin.hitRadius;

	// 距離（XZ）
	const Vector3 bossPos = body_.transform.translate_;
	const Vector3 playerPos = player_->GetCenterPosition();
	const Vector3 toPlayer{ playerPos.x - bossPos.x, 0.0f, playerPos.z - bossPos.z };

	const float distSq = toPlayer.x * toPlayer.x + toPlayer.z * toPlayer.z;
	const float spinRangeSq = spinRange * spinRange;
	const float rushDistanceSq = rushDistance * rushDistance;

	// 近距離なら回転攻撃を優先
	if (distSq <= spinRangeSq) return AttackKind::kSpinAttack;

	// 遠距離かつ、遠い状態が一定時間続いていたら突進
	if (distSq > rushDistanceSq && farFromPlayerTimer_ >= rushFarTime)
		return AttackKind::kRush;

	return AttackKind::kNone;
}


////// --------------------------------(BTから呼ばれる関数群)-------------------------------- //////


/// -------------------------------------------------------------
///					HP 0 か？（死亡判定用）
/// -------------------------------------------------------------
bool BossEnemy::IsDead() const
{
	return currentHP_ <= 0.0f; // HP 0以下かどうか
}

/// -------------------------------------------------------------
///			死亡ステートへの遷移要求（BT から呼ばれる）
/// -------------------------------------------------------------
void BossEnemy::RequestDeadState()
{
	if (state_ == State::Dead) return;

	state_ = State::Dead;
	ChangeAttackState(AttackKind::kDead); // ここでDeadState生成→OnEnterが走る
}

/// -------------------------------------------------------------
///					登場演出が終わったか？
/// -------------------------------------------------------------
bool BossEnemy::IsAppearFinished() const
{
	return appear_.finished; // 登場演出が完了したか
}

/// -------------------------------------------------------------
///			登場演出の更新（BT のアクションから呼ばれる）
/// -------------------------------------------------------------
void BossEnemy::UpdateAppear(float deltaTime)
{
	// すでに登場演出が完了しているなら何もしない
	if (appear_.finished) return;

	// 経過時間を進める
	appear_.timer += deltaTime;

	// 0.0 ～ 1.0 の正規化時間
	float t = 0.0f;
	if (appear_.duration > 0.0f)
	{
		t = appear_.timer / appear_.duration;
	}

	if (t < 0.0f) t = 0.0f;
	if (t > 1.0f) t = 1.0f;

	// 簡単なイージング（smoothstep）
	const float eased = t * t * (3.0f - 2.0f * t);

	// 下からせり上がってくるように補間
	body_.transform.translate_.x =
		appear_.startPosition.x + (appear_.endPosition.x - appear_.startPosition.x) * eased;
	body_.transform.translate_.y =
		appear_.startPosition.y + (appear_.endPosition.y - appear_.startPosition.y) * eased;
	body_.transform.translate_.z =
		appear_.startPosition.z + (appear_.endPosition.z - appear_.startPosition.z) * eased;

	// 砂埃VFX更新
	if (vfx_) vfx_->UpdateAppearDust({ 0.0f,0.0f,0.0f }, 5);

	// 登場演出完了判定
	if (appear_.timer >= appear_.duration)
	{
		appear_.finished = true; // 登場演出完了
		appear_.active = false;  // 登場演出非アクティブ化
		state_ = State::Battle;  // 戦闘状態に遷移

		// 最終位置をきっちり合わせておく
		body_.transform.translate_ = appear_.endPosition;
	}
}

/// -------------------------------------------------------------
///					　		HP割合取得
/// -------------------------------------------------------------
float BossEnemy::GetHPRate() const
{
	// HP割合計算
	if (maxHP_ <= 0.0f) return 0.0f;

	// 正常な割合を返す
	return currentHP_ / maxHP_;
}

/// -------------------------------------------------------------
///						攻撃状態変更
/// -------------------------------------------------------------
void BossEnemy::ChangeAttackState(AttackKind nextKind)
{
	// 同じステートなら何もしない
	if (attackState_ && currentAttackKind_ == nextKind) return;

	// 既存ステートの終了処理
	if (attackState_) attackState_->OnExit(this);

	currentAttackKind_ = nextKind;

	// 新しいステートを生成
	switch (nextKind)
	{
	case AttackKind::kNone:
	default:
		// 待機・追尾状態
		attackState_ = std::make_unique<BossChaseState>();
		break;

	case AttackKind::kRush:
		attackState_ = std::make_unique<BossRushState>();
		break;

	case AttackKind::kSpinAttack:
		attackState_ = std::make_unique<BossSpinState>();
		break;

	case AttackKind::kDead:
		attackState_ = std::make_unique<BossDeadState>();
		break;
	}

	// ステート開始処理
	if (attackState_) attackState_->OnEnter(this);
}

/// -------------------------------------------------------------
///					通常フェーズ更新（Phase1）
/// -------------------------------------------------------------
BehaviorStatus BossEnemy::UpdateNormalPhase(float deltaTime)
{
	if (IsDead()) return BehaviorStatus::Failure;

	// ステートが無ければ追尾から開始
	if (!attackState_)
	{
		ChangeAttackState(AttackKind::kNone);
	}

	BehaviorStatus status = attackState_->Update(this, deltaTime);

	// 追尾（kNone）のときは、「攻撃に入るべきか？」を毎フレームチェック
	if (currentAttackKind_ == AttackKind::kNone)
	{
		AttackKind next = DecideNextAttackKind();
		if (next != AttackKind::kNone)
		{
			ChangeAttackState(next);
		}
	}
	else
	{
		// 攻撃ステート中は、Success が返ってきたら終了とみなして追尾に戻す
		if (status == BehaviorStatus::Success)
		{
			ChangeAttackState(AttackKind::kNone);
		}
	}

	return BehaviorStatus::Running;
}

/// -------------------------------------------------------------
///					激怒フェーズ更新（Phase2）
/// -------------------------------------------------------------
BehaviorStatus BossEnemy::UpdateRagePhase(float deltaTime)
{
	// TODO: 激怒フェーズの行動（弾幕・レーザー・召喚など）をここに書く
	return UpdateNormalPhase(deltaTime);
}

