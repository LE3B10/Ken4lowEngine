#include "ModelParticle.h"
#include "Input.h"
#include "LinearInterpolation.h"

// 省略 <numbers>
using namespace std::numbers;

/// -------------------------------------------------------------
///				　		　初期化処理
/// -------------------------------------------------------------
void ModelParticle::Initialize()
{
	input_ = Input::GetInstance();

	object3D_ = std::make_unique<Object3D>();
	object3D_->Initialize("cube.gltf");

	// パーティクルプールの確保
	pool_.resize(poolMax_);
}

/// -------------------------------------------------------------
///				　			　更新処理
/// -------------------------------------------------------------
void ModelParticle::Update()
{
	// テスト：スペースで原点から上向きにバースト
	if (input_->TriggerKey(DIK_RETURN))
	{
		OnHit({ 0,0,0 }, { 0,1,0 });
		isActive_ = true; // 単体表示をONにしたい場合だけ
	}

	// 単体（従来）の更新は有効時のみ
	if (isActive_ && object3D_)
	{
		object3D_->Update();
	}

	// パーティクル更新（固定Δt。必要ならエンジンのdeltaTimeに置換）
	constexpr float dt = 1.0f / 60.0f;

	for (auto& p : pool_)
	{
		if (!p.alive) continue;

		// 物理
		p.vel.y += gravityY_ * dt;
		p.pos.x += p.vel.x * dt;
		p.pos.y += p.vel.y * dt;
		p.pos.z += p.vel.z * dt;

		p.euler.x += p.angVel.x * dt;
		p.euler.y += p.angVel.y * dt;
		p.euler.z += p.angVel.z * dt;

		// 縮小（ピクセルガンっぽく消えていく）
		p.scale *= std::pow(shrinkRate_, dt); // 連続時間の減衰

		// 反映
		if (p.obj)
		{
			p.obj->SetTranslate(p.pos);
			p.obj->SetRotate(p.euler);
			p.obj->SetScale({ p.scale, p.scale, p.scale });
			p.obj->Update();
		}

		// 寿命
		p.ttl -= dt;
		if (p.ttl <= 0.0f || p.scale < 0.01f)
		{
			p.alive = false;
			p.obj.reset();
		}
	}
}

/// -------------------------------------------------------------
///				　			　描画処理
/// -------------------------------------------------------------
void ModelParticle::Draw()
{
	if (!object3D_) return;

	// パーティクル描画
	for (auto& p : pool_) {
		if (p.alive && p.obj) {
			p.obj->Draw();
		}
	}
}

/// -------------------------------------------------------------
///				　			ImGui描画処理
/// -------------------------------------------------------------
void ModelParticle::DrawImGui()
{
	if (object3D_)
	{
		object3D_->DrawImGui();
	}
}

/// -------------------------------------------------------------
///				　			バースト生成
/// -------------------------------------------------------------
void ModelParticle::SpawnBurst(const Vector3& pos, const Vector3& normal, uint32_t count)
{
	Vector3 n = Vector3::Normalize(normal);
	// nに直交する基底を作る（拡散用）
	Vector3 tangent = std::abs(n.y) < 0.99f ? Vector3::Normalize(Vector3::Cross(n, { 0,1,0 })) : Vector3::Normalize(Vector3::Cross(n, { 1,0,0 }));
	Vector3 bitan = Vector3::Cross(n, tangent);

	for (uint32_t i = 0; i < count; ++i)
	{
		// 空きスロットを探す
		ModelParticleInfo* p = nullptr;
		for (auto& slot : pool_) { if (!slot.alive) { p = &slot; break; } }
		if (!p) break; // いっぱいなら諦める

		// 新規オブジェクト
		p->obj = std::make_unique<Object3D>();
		p->obj->Initialize("cube.gltf");

		// 拡散方向（法線nを中心に円錐分布）
		float r1 = urand_(rng_);
		float r2 = urand_(rng_);
		float theta = 2.0f * pi_v<float> *r1;
		float cone = spread_ * r2; // 0〜spread
		Vector3 dir = Vector3::Normalize(n + tangent * (std::cos(theta) * cone) + bitan * (std::sin(theta) * cone));

		// 速度・角速度
		p->vel = { dir.x * baseSpeed_, dir.y * baseSpeed_, dir.z * baseSpeed_ };
		p->angVel = { (urand_(rng_) - 0.5f) * 8.0f, (urand_(rng_) - 0.5f) * 8.0f, (urand_(rng_) - 0.5f) * 8.0f };

		// 寿命・サイズ・色
		p->ttl = lifeMin_ + (lifeMax_ - lifeMin_) * urand_(rng_);
		p->scale = startScale_;
		p->obj->SetColor({ 1.0f, 0.5f, 0.0f, 1.0f });

		// 位置・姿勢
		p->pos = pos;
		p->euler = { urand_(rng_) * 2.0f * pi_v<float>, urand_(rng_) * 2.0f * pi_v<float>, urand_(rng_) * 2.0f * pi_v<float> };

		// 反映
		p->obj->SetTranslate(p->pos);
		p->obj->SetScale({ p->scale, p->scale, p->scale });
		p->obj->SetRotate(p->euler);

		p->alive = true;
	}
}

/// -------------------------------------------------------------
///				　			ヒット時処理
/// -------------------------------------------------------------
void ModelParticle::OnHit(const Vector3& hitPos, const Vector3& hitNormal)
{
	SpawnBurst(hitPos, hitNormal, defaultCount_);
}
