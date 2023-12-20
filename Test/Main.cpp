# include <Siv3D.hpp> // Siv3D v0.6.12
# include "../SivPSD/SivPSD.h"

using namespace SivPSD;

void Main()
{
	Window::SetTitle(U"SivPSD Test");
	Window::SetStyle(WindowStyle::Sizable);
	Window::Resize(1280, 720);
	Scene::SetBackground(ColorF{0.3});

	Stopwatch sw{};
	sw.start();
	PSDLoader psdLoader{
		{
			.filepath = U"psd/miko15.psd",
			.storeTarget = StoreTarget::Texture,
			.maxThreads = 4,
			.loadAsync = true
		}
	};
	auto psdObject = psdLoader.getObject();
	sw.pause();
	Console.writeln(U"Passed: {}"_fmt(sw.sF()));

	int mode = 0;
	int textureIndex = 0;

	Camera2D camera2D{psdObject.documentSize / 2};

	for (auto&& o : psdObject.layers)
	{
		Console.writeln(Format(o));
	}

	while (System::Update())
	{
		ClearPrint();
		if (psdObject.layers.size() == 0)
		{
			if (psdLoader.isReady()) psdObject = psdLoader.getObject();
			Print(U"Waiting...");
			continue;
		}

		camera2D.update();
		{
			Transformer2D t{camera2D.createTransformer()};

			switch (mode)
			{
			case 0:
				if (MouseL.down())
				{
					textureIndex = (textureIndex + 1) % psdObject.layers.size();
				}
				(void)psdObject.layers[textureIndex].texture.draw();
				break;
			case 1:
				psdObject.draw();
				break;
			default:
				break;
			}
		}

		SimpleGUI::Headline(U"Mode: {}"_fmt(mode), Rect(Scene::Size()).tr().movedBy(-150, 50));
		SimpleGUI::Headline(U"All layers: {}"_fmt(
			                    psdObject.layers.size()), Rect(Scene::Size()).tr().movedBy(-150, 100));

		if (MouseR.down()) mode = (mode + 1) % 2;
	}
}
