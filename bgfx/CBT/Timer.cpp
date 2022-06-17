#include "Timer.h"
#include <common.h>

float Timer::DeltaTime()
{
	int64_t now = bx::getHPCounter();
	static int64_t prev = now;
	const int64_t frame = now - prev;
	prev = now;
	const auto freq = static_cast<double>(bx::getHPFrequency());
	return static_cast<float>(static_cast<double>(frame) / freq);
}
