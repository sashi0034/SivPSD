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

		/// @brief レイヤー名
		String name{};

		/// @brief フォルダかどうか
		bool isFolder{};

		/// @brief 表示フラグ
		bool isVisible{};

		/// @brief アクセス可能画素配列 (読み込み時の設定によっては空になります)
		Image image{};

		/// @brief image から作られたテクスチャ (読み込み時の設定によっては空になります)
		DynamicTexture texture{};

		/// @brief 読み込み時などで発生したエラー
		Optional<PSDError> error{};

		/// @brief isVisible が true でテクスチャを持ったレイヤーか
		bool isDrawable() const;

		friend void Formatter(FormatData& formatData, const PSDLayer& layer);
	};

	/// @brief PSDオブジェクト情報
	struct PSDObject
	{
		Size documentSize{};
		Array<PSDLayer> layers{};

		/// @brief レイヤーに含まれているすべてのエラーを統合し文字列にして返します
		String concatLayerErrors() const;

		/// @brief レイヤーに含まれているすべてのエラーをレイヤーIDとともに配列として返します
		Array<std::pair<PSDLayer::id_type, PSDError>> getLayerErrors() const;

		/// @brief isDrawable() が true のレイヤーを描画
		const PSDObject& draw(const Vec2& pos = Vec2{}) const;

		/// @brief isDrawable() が true のレイヤーを描画
		const PSDObject& drawAt(const Vec2& pos = Vec2{}) const;

		friend void Formatter(FormatData& formatData, const PSDObject& obj);
	};
}
