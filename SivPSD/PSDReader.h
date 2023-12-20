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

	class PSDReader
	{
	public:
		struct Config
		{
			FilePath filepath{};
			StoreTarget storeTarget = StoreTarget::Texture;
			int maxThreads = 1;
			bool loadAsync = false;
		};

		PSDReader();
		explicit PSDReader(const Config& config = {});

		/// @brief ファイル読み込み時などで発生したエラー (レイヤー単体にはエラーが含まれている可能性があります)
		[[nodiscard]]
		Optional<PSDError> getCriticalError() const;

		[[nodiscard]]
		PSDObject getObject() const;

		[[nodiscard]]
		bool isReady() const noexcept;

	private:
		struct Impl;
		std::shared_ptr<Impl> p_impl;
	};
}
