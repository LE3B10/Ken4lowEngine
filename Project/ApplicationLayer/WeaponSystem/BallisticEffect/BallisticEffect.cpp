#define NOMINMAX
#include "BallisticEffect.h"
#include "CollisionManager.h"
#include <CollisionTypeIdDef.h>
#include <CollisionUtility.h>

#include <algorithm>
#include <cmath>

namespace K4E = ::Ken4lowEngine;

// 省略 <numbers>
using namespace std::numbers;

// ------------------------------------------------------------
// K4E::Collider の RAII 破棄（RemoveCollider → delete）
// ------------------------------------------------------------
void BallisticEffect::ColliderDeleter::operator()(K4E::Collider* p) const noexcept
{
	if (!p) { return; }
	if (mgr) { mgr->RemoveCollider(p); }
	delete p;
}



/// 銃口のワールド座標を計算する（親Transform＋ローカルオフセット）
static inline K4E::Vector3 ComputeMuzzleWorld(const K4E::WorldTransformEx* parent, const K4E::WorldTransformEx& self, const K4E::Vector3& localOffset)
{
	// 親が無いなら自分の transform から（フォールバック）
	if (!parent) {
		// self.worldMatrix_ が最新でない可能性もあるので Update
		const_cast<K4E::WorldTransformEx&>(self).Update();
		return K4E::Matrix4x4::Transform(localOffset, self.worldMatrix_);
	}

	// 親の回転（Yaw→Pitch）を作る
	K4E::Matrix4x4 Rx = K4E::Matrix4x4::MakeRotateX(parent->rotate_.x);
	K4E::Matrix4x4 Ry = K4E::Matrix4x4::MakeRotateY(parent->rotate_.y);
	K4E::Matrix4x4 R = K4E::Matrix4x4::Multiply(Rx, Ry);

	// 右腕モデル由来の -90° を打ち消す +90° 補正（Pistol と同じ方針）
	constexpr float kHalfPi = std::numbers::pi_v<float> *0.5f;
	K4E::Matrix4x4 RxFix = K4E::Matrix4x4::MakeRotateX(+kHalfPi);

	// ローカルオフセットを補正→親回転へ→親位置へ
	K4E::Vector3 ofsFixed = K4E::Matrix4x4::Transform(localOffset, RxFix);
	K4E::Vector3 ofsWorld = K4E::Matrix4x4::Transform(ofsFixed, R);
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
		auto obj = std::make_unique<K4E::Object3D>();
		obj->Initialize("cube.gltf");

		K4E::Object3D* raw = obj.get();                // 先に生ポインタを取る
		objectPool_.push_back(std::move(obj));    // 1) プールに入れる
		freeList_.push_back(raw);                 // 2) 空きリストに積む
	}

	// --- マズルフラッシュ用プール（板 or 短い棒） ---
	flashPool_.clear();
	flashFree_.clear();
	flashPool_.reserve(maxFlashes_);
	for (uint32_t i = 0; i < maxFlashes_; ++i)
	{
		auto obj = std::make_unique<K4E::Object3D>();
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
		auto obj = std::make_unique<K4E::Object3D>();
		obj->Initialize("cube.gltf");        // 四角を細く伸ばして使う
		sparkFree_.push_back(obj.get());
		sparkPool_.push_back(std::move(obj));
	}

	// 薬莢用プール
	casingPool_.clear();
	casingFree_.clear();
	casingPool_.reserve(maxCasings_);
	for (uint32_t i = 0; i < maxCasings_; ++i) {
		auto obj = std::make_unique<K4E::Object3D>();
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

		K4E::Vector3 prev = b.position;

		// 物理
		b.velocity.y += gravityY_ * dt;							 // 重力
		if (drag_ > 0.0f) b.velocity -= b.velocity * drag_ * dt; // 空気抵抗
		b.position += b.velocity * dt;							 // 位置更新

		// 衝突判定
		b.traveled += K4E::Vector3::Length(b.position - prev);

		// ===== 単一セグメント（1発＝1本）を更新 =====
		if (currentWeapon_.tracer.enabled)
		{
			K4E::Vector3 v = b.velocity;
			float   speed = K4E::Vector3::Length(v);
			K4E::Vector3 dir = (speed > 1e-6f) ? (v / speed) : K4E::Vector3{ 0,0,1 };

			// 望む見た目の長さ
			float len = currentWeapon_.tracer.tracerLength;
			K4E::Vector3 tail = b.position - dir * len;

			// 自分のセグメントを探す
			TrailSegment* seg = nullptr;
			for (auto& s : trails_) {
				if (s.attached && s.ownerId == b.userShotCount) { seg = &s; break; }
			}

			if (!seg)
			{
				// まだない → プールから1本借りて作る
				if (!freeList_.empty()) {
					K4E::Object3D* obj = freeList_.back(); freeList_.pop_back();
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
				// 更新（フェードさせないので age を毎フレ0に）
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
		float speedNow = K4E::Vector3::Length(b.velocity);
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
	}

	// 死んだ弾を消す
	bullets_.erase(std::remove_if(bullets_.begin(), bullets_.end(),
		[](const Bullet& b) { return !b.alive; }), bullets_.end());

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

	// ----- コライダー弾丸の更新 -----
	for (auto& c : colliderBullets_)
	{
		if (!c.alive) continue;

		c.prev = c.position;

		// 物理
		c.velocity.y += gravityY_ * dt;
		if (drag_ > 0.0f) c.velocity -= c.velocity * drag_ * dt;
		c.position += c.velocity * dt;

		c.traveled += K4E::Vector3::Length(c.position - c.prev);

		// === コライダー更新 ===
		if (c.collider)
		{
			c.collider->SetCenterPosition(c.position); // デバッグ可視化用
			K4E::Segment seg{};
			seg.origin = c.prev;
			seg.diff = (c.position - c.prev);
			c.collider->SetSegment(seg);
		}

		// 寿命/速度/距離で終了
		float speedNow = K4E::Vector3::Length(c.velocity);
		if (c.traveled > currentWeapon_.maxDistance || speedNow < 1.0f) {
			c.alive = false;
		}

		// 死亡後の後片付け（RemoveCollider→delete は unique_ptr のデリータで行う）
		if (!c.alive && c.collider) {
			c.collider.reset();
		}
	}

	// 死んだ弾の回収
	colliderBullets_.erase(
		std::remove_if(colliderBullets_.begin(), colliderBullets_.end(),
			[](const ColliderBullet& x) { return !x.alive; }),
		colliderBullets_.end()
	);

	// ----- マズルフラッシュ更新 -----
	for (auto& f : flashes_)
	{
		if (!f.alive) continue;

		// 追従（親＋offset_ を毎フレ再計算）
		if (parentTransform_) {
			K4E::Vector3 base = ComputeMuzzleWorld(parentTransform_, transform_, offset_);
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

		K4E::Vector3 dir = s.p1 - s.p0;
		float   len = K4E::Vector3::Length(dir);
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
		K4E::Vector4 col{
			s.col0.x + (s.col1.x - s.col0.x) * t,
			s.col0.y + (s.col1.y - s.col0.y) * t,
			s.col0.z + (s.col1.z - s.col0.z) * t,
			s.col0.w + (s.col1.w - s.col0.w) * t
		};

		// 向き＆“尾”っぽい長さ（速度に比例）
		K4E::Vector3 dir = (K4E::Vector3::Length(s.vel) > 1e-6f) ? K4E::Vector3::Normalize(s.vel) : K4E::Vector3{ 0,0,1 };
		float   len = std::clamp(K4E::Vector3::Length(s.vel) * 0.015f, 0.03f, 0.12f);

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
void BallisticEffect::Start(const K4E::Vector3& position, const K4E::Vector3& velocity, const WeaponConfig& weapon)
{
	currentWeapon_ = weapon;

	// --- 起点を分離 ---
	// 右腕（親Transform＋offset）…見た目・マズル用
	K4E::Vector3 basePosMuzzle = parentTransform_
		? ComputeMuzzleWorld(parentTransform_, transform_, offset_)
		: position;
	// プレイヤーのボディ（呼び出し側が渡す position）…衝突（セグメント）用
	K4E::Vector3 basePosBody = position;

	// 前方ベクトル
	K4E::Vector3 fwd = (K4E::Vector3::Length(velocity) > 1e-6f) ? K4E::Vector3::Normalize(velocity) : K4E::Vector3{ 0,0,1 };

	// マズル／スパークの出現位置（右腕基準）
	K4E::Vector3 muzzlePos = basePosMuzzle + fwd * weapon.muzzle.offsetForward;
	K4E::Vector3 sparkPos = basePosMuzzle + fwd * weapon.muzzle.sparkOffsetForward;

	// 見た目用の弾の初期位置（右腕＝銃口側から少し押し出す）
	K4E::Vector3 bulletBasePosVFX = basePosMuzzle + fwd * weapon.tracer.startOffsetForward;
	// 衝突用の弾の初期位置（ボディ側。自爆が気になるなら +fwd*小オフセット を好みで）
	K4E::Vector3 bulletBasePosCOL = basePosBody /* + fwd * 0.0f */;

	// --- 演出 ---
	if (weapon.muzzle.enabled) {
		SpawnMuzzleFlash(muzzlePos, fwd, weapon);
		if (weapon.muzzle.sparksEnabled) SpawnMuzzleSparks(sparkPos, fwd, weapon);
	}
	if (weapon.casing.enabled) SpawnCasing(basePosMuzzle, fwd, weapon);

	// --- 散弾設定（既存ロジック） ---
	int pellets = std::max(1u, weapon.bulletsPerShot);
	float coneRad = (weapon.spreadDeg * (std::numbers::pi_v<float> / 180.0f)) * 0.5f;
	auto rand01 = []() { return (float)rand() / (float)RAND_MAX; };

	int tracerMode = weapon.pelletTracerMode;
	int tracerCount = std::max(1, weapon.pelletTracerCount);
	std::vector<int> tracerIndices;
	if (tracerMode == 1) {
		tracerIndices.reserve(tracerCount);
		for (int i = 0; i < tracerCount; ++i) {
			int idx = (int)(rand01() * pellets);
			tracerIndices.push_back(idx % pellets);
		}
	}

	// --- 各ペレット発射 ---
	for (int i = 0; i < pellets; ++i)
	{
		// 円錐分布で散らす（既存）
		float u = rand01(), v = rand01();
		float theta = coneRad * std::sqrt(u);
		float phi_ = 2.0f * std::numbers::pi_v<float> *v;

		K4E::Vector3 z = K4E::Vector3::Normalize(fwd);
		K4E::Vector3 x = K4E::Vector3::Normalize((fabs(z.y) < 0.999f) ? K4E::Vector3{ -z.z,0,z.x } : K4E::Vector3{ 1,0,0 });
		K4E::Vector3 y = K4E::Vector3::Normalize(K4E::Vector3::Cross(z, x));
		K4E::Vector3 dir = K4E::Vector3::Normalize(
			x * (std::sin(theta) * std::cos(phi_)) +
			y * (std::sin(theta) * std::sin(phi_)) +
			z * (std::cos(theta))
		);
		K4E::Vector3 pelletVel = dir * weapon.muzzleSpeed;

		// ===== 見た目用（bullets_）：右腕＝マズル起点 =====
		bool placed = false;
		for (auto& b : bullets_) {
			if (!b.alive) {
				b.position = bulletBasePosVFX;
				b.velocity = pelletVel;
				b.alive = true;
				b.traveled = 0.0f;
				b.userShotCount = ++shotCounter_;
				placed = true;
				break;
			}
		}
		if (!placed) {
			Bullet nb{};
			nb.position = bulletBasePosVFX;
			nb.velocity = pelletVel;
			nb.alive = true;
			nb.traveled = 0.0f;
			nb.userShotCount = ++shotCounter_;
			bullets_.push_back(nb);
		}

		// ===== 衝突用（colliderBullets_）：ボディ起点 =====
		ColliderBullet cb{};
		cb.position = bulletBasePosCOL;
		cb.prev = cb.position;
		cb.velocity = pelletVel;
		cb.traveled = 0.0f;
		cb.alive = true;
		cb.userShotCount = shotCounter_; // 同じ発射IDを共有しておく

		// 衝突用コライダーの生成・登録（セグメント初期化）
		auto col = std::make_unique<K4E::Collider>();
		cb.collider = ColliderPtr(col.release(), ColliderDeleter{ collisionMgr_ });
		cb.collider->Initialize();
		cb.collider->SetTypeID(static_cast<uint32_t>(CollisionTypeIdDef::kBullet));
		cb.collider->SetOBBHalfSize({ 0,0,0 });           // セグメント専用
		cb.collider->SetCenterPosition(cb.position);
		K4E::Segment s{}; s.origin = cb.position; s.diff = { 0,0,0 };
		cb.collider->SetSegment(s);
		if (collisionMgr_) collisionMgr_->AddCollider(cb.collider.get());

		colliderBullets_.push_back(std::move(cb));

		// （トレーサ出すかの判定は従来通り Update() 側で ownerId を見て処理）
		// あるいはここで spawnTracer を見てフラグを記録してもよい
		(void)tracerMode; (void)tracerCount; (void)tracerIndices; // 使い方は既存ロジックに合わせて
	}
}

/// -------------------------------------------------------------
///				　	マズル位置のワールド座標を取得
/// -------------------------------------------------------------
K4E::Vector3 BallisticEffect::GetMuzzleWorld() const
{
	return ComputeMuzzleWorld(parentTransform_, transform_, offset_);
}

void BallisticEffect::RegisterColliders(CollisionManager* mgr)
{
	if (!mgr) return;
	for (auto& b : colliderBullets_)
	{
		if (b.alive && b.collider) {
			// 念のためデリータの mgr も更新しておく
			b.collider.get_deleter().mgr = mgr;
			mgr->AddCollider(b.collider.get());
		}
	}
}

/// -------------------------------------------------------------
///				　		　セグメントを1本追加
/// -------------------------------------------------------------
void BallisticEffect::PushTrail(const K4E::Vector3& p0, const K4E::Vector3& p1, float speed, const WeaponConfig& weapon)
{
	// 間引き
	float segLen = K4E::Vector3::Length(p1 - p0);
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
	K4E::Object3D* obj = freeList_.back(); freeList_.pop_back();

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
void BallisticEffect::SpawnMuzzleFlash(const K4E::Vector3& position, const K4E::Vector3& forward, const WeaponConfig& weapon)
{
	// プールに空きがなければ出せない
	if (flashFree_.empty()) return;

	// 方向を少しランダムに散らす（過度にしない）
	auto rand01 = []() { return (float)rand() / (float)RAND_MAX; };
	float yawRad = weapon.muzzle.randomYawDeg * (std::numbers::pi_v<float> / 180.0f) * (rand01() * 2.0f - 1.0f);

	// forward をXZで少し回す
	K4E::Vector3 dir = forward;
	{
		float c = std::cos(yawRad), s = std::sin(yawRad);
		K4E::Vector3 xz = { dir.x * c - dir.z * s, dir.y, dir.x * s + dir.z * c };
		dir = K4E::Vector3::Normalize(xz);
	}

	K4E::Object3D* obj = flashFree_.back(); flashFree_.pop_back();

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
void BallisticEffect::SpawnMuzzleSparks(const K4E::Vector3& pos, const K4E::Vector3& forward, const WeaponConfig& weapon)
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
		K4E::Vector3 z = K4E::Vector3::Normalize(forward);
		K4E::Vector3 x = K4E::Vector3::Normalize((fabs(z.y) < 0.999f) ? K4E::Vector3{ -z.z,0,z.x } : K4E::Vector3{ 1,0,0 });
		K4E::Vector3 y = K4E::Vector3::Normalize(K4E::Vector3::Cross(z, x));
		K4E::Vector3 dir = K4E::Vector3::Normalize(x * (std::sin(theta) * std::cos(phi_)) +
			y * (std::sin(theta) * std::sin(phi_)) +
			z * (std::cos(theta)));

		float speed = weapon.muzzle.sparkSpeedMin +
			(weapon.muzzle.sparkSpeedMax - weapon.muzzle.sparkSpeedMin) * rand01();

		if (sparkFree_.empty()) break;
		K4E::Object3D* obj = sparkFree_.back(); sparkFree_.pop_back();

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
void BallisticEffect::SpawnCasing(const K4E::Vector3& basePos, const K4E::Vector3& forward, const WeaponConfig& weapon)
{
	if (casingFree_.empty()) return;

	K4E::Vector3 z = K4E::Vector3::Normalize(forward);
	K4E::Vector3 worldUp = { 0,1,0 };

	// 右 = worldUp × forward
	K4E::Vector3 x = K4E::Vector3::Normalize(K4E::Vector3::Cross(worldUp, z));

	// 上 = forward × 右
	K4E::Vector3 y = K4E::Vector3::Normalize(K4E::Vector3::Cross(z, x));

	// スポーン位置：銃口根元から 右・上・後ろ へずらす
	K4E::Vector3 spawnPos = basePos
		+ x * weapon.casing.offsetRight
		+ y * weapon.casing.offsetUp
		- z * weapon.casing.offsetBack;

	// 右方向を中心に円錐でばらす
	auto rand01 = []() { return (float)rand() / (float)RAND_MAX; };
	float theta = (weapon.casing.coneDeg * std::numbers::pi_v<float> / 180.0f) * std::sqrt(rand01());
	float phi_ = 2.0f * std::numbers::pi_v<float> *rand01();
	// 右(x)を中心軸にする
	K4E::Vector3 dir = K4E::Vector3::Normalize(
		x * std::cos(theta) +
		(y * std::cos(phi_) + z * std::sin(phi_)) * std::sin(theta)
	);

	dir = K4E::Vector3::Normalize(dir + y * weapon.casing.upBias);

	float speed = weapon.casing.speedMin + (weapon.casing.speedMax - weapon.casing.speedMin) * rand01();

	K4E::Vector3 vel = dir * speed + y * weapon.casing.upKick;

	K4E::Object3D* obj = casingFree_.back(); casingFree_.pop_back();

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
