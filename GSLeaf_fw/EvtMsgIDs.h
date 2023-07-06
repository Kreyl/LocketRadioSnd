#ifndef EVTMSGIDS_H__
#define EVTMSGIDS_H__

enum EvtMsgId_t {
    evtIdNone = 0, // Always

    // Pretending to eternity
    evtIdShellCmdRcvd,
    evtIdEverySecond,

    // Usb
    evtIdUsbConnect,
    evtIdUsbDisconnect,
    evtIdUsbReady,
    evtIdUsbNewCmd,
    evtIdUsbInDone,
    evtIdUsbOutDone,

    // Misc periph
    evtIdButtons,
    evtIdPwrOffTimeout,
    evtIdOnRadioRx,

    evtIdMotion,
    evtIdStable,
    evtIdPauseRestart,

    // Audio
    evtIdAudioPlayStop,
};

#endif //EVTMSGIDS_H__
