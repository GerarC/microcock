#include "probe.hpp"

namespace {

constexpr uint32_t NOT_CONSTRUCTED = 0;
constexpr uint32_t CONSTRUCTED_VALUE = 0x5C70;

volatile uint32_t constructedValue = NOT_CONSTRUCTED;

struct StartupMarker {
	StartupMarker() { constructedValue = CONSTRUCTED_VALUE; }
};

StartupMarker startupMarker;

} // namespace

namespace probe {

bool staticConstructorsRun() {
	return constructedValue == CONSTRUCTED_VALUE;
}

} // namespace probe
