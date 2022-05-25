/*
 * SnsPins.h
 *
 *  Created on: 17 џэт. 2015 у.
 *      Author: Kreyl
 */

/* ================ Documentation =================
 * There are several (may be 1) groups of sensors (say, buttons and USB connection).
 *
 */

#pragma once

#include "SimpleSensors.h"

#ifndef SIMPLESENSORS_ENABLED
#define SIMPLESENSORS_ENABLED   FALSE
#endif

#if SIMPLESENSORS_ENABLED
#define SNS_POLL_PERIOD_MS      72

// Handlers
//extern void ProcessButtons(PinSnsState_t *PState, uint32_t Len);
//extern void ProcessChargePin(PinSnsState_t *PState, uint32_t Len);
extern void ProcessSns(PinSnsState_t *PState, uint32_t Len);

const PinSns_t PinSns[] = {
        // Buttons
        {BTN1_PIN, ProcessSns},
//        {BTN2_PIN, ProcessButtons},
        // Charge pin
//        {CHARGE_PIN, ProcessChargePin},
};
#define PIN_SNS_CNT     countof(PinSns)

#endif  // if enabled
