#define NOMINMAX
#include "Player.h"
#include <Object3DCommon.h>
#include <CollisionTypeIdDef.h>
#include <Input.h>
#include <AudioManager.h>
#include "LinearInterpolation.h"
#include "ToWeaponConfig.h"
#include "LevelObjectManager.h"
#include "Enemy.h"

#include <algorithm>
#include <filesystem>
#include <AABB.h>

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

namespace
{
	static std::mt19937 sRng{ std::random_device{}() };

	static Vector3 ApplySpreadCone(const Vector3& forwardN, float angleRad)
	{
		if (angleRad <= 0.0f) return forwardN;

		std::uniform_real_distribution<float> dist01(0.0f, 1.0f);
		const float u = dist01(sRng);
		const float v = dist01(sRng);

		const float cosMax = std::cosf(angleRad);
		const float cosTheta = (1.0f - u) + u * cosMax; // [cosMax,1]
		const float sinTheta = std::sqrtf(std::max(0.0f, 1.0f - cosTheta * cosTheta));
		const float phi = 2.0f * std::numbers::pi_v<float> *v;

		Vector3 up = { 0.0f, 1.0f, 0.0f };
		Vector3 right = Vector3::Cross(up, forwardN);
		if (Vector3::Length(right) < 1e-4f) right = { 1.0f, 0.0f, 0.0f };
		right = Vector3::Normalize(right);
		up = Vector3::Normalize(Vector3::Cross(forwardN, right));

		Vector3 dir = forwardN * cosTheta
			+ right * (std::cosf(phi) * sinTheta)
			+ up * (std::sinf(phi) * sinTheta);
		return Vector3::Normalize(dir);
	}
}

/// -------------------------------------------------------------
///				　			　 初期化処理
/// -------------------------------------------------------------
void Player::Initialize()
{
	// ベースキャラクター初期化
	BaseCharacter::Initialize();

	// 入力取得
	input_ = Input::GetInstance();

	// テクスチャの設定
	BaseCharacter::ApplySkinToAllParts(skinTexturePath_);

	// ID登録
	Collider::SetTypeID(static_cast<uint32_t>(CollisionTypeIdDef::kPlayer));
	Collider::SetOwner<Player>(this);
	Collider::SetOBBHalfSize({ 0.8f, 2.0f, 0.8f });

	// FPSカメラ
	fpsCamera_ = std::make_unique<FpsCamera>();
	fpsCamera_->Initialize(this);

	// 武器マネージャー初期化
	weaponManager_ = std::make_unique<WeaponManager>();
	weaponManager_->SetParentTransforms(&parts_[partIndices_.rightArm].transform); // 右腕を親に設定
	weaponManager_->SetPlayerBody(&body_.transform);				   // 体幹をプレイヤーボディに設定
	weaponManager_->InitializeWeapons(fireState_, deathState_);					   // 武器初期化
	weaponManager_->SetCollisionManager(collisionManager_); // 衝突管理者設定

	// 体力初期化
	hp_ = maxHp_;
	hurtTimer_ = 0.0f;
}


/// -------------------------------------------------------------
///				　			　 更新処理
/// -------------------------------------------------------------
void Player::Update(float deltaTime)
{
#ifdef _DEBUG

	// カメラモード切替
	if (input_->TriggerKey(DIK_F5)) { fpsCamera_->CycleViewMode(); }

	if (input_->TriggerKey(DIK_J)) { StartDeath(DeathMode::BlowAway); }

#endif // _DEBUG

	// ここで表示切替
	switch (fpsCamera_->GetViewMode()) // ← カメラの現在モード
	{
	case FpsCamera::ViewMode::FirstPerson:
	{
		//SetBodyActive(false);          // 体幹は映さない
		//SetAllPartsActive(false);      // 一旦全部オフ

		//// ADS中は腕が邪魔なら非表示
		//const bool hideArm = vm_.hideArmInAds && (fpsCamera_->GetAimAlpha() >= vm_.hideArmAimAlpha);
		//SetPartActive(partIndices_.rightArm, !hideArm);

		SetBodyActive(false);
		SetAllPartsActive(false);
		SetPartActive(partIndices_.rightArm, true);
		break;
	}

	case FpsCamera::ViewMode::ThirdBack:
	case FpsCamera::ViewMode::ThirdFront:
		SetBodyActive(true);
		SetAllPartsActive(true);       // 全パーツオン
		break;
	}

	if (deathState_.inDeathSeq || deathState_.isDead)
	{
		SetBodyActive(true);       // 死亡中は体幹を表示
		SetAllPartsActive(true);   // 死亡中は全部表示
		parts_[partIndices_.rightArm].active = &body_.transform; // 右腕も体幹に追従
	}

	// 死亡処理更新
	if (deathState_.inDeathSeq)
	{
		UpdateDeath(deltaTime);
		BaseCharacter::Update(deltaTime); // ベースキャラクターの更新は行う
		return; // 死亡中は以降の処理を行わない
	}

	// 死亡完了
	if (deathState_.isDead)
	{
		BaseCharacter::Update(deltaTime); // ベースキャラクターの更新は行う
		return;
	}

	// ダメージタイマー更新
	if (hurtTimer_ > 0.0f) {
		hurtTimer_ -= deltaTime;
		if (hurtTimer_ < 0.0f) hurtTimer_ = 0.0f;
	}

	// デバッグカメラ中は移動処理しない
	if (IsDebugCamera())
	{
		// 武器更新
		weaponManager_->UpdateWeapons(deltaTime);
		BaseCharacter::Update(deltaTime); // ベースキャラクターの更新は行う
		return;
	}

	// FPSカメラへ Δt を渡す（揺れ・ADS補間に必要）
	fpsCamera_->SetDeltaTime(deltaTime);

	// 右クリック（Mouse1）でADS。※一人称のみ有効
	const bool wantAds = input_->PushMouse(1);
	const bool isFPNow = (fpsCamera_->GetViewMode() == FpsCamera::ViewMode::FirstPerson);
	fpsCamera_->SetAiming(isFPNow && wantAds);

	// 移動処理
	Move(deltaTime);

	// クールダウン更新
	if (fireState_.cooldown > 0.0f) fireState_.cooldown -= deltaTime;

	// マウス左ボタンで射撃
	if (input_->PushMouse(0))
	{
		// リロード中は発射ループを止める（超重要）
		const auto ammo = weaponManager_->GetCurrentAmmoView();
		if (ammo.reloading)
		{
			// 押しっぱでも何もしない
		}
		else if (fireState_.cooldown <= 0.0f)
		{
			/*float yaw = fpsCamera_->GetYaw();
			float pitch = fpsCamera_->GetPitch();
			Vector3 forward = {
				-std::sinf(yaw) * std::cosf(pitch),
				-std::sinf(pitch),
				 std::cosf(yaw) * std::cosf(pitch)
			};
			forward = Vector3::Normalize(forward);*/

			Camera* cam = fpsCamera_->GetCamera();
			Vector3 forward = Vector3::Normalize(cam->GetForward());

			// 腰だめ/ADSでスプレッドを変える（百発百中を崩す）
			const float hipSpreadDeg = 0.9f;
			const float adsSpreadDeg = 0.15f;
			const float spreadDeg = Lerp(hipSpreadDeg, adsSpreadDeg, fpsCamera_->GetAimAlpha());
			forward = ApplySpreadCone(forward, DegToRad(spreadDeg));

			const WeaponConfig& config = weaponManager_->GetCurrentConfig();
			Vector3 velocity = forward * config.muzzleSpeed;

			// 実際に撃てたかを受け取る
			const bool fired = weaponManager_->StartFireBallisticEffect(GetCenterPosition(), velocity);

			if (fired)
			{
				fireState_.interval = 60.0f / config.rpm;
				fireState_.cooldown = fireState_.interval;

				// リコイルも“撃てた時だけ”
				fpsCamera_->AddRecoil(DegToRad(0.3f), DegToRad(0.2f));
				fpsCamera_->Update(true);
			}
			else
			{
				// 弾切れ・リロード中などで撃てなかった時：
				// 毎フレーム呼び続けて重くなる/音を鳴らし続けるのを防ぐ保険（任意）
				fireState_.cooldown = 0.05f;
			}
		}
	}

	// 武器更新
	weaponManager_->UpdateWeapons(deltaTime);

	// ベースキャラクターの更新
	BaseCharacter::Update(deltaTime);
}


/// -------------------------------------------------------------
///				　			　 描画処理
/// -------------------------------------------------------------
void Player::Draw()
{
	// ベースキャラクター描画
	BaseCharacter::Draw();

	// 武器描画
	if (!IsDeadNow()) weaponManager_->DrawWeapons();
}

/// -------------------------------------------------------------
///				　	衝突時に呼ばれる仮想関数
/// -------------------------------------------------------------
void Player::OnCollision(Collider* other)
{
	uint32_t serialNumber = other->GetUniqueID(); // 相手のシリアルナンバー取得

	if (other->GetTypeID() == static_cast<uint32_t>(CollisionTypeIdDef::kWorld))
	{

	}

	// 衝突相手がエネミーの場合
	if (other->GetTypeID() == static_cast<uint32_t>(CollisionTypeIdDef::kEnemy))
	{
		Enemy* enemy = other->GetOwner<Enemy>();
		if (!enemy) return; // オーナー取得失敗なら抜ける

		// 接触記録があれば何もせず抜ける
		if (contactRecord_.Check(serialNumber)) return;

		// 接触記録に登録
		contactRecord_.Add(serialNumber);

		// 弾丸と衝突したときの処理
		OutputDebugStringA("Enemy hit by bullet!\n");
	}
}

/// -------------------------------------------------------------
///				　中心座標を取得する純粋仮想関数
/// -------------------------------------------------------------
Vector3 Player::GetCenterPosition() const
{
	const Vector3 offset = { 0.0f,1.5f,0.0f };
	Vector3 worldPosition = body_.transform.translate_ + offset;
	return worldPosition;
}

/// -------------------------------------------------------------
///				　	コライダー登録処理
/// -------------------------------------------------------------
void Player::RegisterColliders(CollisionManager* mgr)
{
	if (weaponManager_) weaponManager_->RegisterColliders(mgr);
}

/// -------------------------------------------------------------
///				　　敵から殴られた瞬間のリアクション用
/// -------------------------------------------------------------
void Player::ApplyDamageImpulse(const Vector3& dir, float horizontalPow, float upPow)
{
	// 水平向きだけ取り出して正規化
	Vector3 flatDir = dir;
	flatDir.y = 0.0f;
	if (Vector3::Length(flatDir) > 0.0001f) {
		flatDir = Vector3::Normalize(flatDir);
	}
	else {
		flatDir = { 0.0f, 0.0f, -1.0f };
	}

	// 「こう押し飛ばされたい」理想の速度
	Vector3 desired = flatDir * horizontalPow;

	// 今の knockbackVel_ を desired に近づける（いきなりドンじゃなく、スッと押される感じ）
	// lerp: v = v + (desired - v) * gain
	knockbackVel_ += (desired - knockbackVel_) * hurtKnockbackGain_;

	// 空中に飛ばす
	jumpState_.isGrounded = false;

	// 上向きは殴られた瞬間にちゃんと吹き上げたいので、こっちは即セット寄りでOK
	if (jumpState_.jumpVelocity < upPow) {
		jumpState_.jumpVelocity = upPow;
	}
}

/// -------------------------------------------------------------
///				　		　 ダメージ処理
/// -------------------------------------------------------------
void Player::TakeDamage(float amount)
{
	// すでに死亡中 or 死亡後なら何もしない
	if (deathState_.inDeathSeq || deathState_.isDead) {
		return;
	}

	// 無敵中なら食らわない
	if (hurtTimer_ > 0.0f) {
		return;
	}

	// HPを減らす
	hp_ -= amount;
	if (hp_ < 0.0f) { hp_ = 0.0f; }

	// 無敵時間スタート
	hurtTimer_ = hurtInvincibleTime_;

	// HPが0以下なら死亡演出開始
	if (hp_ <= 0.0f)
	{
		StartDeath(DeathMode::BlowAway);
		// StartDeath() は deathState_.inDeathSeq = true にして
		// Update() 側が以降の操作を死亡演出モードに切り替える。:contentReference[oaicite:10]{index=10}
	}
}

/// -------------------------------------------------------------
///				　			　 移動処理
/// -------------------------------------------------------------
void Player::Move(float deltaTime)
{
	(void)deltaTime; // 未使用

	// 物理中心 ←→ 描画ピボットの固定オフセット（いままで -0.25 を使っていた値）
	const Vector3 kCenterOffset = { 0.0f, 0.25f, 0.0f };

	// 物理は「中心」で扱うので、まず現在の物理中心を求める
	Vector3 physCenter = body_.transform.translate_ - kCenterOffset;

	// コライダー中心も同期（物理中心を渡す）
	Collider::SetCenterPosition(physCenter);  // ← ここを body_ から計算した physCenter に統一

	// --- 前準備 ---
	const float baseMoveSpeed = 0.1f;
	const float adsMoveMul = 0.4f; // ADS中の移動速度倍率（0.5〜0.8で調整）
	const bool aimingNow = fpsCamera_->IsAiming() && (fpsCamera_->GetViewMode() == FpsCamera::ViewMode::FirstPerson);
	const float moveSpeed = baseMoveSpeed * (aimingNow ? adsMoveMul : 1.0f);

	const Vector3 half = { 0.8f, 2.0f, 0.8f }; // Collider::SetOBBHalfSize と同じ半サイズ
	const float kEps = 0.002f;

	// AABBユーティリティ
	auto makeAABB = [&](const Vector3& c) { return AABB{ c - half, c + half }; };

	// --- 入力から水平移動ベクトル ---
	Vector3 move{ 0,0,0 };
	if (!viewState_.isDebugCamera)
	{
		if (input_->PushKey(DIK_W)) move.z += moveSpeed;
		if (input_->PushKey(DIK_S)) move.z -= moveSpeed;
		if (input_->PushKey(DIK_A)) move.x -= moveSpeed;
		if (input_->PushKey(DIK_D)) move.x += moveSpeed;
	}

	// 正規化して移動速度に調整
	if (Vector3::Length(move) > 0.0f) move = Vector3::Normalize(move) * moveSpeed;

	// 三人称前方視点なら前後反転
	if (fpsCamera_->GetViewMode() == FpsCamera::ViewMode::ThirdFront) move.z *= -1.0f;

	// カメラYawで回す（水平）
	const float yaw = fpsCamera_->GetCamera()->GetRotate().y;
	const float s = std::sinf(yaw), c = std::cosf(yaw);
	move = { move.x * c - move.z * s, 0.0f, move.x * s + move.z * c };

	// --- ジャンプ ---
	if (jumpState_.isGrounded && input_->PushKey(DIK_SPACE))
	{
		jumpState_.jumpVelocity = jumpState_.jumpPower;
		jumpState_.isGrounded = false;
	}

	// --- 重力 ---
	jumpState_.jumpVelocity -= jumpState_.gravity;
	move.y = jumpState_.jumpVelocity; // ← Yは毎フレームの速度ぶんだけ

	move += knockbackVel_; // ノックバック速度を加算

	// レベルオブジェクトのAABBリスト取得
	const auto worldAABBs = levelObjectManager_->GetWorldAABBs();

	// 物理中心
	Vector3 oldCenter = physCenter;
	Vector3 newCenter = oldCenter;

	// 衝突解決ラムダ
	auto resolveAxis = [&](int axis, float delta)
		{
			if (delta == 0.0f) return;
			if (axis == 0) newCenter.x += delta;
			if (axis == 1) newCenter.y += delta;
			if (axis == 2) newCenter.z += delta;

			AABB p = makeAABB(newCenter);

			bool hit = false; float bestFix = 0.0f; float bestDist = FLT_MAX;

			// 全ワールドAABBと当たり判定チェック
			for (const auto& w : worldAABBs)
			{
				if (!(p.min.x <= w.max.x && p.max.x >= w.min.x &&
					p.min.y <= w.max.y && p.max.y >= w.min.y &&
					p.min.z <= w.max.z && p.max.z >= w.min.z)) continue;

				float cand = 0.0f; bool valid = false;

				if (axis == 0)
				{
					if (oldCenter.x + half.x <= w.min.x) { cand = (w.min.x - half.x) - kEps; valid = true; }
					else if (oldCenter.x - half.x >= w.max.x) { cand = (w.max.x + half.x) + kEps; valid = true; }
					else
					{
						float dMin = fabsf((w.min.x - half.x) - oldCenter.x);
						float dMax = fabsf((w.max.x + half.x) - oldCenter.x);
						cand = (dMin <= dMax) ? (w.min.x - half.x - kEps) : (w.max.x + half.x + kEps);
						valid = true;
					}
					if (valid) { float dist = fabsf(cand - newCenter.x); if (dist < bestDist) { bestDist = dist; bestFix = cand; hit = true; } }
				}
				else if (axis == 2)
				{
					if (oldCenter.z + half.z <= w.min.z) { cand = (w.min.z - half.z) - kEps; valid = true; }
					else if (oldCenter.z - half.z >= w.max.z) { cand = (w.max.z + half.z) + kEps; valid = true; }
					else
					{
						float dMin = fabsf((w.min.z - half.z) - oldCenter.z);
						float dMax = fabsf((w.max.z + half.z) - oldCenter.z);
						cand = (dMin <= dMax) ? (w.min.z - half.z - kEps) : (w.max.z + half.z + kEps);
						valid = true;
					}
					if (valid) { float dist = fabsf(cand - newCenter.z); if (dist < bestDist) { bestDist = dist; bestFix = cand; hit = true; } }
				}
				else
				{
					// Y（床/天井）
					if (oldCenter.y - half.y >= w.max.y) { cand = (w.max.y + half.y) + kEps; valid = true; } // 床
					else if (oldCenter.y + half.y <= w.min.y) { cand = (w.min.y - half.y) - kEps; valid = true; } // 天井
					else
					{
						float dFloor = fabsf((w.max.y + half.y) - oldCenter.y);
						float dCeil = fabsf((w.min.y - half.y) - oldCenter.y);
						cand = (dFloor <= dCeil) ? (w.max.y + half.y + kEps) : (w.min.y - half.y - kEps);
						valid = true;
					}
					if (valid) { float dist = fabsf(cand - newCenter.y); if (dist < bestDist) { bestDist = dist; bestFix = cand; hit = true; } }
				}
			}

			if (hit)
			{
				if (axis == 0) newCenter.x = bestFix;
				if (axis == 2) newCenter.z = bestFix;
				if (axis == 1)
				{
					newCenter.y = bestFix;
					if (delta < 0.0f) { jumpState_.isGrounded = true; jumpState_.jumpVelocity = 0.0f; }
					else if (jumpState_.jumpVelocity > 0.0f) { jumpState_.jumpVelocity = 0.0f; }
				}
			}
		};

	jumpState_.isGrounded = false;
	resolveAxis(0, move.x);
	resolveAxis(2, move.z);
	resolveAxis(1, move.y);

	// 描画は「物理中心 + オフセット」
	physCenter = newCenter;
	body_.transform.translate_ = physCenter + kCenterOffset;

	/// ---------- 体と頭の回転処理 ---------- ///
	const bool  isFP = (fpsCamera_->GetViewMode() == FpsCamera::ViewMode::FirstPerson);
	const float camYaw = fpsCamera_->GetYaw();
	const float camPitch = fpsCamera_->GetPitch();

	// 頭と体の回転更新
	if (isFP)
	{
		// 一人称：回転は即時
		viewState_.bodyYaw = camYaw;
		viewState_.headYawLocal = 0.0f;
		parts_[partIndices_.head].transform.rotate_.x = std::clamp(camPitch, -viewState_.headPitchLimit, viewState_.headPitchLimit);
	}
	else
	{
		// 三人称：補間
		float targetHeadYawLocal = NormalizeAngle(camYaw - viewState_.bodyYaw);
		targetHeadYawLocal = std::clamp(targetHeadYawLocal, -viewState_.headYawLimit, viewState_.headYawLimit);
		viewState_.headYawLocal = Lerp(viewState_.headYawLocal, targetHeadYawLocal, 0.25f);
		viewState_.bodyYaw = LerpAngle(viewState_.bodyYaw, camYaw, 0.10f);
		parts_[partIndices_.head].transform.rotate_.x = std::clamp(camPitch, -viewState_.headPitchLimit, viewState_.headPitchLimit);
	}

	// 体と頭の回転反映
	body_.transform.rotate_.y = viewState_.bodyYaw;
	parts_[partIndices_.head].transform.rotate_.y = viewState_.headYawLocal;

	/// ---------- 右手：FPはカメラ基準で固定 ---------- ///
	WorldTransformEx& armT = parts_[partIndices_.rightArm].transform;

	if (isFP)
	{
		fpsCamera_->Update();
		armT.parent_ = nullptr;

		Camera* cam = fpsCamera_->GetCamera();
		const float fovNow = cam->GetFovY();                         // 現在FOV
		const float fovBase = vm_.baseFovDeg * std::numbers::pi_v<float> / 180.0f;
		const float k = std::tanf(fovNow * 0.5f) / std::tanf(fovBase * 0.5f); // ←FOV係数

		// ADS補間値（FpsCamera側でスムーズに 0→1）
		const float aimA = fpsCamera_->GetAimAlpha();

		// 腰だめ→ADSでオフセットを補間（顔の前へ寄せる）
		Vector3 off = Lerp(vm_.baseOffset, vm_.adsOffset, aimA);

		// 右(X)/上(Y)はFOV補正、前(Z)はそのまま
		Vector3 local = { off.x * k, off.y * k, off.z };

		// カメラ姿勢（Yaw→Pitch）でローカル→ワールドへ
		Matrix4x4 Ry = Matrix4x4::MakeRotateY(camYaw);
		Matrix4x4 Rx = Matrix4x4::MakeRotateX(camPitch);
		Matrix4x4 R = Matrix4x4::Multiply(Rx, Ry);
		Vector3 offset = Matrix4x4::Transform(local, R);

		const Vector3 camPos = cam->GetTranslate();
		armT.translate_ = camPos + offset;
		armT.rotate_.y = camYaw;
		armT.rotate_.x = camPitch - (90.0f * std::numbers::pi_v<float> / 180.0f);

		// 見た目サイズをFOVに依存させない
		if (vm_.lockSizeByFov)
		{
			Vector3 sc = vm_.baseScale * k;      // ← 逆じゃなくて k
			armT.scale_ = sc;
			parts_[partIndices_.rightArm].object->SetScale(sc); // 念のため両方
		}
		else
		{
			armT.scale_ = vm_.baseScale;
			parts_[partIndices_.rightArm].object->SetScale(vm_.baseScale);
		}
	}
	else
	{
		// 三人称では元の階層に戻す
		armT.parent_ = &body_.transform;
		armT.translate_ = { 0.75f, 0.75f, 0 };
		armT.rotate_ = { 0.0f, 0.0f, 0.0f };     // ← yaw も roll もゼロに
		fpsCamera_->Update();
	}

	// 最後にノックバックを減衰させる
	float damp = jumpState_.isGrounded ? groundKnockbackDamping_ : airKnockbackDamping_;
	knockbackVel_ *= damp;

	if (Vector3::Length(knockbackVel_) < 0.001f) {
		knockbackVel_ = { 0.0f, 0.0f, 0.0f };
	}
}

/// -------------------------------------------------------------
///				　			死亡処理開始
/// -------------------------------------------------------------
void Player::StartDeath(DeathMode mode)
{
	deathState_.inDeathSeq = true;
	deathState_.isDead = false;
	deathState_.mode = mode;
	deathState_.timer = 0.0f;
	deathState_.length = 2.5f;  // 吹っ飛びは少し長めに
	deathState_.side = (rand() & 1) ? +1 : -1;

	SetBodyActive(true);       // 体幹は映す
	SetAllPartsActive(true);   // 全パーツオン
	parts_[partIndices_.rightArm].transform.parent_ = &body_.transform; // 右腕を体に戻す
	parts_[partIndices_.rightArm].transform.translate_ = { 0.75f, 0.75f, 0 };
	parts_[partIndices_.rightArm].transform.rotate_ = { 0.0f, 0.0f, 0.0f }; // 回転リセット

	// カメラを三人称視点に切替
	fpsCamera_->SetViewMode(FpsCamera::ViewMode::ThirdBack);

	Camera* cam = fpsCamera_->GetCamera();
	deathState_.camLockPos = cam->GetTranslate();
	deathState_.camLockRot = cam->GetRotate();
	deathState_.camLock = true;    // 固定ON

	// カメラ奪取開始位置（今の一人称/三人称どちらでもOK）
	deathState_.cameraStartPos = fpsCamera_->GetCamera()->GetTranslate();

	// プレイヤーの視線の逆方向に後方へ + 少し上へ
	const float yaw = fpsCamera_->GetYaw();
	const float pitch = fpsCamera_->GetPitch();
	Vector3 fwd = { -sinf(yaw) * cosf(pitch), -sinf(pitch), cosf(yaw) * cosf(pitch) };

	// チューンしやすい初速（m/s想定）
	float speedBack = 64.0f;   // 後ろ方向
	float speedUp = 16.0f;    // 上方向ブースト
	// ランダムな左右ブレ
	float side = ((rand() & 1) ? 1.0f : -1.0f) * 2.5f;

	// 右方向ベクトルを作って横ブレを足す
	Vector3 worldUp{ 0,1,0 };
	Vector3 right = Vector3::Normalize(Vector3::Cross(worldUp, fwd));
	deathState_.velocity = (-fwd * speedBack) + (worldUp * speedUp) + (right * side);

	// ランダムな角速度（rad/s）
	auto rr = [&](float lo, float hi) { return lo + (hi - lo) * (rand() / (float)RAND_MAX); };
	deathState_.angularVelocity = { rr(-4.0f,4.0f), rr(-6.0f,6.0f), rr(-8.0f,8.0f) }; // かなり回す

	// 姿勢をフラットにしてから開始（見栄え安定）
	body_.transform.rotate_.x = 0.0f;
	body_.transform.rotate_.z = 0.0f;

	body_.object->SetDissolveThreshold(dissolveEffect_.threshold);
	body_.object->SetDissolveEdgeThickness(dissolveEffect_.edgeThickness);
	body_.object->SetDissolveEdgeColor(dissolveEffect_.edgeColor);
	for (auto& p : parts_) {
		p.object->SetDissolveThreshold(dissolveEffect_.threshold);
		p.object->SetDissolveEdgeThickness(dissolveEffect_.edgeThickness);
		p.object->SetDissolveEdgeColor(dissolveEffect_.edgeColor);
	}
}

/// -------------------------------------------------------------
///				　			死亡処理更新
/// -------------------------------------------------------------
void Player::UpdateDeath(float deltaTime)
{
	deathState_.timer += deltaTime;
	float u = clamp01(deathState_.timer / deathState_.length);
	float e = EaseOutCubic(u);

	static float sCamYawFixed = 0.0f;
	static bool sLatched = false;
	if (!sLatched || deathState_.timer <= deltaTime)
	{
		sCamYawFixed = fpsCamera_->GetYaw(); // 最初のフレームでYawを固定
		sLatched = true;
	}

	// ---- 並進：線形 + 二乗空気抵抗（速いほど強く減速）----
	{
		float speed = Vector3::Length(deathState_.velocity);
		if (speed > 0.0f) {
			Vector3 v = deathState_.velocity;
			// 方向は v と同じ、強さは (k1 * v + k2 * |v| * v)
			Vector3 dragAcc = -(deathState_.linDragK * v + deathState_.quadDragK * speed * v); // [m/s^2]
			deathState_.velocity += dragAcc * deltaTime;
		}
		body_.transform.translate_ += deathState_.velocity * deltaTime;
	}

	// ---- 回転：角速度にも線形 + 二乗抵抗 ----
	{
		auto dampOmega = [&](float w)
			{
				float mag = std::fabs(w);
				float dw = -(deathState_.angLink * w + deathState_.angQuadK * mag * w); // [rad/s^2]
				return w + dw * deltaTime;
			};
		deathState_.angularVelocity.x = dampOmega(deathState_.angularVelocity.x);
		deathState_.angularVelocity.y = dampOmega(deathState_.angularVelocity.y);
		deathState_.angularVelocity.z = dampOmega(deathState_.angularVelocity.z);

		body_.transform.rotate_.x += deathState_.angularVelocity.x * deltaTime;
		body_.transform.rotate_.y += deathState_.angularVelocity.y * deltaTime;
		body_.transform.rotate_.z += deathState_.angularVelocity.z * deltaTime;
	}

	// ---- カメラ演出 ----
	Camera* cam = fpsCamera_->GetCamera();

	if (deathState_.camLock)
	{
		// 位置は固定：保存したラッチ位置を使う
		const Vector3 camPos = deathState_.camLockPos;
		cam->SetTranslate(camPos);

		// 向きは毎フレームプレイヤー中心を向く（追従）
		Vector3 to = GetCenterPosition();                  // プレイヤー中心
		Vector3 look = Vector3::Normalize(to - camPos);    // カメラ→プレイヤー方向

		float yaw = std::atan2f(-look.x, look.z);
		float pitch = -std::asinf(look.y);

		// 少し余韻を置いてから消え始める
		const float startDelay = 0.50f;   // 開始まで待つ時間
		const float duration = 3.50f;   // 消え切るまでの時間

		// reloadProgress: 0→1（deltaTimeは掛けない）
		float progress = (deathState_.timer - startDelay) / duration;
		if (progress < 0.0f) progress = 0.0f;
		if (progress > 1.0f) progress = 1.0f;

		// あなたの仕様に合わせて閾値は 1→0 へ
		float threshold = 1.0f - progress;

		body_.object->SetDissolveThreshold(threshold);
		for (auto& p : parts_) {
			p.object->SetDissolveThreshold(threshold);
		}

		cam->SetRotate({ pitch, yaw, 0.0f });
		cam->Update();
	}
	else
	{
		// 背後に引いて注視カメラ
		Matrix4x4 Ry = Matrix4x4::MakeRotateY(sCamYawFixed);
		Vector3 offset = Matrix4x4::Transform(deathState_.cameraEndOffset, Ry);
		Vector3 camTargetPos = body_.transform.translate_ + offset;
		Vector3 camPos = Lerp(deathState_.cameraStartPos, camTargetPos, e);

		Vector3 look = Vector3::Normalize(GetCenterPosition() - camPos);
		float yaw = std::atan2f(-look.x, look.z);
		float pitch = -std::asinf(look.y);

		cam->SetTranslate(camPos);
		cam->SetRotate({ pitch, yaw, 0.0f });
		cam->Update();
	}

	// フェードアウト時間ではなく、速度と角速度が十分に小さくなったら終了でもOK
	bool stopped = (Vector3::Length(deathState_.velocity) < 0.15f) &&
		(std::fabs(deathState_.angularVelocity.x) + std::fabs(deathState_.angularVelocity.y) + std::fabs(deathState_.angularVelocity.z) < 0.30f);
	if (stopped || deathState_.timer > 5.0f) { deathState_.inDeathSeq = false; deathState_.isDead = true; }
}

/// -------------------------------------------------------------
///				　		  ImGui描画処理
/// -------------------------------------------------------------
void Player::DrawImGui()
{
	fpsCamera_->DrawImGui();

#ifdef USE_IMGUI
	ImGui::Begin("Player Dissolve");

	// --- ディゾルブ設定 ---
	if (ImGui::SliderFloat("Start Threshold", &dissolveEffect_.threshold, 0.0f, 1.0f)) {
		body_.object->SetDissolveThreshold(dissolveEffect_.threshold);
		for (auto& p : parts_) { p.object->SetDissolveThreshold(dissolveEffect_.threshold); }
	}
	if (ImGui::SliderFloat("Edge Thickness", &dissolveEffect_.edgeThickness, 0.0f, 0.5f)) {
		body_.object->SetDissolveEdgeThickness(dissolveEffect_.edgeThickness);
		for (auto& p : parts_) { p.object->SetDissolveEdgeThickness(dissolveEffect_.edgeThickness); }
	}
	if (ImGui::ColorEdit4("Edge Color", reinterpret_cast<float*>(&dissolveEffect_.edgeColor))) {
		body_.object->SetDissolveEdgeColor(dissolveEffect_.edgeColor);
		for (auto& p : parts_) { p.object->SetDissolveEdgeColor(dissolveEffect_.edgeColor); }
	}
	ImGui::End();

	// --- ビューモデル設定 ---

	ImGui::Begin("Viewmodel (Arm) Tuning");
	ImGui::SliderFloat("Base FOV (deg)", &vm_.baseFovDeg, 40.0f, 100.0f, "%.1f");
	ImGui::DragFloat3("Base Offset (R,U,F)", &vm_.baseOffset.x, 0.01f);
	ImGui::Checkbox("Lock Size By FOV", &vm_.lockSizeByFov);
	ImGui::DragFloat3("Base Scale", &vm_.baseScale.x, 0.01f, 0.1f, 4.0f);
	ImGui::End();

	// 各パーツの表示/非表示切替
	ImGui::Begin("Player Parts Visibility");
	ImGui::Checkbox("Body", &body_.active);
	ImGui::Checkbox("Head", &parts_[partIndices_.head].active);
	ImGui::Checkbox("Right Arm", &parts_[partIndices_.rightArm].active);
	ImGui::Checkbox("Left Arm", &parts_[partIndices_.leftArm].active);
	ImGui::Checkbox("Right Leg", &parts_[partIndices_.rightLeg].active);
	ImGui::Checkbox("Left Leg", &parts_[partIndices_.leftLeg].active);
	ImGui::End();

	// 各パーツの座標調整
	ImGui::Begin("Player Parts Transform");
	ImGui::DragFloat3("Body Position", &body_.transform.translate_.x, 0.01f);
	ImGui::DragFloat3("Body Rotation", &body_.transform.rotate_.x, 0.01f);
	ImGui::DragFloat3("Head Position", &parts_[partIndices_.head].transform.translate_.x, 0.01f);
	ImGui::DragFloat3("Head Rotation", &parts_[partIndices_.head].transform.rotate_.x, 0.01f);
	ImGui::DragFloat3("Right Arm Position", &parts_[partIndices_.rightArm].transform.translate_.x, 0.01f);
	ImGui::DragFloat3("Right Arm Rotation", &parts_[partIndices_.rightArm].transform.rotate_.x, 0.01f);
	ImGui::DragFloat3("Left Arm Position", &parts_[partIndices_.leftArm].transform.translate_.x, 0.01f);
	ImGui::DragFloat3("Left Arm Rotation", &parts_[partIndices_.leftArm].transform.rotate_.x, 0.01f);
	ImGui::DragFloat3("Right Leg Position", &parts_[partIndices_.rightLeg].transform.translate_.x, 0.01f);
	ImGui::DragFloat3("Right Leg Rotation", &parts_[partIndices_.rightLeg].transform.rotate_.x, 0.01f);
	ImGui::DragFloat3("Left Leg Position", &parts_[partIndices_.leftLeg].transform.translate_.x, 0.01f);
	ImGui::DragFloat3("Left Leg Rotation", &parts_[partIndices_.leftLeg].transform.rotate_.x, 0.01f);
	ImGui::End();

	// --- 武器管理 ---
	weaponManager_->DrawWeaponImGui();
#endif // USE_IMGUI
}
