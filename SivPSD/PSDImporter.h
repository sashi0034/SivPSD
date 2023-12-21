#pragma once
#include "PSDObject.h"

namespace SivPSD
{
	/// @brief 読み込み情報の格納先
	enum class StoreTarget
	{
		Image,
		Texture,
		MipmapTexture,
		ImageAndTexture,
		ImageAndMipmapTexture,
	};

	class PSDImporter
	{
	public:
		struct Config
		{
			/// @brief 読み込むファイルパス
			FilePath filepath{};

			/// @brief 読み込み情報の格納先
			StoreTarget storeTarget = StoreTarget::MipmapTexture;

			/// @brief 最大スレッド数
			int maxThreads = 3;

			/// @brief 非同期にするか
			bool startAsync = false;
		};

		PSDImporter();
		explicit PSDImporter(const FilePath& filepath);
		explicit PSDImporter(const Config& config);

		/// @brief ファイル読み込み時などで発生したエラー (これが none の場合でもレイヤー単体にはエラーが含まれている可能性があります)
		[[nodiscard]]
		Optional<PSDError> getCriticalError() const;

		/// @brief 読み込んだオブジェクト
		[[nodiscard]]
		PSDObject getObject() const;

		/// @brief 読み込みが完了しているか
		[[nodiscard]]
		bool isReady() const noexcept;

	private:
		struct Impl;
		std::shared_ptr<Impl> p_impl;
	};
}
