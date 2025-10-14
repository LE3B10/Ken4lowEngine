//#define NOMINMAX
//#include "Weapon.h"
//#include <Bullet.h>
//#include <AudioManager.h>
//#include <Player.h>
//#include <FpsCamera.h>
//
//#include <imgui.h>
//#include <ParticleManager.h>
//
//
///// -------------------------------------------------------------
/////				　			　 初期化処理
///// -------------------------------------------------------------
//void Weapon::Initialize()
//{
//	bullets_.clear();
//	bullets_.reserve(1024);
//	ammoInfo_.ammoInClip = ammoInfo_.clipSize;
//	isReloading_ = false;
//	reloadTimer_ = 0.0f;
//
//	bullets_.push_back(std::make_unique<Bullet>());
//	bullets_.back()->Initialize();
//
//	// 武器ごとのパラメータ設定
//	switch (type_)
//	{
//	case WeaponType::Primary:
//		ammoInfo_ = { 60, 300, 60, 300, 1, 10.0f, 1000.0f }; // 30回撃てて、1回で1発出す
//		reloadTime_ = 1.5f; // ライフルはリロード時間が短い
//		bulletSpeed_ = 40.0f; // ライフルの弾速はショットガンより速い
//		fireSEPath_ = "gun.mp3";
//		break;
//
//	case WeaponType::Backup:
//		ammoInfo_ = { 6, 30, 6, 30, 8, 10.0f, 100.0f }; // 6回撃てて、1回で8粒出す
//		reloadTime_ = 2.5f; // ショットガンはリロード時間が長い
//		bulletSpeed_ = 24.0f; // ショットガンの弾速はライフルより遅い
//		fireSEPath_ = "shotgunFire.mp3";
//		break;
//	}
//}
//
//
///// -------------------------------------------------------------
/////				　			　 更新処理
///// -------------------------------------------------------------
//void Weapon::Update()
//{
//	// --- リロード進行 ---
//	if (isReloading_) {
//		reloadTimer_ += player_->GetAnimationModel()->GetDeltaTime();
//		if (reloadTimer_ >= reloadTime_) {
//			int needed = ammoInfo_.clipSize - ammoInfo_.ammoInClip;
//			int toLoad = std::min(needed, ammoInfo_.reserveAmmo);
//			ammoInfo_.ammoInClip += toLoad;
//			ammoInfo_.reserveAmmo -= toLoad;
//			isReloading_ = false;
//		}
//	}
//
//	// 発射インターバル計時
//	fireTimer_ += player_->GetAnimationModel()->GetDeltaTime();
//
//	// --- 弾がなければリロードキーだけ処理して終了 ---
//	const size_t N = bullets_.size();
//	if (N == 0) {
//		if (Input::GetInstance()->TriggerKey(DIK_R) &&
//			ammoInfo_.ammoInClip < ammoInfo_.clipSize &&
//			ammoInfo_.reserveAmmo > 0 && !isReloading_) {
//			Reload();
//		}
//		return;
//	}
//
//	// --- 物理だけ並列 Simulate ---
//	// Nが小さいときはスレッド起動コストのほうが高いので閾値を設ける
//	if (N < 256) {
//		for (size_t i = 0; i < N; ++i) {
//			auto& b = bullets_[i];
//			if (!b->IsDead()) b->Simulate();
//		}
//	}
//	else {
//		const unsigned hw = std::max(1u, std::thread::hardware_concurrency());
//		// 過剰スレッドを避けるため、弾256個/スレッドを目安に上限をかける
//		const unsigned T = std::max(1u, std::min<unsigned>(hw, static_cast<unsigned>((N + 255) / 256)));
//		const size_t stride = (N + T - 1) / T;
//
//		std::vector<std::thread> ths;
//		ths.reserve(T);
//		auto worker = [&](size_t b, size_t e) {
//			for (size_t i = b; i < e; ++i) {
//				auto& bullet = bullets_[i];
//				if (!bullet->IsDead()) bullet->Simulate();
//			}
//			};
//		for (unsigned t = 0; t < T; ++t) {
//			const size_t b = t * stride;
//			const size_t e = std::min(N, b + stride);
//			if (b < e) ths.emplace_back(worker, b, e);
//		}
//		for (auto& th : ths) th.join();
//	}
//
//	// --- メインスレッドで描画/衝突登録 ---
//	for (auto& b : bullets_) b->Commit();
//
//	// --- Dead を削除 ---
//	bullets_.erase(std::remove_if(bullets_.begin(), bullets_.end(),
//		[](const std::unique_ptr<Bullet>& p) { return p->IsDead(); }),
//		bullets_.end());
//
//	// --- Rキーでリロード開始 ---
//	if (Input::GetInstance()->TriggerKey(DIK_R) &&
//		ammoInfo_.ammoInClip < ammoInfo_.clipSize &&
//		ammoInfo_.reserveAmmo > 0 && !isReloading_) {
//		Reload();
//	}
//}
//
//
///// -------------------------------------------------------------
/////				　			　 描画処理
///// -------------------------------------------------------------
//void Weapon::Draw()
//{
//	for (auto& bullet : bullets_)
//	{
//		if (!bullet->IsDead())
//		{
//			bullet->Draw();
//		}
//	}
//}
//
//
///// -------------------------------------------------------------
/////				　			　 発射試行
///// -------------------------------------------------------------
//void Weapon::TryFire(const Vector3& position, const Vector3& direction)
//{
//	if (isReloading_ || ammoInfo_.ammoInClip <= 0) return;
//
//	if (fireTimer_ < fireInterval_) return;
//
//	fireTimer_ = 0.0f;
//
//	switch (type_)
//	{
//	case WeaponType::Primary: // ライフルの場合は単発弾を発射
//		FireSingleBullet(position, direction);
//
//		if (fpsCamera_) fpsCamera_->AddRecoil(0.004f, 0.0045f);
//
//		// サウンド再生
//		AudioManager::GetInstance()->PlaySE(fireSEPath_, 0.2f, 2.0f);
//
//		break;
//
//	case WeaponType::Backup: // ショットガンの場合は散弾を発射
//		FireShotgunSpread(position, direction);
//
//		if (fpsCamera_) fpsCamera_->AddRecoil(0.008f, 0.007f);
//
//		// サウンド再生
//		AudioManager::GetInstance()->PlaySE(fireSEPath_, 0.2f);
//
//		break;
//	}
//}
//
//
///// -------------------------------------------------------------
/////				　			　 リロード開始
///// -------------------------------------------------------------
//void Weapon::Reload()
//{
//	if (ammoInfo_.ammoInClip == ammoInfo_.clipSize || ammoInfo_.reserveAmmo == 0) return;
//
//	isReloading_ = true;
//	reloadTimer_ = 0.0f;
//}
//
//
///// -------------------------------------------------------------
/////				　			　 弾丸補充
///// -------------------------------------------------------------
//void Weapon::AddReserveAmmo(int amount)
//{
//	ammoInfo_.reserveAmmo = std::min(ammoInfo_.reserveAmmo + amount, ammoInfo_.maxReserve);
//}
//
//
///// -------------------------------------------------------------
/////				　			　 ImGui描画処理
///// -------------------------------------------------------------
//void Weapon::DrawImGui()
//{
//	ImGui::Text("Ammo: %d / %d", ammoInfo_.ammoInClip, ammoInfo_.clipSize);
//	ImGui::Text("Reserve: %d / %d", ammoInfo_.reserveAmmo, ammoInfo_.maxReserve);
//	ImGui::Text("Ammo / Reserve : %d / %d", ammoInfo_.ammoInClip, ammoInfo_.reserveAmmo);
//
//	ImGui::Text("Reloading: %s", isReloading_ ? "Yes" : "No");
//}
//
//
///// -------------------------------------------------------------
/////				　			　 弾丸のみ更新
///// -------------------------------------------------------------
//void Weapon::UpdateBulletsOnly()
//{
//	for (auto& bullet : bullets_) {
//		bullet->Update();
//	}
//	bullets_.erase(std::remove_if(bullets_.begin(), bullets_.end(),
//		[](const std::unique_ptr<Bullet>& b) { return b->IsDead(); }),
//		bullets_.end());
//}
//
//
///// -------------------------------------------------------------
/////				　			　 単発弾発射
///// -------------------------------------------------------------
//void Weapon::FireSingleBullet(const Vector3& pos, const Vector3& dir)
//{
//	CreateBullet(pos, dir); // 弾丸を作成
//
//	// ショットガン発射時は FireShotgunSpread 側で消費するのでここでは減らさない
//	if (type_ == WeaponType::Primary) {
//		--ammoInfo_.ammoInClip;
//	}
//}
//
//
///// -------------------------------------------------------------
/////				　			　 散弾発射
///// -------------------------------------------------------------
//void Weapon::FireShotgunSpread(const Vector3& pos, const Vector3& dir)
//{
//	for (int i = 0; i < ammoInfo_.firePerShot; ++i)
//	{
//		Vector3 spreadDir = ApplyRandomSpread(dir, 5.0f);
//		FireSingleBullet(pos, spreadDir); // この中で 1発分発射処理
//	}
//
//	--ammoInfo_.ammoInClip; // 1回撃つのにマガジン1つ消費（散弾は8発出るけど1回分の扱い）
//}
//
//
///// -------------------------------------------------------------
/////				　		ランダムスプレッド適用
///// -------------------------------------------------------------
//Vector3 Weapon::ApplyRandomSpread(const Vector3& baseDir, float angleRangeDeg)
//{
//	// 基準方向を正規化
//	Vector3 forward = Vector3::Normalize(baseDir);
//
//	// baseDir に垂直なベクトルを定義（右方向）
//	Vector3 worldUp = { 0.0f, 1.0f, 0.0f };
//
//	// forward と worldUp が平行すぎると右が定義できないので調整
//	if (fabsf(Vector3::Dot(forward, worldUp)) > 0.99f) {
//		worldUp = { 0.0f, 0.0f, 1.0f };
//	}
//
//	Vector3 right = Vector3::Normalize(Vector3::Cross(worldUp, forward));
//	Vector3 up = Vector3::Normalize(Vector3::Cross(forward, right));
//
//	// ランダム角度（度→ラジアン）
//	float yawOffset = DegToRad(GetRandomFloat(-angleRangeDeg, angleRangeDeg));
//	float pitchOffset = DegToRad(GetRandomFloat(-angleRangeDeg, angleRangeDeg));
//
//	// ベース方向に対してスプレッドを適用
//	Vector3 spreadDir = forward;
//	spreadDir += right * tanf(yawOffset);   // 水平方向に拡がる
//	spreadDir += up * tanf(pitchOffset);    // 垂直方向に拡がる
//
//	return Vector3::Normalize(spreadDir);
//}
//
//
//void Weapon::CreateBullet(const Vector3& position, const Vector3& direction)
//{
//	// 弾を作成する処理（既存コード）
//	std::unique_ptr<Bullet> bullet = std::make_unique<Bullet>();
//	Vector3 bulletVelocity = direction * bulletSpeed_;
//	bullet->Initialize();
//	bullet->SetPosition(position);
//	bullet->SetVelocity(bulletVelocity);
//	bullet->SetDamage(ammoInfo_.bulletDamage);  // ダメージ
//	bullet->SetPlayer(player_);                 // 命中通知用に Player を渡す
//	bullets_.push_back(std::move(bullet));
//
//	// ✅ レーザービーム演出
//	float beamLength = 60.0f;
//	int segmentCount = 30;
//	Vector4 beamColor = { 0.0f, 1.0f, 1.0f, 0.8f };
//}
