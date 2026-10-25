#include "ToshiSmoke.h"

#include <Toshi/Defines.h>
#include <Toshi/Endianness.h>
#include <Toshi/THPTimer.h>
#include <Toshi/Verify.h>

#include <bit>
#include <cstdint>

bool RunToshiPortabilitySmokeTest()
{
	alignas( std::uint32_t ) constexpr std::uint8_t bytes[] = { 0x78, 0x56, 0x34, 0x12 };
	Toshi::THPTimer timer;
	timer.Update();

	return sizeof( TUINT32 ) == 4 &&
	       sizeof( TUINTPTR ) == 4 &&
	       std::endian::native == std::endian::little &&
	       CURRENT_ENDIANESS == Endianess_Little &&
	       PARSEDWORD( bytes ) == 0x12345678U &&
	       timer.GetRaw64() > 0 &&
	       timer.GetDelta() >= 0.0f;
}
