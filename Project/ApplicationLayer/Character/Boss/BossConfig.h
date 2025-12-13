#pragma once
#include <string>
#include <vector>
#include <cstdint>

/// ---------- 森のボス用パラメータ構造体 ---------- ///
struct ForestBossParams
{
	/// ---------- 基本パラメータ ---------- ///
	float maxHealth = 2400.0f;          // 最大体力
	float contactDamage = 20.0f;        // 接触ダメージ
	float moveSpeed = 3.6f;             // 移動速度
	float minDistance = 2.5f;			// プレイヤーに近づく最小距離

	/// ---------- 弱点コアパラメータ ---------- ///
	struct WeakPoint
	{
		float radius = 0.5f;			// 弱点コアの当たり判定半径
		float damageMulExposed = 3.0f;	// 露出中の倍率
		float damageMulHidden = 1.5f;	// 非露出中の倍率（弾が通りにくい等）
		float defaultExposeTime = 1.2f; // 露出タイムの基本秒
	} core;

	/// ---------- テレグラフパラメータ ---------- ///
	struct Telegraph
	{
		float time = 0.8f;             // テレグラフ表示時間
		std::string decal = "none";    // テレグラフ用デカール画像ファイル名 : "fan" / "circle" / "ring" など
		float decalScale = 1.0f;       // デカールのスケール倍率
		std::string sfx = "none";      // テレグラフ用サウンドエフェクトファイル名 : 任意
	};

	/// ---------- ヒットパラメータ ---------- ///
	struct Hit
	{
		float damage = 15.0f;      // ヒット時ダメージ量
		float knockbackH = 0.35f;  // ヒット時ノックバックの水平強さ
		float knockbackUp = 0.12f; // ヒット時ノックバックの縦方向
		float hitStop = 0.0f;      // ヒットストップ時間
	};

	/// ---------- 各攻撃パラメータ ---------- ///

	/// ---------- 攻撃1 : ツタ薙ぎ払い（扇形） ---------- ///
	struct VineSweep
	{
		// タイミング
		float windup = 0.45f;	  // 溜め時間
		float windupHold = 0.10f; // 構え完了後に止める時間（溜め）
		float active = 0.20f;     // 攻撃有効時間
		float recovery = 0.65f;   // 回復時間
		float cooldown = 1.0f;    // クールタイム

		// 形状（扇形）
		float radius = 6.0f;     // 攻撃判定半径
		float angleDeg = 120.0f; // 扇形の角度
		float thickness = 1.0f;  // 扇形の厚み

		// ツタ感を出す増幅（演出用）
		float vineCount = 3.0f;      // ツタの本数
		float waveAmplitude = 0.5f; // ツタの波打ち振幅
		float waveSpeed = 8.0f;  // ツタの波打ち速度

		// テレグラフ
		Telegraph telegraph = { 0.55f, "fan", 1.0f, "vine_windup" };

		// ヒット情報
		Hit hit = { 18.0f, 0.45f, 0.12f, 0.0f };

		// この攻撃後の後にコア露出する
		float exposeCoreAfter = 1.0f;

		// デバッグ用 : どの攻撃か識別する文字列
		std::string debugName = "VineSweep";

	} vineSweep;

	/// ---------- 攻撃2 : 種子迫撃（弾道投擲 / 着弾AoE） ---------- ///
	struct SeedMortar
	{
		// タイミング
		float windup = 0.60f; // 溜め時間（地面を殴る前のタメ）
		float riseTime = 0.75f; // 地面の下から飛び出して爆発するまでの時間
		float recovery = 0.55f; // 爆発後の回復時間
		float cooldown = 3.80f; // クールタイム

		// 配置パターン
		int   count = 3;		   // 一度に出現させる種の数
		float spreadRadius = 3.5f; // プレイヤー周辺でばらけさせる半径
		float impactRadius = 3.2f; // 爆発AoE半径

		// 地面からの出方
		float emergeDepth = 0.8f; // 最初に地面の「どれだけ下」からスタートするか
		float emergeHeight = 2.0f; // 地面からどれだけ上まで飛び出すか

		float avoidBossRadius = 2.5f; // ボス中心からこの半径内には種を出さない

		float minSeedSpacing = 1.5f;   // 種と種の距離がこの値未満にならないように配置

		// テレグラフ（予告円など）
		Telegraph telegraph{ 0.70f, "circle", 1.0f, "seed_mark" };

		// ヒット情報（爆発ダメージ）
		Hit hit{ 14.0f, 0.25f, 0.10f, 0.0f };

		// この攻撃のあとにコアをどれだけ露出させるか
		float exposeCoreAfter = 0.8f;

		// デバッグ用 : どの攻撃か識別する文字列
		std::string debugName = "SeedMortar";
	} seedMortar;

	/// ---------- 攻撃3 : 根っこ突き上げ（直線AoE） ---------- ///
	struct RootCage
	{
		float windup = 0.35f;	// 溜め時間
		float growTime = 0.35f; // 檻がせり上がる時間
		float duration = 1.80f; // 檻の維持時間
		float cooldown = 6.0f;	// クールタイム

		float radius = 2.1f;		 // 檻の半径
		float ringThickness = 0.45f; // 檻のリングの厚み

		// 脱出条件（どっちか選べるように）
		int   escapeDodgeCount = 2;		   // 回避行動回数
		float escapeDamageToRoots = 55.0f; // 檻への累積ダメージ量

		Telegraph telegraph{ 0.80f, "ring", 1.0f, "root_cage" };
		Hit hit{ 0.0f, 0.0f, 0.0f, 0.0f }; // 檻自体の接触ダメージは無し推奨

		float exposeCoreAfter = 1.3f; // 拘束を避け/解除できたら反撃チャンス

		// デバッグ用 : どの攻撃か識別する文字列
		std::string debugName = "RootCage";
	} rootCage;

	/// ---------- フェーズ効果 : 花粉霧 ---------- ///
	struct PollenFog
	{
		float duration = 8.0f;	// 効果時間
		float cooldown = 14.0f; // クールタイム
		float density = 0.70f;  // 霧の濃さ（視界阻害量）

		// 命中低下より “視界/エイム阻害” 寄りにすると納得感が出やすい
		float aimAssistScale = 0.80f; // 例：エイム補助があるなら倍率で落とす等
		float exposeCoreBonus = 0.25f; // 霧中は露出時間を少し増やす等の救済

		// デバッグ用 : どの効果か識別する文字列
		std::string debugName = "PollenFog";
	} pollenFog;

	/// ---------- フェーズ効果 : 床ギミック 根スパイク（HP35%〜） ---------- ///
	struct RootSpikes
	{
		float interval = 1.20f; // 発生間隔
		int   countPerWave = 6; // 1波あたりの本数

		float telegraphTime = 0.70f; // 発生前テレグラフ時間
		float activeTime = 0.20f;	 // スパイクの有効時間

		float minRadiusFromPlayer = 2.5f; // プレイヤーからこれ以上離れた位置に出現
		float maxRadiusFromPlayer = 7.0f; // プレイヤーからこれ以上離れない位置に出現

		Hit hit{ 16.0f, 0.35f, 0.18f, 0.0f };

		// デバッグ用 : どの効果か識別する文字列
		std::string debugName = "RootSpikes";
	} rootSpikes;

	/// ---------- フェーズ定義 ---------- ///
	struct Phase
	{
		float hpBelow = 1.0f; // この値未満で適用（例: 0.70 / 0.35）
		int vineCountBonus = 0;
		bool enablePollen = false;
		bool enableRootSpikes = false;

		// 攻撃の選択テーブル（重み + 条件）
		struct WeightedAttack
		{
			enum class Id { VineSweep, SeedMortar, RootCage } id;
			float weight = 1.0f;
			float minDist = 0.0f;
			float maxDist = 9999.0f;
		};
		std::vector<WeightedAttack> table;
	};

	// 70% / 35% の想定
	std::vector<Phase> phases = {
		// Phase0: HP>70%
		Phase{
			1.00f, 0, false, false,
			{
				{ Phase::WeightedAttack::Id::VineSweep,  1.2f, 0.0f, 7.0f },
				{ Phase::WeightedAttack::Id::SeedMortar, 0.9f, 4.0f, 9999.0f },
				{ Phase::WeightedAttack::Id::RootCage,   0.6f, 0.0f, 9999.0f },
			}
		},
		// Phase1: HP<=70%
		Phase{
			0.70f, 1, true, false,
			{
				{ Phase::WeightedAttack::Id::VineSweep,  1.3f, 0.0f, 8.0f },
				{ Phase::WeightedAttack::Id::SeedMortar, 1.0f, 4.0f, 9999.0f },
				{ Phase::WeightedAttack::Id::RootCage,   0.8f, 0.0f, 9999.0f },
			}
		},
		// Phase2: HP<=35%
		Phase{
			0.35f, 2, true, true,
			{
				{ Phase::WeightedAttack::Id::VineSweep,  1.35f, 0.0f, 9.0f },
				{ Phase::WeightedAttack::Id::SeedMortar, 1.10f, 4.0f, 9999.0f },
				{ Phase::WeightedAttack::Id::RootCage,   0.90f, 0.0f, 9999.0f },
			}
		},
	};
};