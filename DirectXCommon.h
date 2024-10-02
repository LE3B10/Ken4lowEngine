#pragma once
#include "DirectXInclude.h"

// 前方宣言
class WinApp;

///----- DirectXCommonクラス -----///
class DirectXCommon
{
public:
	// Microsoft::WRL省略
	template <class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

public:
	///----- メンバ関数 -----///

	// 初期化処理
	void Initialize(WinApp* winApp);

	// 描画開始・終了処理
	void BeginDraw();
	void EndDraw();

	// 終了処理
	void Finalize();

private:
	///----- メンバ関数 -----///

	// デバッグ表示
	void DebugLayer();
	
	// エラー警告
	void ErrorWarning();

private:
	///----- メンバ変数 -----///
	DirectXDevice* device;

};

