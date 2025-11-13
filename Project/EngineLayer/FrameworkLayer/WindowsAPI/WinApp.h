#pragma once
#include <Windows.h>
#include <cstdint>

/// -------------------------------------------------------------
///				WIndowsAPI - ウィンドウズ作成クラス
/// -------------------------------------------------------------
class WinApp
{
public: /// ---------- メンバ関数 ---------- ///

	/// <summary>
	/// WinApp のシングルトンインスタンスを取得します。
	/// </summary>
	/// <returns>WinApp の唯一のインスタンス。</returns>
	static WinApp* GetInstance();

	/// <summary>
	/// メインウィンドウを作成します。<br/>
	/// ・COM ライブラリの初期化（CoInitializeEx）<br/>
	/// ・ウィンドウクラス（WNDCLASS）の登録<br/>
	/// ・指定されたクライアントサイズでウィンドウを生成<br/>
	/// ・ウィンドウの表示（ShowWindow）<br/>
	/// を行います。
	/// </summary>
	/// <param name="Width">クライアント領域の幅。省略時は kClientWidth。</param>
	/// <param name="Height">クライアント領域の高さ。省略時は kClientHeight。</param>
	void CreateMainWindow(uint32_t Width = kClientWidth, uint32_t Height = kClientHeight);

	/// <summary>
	/// 終了処理を行います。<br/>
	/// ・ウィンドウの破棄（CloseWindow）<br/>
	/// ・COM ライブラリの終了処理（CoUninitialize）<br/>
	/// を実行します。
	/// </summary>
	void Finalize();

	/// <summary>
	/// Windows メッセージを処理します。<br/>
	/// PeekMessage / TranslateMessage / DispatchMessage を使ってメッセージループを回し、<br/>
	/// WM_QUIT が検出されたら true を返します。<br/>
	/// メインループ側では、この戻り値が true になったらゲームループを抜けるようにします。
	/// </summary>
	/// <returns>アプリ終了リクエスト（WM_QUIT）が来たら true、それ以外は false。</returns>
	bool ProcessMessage();

	/// <summary>
	/// 作成されたウィンドウの HWND を取得します。<br/>
	/// DirectX のスワップチェーン生成などに使用します。
	/// </summary>
	HWND GetHwnd() const { return hwnd; }

	/// <summary>
	/// ウィンドウクラス登録時に使用したインスタンスハンドル (HINSTANCE) を取得します。
	/// </summary>
	HINSTANCE GetHInstance() const { return wc.hInstance; }

	// クライアント領域サイズ
	static inline const UINT32 kClientWidth = 1280;
	static inline const UINT32 kClientHeight = 720;

private: /// ---------- メンバ関数 ---------- ///

	/// <summary>
	/// ウィンドウプロシージャ。<br/>
	/// ・WM_CLOSE：DestroyWindow を呼び出してウィンドウ破棄<br/>
	/// ・WM_DESTROY：PostQuitMessage(0) を呼び出してアプリ終了を通知<br/>
	/// ・それ以外：DefWindowProc に処理を委譲<br/>
	/// ImGui 使用時は、先に ImGui_ImplWin32_WndProcHandler にメッセージを渡します。
	/// </summary>
	/// <param name="hwnd">対象ウィンドウのハンドル。</param>
	/// <param name="msg">メッセージ ID。</param>
	/// <param name="wparam">メッセージ固有の追加情報。</param>
	/// <param name="lparam">メッセージ固有の追加情報。</param>
	/// <returns>メッセージ処理の結果。</returns>
	static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

private: /// ---------- メンバ変数 ---------- ///

	// ウィンドウハンドル
	HWND hwnd = nullptr;

	// ウィンドウクラスの設定
	WNDCLASS wc{};

private: /// ---------- コピー禁止 ---------- ///
	
	/// <summary>
	/// 外部からの生成を禁止するためのプライベートコンストラクタ。<br/>
	/// シングルトンパターンとして利用します。
	/// </summary>
	WinApp() = default;

	/// <summary>
	/// デフォルトデストラクタ。
	/// </summary>
	~WinApp() = default;

	/// <summary>
	/// コピーコンストラクタは使用禁止です。
	/// </summary>
	WinApp(const WinApp&) = delete;

	/// <summary>
	/// 代入演算子は使用禁止です。
	/// </summary>
	const WinApp& operator=(const WinApp&) = delete;
};
