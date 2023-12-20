# include <Siv3D.hpp> // Siv3D v0.6.12
# include "../SivPSD/SivPSD.h"

using namespace SivPSD;

void Main()
{
	Scene::SetBackground(ColorF{0.3});

	Print(SampleValue());

	while (System::Update())
	{
	}
}
