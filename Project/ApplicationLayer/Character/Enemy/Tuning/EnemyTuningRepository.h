#pragma once
#include <unordered_map>
#include <string>
#include <vector>
#include "EnemyArchetype.h"

/// ------------------------------------------------------------
/// EnemyTuningRepository
/// ------------------------------------------------------------
/// 敵アーキタイプごとの調整値(EnemyTuning)を管理するクラス。
///
/// 役割:
/// - EnemyArchetype から対応する EnemyTuning を取得する
/// - C++ 側の既定値テーブルを持つ
/// - JSON があれば既定値へ上書きする
///
/// 方針:
/// - Enemy 本体は「どこから値が来たか」を知らない
/// - Enemy は Repository から tuning を受け取って使うだけ
/// - JSON 保存先や読み込み方式を変えても Enemy.cpp 側の修正を減らせる
/// ------------------------------------------------------------
class EnemyTuningRepository
{
public:
	/// 初期化
	/// - 既定値を構築
	/// - JSON を読み込んで上書き
	static void Initialize();

	/// 終了処理
	static void Finalize();

	/// 指定 archetype の tuning を取得
	/// - 存在しない場合は例外になるので、基本は BuildDefaults 前提
	static const EnemyTuning& Get(EnemyArchetype archetype);

	/// 編集用の可変取得
	/// - ImGui 上で一時的に編集する値の反映先として使う
	static EnemyTuning& GetMutable(EnemyArchetype archetype);

	/// 全体再読込
	/// - 既定値を作り直してから JSON を再適用
	static void Reload();

	/// 1体分の JSON 読み込み
	/// - JSON に書いてある項目だけ既定値へ上書きする
	static bool LoadOneFromJson(EnemyArchetype archetype, const std::string& filePath);

	/// 1体分の JSON 保存
	static bool SaveOneToJson(EnemyArchetype archetype, const std::string& filePath);

	/// 既定パスへ保存
	static bool SaveOne(EnemyArchetype archetype);

	/// 指定ファイルを削除
	static bool DeleteOneJson(const std::string& filePath);

	/// 既定パスの JSON を削除
	static bool DeleteOne(EnemyArchetype archetype);

	/// 既定パスの JSON が存在するか
	static bool ExistsJson(EnemyArchetype archetype);

	/// archetype 文字列変換/逆変換
	static bool TryParseArchetype(const std::string& name, EnemyArchetype& outArchetype);
	static const char* ToString(EnemyArchetype archetype);

	/// 全 archetype 一覧
	static std::vector<EnemyArchetype> GetAllArchetypes();

	/// 既定 JSON パス取得
	static std::string GetDefaultJsonPath(EnemyArchetype archetype);

private:
	/// C++ 側の既定値を構築
	static void BuildDefaults();

	/// 全 JSON を読み込む
	static void LoadAllFromJson();

	/// 既定保存先ファイルパスを作る
	static std::string GetFilePath(EnemyArchetype archetype);

private:
	/// archetype ごとの tuning テーブル
	static std::unordered_map<EnemyArchetype, EnemyTuning> tunings_;

	/// 初期化済みフラグ
	static bool isInitialized_;

private:
	EnemyTuningRepository() = delete;
};