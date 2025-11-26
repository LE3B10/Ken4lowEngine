#include "BossEnemy.h"
#include "BossBehaviorTreeBuilder.h"
#include "BossChaseState.h"  
#include "BossRushState.h"   
#include "BossSpinState.h"   
#include <CollisionTypeIdDef.h>
#include <LinearInterpolation.h>
#include <LevelObjectManager.h>
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

	// ボス登場用パーティクルエミッター生成
	CreateAppearEmitter();
	CreateAuraEmitter(); // 常時オーラ用パーティクルエミッター生成
	CreateRushTrailEmitter(); // ラッシュ軌跡用パーティクルエミッター生成
	CreateRushHitEmitter(); // ラッシュ軌跡エフェクト
	CreateSpinAttackEmitter(); // スピン攻撃エフェクト生成
	CreateDeathEffectEmitters(); // 死亡用エフェクト
	CreateDeathSoulEmitter();
	CreateDebrisDustEmitter();

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
	// すでに死亡演出中なら、ほかの行動はしない
	if (state_ == State::Dead && death_.active)
	{
		// 死亡アニメだけ更新
		UpdateDeath(deltaTime);

		// バラバラ演出が始まる瞬間に爆発＋衝撃波を出す
		EmitDeathEffects();

		// 死亡開始時の一回だけ：破片の余韻（埃）
		if (!death_.startBurstDone)
		{
			Vector3 c = GetCenterPosition();

			if (debrisDustEmitter_)
			{
				debrisDustEmitter_->SetPosition(c);
				debrisDustEmitter_->RequestEmit(48); // ここが埃の量（30〜80で調整）
			}

			// ここに「小さな爆発 + 衝撃波リング」も同タイミングで足せる
			// 例：deathExplosionEmitter_->RequestEmit(32)、deathShockwaveEmitter_->RequestEmit(1)

			death_.startBurstDone = true;
		}

		// 死亡中しばらく：魂がふわっと上に昇る
		if (deathSoulEmitter_ && death_.timer < 1.6f)
		{
			Vector3 c = GetCenterPosition();
			c.y += 1.2f; // 胸〜頭あたりから出したい
			deathSoulEmitter_->SetPosition(c);
			deathSoulEmitter_->RequestEmit(2); // 毎フレ2〜4くらいが雰囲気出る
		}

		// コライダー位置だけは同期しておく（床とのめり込み防止など）
		Collider::SetCenterPosition(GetCenterPosition());
		BaseCharacter::Update(deltaTime);
		return;
	}

	// コライダーサイズ設定
	// 登場したらコライダーを適用
	if (appear_.finished) Collider::SetOBBHalfSize({ 0.8f, 2.0f, 0.8f });

	// 移動前の位置を保存
	Vector3 oldPos = body_.transform.translate_;

	// ビヘイビアツリー更新（死亡チェック・登場演出・フェーズ行動）
	if (behaviorTree_) behaviorTree_->Tick(*this, deltaTime);

	// ダメージフラッシュ更新
	UpdateDamageFlash(deltaTime);

	// 移動後に必ずワールドとの衝突を解決
	if (state_ != State::Appear) SolveWorldCollision(oldPos); // 登場演出中は衝突解決しない

	if (appear_.finished)
	{
		UpdateAuraEmitter(deltaTime); // 常時オーラエフェクト更新
		UpdateRushTrailEmitter(deltaTime, oldPos); // ラッシュ軌跡エフェクト更新
		UpdateSpinAttackEmitter(deltaTime); // スピン攻撃エフェクト更新
		// ラッシュヒットクールダウン更新
		if (rushHitCooldown_ > 0.0f) rushHitCooldown_ -= deltaTime;
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
			// 連続ヒットを防ぎたい場合は ContactRecord を使っても良い
			// if (contactRecord_.Check(serialNumber)) return;
			// contactRecord_.Add(serialNumber);

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
					EmitRushHit(player->GetCenterPosition());
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
///				　　　登場エミッター生成
/// -------------------------------------------------------------
void BossEnemy::CreateAppearEmitter()
{
	if (!gpuParticleManager_) return;

	// まず既存を取りに行く
	appearEmitter_ = gpuParticleManager_->GetEmitter("BossAppear");
	if (appearEmitter_) return;

	GpuParticleEmitter::EmitterInfo emitterInfo = {};
	emitterInfo.type = GpuParticleType::Boss_Appear_Dust; // ボス登場砂埃
	emitterInfo.radius = 3.0f;							  // 発生範囲半径
	emitterInfo.loopCount = 0;							  // 1回のループで0個発生
	emitterInfo.loopFrequency = 0.0f; 					  // ループなし
	emitterInfo.billboardMode = BillboardMode::Camera;	  // カメラ追従

	// テクスチャ設定
	emitterInfo.textureFilePath = "white.png";

	// エミッター生成
	appearEmitter_ = gpuParticleManager_->CreateEmitter("BossAppear", emitterInfo);
}

/// -------------------------------------------------------------
///				　　　オーラエミッター生成
/// -------------------------------------------------------------
void BossEnemy::CreateAuraEmitter()
{
	if (!gpuParticleManager_) return;

	// まず既存を取りに行く
	auraEmitter_ = gpuParticleManager_->GetEmitter("BossAura");
	if (auraEmitter_) return;

	GpuParticleEmitter::EmitterInfo info{};
	info.type = GpuParticleType::Boss_Aura;   // ボスオーラ
	info.radius = 0.5f;                       // ボス中心からの広がり
	info.loopCount = 0;	                       // ループは使わず毎フレーム RequestEmit 方式
	info.loopFrequency = 0.0f;
	info.billboardMode = BillboardMode::Camera;

	// テクスチャ切り替え機能を入れてあるならここで指定（四角い白 or オーラ用）
	info.textureFilePath = "white.png";

	auraEmitter_ = gpuParticleManager_->CreateEmitter("BossAura", info);
}

void BossEnemy::UpdateAuraEmitter(float deltaTime)
{
	(void)deltaTime;

	if (death_.active) return; // 死亡中は出さない

	// エミッターがなければ何もしない
	if (!auraEmitter_) return;
	// エミッター位置をボス中心にセット
	Vector3 emitterPos = GetCenterPosition();
	emitterPos.y += 0.0f; // 少し上にオフセット
	auraEmitter_->SetPosition(emitterPos);
	// 毎フレーム一定数発生させる
	const int emitCountPerFrame = 2;
	auraEmitter_->RequestEmit(emitCountPerFrame);
}

void BossEnemy::CreateRushTrailEmitter()
{
	if (!gpuParticleManager_) return;

	// まず既存を取りに行く
	rushTrailEmitter_ = gpuParticleManager_->GetEmitter("BossRushTrail");
	if (rushTrailEmitter_) return;

	GpuParticleEmitter::EmitterInfo info{};
	info.type = GpuParticleType::Boss_Rush_Trail;

	info.radius = 0.0f;         // 細め（太いなら 0.2f へ）
	info.loopCount = 0;
	info.loopFrequency = 0.0f;   // ループは使わず、Rush中に RequestEmit する
	info.billboardMode = BillboardMode::Camera;

	// とりあえず既存でOK（後で “横長グラデの白” にすると一気に良くなる）
	info.textureFilePath = "white.png";

	rushTrailEmitter_ = gpuParticleManager_->CreateEmitter("BossRushTrail", info);
}

void BossEnemy::UpdateRushTrailEmitter(float deltaTime, const Vector3& oldPos)
{
	if (death_.active) return; // 死亡中は出さない
	if (!rushTrailEmitter_) return;

	const bool isRush =
		(currentAttackKind_ == AttackKind::kRush) ||
		(currentAttackKind_ == AttackKind::kBackstepRush) ||
		(currentAttackKind_ == AttackKind::kMultiRush);

	if (!isRush) return;

	// 位置はボス中心（少し上げたいなら +0.5f とか）
	Vector3 emitterPos = GetCenterPosition();
	rushTrailEmitter_->SetPosition(emitterPos);

	// 速度に応じて発生数を増減（XZだけ見ると安定）
	Vector3 d = body_.transform.translate_ - oldPos;
	float distXZ = std::sqrt(d.x * d.x + d.z * d.z);
	float dt = (deltaTime > 0.00001f) ? deltaTime : 0.00001f;
	float speed = distXZ / dt;

	// 発生数計算（速度に応じて6～30の範囲で変化）
	uint32_t emitCount = static_cast<uint32_t>(std::clamp(speed, 15.0f, 50.0f));
	rushTrailEmitter_->RequestEmit(emitCount);
}

void BossEnemy::CreateSpinAttackEmitter()
{
	if (!gpuParticleManager_) return;

	// まず既存を取りに行く
	spinAttackEmitter_ = gpuParticleManager_->GetEmitter("BossSpinAttack");
	if (spinAttackEmitter_) return;

	GpuParticleEmitter::EmitterInfo info{};
	info.type = GpuParticleType::Boss_Spin_Slash;
	info.radius = 0.1f;                          // 発生の散り幅（小さめでOK）
	info.loopCount = 0;
	info.loopFrequency = 0.0f;
	info.billboardMode = BillboardMode::Camera;
	info.textureFilePath = "white.png";         // 衝撃波っぽいならこれが相性良い
	spinAttackEmitter_ = gpuParticleManager_->CreateEmitter("BossSpinAttack", info);
}

void BossEnemy::UpdateSpinAttackEmitter(float deltaTime)
{
	(void)deltaTime;

	if (death_.active) return; // 死亡中は出さない
	if (!spinAttackEmitter_) return;
	if (currentAttackKind_ != AttackKind::kSpinAttack) return;

	Vector3 pos = GetCenterPosition();
	pos.y += 0.0f;                 // 体の中心〜胸あたり（好みで調整）
	spinAttackEmitter_->SetPosition(pos);

	// とりあえず毎フレームばら撒く（数は調整）
	spinAttackEmitter_->RequestEmit(10);
}

void BossEnemy::CreateDeathEffectEmitters()
{
	if (!gpuParticleManager_) return;

	// すでに存在する場合は流用（ゲームやり直し対策）
	if (!deathExplosionEmitter_)
	{
		GpuParticleEmitter::EmitterInfo info{};
		info.type = GpuParticleType::Explosion_Fire; // 小さな爆発
		info.radius = 0.5f;                          // ボス中心周りにちょっと広がる
		info.loopCount = 0;
		info.loopFrequency = 0.0f;
		info.billboardMode = BillboardMode::Camera;
		info.textureFilePath = "white.png";          // 炎っぽいテクスチャあるなら差し替え

		deathExplosionEmitter_ =
			gpuParticleManager_->CreateEmitter("BossDeathExplosion", info);
	}

	if (!deathShockwaveEmitter_)
	{
		GpuParticleEmitter::EmitterInfo info{};
		info.type = GpuParticleType::Shockwave;      // 衝撃波タイプ（GPU_PARTICLE_TYPE_SHOCKWAVE と一致）
		info.radius = 0.15f;                         // ほぼ中心から出す
		info.loopCount = 0;
		info.loopFrequency = 0.0f;
		info.billboardMode = BillboardMode::Camera;
		info.textureFilePath = "white.png";        // リングっぽいテクスチャがあればそれ

		deathShockwaveEmitter_ = gpuParticleManager_->CreateEmitter("BossDeathShockwave", info);
	}
}

void BossEnemy::EmitDeathEffects()
{
	// ボス中心
	Vector3 center = GetCenterPosition();

	// ---------- 中心の小さな爆発 ----------
	if (deathExplosionEmitter_)
	{
		deathExplosionEmitter_->SetPosition(center);
		// 火の玉をドッと出す。多いほど派手になる
		deathExplosionEmitter_->RequestEmit(12);
	}

	// ---------- 足元の衝撃波リング ----------
	if (deathShockwaveEmitter_)
	{
		Vector3 ringPos = center;
		// 地面に沿わせたいので少し下げる（コライダー半径が 2.0f なら -1.8f くらい）
		ringPos.y -= 1.8f;
		deathShockwaveEmitter_->SetPosition(ringPos);

		// リングの密度。調整して好みの見た目に
		deathShockwaveEmitter_->RequestEmit(6);
	}
}

void BossEnemy::CreateDeathSoulEmitter()
{
	if (!gpuParticleManager_) return;

	deathSoulEmitter_ = gpuParticleManager_->GetEmitter("BossDeathSoul");
	if (deathSoulEmitter_) return;

	GpuParticleEmitter::EmitterInfo info{};
	info.type = GpuParticleType::Boss_Death_Soul;
	info.radius = 0.35f;                            // 発生の散り幅（小さめ）
	info.loopCount = 0;
	info.loopFrequency = 0.0f;                      // UpdateDeathでRequestEmitする
	info.billboardMode = BillboardMode::Camera;
	info.textureFilePath = "white.png";           // ふわっと感が出やすい

	deathSoulEmitter_ = gpuParticleManager_->CreateEmitter("BossDeathSoul", info);
}

void BossEnemy::CreateDebrisDustEmitter()
{
	if (!gpuParticleManager_) return;

	debrisDustEmitter_ = gpuParticleManager_->GetEmitter("BossDebrisDust");
	if (debrisDustEmitter_) return;

	GpuParticleEmitter::EmitterInfo info{};
	info.type = GpuParticleType::Boss_Debris_Dust;
	info.radius = 1.5f;                             // 破片周りに散る想定で少し広め
	info.loopCount = 0;
	info.loopFrequency = 0.0f;                      // 死亡開始の瞬間だけバースト
	info.billboardMode = BillboardMode::Camera;
	info.textureFilePath = "white.png";             // 粒埃はwhiteでOK

	debrisDustEmitter_ = gpuParticleManager_->CreateEmitter("BossDebrisDust", info);
}

void BossEnemy::CreateRushHitEmitter()
{
	if (!gpuParticleManager_) return;

	// まず既存を取りに行く
	rushHitEmitter_ = gpuParticleManager_->GetEmitter("BossRushHit");
	if (rushHitEmitter_) return;

	GpuParticleEmitter::EmitterInfo info{};
	info.type = GpuParticleType::Shockwave;   // ←新規タイプ
	info.radius = 0.5f;                          // 発生の散り幅（小さめでOK）
	info.loopCount = 0;
	info.loopFrequency = 0.0f;
	info.billboardMode = BillboardMode::Camera;
	info.textureFilePath = "white.png";         // 衝撃波っぽいならこれが相性良い

	rushHitEmitter_ = gpuParticleManager_->CreateEmitter("BossRushHit", info);
}

void BossEnemy::EmitRushHit(const Vector3& hitPos)
{
	if (!rushHitEmitter_) return;
	if (rushHitCooldown_ > 0.0f) return;

	Vector3 pos = hitPos;

	// 地面に貼り付けたいなら、中心から下げる（コライダー半分がy=2.0fなので目安）
	Vector3 bossCenter = GetCenterPosition();
	pos.y = bossCenter.y - 2.0f + 0.05f;

	rushHitEmitter_->SetPosition(pos);

	// ここが“衝撃波の強さ”
	rushHitEmitter_->RequestEmit(64);

	rushHitCooldown_ = 0.20f; // 0.2秒は連続発生させない
}

/// -------------------------------------------------------------
///				　　　ワールド衝突解決処理
/// -------------------------------------------------------------
void BossEnemy::SolveWorldCollision(const Vector3& oldTranslate)
{
	// ステージ情報がなければ何もしない
	if (!levelObjectManager_) return;

	// プレイヤーと同じ当たり判定の前提:
	const Vector3 half = { 0.8f, 2.0f, 0.8f };
	const float kEps = 0.002f;

	// プレイヤーと同じく「見た目の原点」と「物理中心」にオフセット差があるのでそろえる
	const Vector3 kCenterOffset = { 0.0f, 0.0f, 0.0f };

	// old/new の中心
	Vector3 oldCenter = oldTranslate - kCenterOffset;
	Vector3 newCenter = body_.transform.translate_ - kCenterOffset;

	// ワールドの当たり判定AABB群を取得
	const auto worldAABBs = levelObjectManager_->GetWorldAABBs();

	// プレイヤーAABBを作るラムダ
	auto makeAABB = [&](const Vector3& c) {return AABB{ c - half, c + half };	};

	// 押し戻し後の中心位置
	Vector3 fixedCenter = oldCenter;

	// 指定軸方向の押し戻しを解決するラムダ
	auto resolveAxis = [&](int axis, float delta)
		{
			// 動いていなければ何もしない
			if (delta == 0.0f) { return; }

			if (axis == 0) fixedCenter.x += delta;
			if (axis == 1) fixedCenter.y += delta;
			if (axis == 2) fixedCenter.z += delta;

			// プレイヤーAABBを作成
			AABB p = makeAABB(fixedCenter);

			bool hit = false;
			float bestFix = 0.0f;
			float bestDist = FLT_MAX;

			// ワールドAABB群と当たり判定
			for (const auto& w : worldAABBs)
			{
				// AABB同士が交差しているかどうかをチェック（プレイヤーと同じ式）
				if (!(p.min.x <= w.max.x && p.max.x >= w.min.x &&
					p.min.y <= w.max.y && p.max.y >= w.min.y &&
					p.min.z <= w.max.z && p.max.z >= w.min.z)) {
					continue; // 交差していなければ次へ
				}

				float cand = 0.0f;
				bool valid = false;

				// 押し戻し候補を軸ごとに計算
				if (axis == 0)
				{
					// X方向押し戻し
					if (oldCenter.x + half.x <= w.min.x) { cand = (w.min.x - half.x) - kEps; valid = true; }
					else if (oldCenter.x - half.x >= w.max.x) { cand = (w.max.x + half.x) + kEps; valid = true; }
					else
					{
						float dMin = fabsf((w.min.x - half.x) - oldCenter.x);
						float dMax = fabsf((w.max.x + half.x) - oldCenter.x);
						cand = (dMin <= dMax) ?
							(w.min.x - half.x - kEps) :
							(w.max.x + half.x + kEps);
						valid = true;
					}

					// 最も近い押し戻し候補を採用
					if (valid)
					{
						float dist = fabsf(cand - fixedCenter.x);
						if (dist < bestDist) { bestDist = dist; bestFix = cand; hit = true; }
					}
				}
				else if (axis == 2)
				{
					// Z方向押し戻し（ほぼXと同じロジックのZ版）
					if (oldCenter.z + half.z <= w.min.z) { cand = (w.min.z - half.z) - kEps; valid = true; }
					else if (oldCenter.z - half.z >= w.max.z) { cand = (w.max.z + half.z) + kEps; valid = true; }
					else
					{
						float dMin = fabsf((w.min.z - half.z) - oldCenter.z);
						float dMax = fabsf((w.max.z + half.z) - oldCenter.z);
						cand = (dMin <= dMax) ?
							(w.min.z - half.z - kEps) :
							(w.max.z + half.z + kEps);
						valid = true;
					}
					if (valid)
					{
						float dist = fabsf(cand - fixedCenter.z);
						if (dist < bestDist) { bestDist = dist; bestFix = cand; hit = true; }
					}
				}
				else
				{
					// Y方向押し戻し（床/天井処理）
					if (oldCenter.y - half.y >= w.max.y) { cand = (w.max.y + half.y) + kEps; valid = true; }     // 床の上に乗る
					else if (oldCenter.y + half.y <= w.min.y) { cand = (w.min.y - half.y) - kEps; valid = true; } // 天井の下で止まる
					else {
						float dFloor = fabsf((w.max.y + half.y) - oldCenter.y);
						float dCeil = fabsf((w.min.y - half.y) - oldCenter.y);
						cand = (dFloor <= dCeil) ?
							(w.max.y + half.y + kEps) :
							(w.min.y - half.y - kEps);
						valid = true;
					}
					if (valid) {
						float dist = fabsf(cand - fixedCenter.y);
						if (dist < bestDist) { bestDist = dist; bestFix = cand; hit = true; }
					}
				}
			}

			// 押し戻しが発生していれば位置を修正
			if (hit)
			{
				if (axis == 0) fixedCenter.x = bestFix;
				if (axis == 2) fixedCenter.z = bestFix;
				if (axis == 1) fixedCenter.y = bestFix;
			}
		};

	// 軸ごとに、"どれだけ動いたか" を解決
	resolveAxis(0, newCenter.x - oldCenter.x);
	resolveAxis(2, newCenter.z - oldCenter.z);
	resolveAxis(1, newCenter.y - oldCenter.y);

	// 最終的な位置を反映
	body_.transform.translate_ = fixedCenter + kCenterOffset;

	// コライダー中心も同期（プレイヤーと同じく物理中心ベースで渡す）
	Collider::SetCenterPosition(fixedCenter);
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
///						死亡演出
/// -------------------------------------------------------------
void BossEnemy::UpdateDeath(float deltaTime)
{
	death_.timer += deltaTime;
	const Vector3 gravity = { 0.0f, -9.8f * 3.0f, 0.0f };
	const float   groundY = 0.5f; // 床の高さとして扱う

	// 床で止めるためのラムダ
	auto clampToGround = [&](WorldTransformEx& tr, GibMotion& gm)
		{
			if (tr.translate_.y < groundY)
			{
				tr.translate_.y = groundY;

				// 下向きの速度は殺す
				if (gm.velocity.y < 0.0f) gm.velocity.y = 0.0f;

				// すこし摩擦っぽく減速＆回転減衰
				gm.velocity.x *= 0.6f;
				gm.velocity.z *= 0.6f;
				gm.angularVelocity.x *= 0.5f;
				gm.angularVelocity.y *= 0.5f;
				gm.angularVelocity.z *= 0.5f;
			}
		};

	// body も飛ばす
	death_.bodyGib.velocity += gravity * deltaTime;
	body_.transform.translate_ += death_.bodyGib.velocity * deltaTime;
	body_.transform.rotate_ += death_.bodyGib.angularVelocity * deltaTime;
	clampToGround(body_.transform, death_.bodyGib);

	// --- パーツ ---
	for (size_t i = 0; i < parts_.size() && i < death_.gibs.size(); ++i)
	{
		auto& part = parts_[i];
		auto& gm = death_.gibs[i];

		gm.velocity += gravity * deltaTime;
		part.transform.translate_ += gm.velocity * deltaTime;
		part.transform.rotate_ += gm.angularVelocity * deltaTime;

		clampToGround(part.transform, gm);
	}

	// 終了判定
	if (death_.timer >= death_.duration)
	{
		// 一応完全に止めておく（この後は UpdateDeath 呼ばれないけど保険）
		death_.active = false;
		death_.finished = true;

		death_.bodyGib.velocity = { 0.0f, 0.0f, 0.0f };
		death_.bodyGib.angularVelocity = { 0.0f, 0.0f, 0.0f };
		for (auto& gm : death_.gibs)
		{
			gm.velocity = { 0.0f, 0.0f, 0.0f };
			gm.angularVelocity = { 0.0f, 0.0f, 0.0f };
		}
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
	// プレイヤーがいないなら何もしない
	if (!player_) return AttackKind::kNone;

	// パラメータ（ChaseState と共有）
	const float kRushDistance = 8.0f;   // この距離以上離れていると「遠い」
	const float kRushFarTime = 1.0f;   // 遠い状態が続いた時間で Rush 解禁
	const float kSpinRange = 12.0f;   // これより近いと回転攻撃候補

	// クールタイム中なら何もしない
	if (attackCooldown_ > 0.0f) return AttackKind::kNone;

	// 現在距離を計算
	Vector3 bossPos = body_.transform.translate_;
	Vector3 playerPos = player_->GetCenterPosition();

	// XZ 平面の距離ベクトル
	Vector3 toPlayer{ playerPos.x - bossPos.x, 0.0f, playerPos.z - bossPos.z };

	float distSq = toPlayer.x * toPlayer.x + toPlayer.z * toPlayer.z;
	float dist = (distSq > 0.0f) ? std::sqrt(distSq) : 0.0f;

	// 近距離なら回転攻撃を優先
	if (dist <= kSpinRange)	return AttackKind::kSpinAttack;

	// 遠距離かつ、遠い状態が一定時間続いていたら突進
	if (dist > kRushDistance && farFromPlayerTimer_ >= kRushFarTime) return AttackKind::kRush;

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
	// すでに死亡状態なら何もしない
	if (state_ == State::Dead) return;

	state_ = State::Dead; // 死亡状態に遷移
	death_.active = true; // 死亡演出開始

	// とりあえず即終了にしておく（あとで分解演出をここに実装）
	death_.finished = false;
	death_.timer = 0.0f;

	// 簡単な吹っ飛び設定（とりあえず体幹だけ）
	death_.bodyGib.velocity = { 0.0f, 8.0f, 0.0f }; // 上方向へ
	death_.bodyGib.angularVelocity = { 0.0f, 5.0f, 0.0f }; // Y軸回転

	death_.gibs.clear();
	death_.gibs.resize(parts_.size());

	static thread_local std::mt19937 rng{ std::random_device{}() };
	std::uniform_real_distribution<float> dirDist(-1.0f, 1.0f);
	std::uniform_real_distribution<float> upDist(2.0f, 6.0f);
	std::uniform_real_distribution<float> angDist(-6.0f, 6.0f);

	for (size_t i = 0; i < parts_.size(); ++i)
	{
		auto& part = parts_[i];
		GibMotion gm{};

		// ランダム速度
		Vector3 dir{ dirDist(rng), 0.0f, dirDist(rng) };
		if (Vector3::Length(dir) < 0.001f) dir = { 0.0f, 0.0f, 1.0f };
		dir = Vector3::Normalize(dir);
		gm.velocity = dir * 3.0f;
		gm.velocity.y += upDist(rng);
		gm.angularVelocity = { angDist(rng), angDist(rng), angDist(rng) };
		death_.gibs[i] = gm;

		// 今までの translate_ は body からのローカル位置なので、
		// 親を外す前に「ワールド位置」に焼き直す
		Vector3 worldPos = part.transform.translate_;
		if (part.transform.parent_ == &body_.transform) {
			worldPos += body_.transform.translate_;
		}

		part.transform.parent_ = nullptr;
		part.transform.translate_ = worldPos;
	}

	death_.startBurstDone = false;
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

	if (appearEmitter_)
	{
		appearEmitter_->SetPosition({ 0.0f,0.0f,0.0f });
		appearEmitter_->RequestEmit(5); // 毎フレーム5個発生
	}

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
	case AttackKind::kRush:
		attackState_ = std::make_unique<BossRushState>();
		break;

	case AttackKind::kSpinAttack:
		attackState_ = std::make_unique<BossSpinState>();
		break;

	case AttackKind::kNone:
	default:
		// 待機・追尾状態
		attackState_ = std::make_unique<BossChaseState>();
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

