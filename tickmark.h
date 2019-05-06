#pragma once

#include <tuple>
#include <cmath>

struct tickmrk
{
	float min;
	float max;
	float step;
	int decimals;
};

tickmrk get_tickmark(float min, float max, int max_ticks = 5)
{
	auto nice_number = [](float range, bool round) {
		float niceFraction = 1.0f;
		float exponent = std::floor(std::log10(range));
		float fraction = range / static_cast<float>(std::pow(10, exponent));

		if (round) {
			if (fraction < 1.5f)
				niceFraction = 1.0f;
			else if (fraction < 3.0f)
				niceFraction = 2.0f;
			else if (fraction < 7.0f)
				niceFraction = 5.0f;
			else
				niceFraction = 10.0f;
		}
		else {
			if (fraction <= 1.0f)
				niceFraction = 1.0f;
			else if (fraction <= 2.0f)
				niceFraction = 2.0f;
			else if (fraction <= 5.0f)
				niceFraction = 5.0f;
			else
				niceFraction = 10.0f;
		}
		return std::make_pair(
			static_cast<float>(niceFraction * std::pow(10, exponent)),
			static_cast<int>(exponent));
	};

	float range = nice_number(max - min, false).first;
	auto tickSpacing = nice_number(range / (max_ticks - 1), true);
	float niceMin = std::floor(min / tickSpacing.first) * tickSpacing.first;
	float niceMax = std::ceil(max / tickSpacing.first) * tickSpacing.first;

	return tickmrk{ niceMin, niceMax, tickSpacing.first, std::max(-tickSpacing.second, 0) };
}