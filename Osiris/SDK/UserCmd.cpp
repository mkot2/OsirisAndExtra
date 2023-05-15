#include "../Memory.h"

#include "Entity.h"
#include "UserCmd.h"

int getMaxUserCmdProcessTicks() noexcept
{
	if (const auto gameRules = (*memory->gameRules); gameRules)
		return (gameRules->isValveDS()) ? 8 : 21;
	return 21;
}