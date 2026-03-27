#include "EnemyTuningRepository.h"

#include <cassert>
#include <fstream>
#include <filesystem>
#include <json.hpp>

std::unordered_map<EnemyArchetype, EnemyTuning> EnemyTuningRepository::tunings_{};
bool EnemyTuningRepository::isInitialized_ = false;

using json = nlohmann::json;

namespace
{
	/// --------------------------------------------------------
	/// 全 enemy archetype 一覧
	/// --------------------------------------------------------
	/// 敵を追加したらここへ 1 行追加する。
	/// LoadAllFromJson() 側で手書き列挙を減らし、管理を 1 か所に寄せる。
	const EnemyArchetype kAllEnemyArchetypes[] =
	{
		EnemyArchetype::RifleGrunt,
		EnemyArchetype::SMGFlanker,
		EnemyArchetype::Sniper,
		EnemyArchetype::BurstTrooper,
		EnemyArchetype::HeavyRifleman,
		EnemyArchetype::ShotgunRusher,
		EnemyArchetype::Scout,
		EnemyArchetype::Marksman,
		EnemyArchetype::Suppressor,
		EnemyArchetype::EliteFlanker,
		EnemyArchetype::HeavySniper,
	};

	/// --------------------------------------------------------
	/// JSON保存先フォルダ
	/// --------------------------------------------------------
	const std::string kEnemyJsonDirectory = "Resources/JSON/Enemies/";

	/// --------------------------------------------------------
	/// JSONの値があれば float へ読む
	/// --------------------------------------------------------
	void ReadFloatIfExists(const json& root, const char* key, float& value)
	{
		if (root.contains(key) && root[key].is_number())
		{
			value = root[key].get<float>();
		}
	}

	/// --------------------------------------------------------
	/// JSONの値があれば int へ読む
	/// --------------------------------------------------------
	void ReadIntIfExists(const json& root, const char* key, int& value)
	{
		if (root.contains(key) && root[key].is_number_integer())
		{
			value = root[key].get<int>();
		}
	}
}

/// ------------------------------------------------------------
/// 初期化
/// ------------------------------------------------------------
/// 方針:
/// - まず安全な既定値を作る
/// - 次に各敵の JSON があれば上書きする
/// - JSON が無くても既定値で動作継続できるようにする
void EnemyTuningRepository::Initialize()
{
	if (isInitialized_)
	{
		return;
	}

	BuildDefaults();
	LoadAllFromJson();

	isInitialized_ = true;
}

/// ------------------------------------------------------------
/// 終了処理
/// ------------------------------------------------------------
void EnemyTuningRepository::Finalize()
{
	tunings_.clear();
	isInitialized_ = false;
}

/// ------------------------------------------------------------
/// tuning 取得
/// ------------------------------------------------------------
const EnemyTuning& EnemyTuningRepository::Get(EnemyArchetype archetype)
{
	if (!isInitialized_)
	{
		Initialize();
	}

	auto it = tunings_.find(archetype);
	if (it != tunings_.end())
	{
		return it->second;
	}

	// 万一の保険:
	// 定義漏れがあった場合は RifleGrunt を返して落ちにくくする
	return tunings_.at(EnemyArchetype::RifleGrunt);
}

EnemyTuning& EnemyTuningRepository::GetMutable(EnemyArchetype archetype)
{
	if (!isInitialized_)
	{
		Initialize();
	}
	return tunings_[archetype];
}

/// ------------------------------------------------------------
/// 再読込
/// ------------------------------------------------------------
/// 既定値を再構築してから JSON を再読込する。
/// あとで ImGui の「Reload Enemy Tunings」ボタンから呼びやすい。
void EnemyTuningRepository::Reload()
{
	BuildDefaults();
	LoadAllFromJson();
	isInitialized_ = true;
}

/// ------------------------------------------------------------
/// 敵1体分の JSON を読み込む
/// ------------------------------------------------------------
/// 想定フォーマット:
/// {
///   "moveSpeed": 3.0,
///   "attackRange": 44.0,
///   ...
/// }
///
/// 重要:
/// - JSON に書かれている項目だけを既定値へ上書きする
/// - JSON が壊れていても既定値が残る
bool EnemyTuningRepository::LoadOneFromJson(EnemyArchetype archetype, const std::string& filePath)
{
	std::ifstream ifs(filePath);
	if (!ifs.is_open())
	{
		return false;
	}

	json root;
	try
	{
		ifs >> root;
	}
	catch (...)
	{
		return false;
	}

	if (!root.is_object())
	{
		return false;
	}

	// --------------------------------------------------------
	// 既定値をベースにして、JSONにある項目だけ上書きする
	// ※ Get() を使うと初期化再入の原因になるので直接参照
	// --------------------------------------------------------
	auto it = tunings_.find(archetype);
	if (it == tunings_.end())
	{
		return false;
	}

	EnemyTuning tuning = it->second;

	ReadFloatIfExists(root, "moveSpeed", tuning.moveSpeed);
	ReadFloatIfExists(root, "attackRange", tuning.attackRange);
	ReadFloatIfExists(root, "viewRange", tuning.viewRange);

	ReadFloatIfExists(root, "fireInterval", tuning.fireInterval);
	ReadIntIfExists(root, "burstMin", tuning.burstMin);
	ReadIntIfExists(root, "burstMax", tuning.burstMax);

	ReadFloatIfExists(root, "preferredMinRatio", tuning.preferredMinRatio);
	ReadFloatIfExists(root, "preferredMaxRatio", tuning.preferredMaxRatio);
	ReadFloatIfExists(root, "strafeSpeedMul", tuning.strafeSpeedMul);
	ReadFloatIfExists(root, "aimMoveMul", tuning.aimMoveMul);
	ReadFloatIfExists(root, "burstMoveMul", tuning.burstMoveMul);

	ReadFloatIfExists(root, "spreadNearDeg", tuning.spreadNearDeg);
	ReadFloatIfExists(root, "spreadFarDeg", tuning.spreadFarDeg);

	ReadFloatIfExists(root, "reactionDelaySec", tuning.reactionDelaySec);

	ReadFloatIfExists(root, "bulletSpeed", tuning.bulletSpeed);
	ReadFloatIfExists(root, "bulletLifeSec", tuning.bulletLifeSec);
	ReadIntIfExists(root, "bulletDamage", tuning.bulletDamage);

	ReadIntIfExists(root, "maxHp", tuning.maxHp);

	tunings_[archetype] = tuning;
	return true;
}

std::string EnemyTuningRepository::GetFilePath(EnemyArchetype archetype)
{
	// --------------------------------------------------------
	// 保存ファイル名も ToString と同じ名前で統一
	// 例: RifleGrunt -> Resources/JSON/Enemies/RifleGrunt.json
	// --------------------------------------------------------
	return kEnemyJsonDirectory + std::string(ToString(archetype)) + ".json";
}

bool EnemyTuningRepository::SaveOneToJson(EnemyArchetype archetype, const std::string& filePath)
{
	if (!isInitialized_)
	{
		Initialize();
	}

	const auto it = tunings_.find(archetype);
	if (it == tunings_.end())
	{
		return false;
	}

	const EnemyTuning& t = it->second;

	nlohmann::json root;
	root["moveSpeed"] = t.moveSpeed;
	root["attackRange"] = t.attackRange;
	root["viewRange"] = t.viewRange;

	root["fireInterval"] = t.fireInterval;
	root["burstMin"] = t.burstMin;
	root["burstMax"] = t.burstMax;

	root["preferredMinRatio"] = t.preferredMinRatio;
	root["preferredMaxRatio"] = t.preferredMaxRatio;
	root["strafeSpeedMul"] = t.strafeSpeedMul;
	root["aimMoveMul"] = t.aimMoveMul;
	root["burstMoveMul"] = t.burstMoveMul;

	root["spreadNearDeg"] = t.spreadNearDeg;
	root["spreadFarDeg"] = t.spreadFarDeg;

	root["reactionDelaySec"] = t.reactionDelaySec;

	root["bulletSpeed"] = t.bulletSpeed;
	root["bulletLifeSec"] = t.bulletLifeSec;
	root["bulletDamage"] = t.bulletDamage;

	root["maxHp"] = t.maxHp;

	std::ofstream ofs(filePath);
	if (!ofs.is_open())
	{
		return false;
	}

	ofs << root.dump(4);
	return true;
}

bool EnemyTuningRepository::SaveOne(EnemyArchetype archetype)
{
	if (!isInitialized_)
	{
		Initialize();
	}

	std::filesystem::create_directories(kEnemyJsonDirectory);

	const EnemyTuning& tuning = tunings_.at(archetype);

	json root;
	root["moveSpeed"] = tuning.moveSpeed;
	root["attackRange"] = tuning.attackRange;
	root["viewRange"] = tuning.viewRange;

	root["fireInterval"] = tuning.fireInterval;
	root["burstMin"] = tuning.burstMin;
	root["burstMax"] = tuning.burstMax;

	root["preferredMinRatio"] = tuning.preferredMinRatio;
	root["preferredMaxRatio"] = tuning.preferredMaxRatio;
	root["strafeSpeedMul"] = tuning.strafeSpeedMul;
	root["aimMoveMul"] = tuning.aimMoveMul;
	root["burstMoveMul"] = tuning.burstMoveMul;

	root["spreadNearDeg"] = tuning.spreadNearDeg;
	root["spreadFarDeg"] = tuning.spreadFarDeg;

	root["reactionDelaySec"] = tuning.reactionDelaySec;

	root["bulletSpeed"] = tuning.bulletSpeed;
	root["bulletLifeSec"] = tuning.bulletLifeSec;
	root["bulletDamage"] = tuning.bulletDamage;

	root["maxHp"] = tuning.maxHp;

	std::ofstream ofs(GetFilePath(archetype));
	if (!ofs.is_open())
	{
		return false;
	}

	ofs << root.dump(4);
	return true;
}

bool EnemyTuningRepository::DeleteOneJson(const std::string& filePath)
{
	std::error_code ec;
	const bool removed = std::filesystem::remove(filePath, ec);

	// remove は「存在しなかった」場合 false を返す
	// エラーが出た場合も失敗扱いにする
	if (ec)
	{
		return false;
	}

	return removed;
}

bool EnemyTuningRepository::DeleteOne(EnemyArchetype archetype)
{
	return DeleteOneJson(GetFilePath(archetype));
}

bool EnemyTuningRepository::ExistsJson(EnemyArchetype archetype)
{
	return std::filesystem::exists(GetDefaultJsonPath(archetype));
}

/// ------------------------------------------------------------
/// 全敵分の JSON を読み込む
/// ------------------------------------------------------------
/// 配置規約:
/// Resources/JSON/Enemies/<ArchetypeName>.json
///
/// 例:
/// Resources/JSON/Enemies/RifleGrunt.json
/// Resources/JSON/Enemies/ShotgunRusher.json
void EnemyTuningRepository::LoadAllFromJson()
{
	std::filesystem::create_directories(kEnemyJsonDirectory);

	for (EnemyArchetype archetype : GetAllArchetypes())
	{
		LoadOneFromJson(archetype, GetFilePath(archetype));
	}
}

/// ------------------------------------------------------------
/// 既定値構築
/// ------------------------------------------------------------
/// ここに「今のゲームで使う標準 tuning」をまとめる。
/// JSON が無くてもこの値で必ず動くようにする。
void EnemyTuningRepository::BuildDefaults()
{
	tunings_.clear();

	// --------------------------------------------------------
	// ここでは初期値だけ入れる
	// 必要ならあとで各種個別値に調整
	// --------------------------------------------------------
	for (EnemyArchetype archetype : GetAllArchetypes())
	{
		tunings_[archetype] = EnemyTuning{};
	}

	tunings_[EnemyArchetype::RifleGrunt].moveSpeed = 3.0f;
	tunings_[EnemyArchetype::SMGFlanker].moveSpeed = 4.2f;
	tunings_[EnemyArchetype::Sniper].attackRange = 55.0f;
	tunings_[EnemyArchetype::HeavyRifleman].maxHp = 6;
	tunings_[EnemyArchetype::ShotgunRusher].attackRange = 14.0f;
	tunings_[EnemyArchetype::Scout].moveSpeed = 4.8f;
	tunings_[EnemyArchetype::Marksman].attackRange = 42.0f;
	tunings_[EnemyArchetype::Suppressor].fireInterval = 0.10f;
	tunings_[EnemyArchetype::EliteFlanker].moveSpeed = 4.5f;
	tunings_[EnemyArchetype::HeavySniper].bulletDamage = 3;
}

/// ------------------------------------------------------------
/// 文字列 → archetype
/// ------------------------------------------------------------
bool EnemyTuningRepository::TryParseArchetype(const std::string& name, EnemyArchetype& outArchetype)
{
	if (name == "RifleGrunt") { outArchetype = EnemyArchetype::RifleGrunt; return true; }
	if (name == "SMGFlanker") { outArchetype = EnemyArchetype::SMGFlanker; return true; }
	if (name == "Sniper") { outArchetype = EnemyArchetype::Sniper; return true; }
	if (name == "BurstTrooper") { outArchetype = EnemyArchetype::BurstTrooper; return true; }
	if (name == "HeavyRifleman") { outArchetype = EnemyArchetype::HeavyRifleman; return true; }
	if (name == "ShotgunRusher") { outArchetype = EnemyArchetype::ShotgunRusher; return true; }
	if (name == "Scout") { outArchetype = EnemyArchetype::Scout; return true; }
	if (name == "Marksman") { outArchetype = EnemyArchetype::Marksman; return true; }
	if (name == "Suppressor") { outArchetype = EnemyArchetype::Suppressor; return true; }
	if (name == "EliteFlanker") { outArchetype = EnemyArchetype::EliteFlanker; return true; }
	if (name == "HeavySniper") { outArchetype = EnemyArchetype::HeavySniper; return true; }

	return false;
}

/// ------------------------------------------------------------
/// archetype → 文字列
/// ------------------------------------------------------------
const char* EnemyTuningRepository::ToString(EnemyArchetype archetype)
{
	// --------------------------------------------------------
	// enum名と完全一致する文字列を返す
	// - Blenderの archetype
	// - JSONファイル名
	// - ImGui表示名
	// すべてこれを基準にそろえる
	// --------------------------------------------------------
	switch (archetype)
	{
	case EnemyArchetype::RifleGrunt:    return "RifleGrunt";
	case EnemyArchetype::SMGFlanker:    return "SMGFlanker";
	case EnemyArchetype::Sniper:        return "Sniper";
	case EnemyArchetype::BurstTrooper:  return "BurstTrooper";
	case EnemyArchetype::HeavyRifleman: return "HeavyRifleman";
	case EnemyArchetype::ShotgunRusher: return "ShotgunRusher";
	case EnemyArchetype::Scout:         return "Scout";
	case EnemyArchetype::Marksman:      return "Marksman";
	case EnemyArchetype::Suppressor:    return "Suppressor";
	case EnemyArchetype::EliteFlanker:  return "EliteFlanker";
	case EnemyArchetype::HeavySniper:   return "HeavySniper";
	default:                            return "RifleGrunt";
	}
}

std::vector<EnemyArchetype> EnemyTuningRepository::GetAllArchetypes()
{
	// --------------------------------------------------------
	// 全敵種類の一覧
	// ImGuiのプルダウンや一括ロード時に使える
	// --------------------------------------------------------
	static const std::vector<EnemyArchetype> kAll =
	{
		EnemyArchetype::RifleGrunt,
		EnemyArchetype::SMGFlanker,
		EnemyArchetype::Sniper,
		EnemyArchetype::BurstTrooper,
		EnemyArchetype::HeavyRifleman,
		EnemyArchetype::ShotgunRusher,
		EnemyArchetype::Scout,
		EnemyArchetype::Marksman,
		EnemyArchetype::Suppressor,
		EnemyArchetype::EliteFlanker,
		EnemyArchetype::HeavySniper,
	};
	return kAll;
}

std::string EnemyTuningRepository::GetDefaultJsonPath(EnemyArchetype archetype)
{
	return std::string("Resources/JSON/Enemies/") + ToString(archetype) + ".json";
}
