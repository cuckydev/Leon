#pragma once

#include <Leon/Leon.h>

#include <vector>

namespace MyCoolGame
{

template <int bits, typename T>
struct TemplateTest
{

};

struct LEON StupidClassToInheritFrom
{

};

struct StupidUnknownClassToInheritFrom
{

};

namespace Component
{

class LEON_KV("type", "cool") CoolComponent : public StupidClassToInheritFrom, public StupidUnknownClassToInheritFrom
{
	void LEON Test(const TemplateTest<16, const TemplateTest<32, int>> *&& bits)
	{

	}
};

}

}
