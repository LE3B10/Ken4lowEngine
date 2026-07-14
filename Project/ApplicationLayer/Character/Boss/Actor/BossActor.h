#pragma once

#include <Scene/Actor/Character/CharacterActor.h>

#include <string_view>

namespace Ken4lowEngine
{
	class BossAttackComponent;
	class BossBrainComponent;
	class BossPhaseComponent;
	class BossPresentationComponent;
	class BossWeakPointComponent;
	class GaugeComponent;
	class HumanoidVisualComponent;
	class TextComponent;

	/// 共通Character機能とBoss専用判断・攻撃・フェーズ・弱点・演出・HP HUDを束ねるActor。
	class BossActor final : public CharacterActor
	{
	public:
		/// Boss専用Componentを不足分だけ生成し、共通Character Componentへ接続する。
		void Initialize() override;

		/// 共通更新後にBoss Healthを画面上部Gaugeへ同期する。
		void Update(float deltaTime) override;

		/// JSON保存・復元で使用するActor識別名を返す。
		std::string GetClassTypeName() const override { return "BossActor"; }

		/// BrainとAttackへ同じ追跡対象を設定する。
		void SetTargetActor(CharacterActor* targetActor);

		/// 弱点部位IDを解決し、倍率適用後のダメージを共通Healthへ渡す。
		CharacterDamageResult ApplyWeakPointDamage(std::string_view partId, float baseDamage);

		/// DebugSceneで同じ個体を再検証できる初期状態へ戻す。
		void ResetForValidation(const Vector3& worldPosition);

		/// Bossの行動判断Componentを返す。
		BossBrainComponent* GetBossBrainComponent();

		/// Bossの攻撃選択と共通攻撃実行Componentを返す。
		BossAttackComponent* GetBossAttackComponent();

		/// HP割合からフェーズを判定するComponentを返す。
		BossPhaseComponent* GetBossPhaseComponent();

		/// 部位ID参照で弱点倍率を管理するComponentを返す。
		BossWeakPointComponent* GetBossWeakPointComponent();

		/// Phase遷移と死亡演出を管理するComponentを返す。
		BossPresentationComponent* GetBossPresentationComponent();

		/// Bossの全部位描画を担当するComponentを返す。
		HumanoidVisualComponent* GetHumanoidVisualComponent();

		/// 画面上部のBoss HP Gaugeを返す。
		GaugeComponent* GetHealthGaugeComponent();

		/// Boss HPラベルを返す。
		TextComponent* GetHealthLabelComponent();

	protected:
		/// 死亡時は判断・攻撃・移動・Colliderを止め、死亡演出を開始する。
		void OnDeath(const CharacterDeathEvent& deathEvent) override;

	private:
		/// CharacterHealthの現在値を画面固定Gaugeへ反映する。
		void SyncHealthHud();
	};
} // namespace Ken4lowEngine
