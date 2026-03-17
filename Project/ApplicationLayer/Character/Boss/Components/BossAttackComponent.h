#pragma once
#include "Attacks/IBossAttack.h"

#include <memory>
#include <string>
#include <vector>

/// ---------- 前方宣言 ---------- ///
class BossBase;

/// -------------------------------------------------------------
/// ボス攻撃コンポーネント
///
/// 役割:
/// - 攻撃一覧を保持する
/// - 各攻撃の Initialize をまとめて呼ぶ
/// - クールダウンを更新する
/// - 現在実行中の攻撃を1つ管理する
/// - 攻撃開始 / 更新 / 終了を一元管理する
///
/// 方針:
/// - BossBase は「どの攻撃を持つか」だけ決める
/// - 実際の攻撃管理はこのクラスに寄せる
/// - 量産時に BossBase が肥大化しないようにする
/// -------------------------------------------------------------
class BossAttackComponent
{
public: /// ---------- 初期化 / 終了 ---------- ///

	/// <summary>
	/// 所有者ボスを受け取って初期化する
	/// 登録済み攻撃すべてに owner を流す
	/// </summary>
	void Initialize(BossBase* owner);

	/// <summary>
	/// 後始末
	/// 攻撃一覧を破棄する
	/// </summary>
	void Finalize();

public: /// ---------- 攻撃登録 ---------- ///

	/// <summary>
	/// 攻撃を登録する
	/// BossBase::SetupAttacks() などから呼ぶ想定
	/// </summary>
	void RegisterAttack(std::unique_ptr<IBossAttack> attack);

public: /// ---------- 更新 ---------- ///

	/// <summary>
	/// 毎フレーム更新
	/// - 実行中でない攻撃はクールダウンを進める
	/// - 実行中攻撃があれば Update する
	/// - 終了済みなら End して currentAttack_ を外す
	/// </summary>
	void Update(float deltaTime);

public: /// ---------- 攻撃開始 / 終了 ---------- ///

	/// <summary>
	/// 名前で攻撃開始
	/// 同名攻撃が見つかり、かつ開始可能なら true
	/// </summary>
	bool StartAttackByName(const std::string& attackName);

	/// <summary>
	/// インデックスで攻撃開始
	/// 有効範囲外や開始不可なら false
	/// </summary>
	bool StartAttackByIndex(size_t index);

	/// <summary>
	/// 現在攻撃中のものを強制終了する
	/// フェーズ移行やスタンなどで使える
	/// </summary>
	void ForceEndCurrentAttack();

public: /// ---------- 参照 ---------- ///

	/// <summary>
	/// 何かしら攻撃中か
	/// </summary>
	bool IsAttacking() const { return currentAttack_ != nullptr; }

	/// <summary>
	/// 現在の攻撃
	/// 無ければ nullptr
	/// </summary>
	IBossAttack* GetCurrentAttack() const { return currentAttack_; }

	/// <summary>
	/// 登録されている攻撃数
	/// </summary>
	size_t GetAttackCount() const { return attacks_.size(); }

	/// <summary>
	/// インデックスで攻撃取得
	/// 範囲外なら nullptr
	/// </summary>
	IBossAttack* GetAttack(size_t index) const;

	/// <summary>
	/// 名前で攻撃取得
	/// 見つからなければ nullptr
	/// </summary>
	IBossAttack* FindAttackByName(const std::string& attackName) const;

	/// <summary>
	/// 今開始可能な攻撃一覧を返す
	/// Brain 側の重み付き選択などに使える
	/// </summary>
	std::vector<IBossAttack*> CollectStartableAttacks() const;

public: /// ---------- 描画 ---------- ///

	/// <summary>
	/// 攻撃固有描画
	/// 基本は全攻撃に対して呼んでよい
	/// Active でない攻撃側は何も描かなくてよい
	/// </summary>
	void Draw();

	/// <summary>
	/// 攻撃固有シャドウ描画
	/// </summary>
	void DrawShadow();

	/// <summary>
	/// デバッグUI描画
	/// </summary>
	void DrawImGui();

private: /// ---------- 内部補助 ---------- ///

	/// <summary>
	/// 攻撃開始の共通処理
	/// </summary>
	bool StartAttackInternal(IBossAttack* attack);

private: /// ---------- メンバ変数 ---------- ///

	// 所有者
	BossBase* owner_ = nullptr;

	// 所有する攻撃一覧
	std::vector<std::unique_ptr<IBossAttack>> attacks_;

	// 現在実行中の攻撃
	IBossAttack* currentAttack_ = nullptr;
};