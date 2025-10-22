#include "Weapon.h"
#include <iomanip>
#include <filesystem>
#include <cctype>

using nlohmann::json;
namespace fs = std::filesystem;
using ojson = nlohmann::ordered_json;

template<class T>
static void read_if(const nlohmann::json& j, const char* key, T& dst) {
	if (j.contains(key)) dst = j.at(key).get<T>(); // at() は例外、[]のconst版はアサート
}
static void read_vec4(const nlohmann::json& j, const char* key, Vector4& dst) {
	if (j.contains(key) && j.at(key).is_array() && j.at(key).size() >= 4) {
		const auto& a = j.at(key);
		a[0].get_to(dst.x); a[1].get_to(dst.y); a[2].get_to(dst.z); a[3].get_to(dst.w);
	}
}
static void read_vec3(const nlohmann::json& j, const char* key, Vector3& dst) {
	if (j.contains(key) && j.at(key).is_array() && j.at(key).size() >= 3) {
		const auto& a = j.at(key);
		a[0].get_to(dst.x); a[1].get_to(dst.y); a[2].get_to(dst.z);
	}
}

// JSON内の「浮動小数」をすべて指定小数で丸める（再帰）
static void RoundFloatsInJson(ojson& j, int decimals) {
	if (j.is_object()) {
		for (auto& kv : j.items()) RoundFloatsInJson(kv.value(), decimals);
	}
	else if (j.is_array()) {
		for (auto& v : j) RoundFloatsInJson(v, decimals);
	}
	else if (j.is_number_float()) {
		double x = j.get<double>();
		const double p = std::pow(10.0, decimals);
		j = std::round(x * p) / p;
	}
}

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

// WeaponClass → 文字列変換
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

// jsonオブジェクトから WeaponClass へ変換
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

// 単一武器オブジェクト → WeaponData へ (巨大JSONのループ処理を関数化)
static bool FromJsonWeaponObject(const json& wj, WeaponData& w) {
	try {
		// 基本
		if (!wj.contains("name")) return false;
		w.name = wj.at("name").get<std::string>();
		w.clazz = ParseClass(wj);

		read_if(wj, "muzzleSpeed", w.muzzleSpeed);
		read_if(wj, "maxDistance", w.maxDistance);
		read_if(wj, "rpm", w.rpm);
		read_if(wj, "magCapacity", w.magCapacity);
		read_if(wj, "startingReserve", w.startingReserve);
		read_if(wj, "reloadTime", w.reloadTime);
		read_if(wj, "bulletsPerShot", w.bulletsPerShot);
		read_if(wj, "autoReload", w.autoReload);
		read_if(wj, "requestedMaxSegments", w.requestedMaxSegments);
		read_if(wj, "spreadDeg", w.spreadDeg);
		read_if(wj, "pelletTracerMode", w.pelletTracerMode);
		read_if(wj, "pelletTracerCount", w.pelletTracerCount);

		// tracer
		if (wj.contains("tracer") && wj.at("tracer").is_object()) {
			const auto& tj = wj.at("tracer");
			read_if(tj, "enabled", w.tracer.enabled);
			read_if(tj, "tracerLength", w.tracer.tracerLength);
			read_if(tj, "tracerWidth", w.tracer.tracerWidth);
			read_if(tj, "minSegLength", w.tracer.minSegLength);
			read_if(tj, "startOffsetForward", w.tracer.startOffsetForward);
			read_vec4(tj, "color", w.tracer.color);
		}

		// muzzle
		if (wj.contains("muzzle") && wj.at("muzzle").is_object()) {
			const auto& mj = wj.at("muzzle");
			read_if(mj, "enabled", w.muzzle.enabled);
			read_if(mj, "life", w.muzzle.life);
			read_if(mj, "startLength", w.muzzle.startLength);
			read_if(mj, "endLength", w.muzzle.endLength);
			read_if(mj, "startWidth", w.muzzle.startWidth);
			read_if(mj, "endWidth", w.muzzle.endWidth);
			read_if(mj, "randomYawDeg", w.muzzle.randomYawDeg);
			read_vec4(mj, "color", w.muzzle.color);

			read_if(mj, "offsetForward", w.muzzle.offsetForward);
			read_if(mj, "sparksEnabled", w.muzzle.sparksEnabled);
			read_if(mj, "sparkCount", w.muzzle.sparkCount);
			read_if(mj, "sparkLifeMin", w.muzzle.sparkLifeMin);
			read_if(mj, "sparkLifeMax", w.muzzle.sparkLifeMax);
			read_if(mj, "sparkSpeedMin", w.muzzle.sparkSpeedMin);
			read_if(mj, "sparkSpeedMax", w.muzzle.sparkSpeedMax);
			read_if(mj, "sparkConeDeg", w.muzzle.sparkConeDeg);
			read_if(mj, "sparkGravityY", w.muzzle.sparkGravityY);
			read_if(mj, "sparkWidth", w.muzzle.sparkWidth);
			read_if(mj, "sparkOffsetForward", w.muzzle.sparkOffsetForward);
			read_vec4(mj, "sparkColorStart", w.muzzle.sparkColorStart);
			read_vec4(mj, "sparkColorEnd", w.muzzle.sparkColorEnd);
		}

		// casing
		if (wj.contains("casing") && wj.at("casing").is_object()) {
			const auto& cj = wj.at("casing");
			read_if(cj, "enabled", w.casing.enabled);
			read_if(cj, "offsetRight", w.casing.offsetRight);
			read_if(cj, "offsetUp", w.casing.offsetUp);
			read_if(cj, "offsetBack", w.casing.offsetBack);
			read_if(cj, "speedMin", w.casing.speedMin);
			read_if(cj, "speedMax", w.casing.speedMax);
			read_if(cj, "coneDeg", w.casing.coneDeg);
			read_if(cj, "gravityY", w.casing.gravityY);
			read_if(cj, "life", w.casing.life);
			read_if(cj, "drag", w.casing.drag);
			read_if(cj, "upKick", w.casing.upKick);
			read_if(cj, "upBias", w.casing.upBias);
			read_if(cj, "spinMin", w.casing.spinMin);
			read_if(cj, "spinMax", w.casing.spinMax);
			read_vec4(cj, "color", w.casing.color);
			read_vec3(cj, "scale", w.casing.scale);
		}

		return true;
	}
	catch (...) {
		return false;
	}
}

static ojson ToPrettyOrderedJson(const WeaponData& w) {
	ojson j;
	// 「基本 → トレーサ → マズル → 薬莢」など、出したい順で手動で詰める
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
	{
		ojson tj;
		tj["enabled"] = w.tracer.enabled;
		tj["tracerLength"] = w.tracer.tracerLength;
		tj["tracerWidth"] = w.tracer.tracerWidth;
		tj["minSegLength"] = w.tracer.minSegLength;
		tj["startOffsetForward"] = w.tracer.startOffsetForward;
		tj["color"] = { w.tracer.color.x, w.tracer.color.y, w.tracer.color.z, w.tracer.color.w };
		j["tracer"] = std::move(tj);
	}
	// muzzle
	{
		ojson mj;
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
		j["muzzle"] = std::move(mj);
	}
	// casing
	{
		ojson cj;
		cj["enabled"] = w.casing.enabled;
		cj["offsetRight"] = w.casing.offsetRight;
		cj["offsetUp"] = w.casing.offsetUp;
		cj["offsetBack"] = w.casing.offsetBack;
		cj["speedMin"] = w.casing.speedMin;
		cj["speedMax"] = w.casing.speedMax;
		cj["coneDeg"] = w.casing.coneDeg;
		cj["gravityY"] = w.casing.gravityY;
		cj["life"] = w.casing.life;
		cj["drag"] = w.casing.drag;
		cj["upKick"] = w.casing.upKick;
		cj["upBias"] = w.casing.upBias;
		cj["spinMin"] = w.casing.spinMin;
		cj["spinMax"] = w.casing.spinMax;
		cj["color"] = ToJsonColor(w.casing.color);
		cj["scale"] = ToJsonVec3(w.casing.scale);
		// …同様に希望順で詰める…
		j["casing"] = std::move(cj);
	}
	return j;
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
	ojson j = ToPrettyOrderedJson(weaponData); // ★順序固定版
	RoundFloatsInJson(j, 3); // 浮動小数点を4桁に丸める
	std::ofstream ofs(filePath); // ファイルを開く
	if (!ofs) return false;
	ofs << j.dump(2) << '\n'; // 整形して書き込み
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
