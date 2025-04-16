#pragma once
#include <vector>


/// -------------------------------------------------------------
///						　接触履歴クラス
/// -------------------------------------------------------------
class ContactRecord
{
public: /// ---------- メンバ関数 ---------- ///

	// 履歴を追加する関数
	void Add(uint32_t number);

	// 履歴を確認する関数
	bool Check(uint32_t number);

	// 履歴を削除する関数
	void Crear();

private: /// ---------- メンバ変数 ---------- ///

	// 履歴を記録する変数
	std::vector<uint32_t> record_;

};

