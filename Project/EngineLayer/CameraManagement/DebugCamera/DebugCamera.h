#pragma once
#include "WorldTransform.h"
#include "Quaternion.h"

namespace Ken4lowEngine
{


/// -------------------------------------------------------------
///						デバッグカメラクラス
/// -------------------------------------------------------------
class DebugCamera
{
public: /// ---------- メンバ関数 ---------- ///

	/// <summary>
	/// DebugCamera のシングルトンインスタンスを取得します。
	/// </summary>
	/// <returns>DebugCamera の唯一のインスタンス。</returns>
	static DebugCamera* GetInstance();

	/// <summary>
	/// デバッグカメラの初期化処理。<br/>
	/// ・WorldTransform の初期化<br/>
	/// ・初期位置を (0,0,-50) に設定<br/>
	/// ・FOV / アスペクト比 / ニアクリップ / ファークリップを設定<br/>
	/// ・ビュー行列 / 射影行列 / ビュー射影行列の初期計算<br/>
	/// を行います。
	/// </summary>
	void Initialize();

	/// <summary>
	/// 終了処理
	/// </summary>
	void Finalize();

	/// <summary>
	/// 毎フレームの更新処理。<br/>
	/// ・キーボード入力によるカメラ移動／回転（Move）<br/>
	/// ・それに基づくビュー行列・射影行列・ビュー射影行列の更新（UpdateViewProjection）<br/>
	/// を行います。
	/// </summary>
	void Update();

private: /// ---------- メンバ関数 ---------- ///

	/// <summary>
	/// カメラの移動／回転入力を処理します。<br/>
	/// ・W / S：前後移動<br/>
	/// ・A / D：左右移動<br/>
	/// ・Space / LShift：上下移動<br/>
	/// ・カーソルキー：回転（X / Y 軸）<br/>
	/// 入力に応じて移動ベクトルを作成し、回転行列を使ってワールド空間に変換してから
	/// worldTransform_.translate_ に加算します。
	/// </summary>
	void Move();

	/// <summary>
	/// ビュー行列・射影行列・ビュー射影行列を更新します。<br/>
	/// ・worldTransform_ から回転行列 rotateMatrix_ を作成<br/>
	/// ・スケール / 回転 / 平行移動からワールド行列 worldMatrix_ を生成<br/>
	/// ・ビュー行列 viewMatrix_ を worldMatrix_ の逆行列として計算<br/>
	/// ・射影行列 projectionMatrix_ を透視射影として作成<br/>
	/// ・viewProjectionMatrix_ = viewMatrix_ × projectionMatrix_ を計算<br/>
	/// という流れで各行列を更新します。
	/// </summary>
	void UpdateViewProjection();

public: /// ---------- 設定 ---------- ///

	/// <summary>
	/// ビュー行列を直接設定します。<br/>
	/// 外部から行列を上書きしたい場合に使用します。
	/// </summary>
	/// <param name="viewMatrix">新しいビュー行列。</param>
	void SetViewMatrix(const Matrix4x4& viewMatrix) { viewMatrix_ = viewMatrix; }

	/// <summary>
	/// 射影行列を直接設定します。
	/// </summary>
	/// <param name="projectionMatrix">新しい射影行列。</param>
	void SetProjectionMatrix(const Matrix4x4& projectionMatrix) { projectionMatrix_ = projectionMatrix; }

	/// <summary>
	/// ビュー射影行列を直接設定します。
	/// </summary>
	/// <param name="viewProjectionMatrix">新しいビュー射影行列。</param>
	void SetViewProjectionMatrix(const Matrix4x4& viewProjectionMatrix) { viewProjectionMatrix_ = viewProjectionMatrix; }

	/// <summary>
	/// 回転角（オイラー角）を設定します。<br/>
	/// 次回 UpdateViewProjection() で反映されます。
	/// </summary>
	/// <param name="rotate">X,Y,Z 回転（ラジアン）。</param>
	void SetRotate(const Vector3& rotate) { worldTransform_.rotate_ = rotate; }

	/// <summary>
	/// カメラの位置を設定します。<br/>
	/// 次回 UpdateViewProjection() で反映されます。
	/// </summary>
	/// <param name="translate">新しいカメラ位置。</param>
	void SetTranslate(const Vector3& translate) { worldTransform_.translate_ = translate; }

	/// <summary>
	/// 垂直方向視野角(FoV Y)を設定します。
	/// </summary>
	/// <param name="fovY">垂直方向の視野角（ラジアン）。</param>
	void SetFovY(float fovY) { fovY_ = fovY; }

	/// <summary>
	/// アスペクト比を設定します。<br/>
	/// 例：画面幅 / 画面高さ。
	/// </summary>
	/// <param name="aspectRatio">アスペクト比。</param>
	void SetAspectRatio(float aspectRatio) { aspectRatio_ = aspectRatio; }

	/// <summary>
	/// ニアクリップ面までの距離を設定します。
	/// </summary>
	/// <param name="nearClip">ニアクリップ距離。</param>
	void SetNearClip(float nearClip) { nearClip_ = nearClip; }

	/// <summary>
	/// ファークリップ面までの距離を設定します。
	/// </summary>
	/// <param name="farClip">ファークリップ距離。</param>
	void SetFarClip(float farClip) { farClip_ = farClip; }

public: /// ---------- 取得 ---------- ///

	/// <summary>
	/// 現在のビュー行列を取得します。
	/// </summary>
	Matrix4x4 GetViewMatrix() const { return viewMatrix_; }

	/// <summary>
	/// 現在の射影行列を取得します。
	/// </summary>
	Matrix4x4 GetProjectionMatrix() const { return projectionMatrix_; }

	/// <summary>
	/// 現在のビュー射影行列を取得します。
	/// </summary>
	Matrix4x4 GetViewProjectionMatrix() const { return viewProjectionMatrix_; }

	/// <summary>
	/// 現在の回転角（オイラー角）を取得します。
	/// </summary>
	Vector3 GetRotate() const { return worldTransform_.rotate_; }

	/// <summary>
	/// 現在のカメラ位置を取得します。
	/// </summary>
	Vector3 GetTranslate() const { return worldTransform_.translate_; }

private: /// ---------- メンバ変数 ---------- ///

	// ワールドトランスフォーム
	WorldTransform worldTransform_;

	// ワールド行列データ
	Matrix4x4 worldMatrix_;

	// 回転行列
	Matrix4x4 rotateMatrix_;

	// ビュー行列データ
	Matrix4x4 viewMatrix_;

	// プロジェクション行列データ
	Matrix4x4 projectionMatrix_;
	float fovY_ = 0.0f;		   // 水平方向視野角
	float aspectRatio_ = 0.0f; // アスペクト比
	float nearClip_ = 0.0f;    // ニアクリップ
	float farClip_ = 0.0f;	   // ファークリップ

	// 合成行列
	Matrix4x4 viewProjectionMatrix_;

	// クォータニオン
	Quaternion rotation_{};

private: /// ---------- コピー禁止 ---------- ///

	/// <summary>外部からの生成を禁止するプライベートコンストラクタ。</summary>
	DebugCamera() = default;
	/// <summary>デフォルトデストラクタ。</summary>
	~DebugCamera() = default;
	/// <summary>コピーコンストラクタは禁止。</summary>
	DebugCamera(const DebugCamera&) = delete;
	/// <summary>代入演算子は禁止。</summary>
	DebugCamera& operator=(const DebugCamera&) = delete;
};


} // namespace Ken4lowEngine
