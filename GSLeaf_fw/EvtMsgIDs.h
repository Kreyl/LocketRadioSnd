#pragma once

enum EvtMsgId_t {
    evtIdNone = 0, // Always

    // Pretending to eternity
    evtIdShellCmd,
    evtIdEverySecond,

    evtIdButtons,
    evtIdAcc,
    evtIdOnRx,

    evtIdSoundPlayStop,

    evtIdSns,
    evtIdOpen,
    evtIdDoorIsClosing,
};
