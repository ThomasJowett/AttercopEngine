#pragma once

#include "Migration.h"

namespace Migration
{
	// The current FormatVersion of each type. When raising one, replace that type's step with the one from the
	// previous format.
	constexpr int k_SceneVersion = 2;
	constexpr int k_StateMachineVersion = 1;

	// The step from each type's previous format, or none while it's at 1
	std::vector<Step> SceneSteps();
	std::vector<Step> StateMachineSteps();
}
