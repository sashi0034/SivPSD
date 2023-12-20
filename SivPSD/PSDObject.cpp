#include "stdafx.h"
#include "PSDObject.h"

namespace SivPSD
{
	StringView PSDError::type() const noexcept
	{
		return U"PSDError"_sv;
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
