#define NOMINMAX
#include "SceneManager.h"

#include "DirectXCommon.h"

// 「重いシーンだけロード完了を待ってから開く」ための判定に使う
#include "GamePlayScene.h"
#include "StageSelectScene.h"

#include <cassert>

/// -------------------------------------------------------------
///                     シングルトンインスタンス
/// -------------------------------------------------------------
SceneManager* SceneManager::GetInstance()
{
	static SceneManager instance;
	return &instance;
}

SceneManager::~SceneManager() = default;

void SceneManager::Initialize()
{
	// 遷移開始時のヒッチ回避（タイルSpriteを先に生成）
	fadeManager_.Initialize();
}

void SceneManager::Update()
{
	// 遷移中（閉じ中）は現シーンUpdate止める。
	// ※ Hold/Uncover中は「次のシーン」をUpdateしてロードを進めたいので止めない
	if (scene_ && !fadeManager_.IsCovering())
	{
		scene_->Update();
	}

	const float dt = DirectXCommon::GetInstance()->GetFPSCounter().GetDeltaTime();
	const bool coverJustFinished = fadeManager_.Update(dt);

	// 完全に覆った瞬間：ここで nextScene を生成＆差し替え（黒で隠れている）
	if (coverJustFinished)
	{
		// CreateScene をここで実行（ChangeScene時にはしない）
		if (!CreateReservedNextScene())
		{
			// 生成失敗：元に戻す（開いて戻る）
			waitingUncover_ = false;
			fadeManager_.StartUncover();
			return;
		}

		ApplyNextScene();
		waitingUncover_ = true; // ロード完了まで Hold で待つ
	}

	// Hold中：シーン側が「開ける」状態になったら Uncover 開始
	if (waitingUncover_)
	{
		// 何らかの理由で遷移が終わっていたら安全に解除
		if (!fadeManager_.IsTransitioning())
		{
			waitingUncover_ = false;
			return;
		}

		if (fadeManager_.IsHolding() && fadeManager_.IsHoldMinSatisfied() && IsSceneReadyForUncover())
		{
			fadeManager_.StartUncover();
			waitingUncover_ = false;
		}
	}
}

void SceneManager::Draw3DObjects()
{
	if (scene_) { scene_->Draw3DObjects(); }
}

void SceneManager::Draw2DSprites()
{
	if (scene_) { scene_->Draw2DSprites(); }
	// タイルオーバーレイは常に最前面
	fadeManager_.Draw2DSprites();
}

void SceneManager::DrawImGui()
{
	if (scene_) { scene_->DrawImGui(); }
}

void SceneManager::Finalize()
{
	if (scene_) { scene_->Finalize(); }
	nextScene_.reset();
	sceneFactory_.reset();
	fadeManager_.Finalize();
	hasReservedScene_ = false;
	reservedSceneName_.clear();
	waitingUncover_ = false;
}

void SceneManager::DrawTransitionOverlay()
{
	// ImGuiの後など「本当に最後」に被せたい時用
	fadeManager_.Draw2DSprites();
}

void SceneManager::ChangeScene(const std::string& sceneName, bool useFade)
{
	assert(sceneFactory_);

	// すでに遷移中/予約済みなら無視
	if (fadeManager_.IsTransitioning() || nextScene_ || hasReservedScene_) return;

	// 最初のシーンは即
	if (!scene_)
	{
		nextScene_ = sceneFactory_->CreateScene(sceneName);
		ApplyNextScene();
		fadeManager_.Cancel();
		waitingUncover_ = false;
		return;
	}

	// フェード無しは即
	if (!useFade)
	{
		nextScene_ = sceneFactory_->CreateScene(sceneName);
		ApplyNextScene();
		fadeManager_.Cancel();
		waitingUncover_ = false;
		return;
	}

	// CreateSceneはしない：完全に閉じてから生成する
	reservedSceneName_ = sceneName;
	hasReservedScene_ = true;
	waitingUncover_ = false;

	// タイルで閉じる開始（最低でも数フレームは保持してから開ける）
	fadeManager_.StartCover(/*minHoldSec*/0.05f, /*minHoldFrames*/2);
}

void SceneManager::ApplyNextScene()
{
	if (!nextScene_) return;

	// 現在のシーンを終了
	if (scene_) { scene_->Finalize(); }

	// 差し替え
	scene_ = std::move(nextScene_);

	// シーンマネージャーをセット
	if (scene_) { scene_->SetSceneManager(this); }

	// 初期化
	if (scene_) { scene_->Initialize(); }
}

bool SceneManager::CreateReservedNextScene()
{
	if (!hasReservedScene_) return true; // 予約無し

	// 生成
	nextScene_ = sceneFactory_->CreateScene(reservedSceneName_);

	// 予約解除
	hasReservedScene_ = false;
	reservedSceneName_.clear();

	return (nextScene_ != nullptr);
}

bool SceneManager::IsSceneReadyForUncover() const
{
	if (!scene_) return true;

	// GamePlay はロード状態が終わるまで待つ
	if (const auto* gp = dynamic_cast<const GamePlayScene*>(scene_.get()))
	{
		const auto st = gp->GetState();
		return st != GamePlayScene::State::Loading && st != GamePlayScene::State::SettingUp;
	}

	// StageSelect も Loading 中なら待つ（使っていないなら常にSelectingでOK）
	if (const auto* ss = dynamic_cast<const StageSelectScene*>(scene_.get()))
	{
		return ss->GetState() != StageSelectScene::State::Loading;
	}

	// それ以外は即OK（←これが「Title/StageSelectで開かない」系のデッドロックを防ぐ）
	return true;
}
