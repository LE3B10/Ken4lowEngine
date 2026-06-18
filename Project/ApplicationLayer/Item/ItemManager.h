#pragma once
#include <memory>
#include <random>
#include <string>
#include <vector>
#include "Item.h"
#include "ItemVisualEffect.h"

namespace K4E = ::Ken4lowEngine;

/// ---------- 前方宣言 --------- ///
class Player;
class CollisionManager;

/// -------------------------------------------------------------
/// アイテム生成・更新・取得効果を管理するクラス。
///
/// Heal/Ammo/Key などのItem実体を所有し、CollisionManager登録、
/// プレイヤー取得判定、取得済みItemの安全な削除までを担当する。
/// -------------------------------------------------------------
class ItemManager
{
private: /// ---------- 構造体 ---------- ///

	// アイテム効果適用のデバッグ情報を保持する構造体
	struct ItemEffectDebugInfo
	{
		ItemType itemType = ItemType::None;		   // アイテムの種類
		bool pickupDetected = false;			   // プレイヤーがアイテムを拾ったと判定されたか
		bool effectApplied = false;				   // アイテムの効果がプレイヤーに適用されたか
		bool noEffectBecauseFull = false;		   // プレイヤーの状態が満タンで、効果が適用されなかったか
		std::string failReason = "未取得";		   // 効果が適用されなかった理由の説明
		float hpBefore = 0.0f;					   // プレイヤーのHPが回復アイテム取得前にいくつだったか
		float hpAfter = 0.0f;					   // プレイヤーのHPが回復アイテム取得後にいくつになったか
		float maxHp = 0.0f;						   // プレイヤーの最大HP
		int reserveBefore = 0;					   // プレイヤーの予備弾薬が弾薬アイテム取得前にいくつだったか
		int reserveAfter = 0;					   // プレイヤーの予備弾薬が弾薬アイテム取得後にいくつになったか
		int maxReserve = 0;						   // プレイヤーの最大予備弾薬
		int magazineAmmo = 0;					   // プレイヤーのマガジン弾数
		int hudMagazineAmmo = 0;				   // HUD表示用のマガジン弾数（リロード中など実際の値と異なる可能性がある）
		int hudReserveAmmo = 0;					   // HUD表示用の予備弾薬量（リロード中など実際の値と異なる可能性がある）
		std::string currentWeaponName = "未取得";  // プレイヤーの現在の武器名
		bool currentWeaponAmmoRecoverable = false; // プレイヤーの現在の武器が弾薬回復可能かどうか
	};

public: /// ---------- メンバ関数 ---------- ///

	// アイテムの初期化、更新、描画、衝突判定登録、スポーン、取得判定、効果適用、削除などを担当する。
	void Initialize();

	// アイテムの描画処理
	void Update(Player* player);

	// アイテムの描画処理
	void Draw();

	// CollisionManagerにアイテムのコライダーを登録する
	void RegisterColliders(CollisionManager* collisionManager);

	// アイテムをスポーンさせる
	void Spawn(ItemType type, const K4E::Vector3& position);

	// アイテムスポーンの個別処理
	void SpawnHealSmall(const K4E::Vector3& position);

	// アイテムスポーンの個別処理（ammoAmountを上書きするオーバーロードも）
	void SpawnAmmoSmall(const K4E::Vector3& position);

	// ammoAmountを上書きするオーバーロード
	void SpawnAmmoSmall(const K4E::Vector3& position, int ammoAmount);

	// アイテムスポーンの個別処理
	bool TryGetFirstActiveItemPosition(ItemType type, K4E::Vector3& outPosition) const;

	// 敵の死亡位置にアイテムを落とす試行。ドロップ条件を満たす場合、アイテムをスポーンさせる。
	void TryDropFromEnemyDeath(const K4E::Vector3& deathPosition);

	// 敵の死亡位置にアイテムを落とす試行。ドロップ条件を満たす場合、アイテムをスポーンさせる。強制的に何かをドロップさせるバージョン。
	void TryDropEnemyItem(const K4E::Vector3& deathPosition);

	// 敵の死亡位置にアイテムを落とす試行。ドロップ条件を満たす場合、アイテムをスポーンさせる。ドロップさせるアイテムの種類を指定するバージョン。
	ItemType RollEnemyDrop();

	// プレイヤーがアイテムを拾ったかどうかを判定し、拾っていれば効果を適用する
	void CheckPickup(Player& player);

	// アイテムの効果をプレイヤーに適用する。効果が適用された場合はtrueを返す。
	bool ApplyItemEffect(Item& item, Player& player);

	// 取得済みまたは寿命切れのアイテムを安全に削除する
	void Clear();

	// 取得イベントを消費する。指定した種類のアイテムが取得されたイベントがあればtrueを返し、そのイベントは消費される。
	bool ConsumeCollected(ItemType type);

	// 現在アクティブなアイテムの数を返す。引数で種類を指定した場合は、その種類のアクティブなアイテムの数を返す。
	int GetActiveItemCount() const;

	// 現在アクティブな指定種類のアイテムの数を返す。
	int GetActiveItemCount(ItemType type) const;

	// アイテム管理のデバッグ情報をImGuiで表示する
	void DrawImGui();

public: /// ---------- アクセッサ ---------- ///

	// アイテムドロップの確率や回復量、取得半径などの設定を行う
	void SetConsumeItemWhenFull(bool enabled) { consumeItemWhenFull_ = enabled; }

	// 敵の死亡位置にアイテムを落とすかどうかの設定を行う
	void SetEnemyDeathDropEnabled(bool enabled) { enemyDeathDropEnabled_ = enabled; }

	// ヒールドロップ率を取得する
	float GetHealDropChance() const { return healDropChance_; }

	// アモドロップ率を取得する
	float GetAmmoDropChance() const { return ammoDropChance_; }

	// アイテムなしドロップ率を取得する
	float GetNoneDropChance() const;

	// 回復量を取得する
	int GetHealAmount() const { return healAmount_; }

	// 弾薬量を取得する
	int GetAmmoAmount() const { return ammoAmount_; }

	// アイテム取得の判定に使う半径を取得する
	float GetPickupRadius() const { return pickupRadius_; }

	// 敵の死亡位置にアイテムを落とす設定を取得する
	bool IsEnemyDeathDropEnabled() const { return enemyDeathDropEnabled_; }

	// 強制的に何かをドロップさせる設定を取得する
	bool IsForceEnemyDeathDropEnabled() const { return forceEnemyDeathDrop_; }

	// 最後にアイテムが拾われた種類を取得する
	ItemType GetLastPickedItemType() const { return lastPickedItemType_; }

	// 最後にアイテムが落ちた種類と位置を取得する
	ItemType GetLastDroppedItemType() const { return lastDroppedItemType_; }

	// 最後にアイテムが落ちた位置を取得する
	const K4E::Vector3& GetLastDropPosition() const { return lastDropPosition_; }

	// アイテム効果適用のデバッグ情報を取得する
	const ItemEffectDebugInfo& GetLastItemEffectDebugInfo() const { return lastItemEffectDebugInfo_; }

private: /// ---------- メンバ関数 ---------- ///

	// 取得済みまたは寿命切れのアイテムを安全に削除する
	void RemoveInactiveItems();

	// アイテムのスポーン処理の共通部分をまとめる関数
	void SpawnDropItem(ItemType type, const K4E::Vector3& position);

	// アイテムのスポーン処理の共通部分をまとめる関数。アイテムの効果量を上書きするオーバーロードも。
	void SpawnConfigured(ItemType type, const K4E::Vector3& position, int overrideAmmoAmount = -1);

	// 敵の死亡位置にアイテムを落とす試行。ドロップ条件を満たす場合、アイテムをスポーンさせる。ドロップさせるアイテムの種類を指定するバージョン。
	void LogDropRollResult(ItemType type, const K4E::Vector3& position) const;

	// プレイヤーがアイテムを拾ったかどうかを判定し、拾っていれば効果を適用する。デバッグ情報の記録も行う。
	void LogItemEffectResult() const;

	// アイテムの効果をプレイヤーに適用する。効果が適用された場合はtrueを返す。デバッグ情報の記録も行う。
	void ApplyVisualSettings(Item& item);

private: /// ---------- メンバ変数 ---------- ///

	// アイテムの実体を保持する
	std::vector<std::unique_ptr<Item>> items_;

	// プレイヤーがアイテムを拾ったイベントを保持する
	std::vector<ItemType> collectedEvents_;

	// アイテム効果適用のデバッグ情報を保持する構造体のインスタンス
	ItemEffectDebugInfo lastItemEffectDebugInfo_{};

	// CollisionManagerへの登録状態を管理するフラグ
	CollisionManager* registeredCollisionManager_ = nullptr;

	// アイテムのビジュアル表現を管理するクラスのインスタンス
	ItemVisualEffect itemVisualEffect_;

	// 最後にアイテムが拾われた種類を保持する変数
	ItemType lastPickedItemType_ = ItemType::None;

	// 最後にアイテムが落ちた種類を保持する変数
	ItemType lastDroppedItemType_ = ItemType::None;

	// 最後にアイテムが落ちた位置を保持する変数
	K4E::Vector3 lastDropPosition_ = {};

	float healDropChance_ = 0.25f; // 敵の死亡位置にアイテムを落とす確率（回復アイテム）
	float ammoDropChance_ = 0.35f; // 敵の死亡位置にアイテムを落とす確率（弾薬アイテム）
	int healAmount_ = 25;		   // 回復量
	int ammoAmount_ = 30;		   // 弾薬量

	float pickupRadius_ = 2.0f;				// アイテム取得の判定に使う半径
	int lastKnownMagazineAmmo_ = 0;			// プレイヤーのマガジン弾数を最後に確認した値を保持する変数
	int lastKnownReserveAmmo_ = 0;			// プレイヤーの予備弾薬量を最後に確認した値を保持する変数
	int lastKnownMaxReserveAmmo_ = 0;		// プレイヤーの最大予備弾薬量を最後に確認した値を保持する変数
	int lastAmmoSmallReserveRestored_ = 0;	// AmmoSmallアイテム取得時に回復した予備弾薬量を保持する変数
	bool consumeItemWhenFull_ = false;		// プレイヤーの状態が満タンでもアイテムを消費するかどうかの設定
	bool enemyDeathDropEnabled_ = true;		// 敵の死亡位置にアイテムを落とすかどうかの設定
	bool forceEnemyDeathDrop_ = false;		// 敵の死亡位置に必ず何かを落とすかどうかの設定


	// アイテムスポーンのランダムロールに使用する乱数生成器
	std::mt19937 rng_{ std::random_device{}() };
};
