#define NOMINMAX
#include "Player.h"
#include <Object3DCommon.h>
#include <CollisionTypeIdDef.h>
#include <Input.h>
#include <AudioManager.h>
#include "LinearInterpolation.h"

#include <algorithm>
#include <imgui.h>


// --- WeaponData -> WeaponConfig 変換（最小） ---
static WeaponConfig ToWeaponConfig(const WeaponData& E)
{
	WeaponConfig w{};
	w.name = E.name;
	w.muzzleSpeed = E.muzzleSpeed;
	w.maxDistance = E.maxDistance;
	w.rpm = E.rpm;
	w.magCapacity = static_cast<uint32_t>(E.magCapacity);
	w.startingReserve = static_cast<uint32_t>(E.startingReserve);
	w.reloadTime = E.reloadTime;
	w.bulletsPerShot = static_cast<uint32_t>(E.bulletsPerShot);

	w.autoReload = E.autoReload;
	w.requestedMaxSegments = E.requestedMaxSegments;

	w.spreadDeg = E.spreadDeg;
	w.pelletTracerMode = E.pelletTracerMode;
	w.pelletTracerCount = E.pelletTracerCount;

	// tracer
	w.tracer.enabled = E.tracer.enabled;
	w.tracer.tracerLength = E.tracer.tracerLength;
	w.tracer.tracerWidth = E.tracer.tracerWidth;
	w.tracer.minSegLength = E.tracer.minSegLength;
	w.tracer.startOffsetForward = E.tracer.startOffsetForward;
	w.tracer.tracerColor = E.tracer.color;

	// muzzle
	w.muzzle.enabled = E.muzzle.enabled;
	w.muzzle.life = E.muzzle.life;
	w.muzzle.startLength = E.muzzle.startLength;
	w.muzzle.endLength = E.muzzle.endLength;
	w.muzzle.startWidth = E.muzzle.startWidth;
	w.muzzle.endWidth = E.muzzle.endWidth;
	w.muzzle.randomYawDeg = E.muzzle.randomYawDeg;
	w.muzzle.color = E.muzzle.color;

	w.muzzle.offsetForward = E.muzzle.offsetForward;
	w.muzzle.sparksEnabled = E.muzzle.sparksEnabled;
	w.muzzle.sparkCount = E.muzzle.sparkCount;
	w.muzzle.sparkLifeMin = E.muzzle.sparkLifeMin;
	w.muzzle.sparkLifeMax = E.muzzle.sparkLifeMax;
	w.muzzle.sparkSpeedMin = E.muzzle.sparkSpeedMin;
	w.muzzle.sparkSpeedMax = E.muzzle.sparkSpeedMax;
	w.muzzle.sparkConeDeg = E.muzzle.sparkConeDeg;
	w.muzzle.sparkGravityY = E.muzzle.sparkGravityY;
	w.muzzle.sparkWidth = E.muzzle.sparkWidth;
	w.muzzle.sparkOffsetForward = E.muzzle.sparkOffsetForward;
	w.muzzle.sparkColorStart = E.muzzle.sparkColorStart;
	w.muzzle.sparkColorEnd = E.muzzle.sparkColorEnd;

	// casing
	w.casing.enabled = E.casing.enabled;
	w.casing.offsetRight = E.casing.offsetRight;
	w.casing.offsetUp = E.casing.offsetUp;
	w.casing.offsetBack = E.casing.offsetBack;
	w.casing.speedMin = E.casing.speedMin;
	w.casing.speedMax = E.casing.speedMax;
	w.casing.coneDeg = E.casing.coneDeg;
	w.casing.gravityY = E.casing.gravityY;
	w.casing.life = E.casing.life;
	w.casing.drag = E.casing.drag;
	w.casing.upKick = E.casing.upKick;
	w.casing.upBias = E.casing.upBias;
	w.casing.spinMin = E.casing.spinMin;
	w.casing.spinMax = E.casing.spinMax;
	w.casing.color = E.casing.color;
	w.casing.scale = E.casing.scale;
	return w;
}

// --- ピストル武器設定プリセット作成 ---
static std::string MakeUniqueName(const std::unordered_map<std::string, WeaponData>& table, std::string name)
{
	if (!table.count(name)) return name;
	int i = 1;
	while (table.count(name + "_" + std::to_string(i))) ++i;
	return name + "_" + std::to_string(i);
}

/// -------------------------------------------------------------
///				　			　 デストラクタ
/// -------------------------------------------------------------
Player::~Player()
{

}

/// -------------------------------------------------------------
///				　			　 初期化処理
/// -------------------------------------------------------------
void Player::Initialize()
{
	input_ = Input::GetInstance();

	// ID登録
	Collider::SetTypeID(static_cast<uint32_t>(CollisionTypeIdDef::kPlayer));

	body_.object = std::make_unique<Object3D>();
	body_.object->Initialize("PlayerRoot/player_body.gltf");

	// 初期位置
	body_.transform.translate_ = { 0.0f, 2.25f, 0.0f };

	// 子オブジェクト（頭、腕）をリストに追加
	std::vector<std::pair<std::string, Vector3>> partData =
	{
		{"PlayerRoot/player_head.gltf", {0.0f, 0.75f, 0.0f}},     // 頭
		{"PlayerRoot/player_LeftArm.gltf", {-0.75f, 0.75f, 0.0f}}, // 左腕
		{"PlayerRoot/player_RightArm.gltf",{ 0.75f, -0.75f, 0}},  // 右腕
		{"PlayerRoot/player_LeftLeg.gltf", {-0.25f, -0.75f, 0} },  // 左脚
		{"PlayerRoot/player_RightLeg.gltf", {0.25f, -0.75f, 0} }  // 右脚
	};

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
	isGrounded_ = true;
	vY_ = 0.0f;

	pistolWeapon_ = std::make_unique<PistolWeapon>();
	pistolWeapon_->Initialize(MakePistolConfig());
	pistolWeapon_->SetParentTransform(&parts_[2].transform);

	ballisticEffect_ = std::make_unique<BallisticEffect>();
	ballisticEffect_->Initialize();
	ballisticEffect_->SetParentTransform(&parts_[2].transform);

	// 武器設定プリセット
	weaponTable_ = Weapon::LoadWeapon("Resources/JSON/weapons.json");
	auto it = weaponTable_.find("Pistol"); // 例: ピストル
	if (it != weaponTable_.end()) {
		weapon_ = std::make_unique<Weapon>(it->second);
		currentWeapon_ = ToWeaponConfig(it->second);   // 現在武器設定に反映
	}
	else {
		// フォールバック（JSONが無い/壊れている時だけプリセット）
		currentWeapon_ = pistol_;
	}
}


/// -------------------------------------------------------------
///				　			　 更新処理
/// -------------------------------------------------------------
void Player::Update()
{

	if (input_->TriggerKey(DIK_F5)) { fpsCamera_->CycleViewMode(); }

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

	if (input_->TriggerKey(DIK_1)) { SelectWeapon("Pistol"); } // JSONにPistolがあれば1
	if (input_->TriggerKey(DIK_2)) { SelectWeapon("Rifle"); } // JSONにRifleがあれば2
	if (input_->TriggerKey(DIK_3)) { SelectWeapon("MachineGun"); } // JSONにMachineGunがあれば3
	if (input_->TriggerKey(DIK_4)) { SelectWeapon("ShotGun"); } // JSONにShotGunがあれば4

	Move();

	pistolWeapon_->Update(1.0f / 60.0f);

	ballisticEffect_->Update();

	const float dt = 1.0f / 60.0f;

	// ばね: accel = -k*x - c*v
	auto spring = [&](float& x, float& v) {
		float a = -recoilReturn_ * x - recoilDamping_ * v;
		v += a * dt;
		x += v * dt;
		};

	// 後退＆回転の戻り
	spring(recoilZ_, recoilVz_);
	spring(recoilPitch_, recoilVp_);
	spring(recoilYaw_, recoilVy_);

	// クールダウン更新
	if (fireCooldown_ > 0.0f)
	{
		fireCooldown_ -= dt;
	}

	auto& armT = parts_[rightArmIndex_].transform;

	if (input_->PushMouse(0))
	{
		if (fireCooldown_ <= 0.0f)
		{
			// forward の計算（あなたの式でOK）
			float yaw = fpsCamera_->GetYaw();
			float pitch = fpsCamera_->GetPitch();
			Vector3 forward{
				-std::sinf(yaw) * std::cosf(pitch),
				-std::sinf(pitch),
				 std::cosf(yaw) * std::cosf(pitch)
			};
			forward = Vector3::Normalize(forward);
			Vector3 worldUp{ 0,1,0 };
			Vector3 right = Vector3::Normalize(Vector3::Cross(worldUp, forward));
			Vector3 up = Vector3::Normalize(Vector3::Cross(forward, right));

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

			ApplyRecoil(kick, rise, horiz);
		}
	}

	BaseCharacter::Update();
}


/// -------------------------------------------------------------
///				　			　 描画処理
/// -------------------------------------------------------------
void Player::Draw()
{
	if (body_.active) { body_.object->Draw(); }

	for (auto& part : parts_) {
		if (part.active) { part.object->Draw(); }
	}

	pistolWeapon_->Draw();
	ballisticEffect_->Draw();
}

/// -------------------------------------------------------------
///				　		  中心座標を取得する純粋仮想関数
/// -------------------------------------------------------------
Vector3 Player::GetCenterPosition() const
{
	const Vector3 offset = { 0.0f,3.0f,0.0f };
	Vector3 worldPosition = body_.transform.translate_ + offset;
	return worldPosition;
}

/// -------------------------------------------------------------
///				　			　 移動処理
/// -------------------------------------------------------------
void Player::Move()
{
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

	if (fpsCamera_->GetViewMode() == FpsCamera::ViewMode::ThirdFront)
	{
		move.z *= -1.0f;
	}

	const float yaw = fpsCamera_->GetCamera()->GetRotate().y;
	const float s = std::sinf(yaw), c = std::cosf(yaw);
	move = { move.x * c - move.z * s, 0.0f, move.x * s + move.z * c };

	// 体の移動
	body_.transform.translate_ += move;

	const bool  isFP = (fpsCamera_->GetViewMode() == FpsCamera::ViewMode::FirstPerson);
	const float camYaw = fpsCamera_->GetYaw();
	const float camPitch = fpsCamera_->GetPitch();

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

	const float dt = 1.0f / 60.0f;          // 必要なら実Δtに置換

	// 空中は重力適用
	if (!isGrounded_)
	{
		vY_ += gravity_ * dt;
		if (vY_ < maxFallSpeed_) vY_ = maxFallSpeed_;
		body_.transform.translate_.y += vY_ * dt;

		// 着地（水平床の想定）
		if (body_.transform.translate_.y <= groundY_)
		{
			body_.transform.translate_.y = groundY_;
			vY_ = 0.0f;
			isGrounded_ = true;
		}
	}
	else
	{
		// 接地中は高さを維持（段差に対応するなら groundY_ を更新する）
		body_.transform.translate_.y = groundY_;
	}

	body_.transform.rotate_.y = bodyYaw_;
	parts_[0].transform.rotate_.y = headYawLocal_;

	// ===== 右手：FPはカメラ基準で“固定” =====
	auto& armT = parts_[rightArmIndex_].transform;
	if (isFP)
	{
		fpsCamera_->Update();
		armT.parent_ = nullptr;  // 親を外してワールド直置きにする

		// 右下・前に出すカメラローカルオフセット（好みで微調整）
		Vector3 offset = { 0.75f, -0.75f, 0.75 };

		// カメラ姿勢でオフセットを回す（Yaw→Pitch）
		Matrix4x4 Ry = Matrix4x4::MakeRotateY(camYaw);
		Matrix4x4 Rx = Matrix4x4::MakeRotateX(camPitch);
		Matrix4x4 R = Matrix4x4::Multiply(Rx, Ry);
		offset = Matrix4x4::Transform(offset, R);

		// カメラ位置＋回したオフセット（＝画面に“固定”される）
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
///				　		 武器データの追加
/// -------------------------------------------------------------
void Player::AddWeaponFrom(const std::string& newName, const WeaponData* base)
{
	// 1) ベースを決める（null なら今の武器の“雛形”を使う or デフォルトを用意）
	WeaponData w = base ? *base : WeaponData{};
	w.name = MakeUniqueName(weaponTable_, newName);

	// 2) テーブルへ追加
	weaponTable_[w.name] = w;

	// 3) 現在の運用系へ即反映（状態は新規）
	weapon_ = std::make_unique<Weapon>(w);
	currentWeapon_ = ToWeaponConfig(w);   // BallisticEffect 側が即この値で撃つため
	// ↑ Player の既存経路は currentWeapon_ を参照して発砲します。 :contentReference[oaicite:2]{index=2}

	// 4) JSON保存（ファイル一括書き戻し）
	// SaveWeapons(パス, テーブル) は Weapon.h に宣言があります（実装が無ければ後述の実装例を追加）。
	Weapon::SaveWeapons("Resources/JSON/weapons.json", weaponTable_);
}

/// -------------------------------------------------------------
///				　			　 武器選択
/// -------------------------------------------------------------
void Player::SelectWeapon(const std::string& name)
{
	// 1) JSONテーブルから探す（なければプリセットにフォールバック）
	auto it = weaponTable_.find(name);
	if (it != weaponTable_.end()) {
		currentWeapon_ = ToWeaponConfig(it->second);  // 発射系が即この値を使う
		// もし Weapon クラス側の状態も使っているなら新しく作り直す
		weapon_ = std::make_unique<Weapon>(it->second);
	}
	else {
		// 旧プリセットを使う場合（必要なら）
		if (name == "Pistol") currentWeapon_ = pistol_;
		else if (name == "Rifle") currentWeapon_ = rifle_;
		else if (name == "MachineGun") currentWeapon_ = mg_;
	}

	// 2) 連射タイマ等をリセット（切替直後の暴発防止）
	fireCooldown_ = 0.0f;
}

/// -------------------------------------------------------------
///				　			武器の削除
/// -------------------------------------------------------------
void Player::DeleteWeapon(const std::string& name)
{
	// 1) 存在チェック
	auto it = weaponTable_.find(name);
	if (it == weaponTable_.end()) return;

	// 2) 最後の1本は削除させない（安全策）
	if (weaponTable_.size() <= 1) {
		// 必要なら ImGui::OpenPopup("Can't delete last weapon"); などで通知
		return;
	}

	// 3) 実際に削除
	weaponTable_.erase(it);

	// 4) 次に選ぶ武器（テーブルの先頭でOK。特定の既定名があればそれを優先しても良い）
	std::string nextName = weaponTable_.begin()->first;

	// 選択中が消えた/消えてないに関わらず、安全に差し替え
	SelectWeapon(nextName);  // currentWeapon_ と weapon_ を更新。タイマもリセット。 :contentReference[oaicite:2]{index=2}

	// 5) JSONへ保存
	Weapon::SaveWeapons("Resources/JSON/weapons.json", weaponTable_); // 既存の保存APIを利用 :contentReference[oaicite:3]{index=3}
}

/// -------------------------------------------------------------
///				　		  ImGui描画処理
/// -------------------------------------------------------------
void Player::DrawImGui()
{
	ImGui::Separator();
	ImGui::Text("JSON / WeaponData check");
	if (weapon_) {
		const auto& D = weapon_->Data();
		ImGui::Text("name: %s", D.name.c_str());
		ImGui::Text("rpm: %.1f  muzzleSpeed: %.1f  maxDist: %.1f",
			D.rpm, D.muzzleSpeed, D.maxDistance);
		ImGui::Text("mag: %d  reserve: %d  reload: %.2f",
			D.magCapacity, D.startingReserve, D.reloadTime);
		ImGui::Text("tracerWidth: %.3f  startOffset: %.2f",
			D.tracer.tracerWidth, D.tracer.startOffsetForward);
		ImGui::Text("muzzle.life: %.3f  sparkCount: %d",
			D.muzzle.life, D.muzzle.sparkCount);
		ImGui::Text("casing.offsetR/U/B: %.2f / %.2f / %.2f",
			D.casing.offsetRight, D.casing.offsetUp, D.casing.offsetBack);
	}

	static std::string selectedName = "Pistol";  // とりあえずPistolから
	if (weapon_) selectedName = weapon_->Data().name;

	// セレクタ（必要なら Combo にしてもOK）
	ImGui::Text("Editing: %s", selectedName.c_str());

	// 編集対象をテーブルから“参照”で取得（ここをいじる → Save で書き出す）
	auto itEdit = weaponTable_.find(selectedName);
	if (itEdit != weaponTable_.end()) {
		WeaponData& E = itEdit->second; // ← ここを ImGui で編集する

		if (ImGui::TreeNode("Basic"))
		{
			ImGui::DragFloat("muzzleSpeed", &E.muzzleSpeed, 1.0f, 10.0f, 2000.0f);
			ImGui::DragFloat("maxDistance", &E.maxDistance, 1.0f, 10.0f, 5000.0f);
			ImGui::DragFloat("rpm", &E.rpm, 1.0f, 1.0f, 2000.0f);
			ImGui::DragInt("magCapacity", &E.magCapacity, 1, 1, 200);
			ImGui::DragInt("startingReserve", &E.startingReserve, 1, 0, 1000);
			ImGui::DragFloat("reloadTime", &E.reloadTime, 0.01f, 0.1f, 10.0f);
			ImGui::DragInt("bulletsPerShot", &E.bulletsPerShot, 1, 1, 20);

			ImGui::Checkbox("autoReload", &E.autoReload);
			ImGui::DragInt("requestedMaxSegments", &E.requestedMaxSegments, 1, 10, 1000);
			ImGui::DragFloat("spreadDeg", &E.spreadDeg, 0.1f, 0.0f, 45.0f);
			int mode = E.pelletTracerMode;
			const char* modes[] = { "None", "One of pellets", "All pellets" };
			if (ImGui::Combo("pelletTracerMode", &mode, modes, IM_ARRAYSIZE(modes))) {
				E.pelletTracerMode = mode;
			}
			ImGui::DragInt("pelletTracerCount", &E.pelletTracerCount, 1, 1, 64);

			ImGui::TreePop();
		}
		if (ImGui::TreeNode("Tracer"))
		{
			ImGui::Checkbox("enabled", &E.tracer.enabled);
			ImGui::DragFloat("tracerLength", &E.tracer.tracerLength, 0.01f, 0.01f, 50.0f);
			ImGui::DragFloat("tracerWidth", &E.tracer.tracerWidth, 0.001f, 0.001f, 1.0f);
			ImGui::DragFloat("minSegLength", &E.tracer.minSegLength, 0.001f, 0.001f, 1.0f);
			ImGui::DragFloat("startOffsetForward", &E.tracer.startOffsetForward, 0.01f, -10.0f, 10.0f);
			ImGui::ColorEdit4("color", &E.tracer.color.x);
			ImGui::TreePop();
		}
		if (ImGui::TreeNode("Muzzle"))
		{
			ImGui::Checkbox("enabled##mz", &E.muzzle.enabled);
			ImGui::DragFloat("life", &E.muzzle.life, 0.005f, 0.01f, 0.5f);
			ImGui::DragFloat("startLength", &E.muzzle.startLength, 0.01f, 0.01f, 1.0f);
			ImGui::DragFloat("endLength", &E.muzzle.endLength, 0.01f, 0.01f, 1.0f);
			ImGui::DragFloat("startWidth", &E.muzzle.startWidth, 0.005f, 0.01f, 0.5f);
			ImGui::DragFloat("endWidth", &E.muzzle.endWidth, 0.005f, 0.01f, 0.5f);
			ImGui::DragFloat("randomYawDeg", &E.muzzle.randomYawDeg, 0.1f, 0.0f, 45.0f);
			ImGui::ColorEdit4("color##mz", &E.muzzle.color.x);
			ImGui::DragFloat("offsetForward", &E.muzzle.offsetForward, 0.01f, -1.0f, 1.0f);
			ImGui::Checkbox("sparksEnabled", &E.muzzle.sparksEnabled);
			ImGui::DragInt("sparkCount", &E.muzzle.sparkCount, 1, 0, 200);
			ImGui::DragFloat("sparkLifeMin", &E.muzzle.sparkLifeMin, 0.005f, 0.01f, 1.0f);
			ImGui::DragFloat("sparkLifeMax", &E.muzzle.sparkLifeMax, 0.005f, 0.01f, 1.0f);
			ImGui::DragFloat("sparkSpeedMin", &E.muzzle.sparkSpeedMin, 0.1f, 0.1f, 100.0f);
			ImGui::DragFloat("sparkSpeedMax", &E.muzzle.sparkSpeedMax, 0.1f, 0.1f, 100.0f);
			ImGui::DragFloat("sparkConeDeg", &E.muzzle.sparkConeDeg, 0.1f, 0.0f, 90.0f);
			ImGui::DragFloat("sparkGravityY", &E.muzzle.sparkGravityY, 0.1f, -100.0f, 0.0f);
			ImGui::DragFloat("sparkWidth", &E.muzzle.sparkWidth, 0.001f, 0.001f, 0.1f);
			ImGui::DragFloat("sparkOffsetForward", &E.muzzle.sparkOffsetForward, 0.01f, -1.0f, 1.0f);
			ImGui::ColorEdit4("sparkColorStart", &E.muzzle.sparkColorStart.x);
			ImGui::ColorEdit4("sparkColorEnd", &E.muzzle.sparkColorEnd.x);
			ImGui::TreePop();
		}
		if (ImGui::TreeNode("Casing"))
		{
			ImGui::Checkbox("enabled##cs", &E.casing.enabled);
			ImGui::DragFloat3("offset R/U/B", &E.casing.offsetRight, 0.005f);
			ImGui::DragFloat("speedMin", &E.casing.speedMin, 0.1f, 0.0f, 20.0f);
			ImGui::DragFloat("speedMax", &E.casing.speedMax, 0.1f, 0.0f, 20.0f);
			ImGui::DragFloat("coneDeg", &E.casing.coneDeg, 0.1f, 0.0f, 90.0f);
			ImGui::DragFloat("gravityY", &E.casing.gravityY, 0.1f, -100.0f, 0.0f);
			ImGui::DragFloat("life", &E.casing.life, 0.05f, 0.1f, 10.0f);
			ImGui::DragFloat("drag", &E.casing.drag, 0.01f, 0.0f, 1.0f);
			ImGui::DragFloat("upKick", &E.casing.upKick, 0.01f, 0.0f, 10.0f);
			ImGui::DragFloat("upBias", &E.casing.upBias, 0.01f, 0.0f, 1.0f);
			ImGui::DragFloat("spinMin", &E.casing.spinMin, 0.1f, 0.0f, 100.0f);
			ImGui::DragFloat("spinMax", &E.casing.spinMax, 0.1f, 0.0f, 100.0f);
			ImGui::ColorEdit4("color##cs", &E.casing.color.x);
			ImGui::DragFloat3("scale", &E.casing.scale.x, 0.001f);
			ImGui::TreePop();
		}

		if (ImGui::Button("Save weapons.json")) {
			Weapon::SaveWeapons("Resources/JSON/weapons.json", weaponTable_);
		}
		ImGui::SameLine();
		if (ImGui::Button("Reload weapons.json")) {
			weaponTable_ = Weapon::LoadWeapon("Resources/JSON/weapons.json");
			auto it = weaponTable_.find(selectedName);
			if (it != weaponTable_.end()) {
				weapon_ = std::make_unique<Weapon>(it->second); // 状態を更新
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Apply to runtime"))
		{
			// 実行中の発射系（currentWeapon_）へ反映：今の経路を維持したまま
			currentWeapon_.name = E.name;
			currentWeapon_.muzzleSpeed = E.muzzleSpeed;
			currentWeapon_.maxDistance = E.maxDistance;
			currentWeapon_.rpm = E.rpm;
			currentWeapon_.autoReload = E.autoReload;
			currentWeapon_.requestedMaxSegments = E.requestedMaxSegments;
			currentWeapon_.spreadDeg = E.spreadDeg;
			currentWeapon_.pelletTracerMode = E.pelletTracerMode;
			currentWeapon_.pelletTracerCount = E.pelletTracerCount;

			// tracer
			currentWeapon_.tracer.enabled = E.tracer.enabled;
			currentWeapon_.tracer.tracerLength = E.tracer.tracerLength;
			currentWeapon_.tracer.tracerWidth = E.tracer.tracerWidth;
			currentWeapon_.tracer.minSegLength = E.tracer.minSegLength;
			currentWeapon_.tracer.startOffsetForward = E.tracer.startOffsetForward;
			currentWeapon_.tracer.tracerColor = E.tracer.color;

			// muzzle
			currentWeapon_.muzzle.enabled = E.muzzle.enabled;
			currentWeapon_.muzzle.life = E.muzzle.life;
			currentWeapon_.muzzle.startLength = E.muzzle.startLength;
			currentWeapon_.muzzle.endLength = E.muzzle.endLength;
			currentWeapon_.muzzle.startWidth = E.muzzle.startWidth;
			currentWeapon_.muzzle.endWidth = E.muzzle.endWidth;
			currentWeapon_.muzzle.randomYawDeg = E.muzzle.randomYawDeg;
			currentWeapon_.muzzle.color = E.muzzle.color;
			currentWeapon_.muzzle.offsetForward = E.muzzle.offsetForward;
			currentWeapon_.muzzle.sparksEnabled = E.muzzle.sparksEnabled;
			currentWeapon_.muzzle.sparkCount = E.muzzle.sparkCount;
			currentWeapon_.muzzle.sparkLifeMin = E.muzzle.sparkLifeMin;
			currentWeapon_.muzzle.sparkLifeMax = E.muzzle.sparkLifeMax;
			currentWeapon_.muzzle.sparkSpeedMin = E.muzzle.sparkSpeedMin;
			currentWeapon_.muzzle.sparkSpeedMax = E.muzzle.sparkSpeedMax;
			currentWeapon_.muzzle.sparkConeDeg = E.muzzle.sparkConeDeg;
			currentWeapon_.muzzle.sparkGravityY = E.muzzle.sparkGravityY;
			currentWeapon_.muzzle.sparkWidth = E.muzzle.sparkWidth;
			currentWeapon_.muzzle.sparkOffsetForward = E.muzzle.sparkOffsetForward;
			currentWeapon_.muzzle.sparkColorStart = E.muzzle.sparkColorStart;
			currentWeapon_.muzzle.sparkColorEnd = E.muzzle.sparkColorEnd;

			// casing
			currentWeapon_.casing.enabled = E.casing.enabled;
			currentWeapon_.casing.offsetRight = E.casing.offsetRight;
			currentWeapon_.casing.offsetUp = E.casing.offsetUp;
			currentWeapon_.casing.offsetBack = E.casing.offsetBack;
			currentWeapon_.casing.speedMin = E.casing.speedMin;
			currentWeapon_.casing.speedMax = E.casing.speedMax;
			currentWeapon_.casing.coneDeg = E.casing.coneDeg;
			currentWeapon_.casing.gravityY = E.casing.gravityY;
			currentWeapon_.casing.drag = E.casing.drag;
			currentWeapon_.casing.life = E.casing.life;
			currentWeapon_.casing.upKick = E.casing.upKick;
			currentWeapon_.casing.upBias = E.casing.upBias;
			currentWeapon_.casing.spinMin = E.casing.spinMin;
			currentWeapon_.casing.spinMax = E.casing.spinMax;
			currentWeapon_.casing.color = E.casing.color;
			currentWeapon_.casing.scale = E.casing.scale;
		}
		ImGui::SameLine();
		if (ImGui::Button("Delete Weapon")) {
			ImGui::OpenPopup("DeleteWeaponConfirm");
		}
		if (ImGui::BeginPopupModal("DeleteWeaponConfirm", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
			ImGui::Text("Delete '%s' ?\nThis cannot be undone.", selectedName.c_str());
			ImGui::Separator();
			if (ImGui::Button("Yes, delete", ImVec2(120, 0))) {
				DeleteWeapon(selectedName);
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel", ImVec2(120, 0))) {
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}
	}

	if (ImGui::Button("Add Weapon"))
	{
		ImGui::OpenPopup("AddWeaponPopup");
	}
	if (ImGui::BeginPopupModal("AddWeaponPopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		static char nameBuf[64] = "NewWeapon";
		static int sourceMode = 0; // 0=Duplicate Current, 1=Empty(Default), 2=Duplicate Selected
		static int selectedIndex = 0;

		ImGui::InputText("Name", nameBuf, IM_ARRAYSIZE(nameBuf));
		ImGui::RadioButton("Duplicate Current", &sourceMode, 0); ImGui::SameLine();
		ImGui::RadioButton("Empty(Default)", &sourceMode, 1); ImGui::SameLine();
		ImGui::RadioButton("Duplicate Selected", &sourceMode, 2);

		// 既存一覧
		std::vector<std::string> names;
		names.reserve(weaponTable_.size());
		for (auto& kv : weaponTable_) names.push_back(kv.first);
		if (names.empty()) { names.push_back("Pistol"); }
		if (sourceMode == 2) {
			if (ImGui::BeginCombo("From", names[selectedIndex].c_str())) {
				for (int i = 0; i < (int)names.size(); ++i) {
					bool sel = (i == selectedIndex);
					if (ImGui::Selectable(names[i].c_str(), sel)) selectedIndex = i;
					if (sel) ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}
		}

		if (ImGui::Button("Create")) {
			WeaponData* base = nullptr;
			if (sourceMode == 0) {
				// 現在編集中の weapon_->Data() を基に
				base = const_cast<WeaponData*>(&weapon_->Data());
			}
			else if (sourceMode == 2) {
				base = &weaponTable_[names[selectedIndex]];
			} // sourceMode==1 は base=nullptr → デフォルト

			AddWeaponFrom(nameBuf, base);
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel")) {
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
}
