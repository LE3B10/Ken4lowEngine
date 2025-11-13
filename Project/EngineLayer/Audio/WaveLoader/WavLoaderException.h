#pragma once
#include <exception> // std::exception の基底クラスを使用するためのヘッダファイル
#include <string>	 // std::string を使用するためのヘッダファイル


/// -------------------------------------------------------------
///		　WavLoader 専用のカスタム例外クラス
/// -------------------------------------------------------------
/// <summary>
/// std::exception を継承した、WavLoader 専用の例外クラス。<br/>
/// WAV 読み込み／XAudio2 関連のエラー時に、詳細なメッセージ付きで例外を投げるために使用します。<br/>
/// what() でエラーメッセージ文字列を取得できます。
/// </summary>
class WavLoaderException : public std::exception
{
private: /// ---------- メンバ変数 ---------- ///

	/// <summary>
	/// エラーメッセージを格納する文字列。
	/// </summary>
	std::string errorMessage;

public: /// ---------- メンバ関数 ---------- ///

	/// <summary>
	/// コンストラクタ。<br/>
	/// エラーメッセージ文字列を受け取り、内部に保持します。
	/// </summary>
	/// <param name="message">エラー内容を表すメッセージ。</param>
	explicit WavLoaderException(const std::string& message) : errorMessage(message) {}

	/// <summary>
	/// エラーメッセージ文字列を C 文字列として返します。<br/>
	/// std::exception::what() をオーバーライドしています。
	/// </summary>
	/// <returns>保持しているエラーメッセージへのポインタ。</returns>
	const char* what() const noexcept override { return errorMessage.c_str(); }
};