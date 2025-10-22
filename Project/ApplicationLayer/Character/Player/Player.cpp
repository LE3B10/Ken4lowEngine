#define NOMINMAX
#include "Player.h"
#include <Object3DCommon.h>
#include <CollisionTypeIdDef.h>
#include <Input.h>
#include <AudioManager.h>
#include "LinearInterpolation.h"
#include "ToWeaponConfig.h"

#include <algorithm>
#include <filesystem>
#include <imgui.h>

/// -------------------------------------------------------------
///				　			　 初期化処理
/// -------------------------------------------------------------
void Player::Initialize()
{
	input_ = Input::GetInstance();

	// ID登録
	Collider::SetTypeID(static_cast<uint32_t>(CollisionTypeIdDef::kPlayer));
	Collider::SetOwner<Player>(this);
	Collider::SetOBBHalfSize({ 0.8f, 2.0f, 0.8f });

	// 体幹部位の初期化
	body_.object = std::make_unique<Object3D>();
	body_.object->Initialize("PlayerRoot/player_body.gltf");
	body_.transform.translate_ = { 0.0f, 2.25f, 0.0f };	// 初期位置

	// 子オブジェクト（頭、腕、脚）をリストに追加
	std::vector<std::pair<std::string, Vector3>> partData =
	{
		{"PlayerRoot/player_head.gltf", {0.0f, 0.75f, 0.0f}},      // 頭
		{"PlayerRoot/player_LeftArm.gltf", {-0.75f, 0.75f, 0.0f}}, // 左腕
		{"PlayerRoot/player_RightArm.gltf",{ 0.75f, -0.75f, 0}},   // 右腕
		{"PlayerRoot/player_LeftLeg.gltf", {-0.25f, -0.75f, 0} },  // 左脚
		{"PlayerRoot/player_RightLeg.gltf", {0.25f, -0.75f, 0} }   // 右脚
	};

	// 部位データをもとに部位オブジェクトを生成
	for (const auto& [modelPath, position] : partData)
	{
		BodyPart part;
		part.object = std::make_unique<Object3D>();
		part.object->Initialize(modelPath);
		part.transform.translate_ = position;
		part.object->SetTranslate(part.transform.translate_);
		part.transform.parent_ = &body_.transform; // 親を設定
		parts_.push_back(std::move(part));
	}

	// FPSカメラ
	fpsCamera_ = std::make_unique<FpsCamera>();
	fpsCamera_->Initialize(this);

	groundY_ = body_.transform.translate_.y;  // 今立っている高さを床として扱う
	isGrounded_ = true;						  // 接地中
	vY_ = 0.0f;								  // 縦速度初期化

	// ピストル武器初期化
	pistolWeapon_ = std::make_unique<PistolWeapon>();
	pistolWeapon_->Initialize(MakePistolConfig());
	pistolWeapon_->SetParentTransform(&parts_[2].transform); // 右腕に追従

	// 弾道エフェクト初期化
	ballisticEffect_ = std::make_unique<BallisticEffect>();
	ballisticEffect_->Initialize();
	ballisticEffect_->SetParentTransform(&parts_[2].transform); // 右腕に追従

	// 武器カタログ初期化
	weaponCatalog_ = std::make_unique<WeaponCatalog>();
	weaponCatalog_->Initialize(kWeaponDir, kWeaponMonolith); // ディレクトリとモノリス両方から読み込み

	// 在庫から初期装備を決める
	const auto& table = weaponCatalog_->All();

	// 初期武器設定
	if (!table.empty())
	{
		auto it = table.find("Pistol");					// まずピストルを探す
		if (it == table.end()) it = table.begin();		// 在庫の最初の武器を使う
		weapon_ = std::make_unique<Weapon>(it->second); // 武器基底ポインタにセット
		currentWeapon_ = ToWeaponConfig(it->second);	// ランタイム用コピー
	}

	// ロードアウト初期化
	loadout_ = std::make_unique<Loadout>();
	loadout_->Rebuild(weaponCatalog_->All()); // 在庫に基づき再構築

	// 初期装備 : プライマリ武器を優先
	std::string useWeaponName = loadout_->SelectNameByClass(WeaponClass::Primary, weaponCatalog_->All());

	// 装備がなければ在庫の最初の武器を使う
	if (useWeaponName.empty() && !weaponCatalog_->All().empty()) useWeaponName = weaponCatalog_->All().begin()->first;

	// 武器選択
	if (!useWeaponName.empty()) SelectWeapon(useWeaponName);
}


/// -------------------------------------------------------------
///				　			　 更新処理
/// -------------------------------------------------------------
void Player::Update(float deltaTime)
{
#ifdef _DEBUG

	// カメラモード切替
	if (input_->TriggerKey(DIK_F5)) { fpsCamera_->CycleViewMode(); }

#endif // _DEBUG

	// ここで表示切替
	switch (fpsCamera_->GetViewMode()) // ← カメラの現在モード
	{
	case FpsCamera::ViewMode::FirstPerson:
		SetBodyActive(false);          // 体幹は映さない
		SetAllPartsActive(false);      // 一旦全部オフ
		SetPartActive(rightArmIndex_, true); // 右手だけオン
		break;

	case FpsCamera::ViewMode::ThirdBack:
	case FpsCamera::ViewMode::ThirdFront:
		SetBodyActive(true);
		SetAllPartsActive(true);       // 全パーツオン
		break;
	}

	// 武器選択 : 数字キー1〜6 : クラス別選択
	if (input_->TriggerKey(DIK_1)) { auto n = loadout_->SelectNameByClass(WeaponClass::Primary, weaponCatalog_->All()); if (!n.empty()) SelectWeapon(n); }
	if (input_->TriggerKey(DIK_2)) { auto n = loadout_->SelectNameByClass(WeaponClass::Backup, weaponCatalog_->All()); if (!n.empty()) SelectWeapon(n); }
	if (input_->TriggerKey(DIK_3)) { auto n = loadout_->SelectNameByClass(WeaponClass::Melee, weaponCatalog_->All()); if (!n.empty()) SelectWeapon(n); }
	if (input_->TriggerKey(DIK_4)) { auto n = loadout_->SelectNameByClass(WeaponClass::Special, weaponCatalog_->All()); if (!n.empty()) SelectWeapon(n); }
	if (input_->TriggerKey(DIK_5)) { auto n = loadout_->SelectNameByClass(WeaponClass::Sniper, weaponCatalog_->All()); if (!n.empty()) SelectWeapon(n); }
	if (input_->TriggerKey(DIK_6)) { auto n = loadout_->SelectNameByClass(WeaponClass::Heavy, weaponCatalog_->All()); if (!n.empty()) SelectWeapon(n); }

	// 移動処理
	Move(deltaTime);

	// 武器更新
	pistolWeapon_->Update(deltaTime);

	// 弾道エフェクト更新
	ballisticEffect_->Update();

	// ばね: accel = -k*x - c*v
	auto spring = [&](float& x, float& v) {
		float a = -recoilReturn_ * x - recoilDamping_ * v; // 加速度
		v += a * deltaTime;								   // 速度更新
		x += v * deltaTime;								   // 位置更新
		};

	// 後退＆回転の戻り
	spring(recoilZ_, recoilVz_);
	spring(recoilPitch_, recoilVp_);
	spring(recoilYaw_, recoilVy_);

	// クールダウン更新
	if (fireCooldown_ > 0.0f) fireCooldown_ -= deltaTime;

	// 射撃処理
	auto& armT = parts_[rightArmIndex_].transform;

	// マウス左ボタンで射撃
	if (input_->PushMouse(0))
	{
		// クールダウン終了で発射可能
		if (fireCooldown_ <= 0.0f)
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
			Vector3 up = Vector3::Normalize(Vector3::Cross(forward, right));	  // 上方向

			// キックバック：前方の反対方向に少し引く、ついでに僅かに上へ
			armT.translate_ += (-forward * recoilZ_) + (up * (recoilZ_ * 0.15f));

			// 回転：上向き＆左右ブレ
			armT.rotate_.x += recoilPitch_;
			armT.rotate_.y += recoilYaw_;

			// クロスヘア収束を使っているなら、ここで aimPoint から shotDir を作る版に差し替え
			Vector3 velocity = forward * currentWeapon_.muzzleSpeed;
			ballisticEffect_->Start({ 0,0,0 }, velocity, currentWeapon_);
			fireInterval_ = 60.0f / currentWeapon_.rpm;
			fireCooldown_ = fireInterval_;

			// 反動インパルス
			// プリセット（武器ごと）
			float kick = 0.055f;      // 後退量
			float rise = 5.0f;        // 上向き（度）
			float horiz = 0.3f;       // 左右（度）
			if (currentWeapon_.name == "Rifle") { kick = 0.075f; rise = 2.6f; horiz = 0.45f; }
			if (currentWeapon_.name == "MachineGun") { kick = 0.045f; rise = 0.9f; horiz = 0.25f; }

			// 反動適用
			ApplyRecoil(kick, rise, horiz);
		}
	}

	// ベースキャラクターの更新
	BaseCharacter::Update(deltaTime);
}


/// -------------------------------------------------------------
///				　			　 描画処理
/// -------------------------------------------------------------
void Player::Draw()
{
	// 体幹部位の描画
	if (body_.active) { body_.object->Draw(); }

	// 部位データ配列の描画
	for (auto& part : parts_) if (part.active) { part.object->Draw(); }

	// 武器描画
	pistolWeapon_->Draw();

	// 弾道エフェクト描画
	ballisticEffect_->Draw();
}

/// -------------------------------------------------------------
///				　	衝突時に呼ばれる仮想関数
/// -------------------------------------------------------------
void Player::OnCollision(Collider* other)
{
	//// 他のコライダーのタイプIDを取得
	//uint32_t otherTypeID = other->GetTypeID();
	//// 地面との衝突判定（仮実装）
	//if (otherTypeID == static_cast<uint32_t>(CollisionTypeIdDef::kGround))
	//{
	//	// 着地処理
	//	if (!isGrounded_)
	//	{
	//		groundY_ = body_.transform.translate_.y; // 今の高さを床として扱う
	//		vY_ = 0.0f;							     // 縦速度リセット
	//		isGrounded_ = true;					     // 接地状態へ
	//		// 着地音再生（仮実装）
	//		AudioManager::GetInstance()->PlayWave("Assets/Audio/footstep.wav", 0.5f);
	//	}
	//}

	(void)other; // 未使用警告回避
}

/// -------------------------------------------------------------
///				　中心座標を取得する純粋仮想関数
/// -------------------------------------------------------------
Vector3 Player::GetCenterPosition() const
{
	const Vector3 offset = { 0.0f,1.0f,0.0f };
	Vector3 worldPosition = body_.transform.translate_ + offset;
	return worldPosition;
}

/// -------------------------------------------------------------
///				　			　 移動処理
/// -------------------------------------------------------------
void Player::Move(float deltaTime)
{
	// コライダー位置更新
	Collider::SetCenterPosition(body_.transform.translate_ - Vector3{ 0.0f,0.3f,0.0f });

	// 仮実装
	const float moveSpeed = 0.1f;
	Vector3 move{ 0,0,0 };

	if (!isDebugCamera_) // デバッグカメラ中は無効
	{
		if (input_->PushKey(DIK_W)) move.z += moveSpeed;
		if (input_->PushKey(DIK_S)) move.z -= moveSpeed;
		if (input_->PushKey(DIK_A)) move.x -= moveSpeed;
		if (input_->PushKey(DIK_D)) move.x += moveSpeed;
	}

	// 斜め移動補正
	if (Vector3::Length(move) > 0.0f) move = Vector3::Normalize(move) * moveSpeed;

	// 三人称前面視点では前後反転
	if (fpsCamera_->GetViewMode() == FpsCamera::ViewMode::ThirdFront) move.z *= -1.0f;

	// カメラの向きに合わせて移動ベクトルを回転
	const float yaw = fpsCamera_->GetCamera()->GetRotate().y;
	const float s = std::sinf(yaw), c = std::cosf(yaw);
	move = { move.x * c - move.z * s, 0.0f, move.x * s + move.z * c };

	// 体の移動
	body_.transform.translate_ += move;

	/// ---------- 体と頭の回転処理 ---------- ///
	const bool  isFP = (fpsCamera_->GetViewMode() == FpsCamera::ViewMode::FirstPerson);
	const float camYaw = fpsCamera_->GetYaw();
	const float camPitch = fpsCamera_->GetPitch();

	// 頭と体の回転更新
	if (isFP)
	{
		// 一人称：回転は即時
		bodyYaw_ = camYaw;
		headYawLocal_ = 0.0f;
		parts_[0].transform.rotate_.x = std::clamp(camPitch, -headPitchLimit_, headPitchLimit_);
	}
	else
	{
		// 三人称：補間（現状のまま）
		float targetHeadYawLocal = NormalizeAngle(camYaw - bodyYaw_);
		targetHeadYawLocal = std::clamp(targetHeadYawLocal, -headYawLimit_, headYawLimit_);
		headYawLocal_ = Lerp(headYawLocal_, targetHeadYawLocal, 0.25f);
		bodyYaw_ = LerpAngle(bodyYaw_, camYaw, 0.10f);
		parts_[0].transform.rotate_.x = std::clamp(camPitch, -headPitchLimit_, headPitchLimit_);
	}

	// ジャンプ
	if (!isDebugCamera_ && isGrounded_ && input_->PushKey(DIK_SPACE))
	{
		vY_ = jumpSpeed_;
		isGrounded_ = false;
	}

	// 空中は重力適用
	if (!isGrounded_)
	{
		vY_ += gravity_ * deltaTime;					 // 重力加算
		if (vY_ < maxFallSpeed_) vY_ = maxFallSpeed_;	 // 落下速度クランプ
		body_.transform.translate_.y += vY_ * deltaTime; // 位置更新

		// 着地（水平床の想定）
		if (body_.transform.translate_.y <= groundY_)
		{
			body_.transform.translate_.y = groundY_; // 床に合わせる
			vY_ = 0.0f;								 // 縦速度リセット
			isGrounded_ = true;						 // 接地状態へ
		}
	}
	else
	{
		// 接地中は高さを維持（段差に対応するなら groundY_ を更新する）
		body_.transform.translate_.y = groundY_;
	}

	// 体と頭の回転反映
	body_.transform.rotate_.y = bodyYaw_;
	parts_[0].transform.rotate_.y = headYawLocal_;

	/// ---------- 右手：FPはカメラ基準で固定 ---------- ///
	auto& armT = parts_[rightArmIndex_].transform;
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
}

/// -------------------------------------------------------------
///				　			　 リコイル
/// -------------------------------------------------------------
void Player::ApplyRecoil(float kickBack, float riseDeg, float horizDeg)
{
	// 後退はそのまま加算（mスケール想定）
	recoilVz_ += kickBack;

	// 角度は度→rad
	const float r = std::numbers::pi_v<float> / 180.0f;
	recoilVp_ += riseDeg * r;
	// 左右ブレは±で
	float sign = (rand() & 1) ? 1.0f : -1.0f;
	recoilVy_ += sign * horizDeg * r;
}

/// -------------------------------------------------------------
///				　			　 武器選択
/// -------------------------------------------------------------
void Player::SelectWeapon(const std::string& name)
{
	// 武器データをカタログから探す
	if (const WeaponData* w = weaponCatalog_->Find(name))
	{
		// 武器基底ポインタにセット
		weapon_ = std::make_unique<Weapon>(*w);

		// ランタイム用コピー
		currentWeapon_ = ToWeaponConfig(*w);
	}
}

/// -------------------------------------------------------------
///				　		  ImGui描画処理
/// -------------------------------------------------------------
void Player::DrawImGui()
{
	// 現在装備の名前（空可）
	const std::string currentName = weapon_ ? weapon_->Data().name : std::string{};

	// ---------- ロードアウト表示 ---------- ///
	static const char* kClassLabels[] = { "Primary","Backup","Melee","Special","Sniper","Heavy" };

	ImGui::Separator();
	ImGui::Text("Loadout by Class");

	const auto& map = loadout_->GetEquipMap();
	for (int c = 0; c < 6; ++c) {
		WeaponClass wc = static_cast<WeaponClass>(c);
		const char* label = kClassLabels[c];
		std::string equipped = "-";
		if (auto it = map.find(wc); it != map.end()) equipped = it->second;
		ImGui::Text("%-8s : %s", label, equipped.c_str());
	}

	if (weapon_)
	{
		const auto& D = weapon_->Data(); // 現在装備中の武器データ（const）
		int idx = static_cast<int>(D.clazz);
		if (0 <= idx && idx < IM_ARRAYSIZE(kClassLabels)) {
			ImGui::Text("Current Category: %s", kClassLabels[idx]);
		}
		else {
			ImGui::Text("Current Category: (Unknown)");
		}
	}

	// --- WeaponEditorUI に渡すフック群 ---
	WeaponEditorHooks hooks{};
	hooks.SaveAll = [&] {
		weaponCatalog_->SaveAll();                                  // 保存
		};
	hooks.RequestReloadFocus = [&](const std::string& focus) {
		weaponCatalog_->RequestReload(focus);                       // 再読込予約
		};
	hooks.RebuildLoadout = [&] {
		loadout_->Rebuild(weaponCatalog_->All());                   // クラス変更時に装備再構築
		};
	hooks.ApplyToRuntimeIfCurrent = [&](const WeaponData& wd) {
		if (weapon_ && weapon_->Data().name == wd.name) {
			currentWeapon_ = ToWeaponConfig(wd);                    // ランタイム反映
		}
		};
	hooks.RequestAdd = [&](const std::string& newName, const std::string& baseName) {
		pendingAdds_.emplace_back(newName, baseName);               // 追加はフレーム末で実行
		};
	hooks.RequestDelete = [&](const std::string& name) {
		pendingDeletes_.push_back(name);                            // 削除もフレーム末で
		};

	// --- メインの “武器編集UI” 呼び出し ---
	if (!weaponEditorUI_) weaponEditorUI_ = std::make_unique<WeaponEditorUI>();
	weaponEditorUI_->DrawImGui(*weaponCatalog_, *loadout_, currentName, hooks);

	// --- フレーム末の遅延実行（Add/Delete） ---
	for (auto& [newName, baseName] : pendingAdds_) {
		const WeaponData* basePtr = baseName.empty() ? nullptr : weaponCatalog_->Find(baseName);
		weaponCatalog_->AddWeapon(newName, basePtr);
	}
	pendingAdds_.clear();

	for (auto& delName : pendingDeletes_) {
		loadout_->RemoveName(delName);
		weaponCatalog_->RemoveWeapon(delName);
	}
	pendingDeletes_.clear();

	// --- “再読込予約”の適用（フォーカス再選択＆装備更新） ---
	weaponCatalog_->ApplyReloadIfNeeded([&](const WeaponData& focused) {
		weapon_ = std::make_unique<Weapon>(focused);
		currentWeapon_ = ToWeaponConfig(focused);
		loadout_->Rebuild(weaponCatalog_->All());
		});
}
