#pragma once
#include "PSDObject.h"

namespace SivPSD
{
	enum class StoreTarget
	{
		Image,
		Texture,
		ImageAndTexture,
	};

	class PSDImporter
	{
	public:
		struct Config
		{
			FilePath filepath{};
			StoreTarget storeTarget = StoreTarget::Texture;
			int maxThreads = 1;
			bool importAsync = false;
		};

		PSDImporter();
		explicit PSDImporter(const Config& config = {});

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
