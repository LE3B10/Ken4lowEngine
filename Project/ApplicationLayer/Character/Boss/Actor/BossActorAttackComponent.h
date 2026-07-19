#pragma once

#include <Scene/Actor/Character/AttackComponent.h>

#include <string>

namespace Ken4lowEngine
{
	/// Boss用の複数攻撃定義とフェーズ別選択を共通AttackComponentへ追加するComponent。
	class BossAttackComponent final : public AttackComponent
	{
	public:
		/// JSON復元済み攻撃プロファイルに応じたBoss攻撃を登録する。
		void Initialize() override;

		/// 共通攻撃状態にBossの直近選択結果を追加表示する。
		void DrawImGui() override;

		/// 攻撃プロファイルと共通攻撃データをJSONへ保存する。
		void ToJson(nlohmann::json& outJson) const override;
		void FromJson(const nlohmann::json& inJson) override;

		/// JSON保存・復元で使用するComponent識別名を返す。
		std::string GetClassTypeName() const override { return "BossAttackComponent"; }

		/// 距離と現在フェーズに適した登録攻撃を優先順に開始する。
		bool TryStartBestAttack(float distanceToTarget, int bossPhase);

		/// 直近に開始できた攻撃IDを返す。
		const std::string& GetLastSelectedAttackId() const { return lastSelectedAttackId_; }
		const std::string& GetAttackProfile() const { return attackProfile_; }

	private:
		/// 選択されたArchetypeに対応する近接・突進・範囲攻撃を登録する。
		void RegisterDefaultAttacks();
		void RegisterGuardianAttacks();
		void RegisterMineCrusherAttacks();

	private:
		std::string attackProfile_ = "Guardian";
		std::string lastSelectedAttackId_ = "None";
	};
} // namespace Ken4lowEngine
