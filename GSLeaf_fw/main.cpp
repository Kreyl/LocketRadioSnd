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
#include "ini_kl.h"
#include "radio_lvl1.h"

#if 1 // ======================== Variables and defines ========================
// Forever
bool OsIsInitialized = false;
EvtMsgQ_t<EvtMsg_t, MAIN_EVT_Q_LEN> EvtQMain;
static const UartParams_t CmdUartParams(115200, CMD_UART_PARAMS);
CmdUart_t Uart{CmdUartParams};
void OnCmd(Shell_t *PShell);
void ITask();

static void Standby();
static void Resume();
bool IsStandby = true;

LedRGB_t Led { LED_RED_CH, LED_GREEN_CH, LED_BLUE_CH }; // Red and Green are used here to control mono LEDs
PinOutput_t PwrEn(PWR_EN_PIN);
CS42L52_t Codec;
static int32_t Volume = 9;
int32_t IdPlayingNow = -1;

TmrKL_t TmrPauseRestart {TIME_S2I(2), evtIdPauseRestart, tktOneShot};
#endif

int main(void) {
    // ==== Setup clock frequency ====
    Clk.SetVoltageRange(mvrHiPerf);
    Clk.SetupFlashLatency(24, mvrHiPerf);
    Clk.EnablePrefetch();
    Clk.SetupPllSrc(pllsrcMsi);
    Clk.SetupM(1);
    // Prepare everything, but do not switch to PLL
    // SysClock = Rout = 24MHz, 48MHz = POut
    Clk.SetupPll(24, 4, 2); // 4 * 24 = 96, 96/4=24, 96/2=48
    Clk.EnablePllROut();
    // Setup PLLQ as 48MHz clock for USB and SDIO
    Clk.EnablePllQOut();
    Clk.Select48MHzClkSrc(src48PllQ);
    // SAI clock = PLLP
    Clk.EnablePllPOut();
    Clk.SelectSAI1Clk(srcSaiPllP);
    Clk.SetupBusDividers(ahbDiv1, apbDiv1, apbDiv1);
    if(Clk.EnablePLL() == retvOk) Clk.SwitchToPLL();
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
    PwrEn.Init();
    PwrEn.SetLo();
    chThdSleepMilliseconds(18);

    AU_i2c.Init();

    Codec.Init();
    Codec.SetSpeakerVolume(-96);    // To remove speaker pop at power on
    Codec.DisableHeadphones();
    Codec.EnableSpeakerMono();
    Codec.SetupMonoStereo(Stereo);  // For wav player
    Codec.SetupSampleRate(22050); // Just default, will be replaced when changed
    chThdSleepMilliseconds(45);
    Codec.SetMasterVolume(9); // 12 is max
    Codec.SetSpeakerVolume(0); // 0 is max

    AuPlayer.Init();
    Resume(); // To generate 48MHz for SD
    SD.Init();
    // Seed pseudorandom generator with truly random seed
    Random::TrueInit();
    Random::SeedWithTrue();
    Random::TrueDeinit();

    if(Radio.Init() != retvOk) {
        Led.StartOrRestart(lsqFailure);
        chThdSleepSeconds(3600);
    }

    if(SD.IsReady) {
#if 1 // Read config
        int32_t tmp;
        if(ini::ReadInt32("Settings.ini", "Common", "Volume", &tmp) == retvOk) {
            if(tmp >= 0 and tmp <= 100) {
                // 0...100 => -38...12
                Volume = (tmp / 2) - 38;
                Printf("Volume: %d -> %d\r", tmp, Volume);
            }
        }
//        if(ini::ReadInt32("Settings.ini", "Common", "Threshold", &tmp) == retvOk) {
//            if(tmp > 0) {
//                Acc.ThresholdStable = tmp;
//                Printf("ThresholdStable: %d\r", tmp);
//            }
//        }
//        if(ini::ReadInt32("Settings.ini", "Common", "Delay", &tmp) == retvOk) {
//            if(tmp > 0) {
//                DelayBeforeNextPlay_s = tmp;
//                Printf("DelayBeforeNextPlay_s: %d\r", DelayBeforeNextPlay_s);
//            }
//        }
#endif
        Codec.SetSpeakerVolume(0);
        Codec.SetMasterVolume(Volume);
        AuPlayer.Play("alive.wav", spmSingle);
        Led.StartOrRestart(lsqStart);
    } // if SD is ready
    else {
        Led.StartOrRestart(lsqFailure);
        chThdSleepMilliseconds(3600);
//        EnterSleep();
    }

#if ACC_REQUIRED
    Acc.Init();
#endif

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

            case evtIdOnRadioRx:
                Printf("RxID: %u\r", Msg.Value);
                if(Msg.Value == 0) { // Btn0 pressed => play
                    if(!TmrPauseRestart.IsRunning()) { // Do not react to a button that is pressed too quickly
                        TmrPauseRestart.StartOrRestart();
                        // Generate new filename
                        int32_t N;
                        do { // Do not repeat what is playing
                            N = Random::Generate(1, 3);
                        } while(N == IdPlayingNow);
                        IdPlayingNow = N;
                        // Play what selected
                        Resume();
                        switch(N) {
                            case 1:
                                Led.StartOrRestart(lsqOn);
                                AuPlayer.Play("1.wav", spmSingle);
                                break;
                            case 2:
                                Led.StartOrRestart(lsqOn);
                                AuPlayer.Play("2.wav", spmSingle);
                                break;
                            case 3:
                                Led.StartOrRestart(lsqOff);
                                AuPlayer.Play("3.wav", spmSingle);
                                break;
                            default: break;
                        } // switch
                    } // if TmrPauseRestart.IsRunning
                } // if Btn0 pressed
                else if(AuPlayer.IsPlayingNow()) { // Stop it
                    Led.StartOrRestart(lsqOff);
                    AuPlayer.FadeOut();
                }
                break;

#if ACC_REQUIRED
            case evtIdMotion:
                switch(State) {
                    case staIdle:
                        Printf("AccWhenIdle\r");
                        Led.StartOrRestart(lsqBlinkGreen);
                        if(DirList.GetRandomFnameFromDir("Random", FName) == retvOk) {
                            IdPlayingNow = 0xFF;
                            Resume();
                            AuPlayer.Play(FName, spmSingle);
                            State = staPlaying;
                        }
                        else Standby();
                        break;

                    case staPlaying:
                        Printf("AccWhenBusy\r");
                        Led.StartOrRestart(lsqBlinkRed);
                        break;

                    case staPauseAfter:
                        Printf("AccWhenPause\r");
                        Led.StartOrRestart(lsqBlinkMagenta);
                        break;
                } // switch state
                break;
#endif

            case evtIdPauseRestart:
                Printf("MayRestart\r");
                break;

            case evtIdAudioPlayStop:
                IdPlayingNow = -1;
                Printf("PlayEnd\r");
                Standby();
                Led.StartOrRestart(lsqOff);
            break;

            default: break;
        } // switch
    } // while true
}

void Resume() {
    if(!IsStandby) return;
    Printf("Resume\r");
    // Clock
//    if(Clk.EnablePLL() == retvOk) Clk.SwitchToPLL();
//    Clk.SetupBusDividers(ahbDiv1, apbDiv1, apbDiv1);
//    Clk.UpdateFreqValues();
//    chSysEnable();
//    Clk.PrintFreqs();
    // Sound
    Codec.Resume();
    IsStandby = false;
}

void Standby() {
    Printf("Standby\r");
//    Codec.Deinit();
    Codec.Standby();
//    // Clock
//    Clk.SwitchToMSI();
//    Clk.DisablePll();
//    Clk.SetupBusDividers(ahbDiv8, apbDiv1, apbDiv1);
//    Clk.UpdateFreqValues();
//    Clk.PrintFreqs();
    IsStandby = true;
}

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

    else if(PCmd->NameIs("A")) {
        Resume();
        AuPlayer.Play("alive.wav", spmSingle);
    }
//    else if(PCmd->NameIs("48")) {
//        Audio.Resume();
//        Player.Play("Mocart48.wav");
//    }
//    else if(PCmd->NameIs("FO")) Player.FadeOut();


    else PShell->CmdUnknown();
}
#endif
