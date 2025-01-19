#pragma once
#include <map>
#include <string>
#include <variant>
#include <json.hpp>

#include "Vector3.h"

// コードの冗長を防ぐため省略
using json = nlohmann::json;
using Item = std::variant<int32_t, float, Vector3, bool>;
using Group = std::map<std::string, Item>;


/// -------------------------------------------------------------
///			　パラメータや調整項目を管理するクラス
/// -------------------------------------------------------------
class ParameterManager
{
public: /// ---------- 構造体 ---------- ///

	// 項目構造体
	struct Item
	{
		// 項目の値
		std::variant<int32_t, float, Vector3, bool> value;
	};

	// グループ構造体
	struct Group
	{
		std::map<std::string, Item> items;
	};

public: /// ---------- メンバ関数 ---------- ///

	// シングルトンインスタンス
	static ParameterManager* GetInstance();

	/// <summary>
	/// グループ作成
	/// </summary>
	/// <param name="groupName">グループ名</param>
	void CreateGroup(const std::string& groupName);

	/// <summary>
	/// 更新処理
	/// </summary>
	void Update();

	/// <summary>
	/// ファイルに書き出し
	/// </summary>
	/// <param name="groupName">グループ名</param>
	void SaveFile(const std::string& groupName);

	/// <summary>
	/// ディレクトリの全ファイル読み込み
	/// </summary>
	void LoadFiles();

	/// <summary>
	/// ファイルから読み込む
	/// </summary>
	/// <param name="groupName">グループ名</param>
	void LoadFile(const std::string& groupName);

public: /// ---------- 項目の設定 ---------- ///

	// 値のセット（int）
	void SetValue(const std::string& groupName, const std::string& key, int32_t value);

	// 値のセット（float）
	void SetValue(const std::string& groupName, const std::string& key, float value);

	// 値のセット（Vector3）	
	void SetValue(const std::string& groupName, const std::string& key, Vector3& value);

	// 値のセット（bool）
	void SetValue(const std::string& groupName, const std::string& key, bool value);

public: /// ---------- 項目の追加 ---------- ///

	// 項目の追加（int）
	void AddItem(const std::string& groupName, const std::string& key, int32_t value);

	// 項目の追加（float）
	void AddItem(const std::string& groupName, const std::string& key, float value);

	// 項目の追加（Vector3）
	void AddItem(const std::string& groupName, const std::string& key, Vector3& value);

	// 項目の追加（bool）
	void AddItem(const std::string& groupName, const std::string& key, bool value);

public: /// ---------- 項目の取得 ---------- ///

	// 値の取得（int）
	int32_t GetIntValue(const std::string& groupName, const std::string& key) const;

	// 値の取得（float）
	float GetFloatValue(const std::string& groupName, const std::string& key) const;

	// 値の取得（vector3）
	Vector3 GetVector3Value(const std::string& groupName, const std::string& key) const;

	// 値の取得（bool）
	bool GetBoolValue(const std::string& groupName, const std::string& key) const;

private: /// ---------- メンバ変数 ---------- ///

	// 全データ
	std::map<std::string, Group> datas_;

	// グローバル変数の保存先ファイルパス
	const std::string kDirectoryPath = "Resources/ParameterManager/";

private: /// ---------- コピー禁止 ---------- ///

	ParameterManager() = default;
	~ParameterManager() = default;
	ParameterManager(const ParameterManager&) = delete;
	ParameterManager& operator=(const ParameterManager&) = delete;
};
