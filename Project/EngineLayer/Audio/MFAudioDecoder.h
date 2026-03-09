#pragma once
#include <xaudio2.h>
#include <string>
#include <vector>

namespace Ken4lowEngine
{
	/// ---------- デコーダー出力データ ---------- ///
	struct DecodedAudioData
	{
		WAVEFORMATEX waveFormat{};
		std::vector<BYTE> pcmData;
	};

	/// -------------------------------------------------------------
	///				　Media Foundation を使ったデコード
	/// -------------------------------------------------------------
	class MFAudioDecoder
	{
	public: /// ---------- メンバ関数 ---------- ///

		// Media Foundation を使って音声ファイルを PCM(16bit) へデコードする
		static bool DecodeFile(const std::string& filePath, DecodedAudioData& outData);
	};

} // namespace Ken4lowEngine
