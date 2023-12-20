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

	constexpr uint32 invalidChannelValue = UINT_MAX;

	uint32 findChannel(const Layer* layer, int16 channelType)
	{
		for (uint32 i = 0; i < layer->channelCount; ++i)
		{
			const Channel* channel = &layer->channels[i];
			if (channel->data && channel->type == channelType)
				return i;
		}

		return invalidChannelValue;
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
		// 一応使えるけど没
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

	PSDError concatError(const Optional<PSDError>& currentError, StringView newError)
	{
		return PSDError(currentError.value_or(PSDError()).what().isEmpty()
			                ? newError
			                : currentError->what() + U"\n"_sv + newError);
	}

	Optional<int> getParentId(const Layer* layer, LayerMaskSection*& layers)
	{
		if (layer->parent == nullptr) return none;
		for (int i = 0; i < layers->layerCount; ++i)
		{
			if (layer->parent == &layers->layers[i])
			{
				return i;
			}
		}
		return none;
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

		LayerReader(Props props) : props(std::move(props))
		{
			m_canvasData.fill(Array<uint8>(props.canvasSize.x * props.canvasSize.y));
			m_colorArray = Array<Color>{props.document->width * props.document->height};
		}

		void readLayer(int index, PSDLayer& outputLayer);

	private:
		Props props;

		std::array<Array<uint8>, 4> m_canvasData{};
		Array<Color> m_colorArray{};
	};

	void LayerReader::readLayer(int index, PSDLayer& outputLayer)
	{
		Layer* layer = &props.layerMaskSection->layers[index];
		ExtractLayer(props.document, props.file, props.allocator, layer);

		// ID情報
		outputLayer.id = index;
		outputLayer.parentId = getParentId(layer, props.layerMaskSection);

		// 可視情報
		outputLayer.isVisible = layer->isVisible;

		// フォルダ情報
		if (layer->type == layerType::OPEN_FOLDER || layer->type == layerType::CLOSED_FOLDER)
		{
			outputLayer.isFolder = true;
		}
		else if (layer->type == layerType::SECTION_DIVIDER)
		{
			outputLayer.error = PSDError(U"Unsupported layer type.");
			return;
		}

		// レイヤー名取得
		std::wstringstream layerName;
		if (layer->utf16Name)
		{
			static_assert(sizeof(wchar_t) == sizeof(uint16)); //In Windows wchar_t is utf16
			layerName << reinterpret_cast<wchar_t*>(layer->utf16Name);
		}
		else
		{
			layerName << layer->name.c_str();
		}
		outputLayer.name = Unicode::FromWstring(layerName.str());

		// チャンネル取得
		const uint32 indexR = findChannel(layer, channelType::R);
		const uint32 indexG = findChannel(layer, channelType::G);
		const uint32 indexB = findChannel(layer, channelType::B);
		const uint32 indexA = findChannel(layer, channelType::TRANSPARENCY_MASK);
		if ((indexR == invalidChannelValue)
			|| (indexG == invalidChannelValue)
			|| (indexB == invalidChannelValue)
			|| (indexA == invalidChannelValue))
		{
			if (not outputLayer.isFolder) outputLayer.error = PSDError(U"Invalid RGBA channel.");
			return;
		}

		// RGB channels were found.
		expandChannelToCanvas(layer, &layer->channels[indexR], m_canvasData[0], props.canvasSize);
		expandChannelToCanvas(layer, &layer->channels[indexG], m_canvasData[1], props.canvasSize);
		expandChannelToCanvas(layer, &layer->channels[indexB], m_canvasData[2], props.canvasSize);

		m_canvasData[3].fill(0);
		expandChannelToCanvas(layer, &layer->channels[indexA], m_canvasData[3], props.canvasSize);

		if (props.document->bitsPerChannel != 8)
		{
			outputLayer.error = PSDError(U"{}-bit / channel is not supported."_fmt(props.document->bitsPerChannel));
			return;
		}

		imageUtil::InterleaveRGBA(
			m_canvasData[0].data(), m_canvasData[1].data(), m_canvasData[2].data(), m_canvasData[3].data(),
			reinterpret_cast<uint8_t*>(m_colorArray.data()),
			props.canvasSize.x, props.canvasSize.y);

		// 配列変換
		const Grid<Color> colorData{props.document->width, props.document->height, m_colorArray};
		const auto image = Image(colorData);

		if (layer->layerMask)
		{
			outputLayer.error = concatError(outputLayer.error, U"Layer mask is not supported.");
		}

		if (layer->vectorMask)
		{
			outputLayer.error = concatError(outputLayer.error, U"Vector mask is not supported.");
		}

		// 格納
		switch (props.config.storeTarget)
		{
		case StoreTarget::Image:
			outputLayer.image = image;
			break;
		case StoreTarget::Texture:
			outputLayer.texture = DynamicTexture(image);
			break;
		case StoreTarget::ImageAndTexture:
			outputLayer.image = image;
			outputLayer.texture = DynamicTexture(image);
			break;
		default: ;
		}
	}
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

		// レイヤー情報抽出
		if (LayerMaskSection* layerMaskSection = ParseLayerMaskSection(document, &file, &allocator))
		{
			extractLayers(&allocator, &file, document, layerMaskSection, canvasSize);

			DestroyLayerMaskSection(layerMaskSection, &allocator);
		}
		else
		{
			m_error = PSDError(U"Layers and masks are missing.");
		}

		DestroyDocument(document, &allocator);
		file.Close();
	}

private:
	void extractLayers(
		MallocAllocator* allocator,
		NativeFile* file,
		Document* document,
		LayerMaskSection* layerMaskSection,
		const Size& canvasSize)
	{
		m_object.layers.resize(layerMaskSection->layerCount);
		LayerReader layerReader{
			{
				.config = m_config,
				.allocator = allocator,
				.file = file,
				.document = document,
				.layerMaskSection = layerMaskSection,
				.canvasSize = canvasSize,
			}
		};

		for (uint32 i = 0; i < layerMaskSection->layerCount; ++i)
		{
			layerReader.readLayer(i, m_object.layers[i]);
		}
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
