#define NOMINMAX
#include "BallisticEffect.h"
#include "CollisionManager.h"
#include <CollisionTypeIdDef.h>

#include <algorithm>
#include <cmath>

#include <imgui.h>
#include <CollisionUtility.h>

// 省略 <numbers>
using namespace std::numbers;

/// 銃口のワールド座標を計算する（親Transform＋ローカルオフセット）
static inline Vector3 ComputeMuzzleWorld(const WorldTransformEx* parent, const WorldTransformEx& self, const Vector3& localOffset)
{
	// 親が無いなら自分の transform から（フォールバック）
	if (!parent) {
		// self.worldMatrix_ が最新でない可能性もあるので Update
		const_cast<WorldTransformEx&>(self).Update();
		return Matrix4x4::Transform(localOffset, self.worldMatrix_);
	}

	// 親の回転（Yaw→Pitch）を作る
	Matrix4x4 Rx = Matrix4x4::MakeRotateX(parent->rotate_.x);
	Matrix4x4 Ry = Matrix4x4::MakeRotateY(parent->rotate_.y);
	Matrix4x4 R = Matrix4x4::Multiply(Rx, Ry);

	// 右腕モデル由来の -90° を打ち消す +90° 補正（Pistol と同じ方針）
	constexpr float kHalfPi = std::numbers::pi_v<float> *0.5f;
	Matrix4x4 RxFix = Matrix4x4::MakeRotateX(+kHalfPi);

	// ローカルオフセットを補正→親回転へ→親位置へ
	Vector3 ofsFixed = Matrix4x4::Transform(localOffset, RxFix);
	Vector3 ofsWorld = Matrix4x4::Transform(ofsFixed, R);
	return parent->translate_ + ofsWorld;
}

/// -------------------------------------------------------------
///				　			　 初期化処理
/// -------------------------------------------------------------
void BallisticEffect::Initialize()
{
	trails_.reserve(maxSegments_); // 軌跡セグメントの最大数を予約
	bullets_.clear();

	objectPool_.clear();
	freeList_.clear();
	objectPool_.reserve(maxSegments_);

	// --- 軌跡セグメント用プール（細長い棒） ---
	for (uint32_t i = 0; i < maxSegments_; ++i)
	{
		auto obj = std::make_unique<Object3D>();
		obj->Initialize("cube.gltf");

		Object3D* raw = obj.get();                // 先に生ポインタを取る
		objectPool_.push_back(std::move(obj));    // 1) プールに入れる
		freeList_.push_back(raw);                 // 2) 空きリストに積む
	}

	// --- マズルフラッシュ用プール（板 or 短い棒） ---
	flashPool_.clear();
	flashFree_.clear();
	flashPool_.reserve(maxFlashes_);
	for (uint32_t i = 0; i < maxFlashes_; ++i)
	{
		auto obj = std::make_unique<Object3D>();
		// 手持ちのモデルでOK： "quad.gltf" が理想。なければ "cube.gltf" を薄く伸ばして使う
		obj->Initialize("cube.gltf");
		flashFree_.push_back(obj.get());
		flashPool_.push_back(std::move(obj));
	}

	// スパーク用プール
	sparkPool_.clear();
	sparkFree_.clear();
	sparkPool_.reserve(maxSparks_);
	for (uint32_t i = 0; i < maxSparks_; ++i)
	{
		auto obj = std::make_unique<Object3D>();
		obj->Initialize("cube.gltf");        // 四角を細く伸ばして使う
		sparkFree_.push_back(obj.get());
		sparkPool_.push_back(std::move(obj));
	}

	// 薬莢用プール
	casingPool_.clear();
	casingFree_.clear();
	casingPool_.reserve(maxCasings_);
	for (uint32_t i = 0; i < maxCasings_; ++i) {
		auto obj = std::make_unique<Object3D>();
		obj->Initialize("cube.gltf"); // 細長い直方体で代用
		casingFree_.push_back(obj.get());
		casingPool_.push_back(std::move(obj));
	}
}

/// -------------------------------------------------------------
///				　			　 更新処理
/// -------------------------------------------------------------
void BallisticEffect::Update()
{
	const float dt = 1.0f / 60.0f;

	// ----- 弾丸の更新 -----
	for (auto& b : bullets_)
	{
		if (!b.alive) continue;

		Vector3 prev = b.position;

		// 物理
		b.velocity.y += gravityY_ * dt;							 // 重力
		if (drag_ > 0.0f) b.velocity -= b.velocity * drag_ * dt; // 空気抵抗
		b.position += b.velocity * dt;							 // 位置更新

		// 衝突判定
		b.traveled += Vector3::Length(b.position - prev);

		// === コライダーを最新化 ===
		if (b.collider)
		{
			// 弾を中心としておく（デバッグ用）
			b.collider->SetCenterPosition(b.position);

			// 1フレームぶんの軌跡を線分として登録
			Segment seg{};
			seg.origin = prev;
			seg.diff = (b.position - prev); // 終点 = prev + diff
			b.collider->SetSegment(seg);
		}

		// ===== 単一セグメント（1発＝1本）を更新 =====
		if (currentWeapon_.tracer.enabled)
		{
			Vector3 v = b.velocity;
			float   speed = Vector3::Length(v);
			Vector3 dir = (speed > 1e-6f) ? (v / speed) : Vector3{ 0,0,1 };

			// 望む見た目の長さ
			float len = currentWeapon_.tracer.tracerLength;
			Vector3 tail = b.position - dir * len;

			// 自分のセグメントを探す
			TrailSegment* seg = nullptr;
			for (auto& s : trails_) {
				if (s.attached && s.ownerId == b.userShotCount) { seg = &s; break; }
			}

			if (!seg)
			{
				// まだない → プールから1本借りて作る
				if (!freeList_.empty()) {
					Object3D* obj = freeList_.back(); freeList_.pop_back();
					TrailSegment t{};
					t.object = obj;
					t.p0 = tail;
					t.p1 = b.position;
					t.width = currentWeapon_.tracer.tracerWidth;
					t.color = currentWeapon_.tracer.tracerColor;
					t.age = 0.0f;
					t.life = 1e9f;     // ほぼ無限（外側ではフェードさせない）
					t.alive = true;
					t.attached = true;     // 弾に付随
					t.ownerId = b.userShotCount;
					trails_.push_back(t);
				}
			}
			else
			{
				// 既存を更新（フェードさせないので age を毎フレ0に）
				seg->p0 = tail;
				seg->p1 = b.position;
				seg->width = currentWeapon_.tracer.tracerWidth;
				seg->color = currentWeapon_.tracer.tracerColor;
				seg->age = 0.0f;
				seg->life = 1e9f;
				seg->alive = true;
			}
		}

		// 最大距離や速度で弾を終了
		float speedNow = Vector3::Length(b.velocity);
		if (b.traveled > currentWeapon_.maxDistance || speedNow < 1.0f)
		{
			b.alive = false;

			// この弾に紐づくトレーサーを終了
			for (auto& s : trails_)
			{
				if (s.attached && s.ownerId == b.userShotCount)
				{
					s.alive = false;
					break;
				}
			}
		}

		// 死んだ弾はコライダーをCollisionManagerから外して破棄
		if (!b.alive && b.collider)
		{
			if (collisionMgr_)
			{
				collisionMgr_->RemoveCollider(b.collider);
			}
			delete b.collider;
			b.collider = nullptr;
		}

	}

	// ----- 軌跡セグメントの寿命管理 -----
	for (auto& s : trails_)
	{
		if (!s.alive) continue;
		if (s.attached) continue; // 弾に付随するものは寿命管理しない
		s.age += dt;
		if (s.age >= s.life) s.alive = false;
	}

	// 死んだセグメントを回収
	for (auto it = trails_.begin(); it != trails_.end();)
	{
		if (!it->alive)
		{
			// プールに戻す
			freeList_.push_back(it->object);
			it = trails_.erase(it);
		}
		else
		{
			++it;
		}
	}

	// 死んだ弾を消す
	bullets_.erase(std::remove_if(bullets_.begin(), bullets_.end(),
		[](const Bullet& b) { return !b.alive; }), bullets_.end());

	// ----- マズルフラッシュ更新 -----
	for (auto& f : flashes_)
	{
		if (!f.alive) continue;

		// 追従（親＋offset_ を毎フレ再計算）
		if (parentTransform_) {
			Vector3 base = ComputeMuzzleWorld(parentTransform_, transform_, offset_);
			f.pos = base + f.dir * currentWeapon_.muzzle.offsetForward;
		}

		f.age += (1.0f / 60.0f);
		if (f.age >= f.life) f.alive = false;
	}
	// 死んだものを返却
	for (auto fl = flashes_.begin(); fl != flashes_.end(); )
	{
		if (!fl->alive)
		{
			if (fl->object) flashFree_.push_back(fl->object);
			fl = flashes_.erase(fl);
		}
		else ++fl;
	}

	// スパーク更新
	for (auto& s : sparks_)
	{
		if (!s.alive) continue;
		s.age += dt;
		if (s.age >= s.life) { s.alive = false; continue; }

		// 重力 & 簡易減衰（空気抵抗が欲しければ少しずつ減衰）
		s.vel.y += currentWeapon_.muzzle.sparkGravityY * dt;
		s.pos += s.vel * dt;
	}

	// 消滅したスパークを返却
	for (auto sp = sparks_.begin(); sp != sparks_.end(); ) {
		if (!sp->alive) {
			if (sp->object) sparkFree_.push_back(sp->object);
			sp = sparks_.erase(sp);
		}
		else ++sp;
	}

	// ----- 薬莢更新 -----
	for (auto& c : casings_) {
		if (!c.alive) continue;

		c.age += dt;
		if (c.age >= c.life) { c.alive = false; continue; }

		// 力学
		c.vel.y += currentWeapon_.casing.gravityY * dt;
		c.vel -= c.vel * currentWeapon_.casing.drag * dt; // 簡易抗力
		c.pos += c.vel * dt;

		// 回転
		c.ang += c.angVel * dt;
	}
	// 返却
	for (auto ca = casings_.begin(); ca != casings_.end(); ) {
		if (!ca->alive) {
			if (ca->object) casingFree_.push_back(ca->object);
			ca = casings_.erase(ca);
		}
		else ++ca;
	}
}

/// -------------------------------------------------------------
///				　			　 描画処理
/// -------------------------------------------------------------
void BallisticEffect::Draw()
{
	for (auto& s : trails_)
	{
		if (!s.alive || !s.object) continue;

		Vector3 dir = s.p1 - s.p0;
		float   len = Vector3::Length(dir);
		if (len <= 1e-6f) continue;
		dir = dir / len;

		// αフェード
		float alpha = 1.0f;
		if (!s.attached) {
			alpha = std::max(0.0f, 1.0f - s.age / s.life);
		}
		s.object->SetColor({ s.color.x, s.color.y, s.color.z, alpha });

		// +Z を dir に合わせる（Yaw-Pitch回転）
		// yaw: XZ 平面の角度, pitch: 上下の角度（ロールは不要）
		float yaw = std::atan2(-dir.x, dir.z);
		float pitch = -std::asin(dir.y);
		s.object->SetRotate({ pitch, yaw, 0.0f });
		s.object->SetScale({ s.width, s.width, len * 0.25f }); // ←使わない
		s.object->SetTranslate((s.p0 + s.p1) * 0.5f);

		// 更新→描画
		s.object->Update(); // ←カメラ＆行列更新が入る
		s.object->Draw();
	}

	// ----- マズルフラッシュ描画 -----
	for (auto& f : flashes_)
	{
		if (!f.alive || !f.object) continue;

		// フェード（先に強く、すぐ消える）
		float t = std::clamp(f.age / f.life, 0.0f, 1.0f);
		float alpha = 1.0f - t; // 直線でOK（好みで曲線に）

		// 大きさを補間（しぼむ）
		float len = f.startLen + (f.endLen - f.startLen) * t;
		float wid = f.startWid + (f.endWid - f.startWid) * t;

		// オブジェクト変形（+Z を dir へ、中点へ）
		float yaw = std::atan2(-f.dir.x, f.dir.z);
		float pitch = -std::asin(f.dir.y);
		f.object->SetRotate({ pitch, yaw, 0.0f });
		f.object->SetScale({ wid, wid, len });
		f.object->SetTranslate(f.pos + f.dir * (len * 0.5f)); // 先端方向に半分押し出す

		f.object->SetColor({ f.color.x, f.color.y, f.color.z, alpha });
		f.object->Update();
		f.object->Draw();  // ※加算合成PSOがあるならそちらを使うとなお良い
	}

	// スパーク描画（短い線分＝細い棒）
	for (auto& s : sparks_)
	{
		if (!s.alive || !s.object) continue;

		float t = std::clamp(s.age / s.life, 0.0f, 1.0f);
		// 色を補間（オレンジ→赤→α0）
		Vector4 col{
			s.col0.x + (s.col1.x - s.col0.x) * t,
			s.col0.y + (s.col1.y - s.col0.y) * t,
			s.col0.z + (s.col1.z - s.col0.z) * t,
			s.col0.w + (s.col1.w - s.col0.w) * t
		};

		// 向き＆“尾”っぽい長さ（速度に比例）
		Vector3 dir = (Vector3::Length(s.vel) > 1e-6f) ? Vector3::Normalize(s.vel) : Vector3{ 0,0,1 };
		float   len = std::clamp(Vector3::Length(s.vel) * 0.015f, 0.03f, 0.12f);

		float yaw = std::atan2(-dir.x, dir.z);
		float pitch = -std::asin(dir.y);

		s.object->SetRotate({ pitch, yaw, 0.0f });
		s.object->SetScale({ s.width, s.width, len });
		s.object->SetTranslate(s.pos - dir * (len * 0.5f)); // 尾が後ろに伸びるよう微オフセット
		s.object->SetColor(col);
		s.object->Update();
		s.object->Draw();
	}

	// ----- 薬莢描画 -----
	for (auto& c : casings_) {
		if (!c.alive || !c.object) continue;

		// 位置・回転・スケール・色
		c.object->SetTranslate(c.pos);
		c.object->SetRotate(c.ang);
		c.object->SetScale(c.scale);
		c.object->SetColor(c.color);

		c.object->Update();
		c.object->Draw();
	}
}

/// -------------------------------------------------------------
///				　			　 弾道開始
/// -------------------------------------------------------------
void BallisticEffect::Start(const Vector3& position, const Vector3& velocity, const WeaponConfig& weapon)
{
	currentWeapon_ = weapon;
	Vector3 basePos = position;
	if (parentTransform_) basePos = ComputeMuzzleWorld(parentTransform_, transform_, offset_);

	Vector3 fwd = Vector3::Length(velocity) > 0.0f ? Vector3::Normalize(velocity) : Vector3{ 0,0,1 };
	Vector3 muzzlePos = basePos + fwd * weapon.muzzle.offsetForward;
	Vector3 sparkPos = basePos + fwd * weapon.muzzle.sparkOffsetForward;
	Vector3 bulletBasePos = basePos + fwd * weapon.tracer.startOffsetForward;

	// マズルフラッシュ & スパーク & 薬莢（既存）
	if (weapon.muzzle.enabled) {
		SpawnMuzzleFlash(muzzlePos, fwd, weapon);
		if (weapon.muzzle.sparksEnabled) SpawnMuzzleSparks(sparkPos, fwd, weapon);
	}
	if (weapon.casing.enabled) SpawnCasing(basePos, fwd, weapon);

	// --- 散弾処理開始 ---
	int pellets = std::max(1u, weapon.bulletsPerShot);
	float coneRad = (weapon.spreadDeg * (std::numbers::pi_v<float> / 180.0f)) * 0.5f; // 半角（左右上下に広がるので半分）
	auto rand01 = []() { return (float)rand() / (float)RAND_MAX; };

	// Decide tracer behavior
	int tracerMode = weapon.pelletTracerMode; // 0=none,1=one,2=all
	int tracerCount = std::max(1, weapon.pelletTracerCount);

	// If tracerMode==1 and tracerCount>1, pick tracerCount distinct pellet indices
	std::vector<int> tracerIndices;
	if (tracerMode == 1) {
		// choose tracerCount unique indices
		tracerIndices.reserve(tracerCount);
		for (int i = 0; i < tracerCount; i++) {
			int idx = (int)(rand01() * pellets);
			tracerIndices.push_back(idx % pellets);
		}
	}

	for (int i = 0; i < pellets; ++i)
	{
		float u = rand01();
		float v = rand01();
		float theta = coneRad * std::sqrt(u); // sqrt to avoid edge clustering
		float phi_ = 2.0f * std::numbers::pi_v<float> *v;

		// build orthonormal basis around fwd
		Vector3 z = Vector3::Normalize(fwd);
		Vector3 x = Vector3::Normalize((fabs(z.y) < 0.999f) ? Vector3{ -z.z,0,z.x } : Vector3{ 1,0,0 });
		Vector3 y = Vector3::Normalize(Vector3::Cross(z, x));

		Vector3 dir = Vector3::Normalize(
			x * (std::sin(theta) * std::cos(phi_)) +
			y * (std::sin(theta) * std::sin(phi_)) +
			z * (std::cos(theta))
		);

		// 弾速ベクトル
		Vector3 pelletVel = dir * weapon.muzzleSpeed;

		// 弾丸を追加
		bool placed = false;
		for (auto& b : bullets_)
		{
			if (!b.alive)
			{
				b.position = bulletBasePos;
				b.velocity = pelletVel;
				b.alive = true;
				b.traveled = 0.0f;
				b.userShotCount = ++shotCounter_;
				placed = true;

				// コライダーの用意
				if (b.collider == nullptr)
				{
					b.collider = new Collider();
					b.collider->Initialize();

					// このコライダーは「弾」なので Bullet のタイプIDを入れる
					b.collider->SetTypeID(static_cast<uint32_t>(CollisionTypeIdDef::kBullet));

					// デバッグ用にOBBを無効っぽくする(半サイズ0なら描画されない仕様)
					b.collider->SetOBBHalfSize({ 0.0f,0.0f,0.0f });

					// CollisionManagerに登録
					if (collisionMgr_) {
						collisionMgr_->AddCollider(b.collider);
					}
				}

				// 初期位置の更新(中心座標として持たせておくとImGuiで見やすい)
				b.collider->SetCenterPosition(b.position);

				// Segment初期化（まだ動いてないので長さ0でOK）
				Segment seg{};
				seg.origin = b.position;
				seg.diff = { 0.0f,0.0f,0.0f };
				b.collider->SetSegment(seg);

				break;
			}
		}

		// 空きがなければ新規追加
		if (!placed)
		{
			Bullet nb{};
			nb.position = bulletBasePos;
			nb.velocity = pelletVel;
			nb.alive = true;
			nb.traveled = 0.0f;
			nb.userShotCount = ++shotCounter_;

			// ★ Collider生成
			nb.collider = new Collider();
			nb.collider->Initialize();
			nb.collider->SetTypeID(static_cast<uint32_t>(CollisionTypeIdDef::kBullet));
			nb.collider->SetOBBHalfSize({ 0.0f,0.0f,0.0f });
			nb.collider->SetCenterPosition(nb.position);

			{
				Segment seg{};
				seg.origin = nb.position;
				seg.diff = { 0.0f,0.0f,0.0f };
				nb.collider->SetSegment(seg);
			}

			// Manager登録
			if (collisionMgr_) collisionMgr_->AddCollider(nb.collider);

			bullets_.push_back(nb);
		}

		// トレーサを出す条件
		bool spawnTracer = false;
		if (tracerMode == 2) spawnTracer = true; // all
		else if (tracerMode == 1) {
			// if this pellet's index was chosen for tracer
			for (int ti : tracerIndices) if (ti == i) { spawnTracer = true; break; }
		}
	}
}

/// -------------------------------------------------------------
///				　	マズル位置のワールド座標を取得
/// -------------------------------------------------------------
Vector3 BallisticEffect::GetMuzzleWorld() const
{
	return ComputeMuzzleWorld(parentTransform_, transform_, offset_);
}

void BallisticEffect::RegisterColliders(CollisionManager* mgr)
{
	if (!mgr) return;
	for (auto& b : bullets_)
	{
		if (b.alive && b.collider) {
			mgr->AddCollider(b.collider);
		}
	}
}

/// -------------------------------------------------------------
///				　		　セグメントを1本追加
/// -------------------------------------------------------------
void BallisticEffect::PushTrail(const Vector3& p0, const Vector3& p1, float speed, const WeaponConfig& weapon)
{
	// 間引き
	float segLen = Vector3::Length(p1 - p0);
	if (segLen < weapon.tracer.minSegLength) return;

	// weapon.tracer.tracerLength(メートル) を基準に life を決定
	// life = desiredLength / speed  (speed in m/s) -> 高速ならlife短くても長く見える
	float life = 0.1f;
	if (weapon.tracer.enabled && speed > 0.001f)
	{
		life = weapon.tracer.tracerLength / speed;
		life = std::clamp(life, 0.02f, 1.0f); // 安全範囲
	}
	else
	{
		life = maxLife_; // フォールバック
	}

	// セグメントを作る（プールから借りる）
	if (freeList_.empty()) return;
	Object3D* obj = freeList_.back(); freeList_.pop_back();

	TrailSegment t{};
	t.p0 = p0; t.p1 = p1;
	t.life = life;
	t.width = weapon.tracer.tracerWidth;
	t.color = weapon.tracer.tracerColor;
	t.age = 0.0f;
	t.alive = true;
	t.object = obj;

	trails_.push_back(std::move(t));
}

/// -------------------------------------------------------------
///				　		　マズルフラッシュを追加
/// -------------------------------------------------------------
void BallisticEffect::SpawnMuzzleFlash(const Vector3& position, const Vector3& forward, const WeaponConfig& weapon)
{
	// プールに空きがなければ出せない
	if (flashFree_.empty()) return;

	// 方向を少しランダムに散らす（過度にしない）
	auto rand01 = []() { return (float)rand() / (float)RAND_MAX; };
	float yawRad = weapon.muzzle.randomYawDeg * (std::numbers::pi_v<float> / 180.0f) * (rand01() * 2.0f - 1.0f);

	// forward をXZで少し回す
	Vector3 dir = forward;
	{
		float c = std::cos(yawRad), s = std::sin(yawRad);
		Vector3 xz = { dir.x * c - dir.z * s, dir.y, dir.x * s + dir.z * c };
		dir = Vector3::Normalize(xz);
	}

	Object3D* obj = flashFree_.back(); flashFree_.pop_back();

	MuzzleFlash mf{};
	mf.object = obj;
	mf.pos = position;
	mf.dir = dir;
	mf.life = weapon.muzzle.life;
	mf.startLen = weapon.muzzle.startLength;
	mf.endLen = weapon.muzzle.endLength;
	mf.startWid = weapon.muzzle.startWidth;
	mf.endWid = weapon.muzzle.endWidth;
	mf.color = weapon.muzzle.color;
	mf.age = 0.0f;
	mf.alive = true;

	flashes_.push_back(mf);
}

/// -------------------------------------------------------------
///				　		　マズルスパークを生成
/// -------------------------------------------------------------
void BallisticEffect::SpawnMuzzleSparks(const Vector3& pos, const Vector3& forward, const WeaponConfig& weapon)
{
	if (sparkFree_.empty()) return;

	auto rand01 = []() { return (float)rand() / (float)RAND_MAX; };
	const float cone = weapon.muzzle.sparkConeDeg * (pi_v<float> / 180.0f);

	uint32_t count = std::min(weapon.muzzle.sparkCount, (int)sparkFree_.size());
	for (uint32_t i = 0; i < count; ++i) {
		// 前方を中心にしたランダム方向（円錐分布）
		float u = rand01(), v = rand01();
		float theta = cone * std::sqrt(u);      // 端に寄り過ぎないように sqrt
		float phi_ = 2.0f * pi_v<float> *v;

		// 直交基底を作って forward を中心に回す
		Vector3 z = Vector3::Normalize(forward);
		Vector3 x = Vector3::Normalize((fabs(z.y) < 0.999f) ? Vector3{ -z.z,0,z.x } : Vector3{ 1,0,0 });
		Vector3 y = Vector3::Normalize(Vector3::Cross(z, x));
		Vector3 dir = Vector3::Normalize(x * (std::sin(theta) * std::cos(phi_)) +
			y * (std::sin(theta) * std::sin(phi_)) +
			z * (std::cos(theta)));

		float speed = weapon.muzzle.sparkSpeedMin +
			(weapon.muzzle.sparkSpeedMax - weapon.muzzle.sparkSpeedMin) * rand01();

		if (sparkFree_.empty()) break;
		Object3D* obj = sparkFree_.back(); sparkFree_.pop_back();

		Spark sp{};
		sp.object = obj;
		sp.pos = pos;
		sp.vel = dir * speed;
		sp.life = weapon.muzzle.sparkLifeMin +
			(weapon.muzzle.sparkLifeMax - weapon.muzzle.sparkLifeMin) * rand01();
		sp.width = weapon.muzzle.sparkWidth;
		sp.col0 = weapon.muzzle.sparkColorStart;
		sp.col1 = weapon.muzzle.sparkColorEnd;
		sp.alive = true;

		sparks_.push_back(sp);
	}
}

/// -------------------------------------------------------------
///				　		　薬莢を生成
/// -------------------------------------------------------------
void BallisticEffect::SpawnCasing(const Vector3& basePos, const Vector3& forward, const WeaponConfig& weapon)
{
	if (casingFree_.empty()) return;

	Vector3 z = Vector3::Normalize(forward);
	Vector3 worldUp = { 0,1,0 };

	// 右 = worldUp × forward
	Vector3 x = Vector3::Normalize(Vector3::Cross(worldUp, z));

	// 上 = forward × 右
	Vector3 y = Vector3::Normalize(Vector3::Cross(z, x));

	// スポーン位置：銃口根元から 右・上・後ろ へずらす
	Vector3 spawnPos = basePos
		+ x * weapon.casing.offsetRight
		+ y * weapon.casing.offsetUp
		- z * weapon.casing.offsetBack;

	// 右方向を中心に円錐でばらす
	auto rand01 = []() { return (float)rand() / (float)RAND_MAX; };
	float theta = (weapon.casing.coneDeg * std::numbers::pi_v<float> / 180.0f) * std::sqrt(rand01());
	float phi_ = 2.0f * std::numbers::pi_v<float> *rand01();
	// 右(x)を中心軸にする
	Vector3 dir = Vector3::Normalize(
		x * std::cos(theta) +
		(y * std::cos(phi_) + z * std::sin(phi_)) * std::sin(theta)
	);

	dir = Vector3::Normalize(dir + y * weapon.casing.upBias);

	float speed = weapon.casing.speedMin + (weapon.casing.speedMax - weapon.casing.speedMin) * rand01();

	Vector3 vel = dir * speed + y * weapon.casing.upKick;

	Object3D* obj = casingFree_.back(); casingFree_.pop_back();

	// 薬莢生成
	Casing c{};
	c.object = obj;
	c.pos = spawnPos;
	c.vel = vel;
	c.ang = { 0,0,0 };
	// 適当にくるくる回す（右へ強め）
	c.angVel = {
		weapon.casing.spinMin + (weapon.casing.spinMax - weapon.casing.spinMin) * rand01(),
		weapon.casing.spinMin * 0.3f,
		weapon.casing.spinMin * 0.2f
	};
	c.life = weapon.casing.life;
	c.color = weapon.casing.color;
	c.scale = weapon.casing.scale;
	c.alive = true;

	casings_.push_back(c);
}
