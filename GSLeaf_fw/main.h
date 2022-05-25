#pragma once

#include "color.h"

#define DIRNAME_SND_OPEN    "SndOpen"
#define DIRNAME_SND_CLOSED  "SndClosed"
#define OPEN_DURATION_S     7

enum State_t { stateClosed, stateOpen };
extern State_t State;
