#pragma once
#include <map>
#include <string>
#include <variant>
#include <json.hpp>
#include <limits>

#include "Vector3.h"
#include "Vector4.h"

// コードの冗長を防ぐため省略
using json = nlohmann::json;


/// -------------------------------------------------------------
///			　パラメータや調整項目を管理するクラス
/// -------------------------------------------------------------
class ParameterManager
{
private: /// ---------- 構造体 ---------- ///

	// 項目構造体
	struct Item
	{
		// 項目の値
		std::variant<int32_t, uint32_t, float, Vector3, Vector4, bool> value;
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

	/// <summary>
	/// 項目の値を設定するテンプレート関数
	/// </summary>
	/// <typeparam name="T">設定する値の型（int32_t, float, Vector3, bool のいずれか）</typeparam>
	/// <param name="groupName">設定対象のグループ名</param>
	/// <param name="key">設定対象の項目名</param>
	/// <param name="value">設定する値</param>
	template<typename T>
	void SetValue(const std::string& groupName, const std::string& key, const T& value)
	{
		static_assert(std::is_same_v<T, int32_t> || std::is_same_v<T, uint32_t> || std::is_same_v<T, float> ||
			std::is_same_v<T, Vector3> || std::is_same_v<T, Vector4> || std::is_same_v<T, bool>,
			"Unsupported type for SetValue"); // サポートされていない型の場合はコンパイルエラー

		// グループを取得し、新しい項目を作成して設定する
		Group& group = datas_[groupName];
		Item newItem{};
		newItem.value = value;
		group.items[key] = newItem;
	}

public: /// ---------- 項目の追加 ---------- ///

	/// <summary>
	/// 項目を追加するテンプレート関数
	/// </summary>
	/// <typeparam name="T">追加する値の型（int32_t, float, Vector3, bool のいずれか）</typeparam>
	/// <param name="groupName">追加対象のグループ名</param>
	/// <param name="key">追加する項目の名前</param>
	/// <param name="value">追加する項目の初期値</param>
	template<typename T>
	void AddItem(const std::string& groupName, const std::string& key, const T& value)
	{
		// キーが存在しない場合のみ項目を追加
		if (datas_[groupName].items.find(key) == datas_[groupName].items.end())
		{
			SetValue(groupName, key, value);
		}
	}

public: /// ---------- 項目の取得 ---------- ///

	/// <summary>
	/// 項目の値を取得するテンプレート関数
	/// </summary>
	/// <typeparam name="T">取得する値の型（int32_t, float, Vector3, bool のいずれか）</typeparam>
	/// <param name="groupName">取得対象のグループ名</param>
	/// <param name="key">取得対象の項目名</param>
	/// <returns>指定された型の値を返す</returns>
	/// <exception cref="std::runtime_error">
	/// グループやキーが見つからない場合、または型が一致しない場合にスローされる
	template<typename T>
	T GetValue(const std::string& groupName, const std::string& key) const
	{
		static_assert(std::is_same_v<T, int32_t> || std::is_same_v<T, uint32_t> || std::is_same_v<T, float> ||
			std::is_same_v<T, Vector3> || std::is_same_v<T, Vector4> || std::is_same_v<T, bool>,
			"Unsupported type for GetValue"); // サポートされていない型の場合はコンパイルエラー

		// グループが存在しない場合のエラーハンドリング
		auto groupIt = datas_.find(groupName);
		if (groupIt == datas_.end())
		{
			throw std::runtime_error("Group not found: " + groupName);
		}

		// キーが存在しない場合のエラーハンドリング
		const auto& group = groupIt->second;
		auto itemIt = group.items.find(key);
		if (itemIt == group.items.end())
		{
			throw std::runtime_error("Key not found: " + key);
		}

		// 型の一致を確認して値を取得
		const auto& item = itemIt->second;
		if (auto value = std::get_if<T>(&item.value))
		{
			return *value; // 値を返す
		}

		throw std::runtime_error("Type mismatch for key: " + key); // 型が一致しない場合
	}

private: /// ---------- メンバ関数 ---------- ///

	void DrawItem(const std::string& itemName, ParameterManager::Item& item);

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
