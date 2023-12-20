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

	const PSDObject& PSDObject::draw(const Vec2& pos) const
	{
		for (auto&& layer : layers)
		{
			if (not layer.isVisible) continue;
			if (layer.texture.isEmpty()) continue;
			(void)layer.texture.draw(pos);
		}
		return *this;
	}

	const PSDObject& PSDObject::drawAt(const Vec2& pos) const
	{
		for (auto&& layer : layers)
		{
			if (not layer.isVisible) continue;
			if (layer.texture.isEmpty()) continue;
			(void)layer.texture.drawAt(pos);
		}
		return *this;
	}
}
