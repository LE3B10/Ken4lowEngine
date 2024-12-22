#pragma once
#include <exception> // std::exception の基底クラスを使用するためのヘッダファイル
#include <string>	 // std::string を使用するためのヘッダファイル


/// -------------------------------------------------------------
///		　カスタム例外クラス WavLoaderException を定義
/// std::exception を継承して、WavLoader クラス専用のエラー処理を提供する　
/// -------------------------------------------------------------
class WavLoaderException : public std::exception
{
private: /// ---------- メンバ変数 ---------- ///

	// エラーメッセージを格納するメンバ変数
	std::string errorMessage;

public: /// ---------- メンバ関数 ---------- ///

	// コンストラクタ
	// エラーメッセージを引数として受け取り、メンバ変数 errorMessage に格納する。
	explicit WavLoaderException(const std::string& message) : errorMessage(message) {}

	// what() メソッドをオーバーライド
	// このメソッドは std::exception のインターフェースを提供し、
	// エラー発生時にエラーメッセージを取得できる。
	const char* what() const noexcept override { return errorMessage.c_str(); } // エラーメッセージを C 文字列として返す

};