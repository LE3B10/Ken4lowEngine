#pragma once

#include "GamePlayWorld.h"
#include "PostEffect/PlayerHealthPostEffectController.h"

#include <PostEffectManager.h>
#include <Player.h>

#include <memory>

/// <summary>
/// GamePlayScene で発生する演出系 Controller です。<br/>
/// 現段階では Player の被弾通知と HP 連動ポストエフェクトを担当し、
/// GPU Particle / EffectSystem などの runtime 演出入口を今後ここへ集約できる境界にします。
/// </summary>
class GamePlayEffectController
{
public:
	/// <summary>
	/// World 内の Player とポストエフェクトを接続します。
	/// </summary>
	void Initialize(GamePlayWorld* world)
	{
		hpPostEffectController_ = std::make_unique<PlayerHealthPostEffectController>();
		hpPostEffectController_->Initialize(Ken4lowEngine::PostEffectManager::GetInstance());
		BindPlayerDamageCallback(world);
	}

	/// <summary>
	/// 古い Player 参照が残らないよう、Scene 終了やリトライ再生成前に接続を解除します。
	/// </summary>
	void Finalize(GamePlayWorld* world)
	{
		if (auto* player = world ? world->GetCharacters().GetPlayer() : nullptr)
		{
			player->SetOnDamageTakenCallback({});
		}

		if (hpPostEffectController_)
		{
			hpPostEffectController_->Finalize();
			hpPostEffectController_.reset();
		}
	}

	/// <summary>
	/// HP 状態と被弾通知に応じたポストエフェクトを更新します。
	/// </summary>
	void Update(float deltaTime, GamePlayWorld* world)
	{
		if (hpPostEffectController_)
		{
			hpPostEffectController_->Update(deltaTime, world ? world->GetCharacters().GetPlayer() : nullptr);
		}
	}

	/// <summary>
	/// Player Debug ウィンドウへ、演出調整用の ImGui 内容を描画します。
	/// </summary>
	void DrawPlayerDebugContent()
	{
		if (hpPostEffectController_)
		{
			hpPostEffectController_->DrawImGuiContent();
		}
	}

	bool HasPlayerDebugContent() const { return hpPostEffectController_ != nullptr; }

private:
	void BindPlayerDamageCallback(GamePlayWorld* world)
	{
		if (auto* player = world ? world->GetCharacters().GetPlayer() : nullptr)
		{
			player->SetOnDamageTakenCallback([this]()
				{
					if (hpPostEffectController_)
					{
						hpPostEffectController_->NotifyDamageTaken();
					}
				});
		}
	}

	std::unique_ptr<PlayerHealthPostEffectController> hpPostEffectController_;
};
