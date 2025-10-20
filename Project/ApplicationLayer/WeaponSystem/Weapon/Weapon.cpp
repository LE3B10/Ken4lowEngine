#include "Weapon.h"
#include <iomanip>
#include <filesystem>
#include <cctype>

using nlohmann::json;
namespace fs = std::filesystem;

/// json配列からfloat[4]へコピー
inline static void Copy4(const json& a, Vector4& out) { a[0].get_to(out.x);	a[1].get_to(out.y);	a[2].get_to(out.z);	a[3].get_to(out.w); }

/// json配列からfloat[3]へコピー
inline static void Copy3(const json& a, Vector3& out) { a[0].get_to(out.x);	a[1].get_to(out.y);	a[2].get_to(out.z); }

// / Vector4→json配列変換
static json ToJsonColor(const Vector4& v) { return json::array({ v.x,v.y,v.z,v.w }); }

// / Vector3→json配列変換
static json ToJsonVec3(const Vector3& v) { return json::array({ v.x,v.y,v.z }); }

// ファイル名用スラッグ（name→"machine_gun.json" など）
static std::string Slugify(const std::string& s) {
	std::string o; o.reserve(s.size());
	auto push_underscore = [&]() {
		if (o.empty() || o.back() != '_') o.push_back('_');
		};
	for (unsigned char ch : s) {
		if (std::isalnum(ch)) o.push_back((char)std::tolower(ch));
		else if (ch == ' ' || ch == '-' || ch == '_') push_underscore();
		else push_underscore();
	}
	if (o.empty()) o = "weapon";
	return o;
}

static const char* ClassToString(WeaponClass c)
{
	switch (c) {
	case WeaponClass::Primary: return "Primary";
	case WeaponClass::Backup:  return "Backup";
	case WeaponClass::Melee:   return "Melee";
	case WeaponClass::Special: return "Special";
	case WeaponClass::Sniper:  return "Sniper";
	case WeaponClass::Heavy:   return "Heavy";
	}
	return "Primary";
}
static WeaponClass ParseClass(const nlohmann::json& j)
{
	if (!j.contains("class")) return WeaponClass::Primary;
	if (j["class"].is_string()) {
		std::string s = j["class"].get<std::string>();
		std::transform(s.begin(), s.end(), s.begin(), ::tolower);
		if (s == "primary") return WeaponClass::Primary;
		if (s == "backup")  return WeaponClass::Backup;
		if (s == "melee")   return WeaponClass::Melee;
		if (s == "special") return WeaponClass::Special;
		if (s == "sniper")  return WeaponClass::Sniper;
		if (s == "heavy")   return WeaponClass::Heavy;
		return WeaponClass::Primary;
	}
	// 数値(0..5)でも受け付け
	int v = j["class"].get<int>();
	v = std::clamp(v, 0, 5);
	return static_cast<WeaponClass>(v);
}


// WeaponData→json変換
static json ToJson(const WeaponData& w)
{
	json j;
	j["name"] = w.name;
	j["class"] = ClassToString(w.clazz);
	j["muzzleSpeed"] = w.muzzleSpeed;
	j["maxDistance"] = w.maxDistance;
	j["rpm"] = w.rpm;

	j["magCapacity"] = w.magCapacity;
	j["startingReserve"] = w.startingReserve;
	j["reloadTime"] = w.reloadTime;
	j["bulletsPerShot"] = w.bulletsPerShot;
	j["autoReload"] = w.autoReload;

	j["requestedMaxSegments"] = w.requestedMaxSegments;

	j["spreadDeg"] = w.spreadDeg;
	j["pelletTracerMode"] = w.pelletTracerMode;
	j["pelletTracerCount"] = w.pelletTracerCount;

	// tracer
	auto& t = j["tracer"];
	t["enabled"] = w.tracer.enabled;
	t["tracerLength"] = w.tracer.tracerLength;
	t["tracerWidth"] = w.tracer.tracerWidth;
	t["minSegLength"] = w.tracer.minSegLength;
	t["startOffsetForward"] = w.tracer.startOffsetForward;
	t["color"] = ToJsonColor(w.tracer.color);

	// muzzle
	auto& m = j["muzzle"];
	m["enabled"] = w.muzzle.enabled;
	m["life"] = w.muzzle.life;
	m["startLength"] = w.muzzle.startLength;
	m["endLength"] = w.muzzle.endLength;
	m["startWidth"] = w.muzzle.startWidth;
	m["endWidth"] = w.muzzle.endWidth;
	m["randomYawDeg"] = w.muzzle.randomYawDeg;
	m["color"] = ToJsonColor(w.muzzle.color);

	m["offsetForward"] = w.muzzle.offsetForward;
	m["sparksEnabled"] = w.muzzle.sparksEnabled;
	m["sparkCount"] = w.muzzle.sparkCount;
	m["sparkLifeMin"] = w.muzzle.sparkLifeMin;
	m["sparkLifeMax"] = w.muzzle.sparkLifeMax;
	m["sparkSpeedMin"] = w.muzzle.sparkSpeedMin;
	m["sparkSpeedMax"] = w.muzzle.sparkSpeedMax;
	m["sparkConeDeg"] = w.muzzle.sparkConeDeg;
	m["sparkGravityY"] = w.muzzle.sparkGravityY;
	m["sparkWidth"] = w.muzzle.sparkWidth;
	m["sparkOffsetForward"] = w.muzzle.sparkOffsetForward;
	m["sparkColorStart"] = ToJsonColor(w.muzzle.sparkColorStart);
	m["sparkColorEnd"] = ToJsonColor(w.muzzle.sparkColorEnd);

	// casing
	auto& c = j["casing"];
	c["enabled"] = w.casing.enabled;
	c["offsetRight"] = w.casing.offsetRight;
	c["offsetUp"] = w.casing.offsetUp;
	c["offsetBack"] = w.casing.offsetBack;
	c["speedMin"] = w.casing.speedMin;
	c["speedMax"] = w.casing.speedMax;
	c["coneDeg"] = w.casing.coneDeg;
	c["gravityY"] = w.casing.gravityY;
	c["life"] = w.casing.life;
	c["drag"] = w.casing.drag;
	c["upKick"] = w.casing.upKick;
	c["upBias"] = w.casing.upBias;
	c["spinMin"] = w.casing.spinMin;
	c["spinMax"] = w.casing.spinMax;
	c["color"] = ToJsonColor(w.casing.color);
	c["scale"] = ToJsonVec3(w.casing.scale);

	return j;
}

// 単一武器オブジェクト → WeaponData へ (巨大JSONのループ処理を関数化)
static bool FromJsonWeaponObject(const json& wj, WeaponData& w) {
	try {
		w.name = wj["name"].get<std::string>();
		w.clazz = ParseClass(wj);
		w.muzzleSpeed = wj["muzzleSpeed"];
		w.maxDistance = wj["maxDistance"];
		w.rpm = wj["rpm"];

		w.magCapacity = wj["magCapacity"];
		w.startingReserve = wj["startingReserve"];
		w.reloadTime = wj["reloadTime"];
		w.bulletsPerShot = wj["bulletsPerShot"];
		w.autoReload = wj["autoReload"];

		w.requestedMaxSegments = wj["requestedMaxSegments"];
		w.spreadDeg = wj["spreadDeg"];
		w.pelletTracerMode = wj["pelletTracerMode"];
		w.pelletTracerCount = wj["pelletTracerCount"];

		auto& tj = wj["tracer"];
		w.tracer.enabled = tj["enabled"];
		w.tracer.tracerLength = tj["tracerLength"];
		w.tracer.tracerWidth = tj["tracerWidth"];
		w.tracer.minSegLength = tj["minSegLength"];
		w.tracer.startOffsetForward = tj["startOffsetForward"];
		Copy4(tj["color"], w.tracer.color);

		auto& mj = wj["muzzle"];
		w.muzzle.enabled = mj["enabled"];
		w.muzzle.life = mj["life"];
		w.muzzle.startLength = mj["startLength"];
		w.muzzle.endLength = mj["endLength"];
		w.muzzle.startWidth = mj["startWidth"];
		w.muzzle.endWidth = mj["endWidth"];
		w.muzzle.randomYawDeg = mj["randomYawDeg"];
		Copy4(mj["color"], w.muzzle.color);

		w.muzzle.offsetForward = mj["offsetForward"];
		w.muzzle.sparksEnabled = mj["sparksEnabled"];
		w.muzzle.sparkCount = mj["sparkCount"];
		w.muzzle.sparkLifeMin = mj["sparkLifeMin"];
		w.muzzle.sparkLifeMax = mj["sparkLifeMax"];
		w.muzzle.sparkSpeedMin = mj["sparkSpeedMin"];
		w.muzzle.sparkSpeedMax = mj["sparkSpeedMax"];
		w.muzzle.sparkConeDeg = mj["sparkConeDeg"];
		w.muzzle.sparkGravityY = mj["sparkGravityY"];
		w.muzzle.sparkWidth = mj["sparkWidth"];
		w.muzzle.sparkOffsetForward = mj["sparkOffsetForward"];
		Copy4(mj["sparkColorStart"], w.muzzle.sparkColorStart);
		Copy4(mj["sparkColorEnd"], w.muzzle.sparkColorEnd);

		auto& cj = wj["casing"];
		w.casing.enabled = cj["enabled"];
		w.casing.offsetRight = cj["offsetRight"];
		w.casing.offsetUp = cj["offsetUp"];
		w.casing.offsetBack = cj["offsetBack"];
		w.casing.speedMin = cj["speedMin"];
		w.casing.speedMax = cj["speedMax"];
		w.casing.coneDeg = cj["coneDeg"];
		w.casing.gravityY = cj["gravityY"];
		w.casing.life = cj["life"];
		w.casing.drag = cj["drag"];
		w.casing.upKick = cj["upKick"];
		w.casing.upBias = cj["upBias"];
		w.casing.spinMin = cj["spinMin"];
		w.casing.spinMax = cj["spinMax"];
		Copy4(cj["color"], w.casing.color);
		Copy3(cj["scale"], w.casing.scale);
		return true;
	}
	catch (...) {
		return false;
	}
}

/// -------------------------------------------------------------
///				　		コンストラクタ
/// -------------------------------------------------------------
Weapon::Weapon(const WeaponData& data) : weaponData_(data)
{
	// 初期弾数セット
	weaponState_.ammoInMag = data.magCapacity;

	// 予備弾数セット
	weaponState_.reserveAmmo = data.startingReserve;

	// 発射間隔計算
	fireInterval_ = 60.0f / data.rpm;
}

/// -------------------------------------------------------------
///				　		武器データ読み込み処理
/// -------------------------------------------------------------
std::unordered_map<std::string, WeaponData> Weapon::LoadWeapon(const std::string& filePath)
{
	// JSONファイルを読み込み、WeaponData構造体に変換して返す
	std::unordered_map<std::string, WeaponData> out;
	std::ifstream ifs(filePath); // ファイルを開く
	json root; ifs >> root;      // JSONパース

	// 武器ごとにループ
	for (auto& wj : root["weapons"])
	{
		WeaponData w{};

		/// ---------- 基本情報読み込み ---------- ///
		w.name = wj["name"].get<std::string>(); // 武器名
		w.clazz = ParseClass(wj);				// カテゴリー
		w.muzzleSpeed = wj["muzzleSpeed"];		// m/s (銃口初速)
		w.maxDistance = wj["maxDistance"];		// m (弾の飛距離)
		w.rpm = wj["rpm"];						// 発射レート (rounds per minute)

		w.magCapacity = wj["magCapacity"];		   // 1マガジンの装弾数
		w.startingReserve = wj["startingReserve"]; // 初期予備弾数（総予備）
		w.reloadTime = wj["reloadTime"];		   // リロード時間[秒]
		w.bulletsPerShot = wj["bulletsPerShot"];   // 1発で何弾出すか（SGなら>1）
		w.autoReload = wj["autoReload"];		   // 自動リロードするか

		// 弾道セグメント数
		w.requestedMaxSegments = wj["requestedMaxSegments"];

		w.spreadDeg = wj["spreadDeg"];               // 発射時の拡がり角（度）
		w.pelletTracerMode = wj["pelletTracerMode"]; // 散弾時のトレーサ動作モード（0=なし,1=1発,2=全発）
		w.pelletTracerCount = wj["pelletTracerCount"]; // 散弾時にトレーサを出す発数（pelletTracerMode==1時のみ有効）

		/// ---------- トレーサ設定読み込み ---------- ///
		auto& tj = wj["tracer"];
		w.tracer.enabled = tj["enabled"];						// トレーサー有効
		w.tracer.tracerLength = tj["tracerLength"];				// 見た目の長さ
		w.tracer.tracerWidth = tj["tracerWidth"];				// 幅
		w.tracer.minSegLength = tj["minSegLength"];				// セグメント間引き閾値
		w.tracer.startOffsetForward = tj["startOffsetForward"]; // 弾/トレーサの開始点を銃口から前後にオフセット
		Copy4(tj["color"], w.tracer.color);				// RGBA

		/// ---------- マズルフラッシュ設定読み込み ---------- ///
		auto& mj = wj["muzzle"];
		w.muzzle.enabled = mj["enabled"];			// マズルフラッシュ有効
		w.muzzle.life = mj["life"];					// 寿命（秒）
		w.muzzle.startLength = mj["startLength"];	// 初期の長さ [m]
		w.muzzle.endLength = mj["endLength"];		// 終了時の長さ（しぼむ） [m]
		w.muzzle.startWidth = mj["startWidth"];		// 初期の太さ [m]
		w.muzzle.endWidth = mj["endWidth"];			// 終了時の太さ [m]
		w.muzzle.randomYawDeg = mj["randomYawDeg"]; // 発射ごとのランダム広がり（度）
		Copy4(mj["color"], w.muzzle.color);			// 色 (RGBA)

		w.muzzle.offsetForward = mj["offsetForward"];		    // フラッシュ根元を前後にオフセット
		w.muzzle.sparksEnabled = mj["sparksEnabled"];		    // 火花を出すか
		w.muzzle.sparkCount = mj["sparkCount"];				    // 1発で何本
		w.muzzle.sparkLifeMin = mj["sparkLifeMin"];			    // 秒 : 火花の最小寿命
		w.muzzle.sparkLifeMax = mj["sparkLifeMax"];			    // 秒 : 火花の最大寿命
		w.muzzle.sparkSpeedMin = mj["sparkSpeedMin"];		    // m/s : 火花の最小速度
		w.muzzle.sparkSpeedMax = mj["sparkSpeedMax"];		    // m/s : 火花の最大速度
		w.muzzle.sparkConeDeg = mj["sparkConeDeg"];			    // 前方への拡がり角（円錐）
		w.muzzle.sparkGravityY = mj["sparkGravityY"];		    // 火花用重力（強めに落とす）
		w.muzzle.sparkWidth = mj["sparkWidth"];				    // 太さ
		w.muzzle.sparkOffsetForward = mj["sparkOffsetForward"]; // 火花の開始位置

		Copy4(mj["sparkColorStart"], w.muzzle.sparkColorStart); // 出現時の色
		Copy4(mj["sparkColorEnd"], w.muzzle.sparkColorEnd);		// 消滅時の色

		/// ---------- 薬莢設定読み込み ---------- ///
		auto& cj = wj["casing"];
		w.casing.enabled = cj["enabled"];		  // 薬莢エフェクト有効
		w.casing.offsetRight = cj["offsetRight"]; // 銃口基準のローカルオフセット（右）
		w.casing.offsetUp = cj["offsetUp"];		  // 銃口基準のローカルオフセット（上）
		w.casing.offsetBack = cj["offsetBack"];	  // 銃口基準のローカルオフセット（後ろ）
		w.casing.speedMin = cj["speedMin"];		  // m/s : 初速最小
		w.casing.speedMax = cj["speedMax"];		  // m/s : 初速最大
		w.casing.coneDeg = cj["coneDeg"];		  // 度 : 右方向を中心にした円錐拡がり
		w.casing.gravityY = cj["gravityY"];		  // m/s^2 : 自然落下
		w.casing.life = cj["life"];				  // 秒 : 寿命
		w.casing.drag = cj["drag"];				  // 空気抵抗（簡易）
		w.casing.upKick = cj["upKick"];			  // [m/s] : 真上方向への瞬間的な“キック”
		w.casing.upBias = cj["upBias"];			  // 方向ベクトルを上向きに寄せるブレンド(0..1)
		w.casing.spinMin = cj["spinMin"];		  // rad/s : 回転速度最小
		w.casing.spinMax = cj["spinMax"];		  // rad/s : 回転速度最大

		Copy4(cj["color"], w.casing.color); // 色
		Copy3(cj["scale"], w.casing.scale); // スケール	

		// マップに追加
		out[w.name] = w;
	}

	// 返す
	return out;
}

/// -------------------------------------------------------------
///				　武器データ保存処理（展開版）
/// -------------------------------------------------------------
bool Weapon::SaveWeapons(const std::string& filePath, const std::unordered_map<std::string, WeaponData>& weaponTable)
{
	json root;
	root["weapons"] = json::array();

	for (auto& [name, w] : weaponTable)
	{
		json wj;
		wj["name"] = w.name;
		wj["muzzleSpeed"] = w.muzzleSpeed;
		wj["maxDistance"] = w.maxDistance;
		wj["rpm"] = w.rpm;

		wj["magCapacity"] = w.magCapacity;
		wj["startingReserve"] = w.startingReserve;
		wj["reloadTime"] = w.reloadTime;
		wj["bulletsPerShot"] = w.bulletsPerShot;
		wj["autoReload"] = w.autoReload;

		wj["requestedMaxSegments"] = w.requestedMaxSegments;
		wj["spreadDeg"] = w.spreadDeg;
		wj["pelletTracerMode"] = w.pelletTracerMode;
		wj["pelletTracerCount"] = w.pelletTracerCount;

		// tracer
		{
			json tj;
			tj["enabled"] = w.tracer.enabled;
			tj["tracerLength"] = w.tracer.tracerLength;
			tj["tracerWidth"] = w.tracer.tracerWidth;
			tj["minSegLength"] = w.tracer.minSegLength;
			tj["startOffsetForward"] = w.tracer.startOffsetForward;
			tj["color"] = ToJsonColor(w.tracer.color);
			wj["tracer"] = tj;
		}
		// muzzle
		{
			json mj;
			mj["enabled"] = w.muzzle.enabled;
			mj["life"] = w.muzzle.life;
			mj["startLength"] = w.muzzle.startLength;
			mj["endLength"] = w.muzzle.endLength;
			mj["startWidth"] = w.muzzle.startWidth;
			mj["endWidth"] = w.muzzle.endWidth;
			mj["randomYawDeg"] = w.muzzle.randomYawDeg;
			mj["color"] = ToJsonColor(w.muzzle.color);
			mj["offsetForward"] = w.muzzle.offsetForward;
			mj["sparksEnabled"] = w.muzzle.sparksEnabled;
			mj["sparkCount"] = w.muzzle.sparkCount;
			mj["sparkLifeMin"] = w.muzzle.sparkLifeMin;
			mj["sparkLifeMax"] = w.muzzle.sparkLifeMax;
			mj["sparkSpeedMin"] = w.muzzle.sparkSpeedMin;
			mj["sparkSpeedMax"] = w.muzzle.sparkSpeedMax;
			mj["sparkConeDeg"] = w.muzzle.sparkConeDeg;
			mj["sparkGravityY"] = w.muzzle.sparkGravityY;
			mj["sparkWidth"] = w.muzzle.sparkWidth;
			mj["sparkOffsetForward"] = w.muzzle.sparkOffsetForward;
			mj["sparkColorStart"] = ToJsonColor(w.muzzle.sparkColorStart);
			mj["sparkColorEnd"] = ToJsonColor(w.muzzle.sparkColorEnd);
			wj["muzzle"] = mj;
		}
		// casing
		{
			json cj;
			cj["enabled"] = w.casing.enabled;
			cj["offsetRight"] = w.casing.offsetRight;
			cj["offsetUp"] = w.casing.offsetUp;
			cj["offsetBack"] = w.casing.offsetBack;
			cj["speedMin"] = w.casing.speedMin;
			cj["speedMax"] = w.casing.speedMax;
			cj["coneDeg"] = w.casing.coneDeg;
			cj["gravityY"] = w.casing.gravityY;
			cj["drag"] = w.casing.drag;
			cj["life"] = w.casing.life;
			cj["upKick"] = w.casing.upKick;
			cj["upBias"] = w.casing.upBias;
			cj["spinMin"] = w.casing.spinMin;
			cj["spinMax"] = w.casing.spinMax;
			cj["color"] = ToJsonColor(w.casing.color);
			cj["scale"] = ToJsonVec3(w.casing.scale);
			wj["casing"] = cj;
		}

		root["weapons"].push_back(wj);
	}

	std::ofstream ofs(filePath);
	if (!ofs) return false;
	ofs << std::setw(2) << root << std::endl;
	return true;
}

/// -------------------------------------------------------------
///				　武器データ読み込み処理（短縮版）
/// -------------------------------------------------------------
std::unordered_map<std::string, WeaponData> Weapon::LoadFromPath(const std::string& filePath)
{
	// 入力ディレクトリ確認
	std::unordered_map<std::string, WeaponData> out;
	std::error_code ec; // エラーコード受け取り用

	if (fs::exists(filePath, ec) && fs::is_directory(filePath, ec))
	{
		for (auto& entry : fs::directory_iterator(filePath))
		{
			if (!entry.is_regular_file()) continue;
			if (entry.path().extension() != ".json") continue;

			std::ifstream ifs(entry.path());
			if (!ifs) continue;
			json root;
			try { ifs >> root; }
			catch (...) { continue; }

			// 1武器オブジェクト or 従来の "weapons" 配列の両対応
			if (root.contains("weapons") && root["weapons"].is_array())
			{
				for (auto& wj : root["weapons"])
				{
					WeaponData w{};
					if (FromJsonWeaponObject(wj, w)) out[w.name] = w;
				}
			}
			else
			{
				WeaponData w{};
				if (FromJsonWeaponObject(root, w)) out[w.name] = w;
			}
		}
		return out;
	}
	// ファイルなら従来ローダに委譲
	return LoadWeapon(filePath);
}

/// -------------------------------------------------------------
///				　武器データ保存処理（短縮版）
/// -------------------------------------------------------------
bool Weapon::SaveToPath(const std::string& filePath, const std::unordered_map<std::string, WeaponData>& weaponTable)
{
	// 出力ディレクトリ作成
	std::error_code ec;
	fs::path p(filePath);

	// ディレクトリが存在しない場合は作成
	if ((fs::exists(p, ec) && fs::is_directory(p, ec)) || !p.has_extension())
	{
		// 存在していてディレクトリならOK
		fs::create_directories(filePath, ec);
		bool allSucceeded = true;
		for (auto& [name, weapon] : weaponTable)
		{
			std::string fileName = Slugify(name) + ".json"; // ファイル名生成
			fs::path out = fs::path(filePath) / fileName; // フルパス生成
			allSucceeded &= SaveOne(out.string(), weapon); // 保存
		}
		return allSucceeded;
	}
	else
	{
		// 従来の巨大ファイル保存方式で保存
		return SaveWeapons(filePath, weaponTable);
	}
}

/// -------------------------------------------------------------
///				　単一武器データ保存処理
/// -------------------------------------------------------------
bool Weapon::SaveOne(const std::string& filePath, const WeaponData& weaponData)
{
	json j = ToJson(weaponData); // WeaponData→json変換
	std::ofstream ofs(filePath); // ファイルを開く
	if (!ofs) return false;
	ofs << std::setw(2) << j << std::endl; // 整形して書き込み
	return true;
}

/// -------------------------------------------------------------
///				　移行ヘルパ（巨大->分離）
/// -------------------------------------------------------------
bool Weapon::MigrateMonolithToDir(const std::string& monolithJson, const std::string& outDir)
{
	auto weaponTable = LoadWeapon(monolithJson);
	return SaveToPath(outDir, weaponTable);
}
