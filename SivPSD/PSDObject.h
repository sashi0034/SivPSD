#pragma once

namespace SivPSD
{
	class PSDError final : public Error
	{
	public:
		using Error::Error;

		[[nodiscard]]
		StringView type() const noexcept override;
	};

	/// @brief PSDレイヤー情報
	struct PSDLayer
	{
		using id_type = int32;

		/// @brief レイヤーID (最上レイヤーが0)
		id_type id{};

		/// @brief 親レイヤーID
		Optional<id_type> parentId{};

		/// @brief 表示フラグ
		bool isVisible{};

		/// @brief アクセス可能画素配列
		Image image{};

		/// @brief image から作られたテクスチャ
		DynamicTexture texture{};

		/// @brief 読み込み時などで発生したエラー
		Optional<PSDError> error{};
	};

	/// @brief PSDオブジェクト情報
	struct PSDObject
	{
		Size documentSize{};
		Array<PSDLayer> layers{};

		const PSDObject& draw(const Vec2& pos = Vec2{}) const;
		const PSDObject& drawAt(const Vec2& pos = Vec2{}) const;
	};
}
