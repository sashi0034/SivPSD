#include "stdafx.h"
#include "PSDImporter.h"

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

	// スレッドごとに作成
	class LayerImporter
	{
	public:
		struct Props
		{
			PSDImporter::Config config;
			MallocAllocator* allocator;
			NativeFile* file;
			Document* document;
			LayerMaskSection* layerMaskSection;
			Size canvasSize;
		};

		LayerImporter(Props props) : props(std::move(props))
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

	void LayerImporter::readLayer(int index, PSDLayer& outputLayer)
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

struct PSDImporter::Impl
{
	Config m_config{};
	PSDError m_error{};
	PSDObject m_object{};
	bool m_ready{};
	Array<AsyncTask<void>> m_layerTasks{};
	AsyncTask<void> m_importTask{};
	std::atomic<int> m_nextLayer{};

	void import()
	{
		if (m_config.importAsync)
		{
			m_importTask = Async([this]()
			{
				importInternal();
			});
		}
		else
		{
			importInternal();
		}
	}

private:
	void importInternal()
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

	void extractLayers(
		MallocAllocator* allocator,
		NativeFile* file,
		Document* document,
		LayerMaskSection* layerMaskSection,
		const Size& canvasSize)
	{
		const int layerCount = layerMaskSection->layerCount;
		m_object.layers.resize(layerCount);

		// スレッドごとにレイヤー処理
		for (int id = 0; id < std::min(m_config.maxThreads, layerCount); ++id)
		{
			m_layerTasks.emplace_back(Async(
				[this, allocator, file, document, layerMaskSection, canvasSize, id]()
				{
					extractLayersAsync(allocator, file, document, layerMaskSection, canvasSize, m_nextLayer, id);
				}));
		}

		// 終了チェック
		for (auto&& t : m_layerTasks) t.wait();
		m_ready = true;
	}

	void extractLayersAsync(
		MallocAllocator* allocator,
		NativeFile* file,
		Document* document,
		LayerMaskSection* layerMaskSection,
		const Size& canvasSize,
		std::atomic<int>& nextLayer,
		int threadId)
	{
		// Stopwatch sw{};
		// sw.start();
		// Console.writeln(U"Thread {} start"_fmt(threadId));
		LayerImporter layerReader{
			{
				.config = m_config,
				.allocator = allocator,
				.file = file,
				.document = document,
				.layerMaskSection = layerMaskSection,
				.canvasSize = canvasSize,
			}
		};

		while (true)
		{
			const int nextIndex = nextLayer.fetch_add(1);
			if (nextIndex >= m_object.layers.size()) break;
			layerReader.readLayer(nextIndex, m_object.layers[nextIndex]);
		}
		// Console.writeln(U"Thread {}: {}"_fmt(threadId, sw.sF()));
	}
};

namespace SivPSD
{
	PSDImporter::PSDImporter() :
		p_impl(std::make_shared<Impl>())
	{
	}

	PSDImporter::PSDImporter(const Config& config) :
		p_impl(std::make_shared<Impl>())
	{
		p_impl->m_config = config;
		p_impl->import();
	}

	Optional<PSDError> PSDImporter::getCriticalError() const
	{
		return p_impl->m_error;
	}

	PSDObject PSDImporter::getObject() const
	{
		return p_impl->m_ready
			       ? p_impl->m_object
			       : PSDObject{};
	}

	bool PSDImporter::isReady() const noexcept
	{
		return p_impl->m_ready;
	}
}
