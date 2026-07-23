#pragma once
#include "signal_pets_app.h"

void cyberdex_save(CyberDex* cyberdex);
void cyberdex_load(CyberDex* cyberdex);
bool cyberdex_add_pet(CyberDex* cyberdex, const Pet* pet);
void cyberdex_reset(CyberDex* cyberdex);