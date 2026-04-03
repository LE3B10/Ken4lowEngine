#define NOMINMAX
#include "DebugScene.h"
#include <DirectXCommon.h>
#include <Input.h>
#include <SpriteManager.h>
#include "CameraManager.h"
#include "Wireframe.h"
#include <GameTimer.h>
#ifdef _DEBUG
#include <DebugCamera.h>
#endif // _DEBUG

#ifdef USE_IMGUI
#include <ImGuiManager.h>
#endif // USE_IMGUI
#include <GpuParticleManager.h>

#include "WeaponMasterDataDatabase.h"
#include "WeaponMasterDataEditor.h"
#include "WeaponMasterDataWriter.h"
#include <filesystem>

using namespace Ken4lowEngine;

namespace
{
	/// -------------------------------------------------------------
	/// Visual Studio の「出力」ウィンドウへ文字列を出す
	/// 改行付きで送るための簡易ヘルパー
	/// -------------------------------------------------------------
	void DebugLog(const std::string& message)
	{
		std::string line = message + "\n";
		OutputDebugStringA(line.c_str());
	}
}

void DebugScene::Initialize()
{
#ifdef _DEBUG
	// デバッグカメラの初期化
	DebugCamera::GetInstance()->Initialize();
#endif // _DEBUG

	dxCommon_ = DirectXCommon::GetInstance();
	input_ = Input::GetInstance();

	/*input_->SetLockCursor(true);
	input_->SetCursorVisible(false);*/

	collisionManager_ = std::make_unique<CollisionManager>();
	collisionManager_->Initialize();

	debugBoss_ = std::make_unique<GuardianBoss>();
	debugBoss_->Initialize();
	collisionManager_->AddCollider(debugBoss_.get());

	// 見やすい位置に置く
	debugBoss_->SetPosition({ 0.0f, 2.25f, 30.0f });
	debugBoss_->SetYaw(3.141592f); // 必要ならプレイヤー側へ向ける
}

void DebugScene::Update()
{
#ifdef _DEBUG
	UpdateDebug();
#endif // _DEBUG

	float deltaTime = K4E::GameTimer::GetInstance()->GetDeltaTime();

	static float animTime = 0.0f;
	animTime += deltaTime;

	// ボス更新
	if (debugBoss_)
	{
		// とりあえずプレイヤー位置をターゲットに渡す
		debugBoss_->SetTargetPosition({});
		debugBoss_->Update(deltaTime);
	}

	UpdateDebugBossHitTest();

	UpdateDebugParticleTest();

	collisionManager_->Update();
	collisionManager_->CheckAllCollisions();

}

void DebugScene::Draw3DObjects()
{
	// ボス描画
	if (debugBoss_)
	{
		debugBoss_->Draw();
	}

#ifdef _DEBUG
	// ワイヤーフレームの描画
	Wireframe::GetInstance()->DrawGrid(100.0f, 50.0f, { 0.25f, 0.25f, 0.25f,1.0f });

	collisionManager_->Draw();
#endif // _DEBUG
}

void DebugScene::DrawShadowObjects()
{
	if (debugBoss_)
	{
		debugBoss_->DrawShadow();
	}
}

void DebugScene::Draw2DSprites()
{
#pragma region スプライトの描画                    

	// 背景用の共通描画設定（後面）
	SpriteManager::GetInstance()->SetRenderSetting_Background();

#pragma endregion


#pragma region UIの描画

	// UI用の共通描画設定
	SpriteManager::GetInstance()->SetRenderSetting_UI();


#pragma endregion
}

void DebugScene::Finalize()
{
	// 入力状態を必ず戻す（ロック/非表示のまま終了しない）
	input_->SetLockCursor(false);
	input_->SetCursorVisible(true);

	debugBoss_.reset();
	collisionManager_.reset();

	input_ = nullptr;
	dxCommon_ = nullptr;
}

void DebugScene::DrawImGui()
{
#ifdef USE_IMGUI

	if (debugBoss_)
	{
		debugBoss_->DrawImGui();
	}

	ImGui::Begin("Debug Boss Hit Test");

	ImGui::Checkbox("Enable Hit Test", &debugBossHitTestEnabled_);
	ImGui::DragFloat("Hit Radius", &debugHitRadius_, 0.01f, 0.1f, 5.0f);
	ImGui::DragFloat("Base Damage", &debugBaseDamage_, 0.1f, 1.0f, 999.0f);

	ImGui::Separator();
	ImGui::Text("Press H to test hit.");
	ImGui::TextWrapped("%s", debugHitLog_.c_str());

	ImGui::End();

	/// ---------- GPUパーティクルデバッグ ---------- ///
	GpuParticleManager::GetInstance()->DrawImGui();

	/// ---------- 武器マスターデータエディタ ---------- ///
	static WeaponMasterDataDatabase weaponDB;
	static WeaponMasterDataEditor weaponEditor;
	static WeaponEditorHooks hooks;
	static bool initialized = false;
	static int32_t lastAppliedID = 0;

	if (!initialized)
	{
		initialized = true;

		// まだ保存/再読込はしないので一旦空実装でOK
		hooks.SaveAll = [&]()
			{
				std::string err;
				const std::filesystem::path outRoot = "Resources/JSON/weapons";
				WeaponMasterDataWriter::SaveAllByCategory(weaponDB, outRoot, &err);
			};
		hooks.RequestReloadFocus = [](int32_t) {};
		hooks.RebuildLoadout = []() {};

		// Applyされたら「最後のID」を更新（動作確認）
		hooks.ApplyToRuntimeIfCurrent =
			[&](int32_t weaponID, const FWeaponMasterData&)
			{
				lastAppliedID = weaponID;
			};

		// 削除はDBから消すだけ（ファイル削除は後で）
		hooks.RequestDelete =
			[&](int32_t weaponID)
			{
				std::string err;
				const std::filesystem::path outRoot = "Resources/JSON/weapons";

				// まずはディスク上のjsonファイルを削除
				WeaponMasterDataWriter::DeleteFilesByWeaponID(outRoot, weaponID, &err);

				// DBから削除
				weaponDB.RemoveByID(weaponID);
			};

		// 追加予約は今は使わないなら空でOK
		hooks.RequestAdd = [](const std::string&, int32_t) {};

		// 初期データを2つだけ作る（任意）
		weaponDB.Clear();
	}

	ImGui::Begin("武器マスターデータエディタ");
	if (ImGui::CollapsingHeader("Weapon Master Editor", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Text("Count: %zu", weaponDB.Size());
		ImGui::Text("Last Applied ID: %d", lastAppliedID);
		ImGui::Separator();

		weaponEditor.DrawImGui(weaponDB, hooks);
	}

	ImGui::Separator();
	ImGui::Text("Press H to test hit.");
	ImGui::TextWrapped("%s", debugHitLog_.c_str());

	ImGui::Separator();
	ImGui::Text("Particle Test");
	ImGui::Text("1 : HitSpark");
	ImGui::Text("2 : Heal_Effect");
	ImGui::Text("3 : Boss_Appear_Dust");
	ImGui::TextWrapped("%s", debugParticleLog_.c_str());

	ImGui::End();

#endif // USE_IMGUI

}

void DebugScene::UpdateDebug()
{
	if (input_->TriggerKey(DIK_F12))
	{
		CameraManager::GetInstance()->SetUseDebugCamera(!CameraManager::GetInstance()->IsUsingDebugCamera());
		Wireframe::GetInstance()->SetDebugCamera(!Wireframe::GetInstance()->GetDebugCamera());
		GpuParticleManager::GetInstance()->SetDebugCameraEnabled(!isDebugCamera_);
		isDebugCamera_ = !isDebugCamera_;

		/*input_->SetLockCursor(!isDebugCamera_);
		input_->SetCursorVisible(isDebugCamera_);*/
	}
}

/// -------------------------------------------------------------
/// BossHitPart をログ用文字列へ変換
/// -------------------------------------------------------------
const char* DebugScene::ToString(BossHitPart part) const
{
	switch (part)
	{
	case BossHitPart::Head:     return "Head";
	case BossHitPart::Body:     return "Body";
	case BossHitPart::LeftArm:  return "LeftArm";
	case BossHitPart::RightArm: return "RightArm";
	case BossHitPart::LeftLeg:  return "LeftLeg";
	case BossHitPart::RightLeg: return "RightLeg";
	default:                    return "None";
	}
}

/// -------------------------------------------------------------
/// DebugScene での仮ヒット確認
///
/// Hキーを押した瞬間に簡易球判定を飛ばし、
/// 結果を OutputDebugStringA で出力する
/// -------------------------------------------------------------
void DebugScene::UpdateDebugBossHitTest()
{
	if (!debugBossHitTestEnabled_)
	{
		return;
	}

	if (!debugBoss_)
	{
		debugHitLog_ = "Boss or Player is null.";
		DebugLog(debugHitLog_);
		return;
	}

	// Hキーを押した瞬間だけ判定
	if (!input_->TriggerKey(DIK_H))
	{
		return;
	}

	// ---------------------------------------------------------
	// 仮の攻撃位置
	// 本来は弾のヒット位置や近接武器先端などを使うが、
	// 今回はデバッグ用としてボス中心より少し上を狙う
	// ---------------------------------------------------------
	Vector3 attackCenter = debugBoss_->GetCenterPosition();
	attackCenter.y += 1.0f; // 頭寄りを狙いやすくする

	// BossBase 側の簡易球判定
	const BossHitResult hitResult =
		debugBoss_->CheckDebugHitSphere(attackCenter, debugHitRadius_);

	if (hitResult.isHit)
	{
		// 倍率込みダメージを適用
		debugBoss_->ApplyDebugHitResult(hitResult, debugBaseDamage_);

		Vector3 effectPos = attackCenter;
		effectPos.y += 0.15f;

		GpuParticleManager::GetInstance()->EmitBurst(
			"Debug_HitSpark_OnHit",
			GpuParticleType::Spark,
			effectPos,
			18);

		// 画面表示用にも保持
		debugHitLog_ =
			std::string("HIT  Part: ") + ToString(hitResult.part) +
			"  Damage: " + std::to_string(debugBaseDamage_ * hitResult.damageMultiplier) +
			"  HP: " + std::to_string(debugBoss_->GetHP()) +
			" / " + std::to_string(debugBoss_->GetMaxHP());

		// Visual Studio の出力ウィンドウへ送る
		DebugLog(debugHitLog_);
	}
	else
	{
		debugHitLog_ = "MISS";
		DebugLog(debugHitLog_);
	}
}

void DebugScene::UpdateDebugParticleTest()
{
	if (!debugBoss_)
	{
		return;
	}

	GpuParticleManager* gpuParticleManager = GpuParticleManager::GetInstance();
	if (!gpuParticleManager)
	{
		debugParticleLog_ = "GpuParticleManager is null.";
		return;
	}

	const Vector3 bossCenter = debugBoss_->GetCenterPosition();

	// ---------------------------------------------------------
	// 1キー: ヒット火花
	// ---------------------------------------------------------
	if (input_->TriggerKey(DIK_1))
	{
		Vector3 pos = bossCenter;
		pos.y += 1.0f;

		gpuParticleManager->EmitBurst(
			"Debug_HitSpark",
			GpuParticleType::Spark,
			pos,
			20);

		debugParticleLog_ = "Spawn: HitSpark";
		DebugLog(debugParticleLog_);
	}

	// ---------------------------------------------------------
	// 2キー: 回復エフェクト
	// ---------------------------------------------------------
	if (input_->TriggerKey(DIK_2))
	{
		Vector3 pos = bossCenter;
		pos.y += 1.5f;

		gpuParticleManager->EmitBurst(
			"Debug_Heal",
			GpuParticleType::Heal,
			pos,
			24);

		debugParticleLog_ = "Spawn: Heal_Effect";
		DebugLog(debugParticleLog_);
	}

	// ---------------------------------------------------------
	// 3キー: ボス登場砂埃
	// ---------------------------------------------------------
	if (input_->TriggerKey(DIK_3))
	{
		Vector3 pos = bossCenter;

		gpuParticleManager->EmitBurst(
			"Debug_BossAppear",
			GpuParticleType::Default,
			pos,
			48);

		debugParticleLog_ = "Spawn: Boss_Appear_Dust";
		DebugLog(debugParticleLog_);
	}
}
