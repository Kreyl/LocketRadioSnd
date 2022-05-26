#include "hal.h"
#include "MsgQ.h"
#include "kl_i2c.h"
#include "Sequences.h"
#include "shell.h"
#include "uart.h"
#include "led.h"
#include "CS42L52.h"
#include "kl_sd.h"
#include "AuPlayer.h"
#include "acc_mma8452.h"
#include "kl_fs_utils.h"
#include "radio_lvl1.h"
#include "SimpleSensors.h"
#include "main.h"

#if 1 // ======================== Variables and defines ========================
// Forever
bool OsIsInitialized = false;
EvtMsgQ_t<EvtMsg_t, MAIN_EVT_Q_LEN> EvtQMain;
static const UartParams_t CmdUartParams(115200, CMD_UART_PARAMS);
CmdUart_t Uart{CmdUartParams};
void OnCmd(Shell_t *PShell);
void ITask();

//static void Standby();
static void Resume();
bool IsStandby = true;

#define PAUSE_BEFORE_REPEAT_S       7

LedRGB_t Led { LED_RED_CH, LED_GREEN_CH, LED_BLUE_CH };
PinOutput_t PwrEn(PWR_EN_PIN);
CS42L52_t Codec;
State_t State = stateClosed;

DirList_t DirList;
//static char FName[MAX_NAME_LEN];
#endif

int main(void) {
    // ==== Setup clock frequency ====
    Clk.SetVoltageRange(mvrHiPerf);
    Clk.SetupFlashLatency(24, mvrHiPerf);
    Clk.EnablePrefetch();
    if(Clk.EnableHSE() == retvOk) {
        Clk.SetupPllSrc(pllsrcHse);
        Clk.SetupM(3);
    }
    else { // PLL fed by MSI
        Clk.SetupPllSrc(pllsrcMsi);
        Clk.SetupM(1);
    }
    // SysClock = Rout = 24MHz, 48MHz = POut
    Clk.SetupPll(24, 4, 2); // 4 * 24 = 96, 96/4=24, 96/2=48
    Clk.SetupBusDividers(ahbDiv1, apbDiv1, apbDiv1);
    if(Clk.EnablePLL() == retvOk) {
        Clk.EnablePllROut();
        Clk.SwitchToPLL();
        // Setup PLLQ as 48MHz clock for USB and SDIO
        Clk.EnablePllQOut();
        Clk.Select48MHzClkSrc(src48PllQ);
        // SAI clock
        Clk.EnablePllPOut();
        Clk.SelectSAI1Clk(srcSaiPllP);
    }
    Clk.UpdateFreqValues();

    // Init OS
    halInit();
    chSysInit();
    // ==== Init hardware ====
    EvtQMain.Init();
    Uart.Init();
    Printf("\r%S %S\r\n", APP_NAME, XSTRINGIFY(BUILD_TIME));
    Clk.PrintFreqs();

    Led.Init();
//    Led.SetColor(clRed);

    SD.Init();

    AuPlayer.Init();
    PwrEn.Init();
    PwrEn.SetLo();
    chThdSleepMilliseconds(18);
    AU_i2c.Init();

    Codec.Init();
    Codec.SetSpeakerVolume(-96);    // To remove speaker pop at power on
    Codec.DisableHeadphones();
    Codec.EnableSpeakerMono();
    Codec.SetupMonoStereo(Stereo); // Always
    Codec.Standby();
    Codec.SetSpeakerVolume(0);
    Codec.SetMasterVolume(9);
    Codec.Resume();

//    Resume();

    if(Radio.Init() != retvOk) {
        Led.StartOrRestart(lsqFailure);
        chThdSleepSeconds(3600);
    }

    if(SD.IsReady) {
        Led.StartOrRestart(lsqStart);
//            UsbMsd.Init();
        AuPlayer.Play("alive.wav", spmSingle);
    } // if SD is ready
    else {
        Led.StartOrRestart(lsqFailure);
        chThdSleepMilliseconds(3600);
//            EnterSleep();
    }

//    AuPlayer.Play("alive.wav", spmSingle);

//    Led.StartOrRestart(lsqIdle);
    SimpleSensors::Init();

    // Main cycle
    ITask();
}

__noreturn
void ITask() {
    while(true) {
        EvtMsg_t Msg = EvtQMain.Fetch(TIME_INFINITE);
        switch(Msg.ID) {
            case evtIdShellCmdRcvd:
                while(((CmdUart_t*)Msg.Ptr)->TryParseRxBuff() == retvOk) OnCmd((Shell_t*)((CmdUart_t*)Msg.Ptr));
                break;

            case evtIdOnRadioRx: {
//                int32_t rxID = Msg.Value;
//                if(IdPlayingNow == ID_SURROUND) {
//                    if(RxTable.EnoughTimePassed(rxID)) {
//                        // Fadeout surround and play rcvd id
//                        IdPlayNext = rxID;
//                        Player.FadeOut();
//                    }
//                }
//                else { // Playing some ID
//                    if(rxID != IdPlayingNow) {  // Some new ID received
//                        // Switch to new ID if current one is offline for enough time
//                        if(RxTable.EnoughTimePassed(IdPlayingNow)) {
//                            // Fadeout current and play rcvd id
//                            IdPlayNext = rxID;
//                            Player.FadeOut();
//                        }
//                    }
//                }
//                // Put timestamp to table
//                RxTable.Put(rxID);
            } break;

//            case evtIdAcc:
//                if(State == stIdle) {
//                    Printf("AccWhenIdle\r");
//                    Led.StartOrRestart(lsqAccIdle);
//                    State = stPlaying;
//                    Audio.Resume();
//                    Player.PlayRandomFileFromDir("Sounds");
////                    Player.Play("Alive.wav");
//                }
//                else if(State == stWaiting) {
//                    Printf("AccWhenW\r");
//                    Led.StartOrRestart(lsqAccWaiting);
//                    tmrPauseAfter.StartOrRestart();
//                }
//                break;

#if 0 // ==== Logic ====
            case evtIdSns:
                Printf("Sns, %u\r", State);
                if(State == stateClosed) {
                    if(DirList.GetRandomFnameFromDir(DIRNAME_SND_CLOSED, FName) == retvOk) {
                        Codec.Resume();
                        Codec.SetMasterVolume(9);
                        Player.Play(FName, spmSingle);
                    }
                    Led.StartOrRestart(lsqClosed);
                }
                else {
                    if(DirList.GetRandomFnameFromDir(DIRNAME_SND_OPEN, FName) == retvOk) {
                        Codec.Resume();
                        Player.Play(FName, spmSingle);
                    }
                    // Close the door
                    tmrOpen.Stop();
                    State = stateClosed;
                }
                break;

            case evtIdDoorIsClosing:
                Printf("Closing\r");
                State = stateClosed;
                break;

            case evtIdOpen:
                Printf("Open\r");
                State = stateOpen;
                tmrOpen.StartOrRestart();
                break;
#endif

            case evtIdAudioPlayStop: {
                Printf("PlayEnd\r");
//                Codec.Standby();
//                IdPlayingNow = IdPlayNext;
//                IdPlayNext = ID_SURROUND;
//                // Decide what to play: surround or some id
//                if(IdPlayingNow == ID_SURROUND) strcpy(DirName, DIRNAME_SURROUND);
//                else itoa(IdPlayingNow, DirName, 10);
////                Printf("Play %S\r", DirName);
//                Player.PlayRandomFileFromDir(DirName);
            } break;

            case evtIdButtons:
//                Printf("Btn %u\r", Msg.BtnEvtInfo.BtnID);
//                if(Msg.BtnEvtInfo.BtnID == 0) Audio.VolumeUp();
//                else Audio.VolumeDown();
                break;

            default: break;
        } // switch
    } // while true
}

void Resume() {
    if(!IsStandby) return;
    Printf("Resume\r");
    // Clock
//    Clk.SetCoreClk(cclk48MHz);
//    Clk.SetupSai1Qas48MhzSrc();
//    Clk.UpdateFreqValues();
//    Clk.PrintFreqs();
    // Sound
    Codec.Init();
    Codec.SetSpeakerVolume(-96);    // To remove speaker pop at power on
    Codec.DisableHeadphones();
    Codec.EnableSpeakerMono();
    Codec.SetupMonoStereo(Stereo);  // For wav player
    Codec.SetupSampleRate(22050); // Just default, will be replaced when changed
    Codec.SetMasterVolume(9); // 12 is max
    Codec.SetSpeakerVolume(0); // 0 is max

    IsStandby = false;
}

/*
void Standby() {
    Printf("Standby\r");
    // Sound
    Codec.Deinit();
    // Clock
    Clk.SwitchToMSI();
//    Clk.DisablePLL();
//    Clk.DisableSai1();

    Clk.UpdateFreqValues();
    Clk.PrintFreqs();
    IsStandby = true;
}
*/

void ProcessSns(PinSnsState_t *PState, uint32_t Len) {
//    if(*PState == pssRising) {
//        EvtQMain.SendNowOrExit(EvtMsg_t(evtIdSns));
//    }
//    Printf("st: %u\r", *PState);
}

//void ProcessChargePin(PinSnsState_t *PState, uint32_t Len) {
//    if(*PState == pssFalling) { // Charge started
//        Led.StartOrContinue(lsqCharging);
//        Printf("Charge started\r");
//    }
//    if(*PState == pssRising) { // Charge ended
////        Led.StartOrContinue(lsqOperational);
//        Printf("Charge ended\r");
//    }
//}

#if 1 // ======================= Command processing ============================
void OnCmd(Shell_t *PShell) {
	Cmd_t *PCmd = &PShell->Cmd;
    // Handle command
    if(PCmd->NameIs("Ping")) PShell->Ok();
    else if(PCmd->NameIs("Version")) PShell->Print("%S %S\r", APP_NAME, XSTRINGIFY(BUILD_TIME));
    else if(PCmd->NameIs("mem")) PrintMemoryInfo();

//    else if(PCmd->NameIs("V")) {
//        int8_t v;
//        if(PCmd->GetNext<int8_t>(&v) != retvOk) { PShell->Ack(retvCmdError); return; }
//        Audio.SetMasterVolume(v);
//    }
//    else if(PCmd->NameIs("SV")) {
//        int8_t v;
//        if(PCmd->GetNext<int8_t>(&v) != retvOk) { PShell->Ack(retvCmdError); return; }
//        Audio.SetSpeakerVolume(v);
//    }

//    else if(PCmd->NameIs("A")) Player.Play("Alive.wav");
//    else if(PCmd->NameIs("48")) {
//        Audio.Resume();
//        Player.Play("Mocart48.wav");
//    }
//    else if(PCmd->NameIs("FO")) Player.FadeOut();


    else PShell->CmdUnknown();
}
#endif
