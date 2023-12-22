# include <Siv3D.hpp> // Siv3D v0.6.12
# include "../SivPSD/PSDImporter.h"

using namespace SivPSD;

namespace
{
	void writePsdSummary(const PSDImporter& psdImporter, const PSDObject& psdObject, Stopwatch& sw)
	{
		sw.pause();
		Console.writeln(U"Import done: {} sec"_fmt(sw.sF()));
		Console.writeln(U"---");
		if (const auto e = psdImporter.getCriticalError())
		{
			Console.writeln(U"Importer error: " + e->what()); // 読み込み時のエラー情報
			Console.writeln(U"---");
		}
		Console.writeln(U"Layer errors: " + psdObject.concatLayerErrors()); // レイヤーに存在するエラー情報
		Console.writeln(U"---");
		Console.writeln(psdObject);
	}
}

void Main2()
{
	Window::SetTitle(U"SivPSD Test");
	Window::SetStyle(WindowStyle::Sizable);
	Scene::SetBackground(ColorF{0.3});

	Stopwatch sw{StartImmediately::Yes};

	// PSD読み込みオブジェクトを作成
	PSDImporter psdImporter{
		{
			.filepath = U"psd/miko15.psd",
			.storeTarget = StoreTarget::MipmapTexture,
			.maxThreads = 4,
			.asyncStart = true,
			.marginRemove = true,
		}
	};

	// 読み込みオブジェクト格納先
	PSDObject psdObject{};

	Camera2D camera2D{};

	bool loading = true; // 読み込み中
	bool showAll = true; // 全レイヤー表示
	double layerCursor{}; // 全レイヤー表示じゃないときに表示するレイヤーのカーソル

	while (System::Update())
	{
		if (loading)
		{
			// 読込中...
			if (not psdImporter.isReady())
			{
				// まだ読み込んでない
				SimpleGUI::Headline(U"Loading...", Vec2{0, 50});
				continue; // ループ終了
			}

			// 完了
			loading = false;
			psdObject = psdImporter.getObject();
			writePsdSummary(psdImporter, psdObject, sw);
			camera2D.jumpTo(Rect(psdObject.documentSize).topCenter().movedBy(0, psdObject.documentSize.y / 4), 0.5);
		}

		// 全レイヤー表示じゃないときに表示するレイヤーID
		const auto showingLayer =
			std::min(static_cast<size_t>(layerCursor * psdObject.layers.size()), (psdObject.layers.size() - 1));

		camera2D.update();
		{
			// PSD描画
			Transformer2D t{camera2D.createTransformer()};

			Rect(psdObject.documentSize).stretched(1).drawFrame(2, Palette::Black);

			if (showAll)
			{
				// isDrawable() が true のレイヤーをすべて描画
				psdObject.draw();
			}
			else
			{
				// レイヤー単体を描画
				psdObject.layers[showingLayer].texture.draw(psdObject.layers[showingLayer].tl());
			}
		}

		// GUI
		SimpleGUI::Headline(U"Completed!", Vec2{0, 50});
		const auto sceneTr = Rect(Scene::Size()).tr();
		SimpleGUI::CheckBox(showAll, U"Show all", sceneTr.movedBy(-300, 50));
		if (not showAll && psdObject.layers.size() > 0)
		{
			SimpleGUI::Slider(U"Show: {}"_fmt(showingLayer), layerCursor, sceneTr.movedBy(-300, 100), 100, 200);
			SimpleGUI::Headline(Format(psdObject.layers[showingLayer]), sceneTr.movedBy(-300, 150));
		}
	}
}
