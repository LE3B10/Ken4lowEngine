#include "ParameterManager.h"
#include <ImGuiManager.h>
#include <fstream>


/// -------------------------------------------------------------
///			　		シングルトンインスタンス
/// -------------------------------------------------------------
ParameterManager* ParameterManager::GetInstance()
{
	static ParameterManager instance;
	return &instance;
}


/// -------------------------------------------------------------
///			　			　グループの作成
/// -------------------------------------------------------------
void ParameterManager::CreateGroup(const std::string& groupName)
{
	// 指定名のオブジェクトがなければ追加する
	datas_[groupName];
}


/// -------------------------------------------------------------
///			　				更新処理
/// -------------------------------------------------------------
void ParameterManager::Update()
{
	// ImGuiウィンドウを開始
	if (!ImGui::Begin("Global Variables", nullptr, ImGuiWindowFlags_MenuBar))
	{
		ImGui::End();
		return;
	}

	// メニューバーの開始
	if (!ImGui::BeginMenuBar())
	{
		ImGui::EndMenuBar();
		return;
	}

	// 全グループをループ
	for (auto& [groupName, group] : datas_)
	{
		// グループ名を表示し、展開可能なメニューを作成
		if (!ImGui::BeginMenu(groupName.c_str()))
			continue;

		for (auto& [groupName, group] : datas_)
		{
			if (ImGui::CollapsingHeader(groupName.c_str()))
			{
				for (auto& [itemName, item] : group.items)
				{
					// 各アイテムの型に応じたUI描画
					DrawItem(itemName, item);
				}
			}
		}

		// 保存ボタンを作成
		if (ImGui::Button("Save"))
		{
			SaveFile(groupName); // グループを保存
			std::string message = std::format("{}.json saved.", groupName);
			MessageBoxA(nullptr, message.c_str(), "GlobalVariables", 0);
		}

		ImGui::EndMenu();
	}

	ImGui::EndMenuBar();
	ImGui::End();
}


/// -------------------------------------------------------------
///			　			ファイルを
/// -------------------------------------------------------------
void ParameterManager::SaveFile(const std::string& groupName)
{
	// グループ検索
	auto itGroup = datas_.find(groupName);

	// 未登録チェック
	assert(itGroup != datas_.end());

	// JSONオブジェクト作成
	json root = json::object();
	root[groupName] = json::object();

	// グループの全項目をループ
	for (const auto& [itemName, item] : itGroup->second.items)
	{
		/// ---------- int32_t型を保持している場合 ---------- ///
		if (std::holds_alternative<int32_t>(item.value))
		{
			root[groupName][itemName] = std::get<int32_t>(item.value);
		}

		/// ---------- float型を保持している場合 ---------- ///
		else if (std::holds_alternative<float>(item.value))
		{
			root[groupName][itemName] = std::get<float>(item.value);
		}

		/// ---------- Vector3を保持している場合 ---------- ///
		else if (std::holds_alternative<Vector3>(item.value))
		{
			const Vector3& vec = std::get<Vector3>(item.value);
			root[groupName][itemName] = json::array({ vec.x, vec.y, vec.z });
		}

		/// ---------- bool型を保持している場合 ---------- ///
		else if (std::holds_alternative<bool>(item.value))
		{
			root[groupName][itemName] = std::get<bool>(item.value);
		}
	}

	// ディレクトリの作成（存在しない場合）
	std::filesystem::path dir(kDirectoryPath);
	if (!std::filesystem::exists(kDirectoryPath))
	{
		std::filesystem::create_directory(kDirectoryPath);
	}

	// JSONファイルのパス
	std::string filePath = kDirectoryPath + groupName + ".json";

	// ファイルストリームで書き込み
	std::ofstream ofs(filePath);
	if (ofs.fail())
	{
		std::string message = "Failed to open data file for write";
		MessageBoxA(nullptr, message.c_str(), "GlobalVariables", 0);
		assert(0);
		return;
	}

	// JSONデータをファイルに書き込む
	ofs << std::setw(4) << root << std::endl;
	ofs.close();
}


/// -------------------------------------------------------------
///			　		　全グループの読み込み処理
/// -------------------------------------------------------------
void ParameterManager::LoadFiles()
{
	// ディレクトリがなければスキップ
	std::filesystem::path dir(kDirectoryPath);
	if (!std::filesystem::exists(kDirectoryPath)) return;

	// 各ファイルの処理
	std::filesystem::directory_iterator dir_it(kDirectoryPath);
	for (const std::filesystem::directory_entry& entry : dir_it)
	{
		// ファイルパスを取得
		const std::filesystem::path& filePath = entry.path();

		// ファイル拡張子を取得
		std::string extension = filePath.extension().string();

		// .jsonファイル以外はスキップ
		if (extension.compare(".json") != 0) continue;

		// ファイル読み込み
		LoadFile(filePath.stem().string());
	}
}


/// -------------------------------------------------------------
///			　		　各グループの読み込み処理
/// -------------------------------------------------------------
void ParameterManager::LoadFile(const std::string& groupName)
{
	// 読み込むJSONファイルのフルパスを合成する
	std::string filePath = kDirectoryPath + groupName + ".json";

	// 読み込み用ファイルストリーム
	std::ifstream ifs;

	// ファイルを読み込み用に開く
	ifs.open(filePath);

	// ファイルオープンが失敗した場合
	if (ifs.fail())
	{
		std::string message = "Failed to open data file: " + filePath;
		MessageBoxA(nullptr, message.c_str(), "GlobalVariables", 0);
		assert(0);
		return;
	}

	// JSON文字列の読み込み
	json root;

	// json文字列からjsonのデータ構造に展開
	ifs >> root;

	// ファイルを閉じる
	ifs.close();

	// グループを検索
	json::iterator itGroup = root.find(groupName);

	// 未登録チェック
	assert(itGroup != root.end());

	// 各アイテムについて
	for (json::iterator itItem = itGroup->begin(); itItem != itGroup->end(); ++itItem)
	{
		// アイテム名を取得
		const std::string& itemName = itItem.key();

		/// ---------- int32_t型を保持している場合 ---------- ///
		if (itItem->is_number_integer())
		{
			int32_t value = itItem->get<int32_t>();
			SetValue(groupName, itemName, value);
		}

		/// ---------- float型を保持している場合 ---------- ///
		else if (itItem->is_number_float())
		{
			float value = itItem->get<float>();
			SetValue(groupName, itemName, value);
		}

		/// ---------- 要素数3の配列である場合 ---------- ///
		else if (itItem->is_array() && itItem->size() == 3)
		{
			Vector3 value = { itItem->at(0), itItem->at(1), itItem->at(2) };
			SetValue(groupName, itemName, value);
		}

		/// ---------- bool型を保持している場合 ---------- ///
		else if (itItem->is_boolean())
		{
			bool value = itItem->get<bool>();
			SetValue(groupName, itemName, value);
		}
		else
		{
			// 不明な型の場合のエラー処理
			std::string message = "Unknown data type in file: " + filePath + " for item: " + itemName;
			MessageBoxA(nullptr, message.c_str(), "GlobalVariables", 0);
			assert(0);
		}
	}
}


/// -------------------------------------------------------------
///                 アイテムを描画する関数
/// -------------------------------------------------------------
void ParameterManager::DrawItem(const std::string& itemName, ParameterManager::Item& item)
{
	/// ---------- int32_t型を保持している場合 ---------- ///
	if (std::holds_alternative<int32_t>(item.value))
	{
		int32_t& value = std::get<int32_t>(item.value);
		ImGui::SliderInt(itemName.c_str(), &value, 0, 100);
	}
	/// ---------- float型を保持している場合 ---------- ///
	else if (std::holds_alternative<float>(item.value))
	{
		float& value = std::get<float>(item.value);
		ImGui::SliderFloat(itemName.c_str(), &value, 0.0f, 100.0f);
	}
	/// ---------- Vector3を保持している場合 ---------- ///
	else if (std::holds_alternative<Vector3>(item.value))
	{
		Vector3& value = std::get<Vector3>(item.value);
		ImGui::SliderFloat3(itemName.c_str(), reinterpret_cast<float*>(&value), -10.0f, 10.0f);
	}
	/// ---------- bool型を保持している場合 ---------- ///
	else if (std::holds_alternative<bool>(item.value))
	{
		bool& value = std::get<bool>(item.value);
		ImGui::Checkbox(itemName.c_str(), &value);
	}
	else
	{
		ImGui::Text("Unsupported type for item: %s", itemName.c_str());
	}
}
