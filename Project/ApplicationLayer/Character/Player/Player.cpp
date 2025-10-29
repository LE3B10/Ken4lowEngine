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
#include <imgui.h>
#include <AABB.h>

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
	if (body_.object) BaseCharacter::ApplySkinTo(body_.object.get(), skinTexturePath_);
	for (auto& part : parts_) { if (part.object) BaseCharacter::ApplySkinTo(part.object.get(), skinTexturePath_); }

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
		SetBodyActive(false);          // 体幹は映さない
		SetAllPartsActive(false);      // 一旦全部オフ
		SetPartActive(partIndices_.rightArm, true); // 右手だけオン
		break;

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

	// 移動処理
	Move(deltaTime);

	// クールダウン更新
	if (fireState_.cooldown > 0.0f) fireState_.cooldown -= deltaTime;

	// マウス左ボタンで射撃
	if (input_->PushMouse(0))
	{
		// クールダウン終了で発射可能
		if (fireState_.cooldown <= 0.0f)
		{
			// forward の計算（あなたの式でOK）
			float yaw = fpsCamera_->GetYaw();
			float pitch = fpsCamera_->GetPitch();
			Vector3 forward = {
				-std::sinf(yaw) * std::cosf(pitch), // x
				-std::sinf(pitch),                  // y
				 std::cosf(yaw) * std::cosf(pitch)  // z
			};

			// 正規化
			forward = Vector3::Normalize(forward);
			Vector3 worldUp = { 0.0f, 1.0f, 0.0f }; // ワールドの上方向
			Vector3 right = Vector3::Normalize(Vector3::Cross(worldUp, forward)); // 右方向

			// 弾道エフェクト開始
			const WeaponConfig& config = weaponManager_->GetCurrentConfig(); // 現在の武器設定取得
			Vector3 velocity = forward * config.muzzleSpeed;				 // 銃口初速を掛ける
			weaponManager_->StartFireBallisticEffect(GetCenterPosition(), velocity); // プレイヤー中心位置から発射
			fireState_.interval = 60.0f / config.rpm; 					 // 連射間隔計算（秒）
			fireState_.cooldown = fireState_.interval; 					 // クールダウンリセット
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
	weaponManager_->DrawWeapons();
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
	const Vector3 offset = { 0.0f,0.0f,0.0f };
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
	const float moveSpeed = 0.1f;
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
		// 三人称：補間（現状のまま）
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
		armT.parent_ = nullptr;  // 親を外してワールド直置きにする

		// 右下・前に出すカメラローカルオフセット
		Vector3 offset = { 0.75f, -0.75f, 0.75 };

		// カメラ姿勢でオフセットを回す（Yaw→Pitch）
		Matrix4x4 Ry = Matrix4x4::MakeRotateY(camYaw);
		Matrix4x4 Rx = Matrix4x4::MakeRotateX(camPitch);
		Matrix4x4 R = Matrix4x4::Multiply(Rx, Ry);
		offset = Matrix4x4::Transform(offset, R);

		// カメラ位置＋回したオフセット（画面に固定される）
		const Vector3 camPos = fpsCamera_->GetCamera()->GetTranslate();
		armT.translate_ = camPos + offset;
		armT.rotate_.y = camYaw;
		armT.rotate_.x = camPitch - (90.0f * std::numbers::pi_v<float> / 180.0f);
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

		// progress: 0→1（deltaTimeは掛けない）
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

	// --- 武器管理 ---
	weaponManager_->DrawWeaponImGui();
}
