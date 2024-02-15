#pragma once

#include <Leon/Leon.h>

#include <cstdint>

#include <vector>

namespace MyCoolGame
{

struct UpStruct
{
	int a;
};

typedef const int32_t MyInt;

enum LEON_KV("enum", "MyEnum") MyEnum
{
	SpongeBob = 0,
	Patrick = 1,
	Plankton = 10,
	Mario = Plankton + Patrick,
	Luigi = Mario + 1000,
	Kiryu,
	Majima,
};

struct LEON Structure
{

	struct LEON InnerStruct
	{
		int LEON b;
	};

};

inline void LEON Function(const Structure::InnerStruct &inner)
{

}

class LEON WeirdClass
{
	virtual void LEON Overrides() = 0;
};

class WeirdUnknownClass
{

};

namespace Component
{

class LEON_KV("type", "engine") AppleComponent : public WeirdClass, public WeirdUnknownClass
{
public:
	enum class LEON_KV("enum", "AppleEnum") AppleEnum
	{
		John = 0,
		Michael,
	};
	
	uint16_t LEON variable;

	static int16_t LEON static_variable;

	void LEON Overrides() override
	{

	}

	const MyInt LEON Test(const UpStruct *const &up, const Structure::InnerStruct in, MyEnum myenum, AppleEnum appleenum, int &&coretype, const volatile int *const &coretype2)
	{
		int mul = 1;
		switch (myenum)
		{
			case SpongeBob:
				mul = 100;
				break;
			case Majima:
				mul = 1000;
				break;
			default:
				break;
		}

		int div = 1;
		switch (appleenum)
		{
			case AppleEnum::John:
				div = 9;
				break;
			default:
				break;
		}

		MyInt my_cool_int = (in.b + up->a) * mul / div;
		return my_cool_int;
	}

	friend void LEON Friend()
	{

	}

	static void LEON Static()
	{

	}

	void LEON Const() const
	{

	}
};

}

}
