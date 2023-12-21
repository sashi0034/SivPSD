#include "stdafx.h"
#include "PSDObject.h"

namespace SivPSD
{
	StringView PSDError::type() const noexcept
	{
		return U"PSDError"_sv;
	}

	void Formatter(FormatData& formatData, const PSDLayer& layer)
	{
		const auto layerFolder = layer.isFolder ? U"Folder"_sv : U"Layer"_sv;
		if (layer.parentId.has_value())
		{
			formatData.string += U"({} {}->{}){}"_fmt(
				layerFolder,
				layer.id,
				layer.parentId.value(),
				layer.name);
		}
		else
		{
			formatData.string += U"({} {}){}"_fmt(
				layerFolder,
				layer.id,
				layer.name);
		}
	}

	void Formatter(FormatData& formatData, const PSDObject& obj)
	{
		bool head = true;
		for (auto&& layer : obj.layers)
		{
			if (not head) formatData.string += U'\n';
			head = false;
			formatData.string += Format(layer);
		}
	}

	bool PSDLayer::isDrawable() const
	{
		return isVisible && not texture.isEmpty() && not isFolder;
	}

	String PSDObject::concatLayerErrors() const
	{
		String error{};
		for (auto&& layer : layers)
		{
			if (const auto e = layer.error)
			{
				if (not error.empty()) error += U'\n';
				error += Format(layer) + U" \""_sv + e->what() + U'\"';
			}
		}
		return error;
	}

	Array<std::pair<PSDLayer::id_type, PSDError>> PSDObject::getLayerErrors() const
	{
		Array<std::pair<PSDLayer::id_type, PSDError>> errors{};
		for (auto&& layer : layers)
		{
			if (const auto e = layer.error) errors.push_back({layer.id, e.value()});
		}
		return errors;
	}

	const PSDObject& PSDObject::draw(const Vec2& pos) const
	{
		for (auto&& layer : layers)
		{
			if (layer.isDrawable()) (void)layer.texture.draw(pos);
		}
		return *this;
	}

	const PSDObject& PSDObject::drawAt(const Vec2& pos) const
	{
		for (auto&& layer : layers)
		{
			if (layer.isDrawable()) (void)layer.texture.drawAt(pos);
		}
		return *this;
	}
}
