#include "stdafx.h"
#include "PSDReader.h"

#include "Psd/Psd.h"
#include "Psd/PsdPlatform.h"
#include "Psd/PsdMallocAllocator.h"
#include "Psd/PsdNativeFile.h"
#include "Psd/PsdDocument.h"
#include "Psd/PsdColorMode.h"
#include "Psd/PsdLayer.h"
#include "Psd/PsdChannel.h"
#include "Psd/PsdChannelType.h"
#include "Psd/PsdLayerMask.h"
#include "Psd/PsdVectorMask.h"
#include "Psd/PsdLayerMaskSection.h"
#include "Psd/PsdParseDocument.h"
#include "Psd/PsdParseLayerMaskSection.h"
#include "Psd/PsdLayerCanvasCopy.h"
#include "Psd/PsdInterleave.h"
#include "Psd/PsdExportDocument.h"
#include "Psd/PsdLayerType.h"

namespace
{
	using namespace SivPSD;
	using namespace psd;

	constexpr uint32 CHANNEL_NOT_FOUND = UINT_MAX;

	uint32 findChannel(const Layer* layer, int16 channelType)
	{
		for (uint32 i = 0; i < layer->channelCount; ++i)
		{
			const Channel* channel = &layer->channels[i];
			if (channel->data && channel->type == channelType)
				return i;
		}

		return CHANNEL_NOT_FOUND;
	}

	void expandChannelToCanvas(
		const Layer* layer, const Channel* channel, Array<uint8>& canvasData, Size canvasSize)
	{
		imageUtil::CopyLayerData(
			static_cast<uint8*>(channel->data),
			canvasData.data(),
			layer->left, layer->top, layer->right, layer->bottom,
			canvasSize.x, canvasSize.y);
	}

	void applyMask(Rect maskRect, Size imageSize, const uint8_t* mask, uint8_t* dest)
	{
		for (int32 y = 0u; y < imageSize.y; ++y)
		{
			for (int32 x = 0u; x < imageSize.x; ++x)
			{
				const Point maskPoint = Point(x, y) - maskRect.tl();
				const int destIndex = (y * imageSize.x + x) * 4u + 3u;

				if (InRange(maskPoint.x, 0, maskRect.w - 1)
					&& InRange(maskPoint.y, 0, maskRect.h - 1))
				{
					const double maskRate = mask[maskPoint.y * maskRect.w + maskPoint.x] / 255.0;
					dest[destIndex] = static_cast<uint8>(dest[destIndex] * maskRate);
				}
				else
				{
					dest[destIndex] = 0;
				}
			}
		}
	}

	class LayerReader
	{
	public:
		struct Props
		{
			PSDReader::Config config;
			MallocAllocator* allocator;
			NativeFile* file;
			Document* document;
			LayerMaskSection* layerMaskSection;
			Size canvasSize;
		};

		LayerReader(const Props& props) : props(props)
		{
			m_canvasData.fill(Array<uint8>(props.canvasSize.x * props.canvasSize.y));
			m_colorArray = Array<Color>{props.document->width * props.document->height};
		}

		void readLayer(int index, PSDLayer& outputLayer)
		{
			Layer* layer = &props.layerMaskSection->layers[index];
			ExtractLayer(props.document, props.file, props.allocator, layer);

			outputLayer.isVisible = layer->isVisible;

			if (layer->type != layerType::ANY)
			{
				Console.writeln(U"Type: {}"_fmt(layer->type));
			}

			// check availability of R, G, B, and A channels.
			// we need to determine the indices of channels individually, because there is no guarantee that R is the first channel,
			// G is the second, B is the third, and so on.
			const uint32 indexR = findChannel(layer, channelType::R);
			const uint32 indexG = findChannel(layer, channelType::G);
			const uint32 indexB = findChannel(layer, channelType::B);
			const uint32 indexA = findChannel(layer, channelType::TRANSPARENCY_MASK);
			if (indexA == CHANNEL_NOT_FOUND)
			{
				Console.writeln(U"Missing alpha");
				return;
			}

			if ((indexR != CHANNEL_NOT_FOUND) && (indexG != CHANNEL_NOT_FOUND) && (indexB != CHANNEL_NOT_FOUND))
			{
				// RGB channels were found.
				expandChannelToCanvas(layer, &layer->channels[indexR], m_canvasData[0], props.canvasSize);
				expandChannelToCanvas(layer, &layer->channels[indexG], m_canvasData[1], props.canvasSize);
				expandChannelToCanvas(layer, &layer->channels[indexB], m_canvasData[2], props.canvasSize);

				m_canvasData[3].fill(0);
				expandChannelToCanvas(layer, &layer->channels[indexA], m_canvasData[3], props.canvasSize);
			}

			if (props.document->bitsPerChannel != 8)
			{
				Console.writeln(U"{}-BPC is not supported."_fmt(props.document->bitsPerChannel));
				return;
			}

			imageUtil::InterleaveRGBA(
				m_canvasData[0].data(), m_canvasData[1].data(), m_canvasData[2].data(), m_canvasData[3].data(),
				reinterpret_cast<uint8_t*>(m_colorArray.data()),
				props.canvasSize.x, props.canvasSize.y);

			// get the layer name.
			// Unicode data is preferred because it is not truncated by Photoshop, but unfortunately it is optional.
			// fall back to the ASCII name in case no Unicode name was found.
			std::wstringstream layerName;
			if (layer->utf16Name)
			{
				//In Windows wchar_t is utf16
				PSD_STATIC_ASSERT(sizeof(wchar_t) == sizeof(uint16));
				layerName << reinterpret_cast<wchar_t*>(layer->utf16Name);
			}
			else
			{
				layerName << layer->name.c_str();
			}
			// at this point, image8, image16 or image32 store either a 8-bit, 16-bit, or 32-bit image, respectively.
			// the image data is stored in interleaved RGB or RGBA, and has the size "document->width*document->height".
			// it is up to you to do whatever you want with the image data. in the sample, we simply write the image to a .TGA file.
			const String layerNameU32 = (Unicode::FromWstring(layerName.str()));
			Print(layerNameU32);

			// RGBA
			const Grid<Color> colorData{props.document->width, props.document->height, m_colorArray};
			auto image = Image(colorData);

			// in addition to the layer data, we also want to extract the user and/or vector mask.
			// luckily, this has been handled already by the ExtractLayer() function. we just need to check whether a mask exists.
			if (layer->layerMask)
			{
				const int32 maskW = layer->layerMask->right - layer->layerMask->left;
				const int32 maskH = layer->layerMask->bottom - layer->layerMask->top;
				const Rect maskRect{layer->layerMask->left, layer->layerMask->top, maskW, maskH};

				Print(U"Mask: " + Unicode::FromWstring(layerName.str()));
				const void* maskData = layer->layerMask->data;
				applyMask(
					maskRect,
					{props.document->width, props.document->height},
					static_cast<const uint8_t*>(maskData),
					image.dataAsUint8());
			}

			if (layer->vectorMask)
			{
				Print(U"Vector Mask: " + Unicode::FromWstring(layerName.str()));
				Console.writeln(U"Vector mask is not supported.");
			}

			outputLayer.texture = DynamicTexture(image);
		}

	private:
		Props props;

		std::array<Array<uint8>, 4> m_canvasData{};
		Array<Color> m_colorArray{};
	};
}

struct PSDReader::Impl
{
	Config m_config{};
	PSDError m_error{};
	PSDObject m_object{};

	void read()
	{
		const std::wstring srcPath = Unicode::ToWstring(m_config.filepath);

		MallocAllocator allocator;
		NativeFile file(&allocator);

		if (not file.OpenRead(srcPath.c_str()))
		{
			m_error = PSDError(U"Cannot open file.");
			return;
		}

		Document* document = CreateDocument(&file, &allocator);
		if (not document)
		{
			m_error = PSDError(U"Cannot create document.");
			file.Close();
			return;
		}

		if (document->colorMode != colorMode::RGB)
		{
			m_error = PSDError(U"Document is not in RGB color mode.");
			DestroyDocument(document, &allocator);
			file.Close();
			return;
		}

		const Size canvasSize{document->width, document->height};
		std::array<Array<uint8>, 4> canvasData{};
		canvasData.fill(Array<uint8>(canvasSize.x * canvasSize.y));
		Array<Color> colorArray{document->width * document->height};

		// Extract all layers and masks.
		if (LayerMaskSection* layerMaskSection = ParseLayerMaskSection(document, &file, &allocator))
		{
			m_object.layers.resize(layerMaskSection->layerCount);
			LayerReader layerReader{
				{
					.config = m_config,
					.allocator = &allocator,
					.file = &file,
					.document = document,
					.layerMaskSection = layerMaskSection,
					.canvasSize = canvasSize,
				}
			};

			for (uint32 i = 0; i < layerMaskSection->layerCount; ++i)
			{
				layerReader.readLayer(i, m_object.layers[i]);
			}

			DestroyLayerMaskSection(layerMaskSection, &allocator);
		}
		else
		{
			m_error = PSDError(U"Layers and masks are missing.");
		}

		DestroyDocument(document, &allocator);
		file.Close();
	}
};

namespace SivPSD
{
	PSDReader::PSDReader() :
		p_impl(std::make_shared<Impl>())
	{
	}

	PSDReader::PSDReader(const Config& config) :
		p_impl(std::make_shared<Impl>())
	{
		p_impl->m_config = config;
		p_impl->read();
	}

	Optional<PSDError> PSDReader::getCriticalError() const
	{
		return p_impl->m_error;
	}

	const PSDObject& PSDReader::getObject() const
	{
		return p_impl->m_object;
	}
}
