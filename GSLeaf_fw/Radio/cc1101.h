/*
 * cc1101.h
 *
 *  Created on: Feb 12, 2013
 *      Author: g.kruglov
 */

#ifndef CC1101_H__
#define CC1101_H__

#include <inttypes.h>
#include "kl_lib.h"
#include "cc1101defins.h"
#include "cc1101_rf_settings.h"

#define CC_BUSYWAIT_TIMEOUT     99000   // tics, not ms

void CCIrqHandler();

class cc1101_t : public IrqHandler_t {
private:
    const Spi_t ISpi;
    const GPIO_TypeDef *SpiGpio, *CSGpio;
    const uint16_t Sck, Miso, Mosi, Cs;
    const PinIrq_t IGdo0;
    uint8_t IState; // Inner CC state, returned as first byte
    thread_reference_t ThdRef;
    ftVoidVoid ICallback = nullptr;
    // Pins
    uint8_t BusyWait() {
        for(uint32_t i=0; i<CC_BUSYWAIT_TIMEOUT; i++) {
            if(PinIsLo(SpiGpio, Miso)) return retvOk;
        }
        return retvFail;
    }
    void CsHi() { PinSetHi((GPIO_TypeDef*)CSGpio, Cs); }
    void CsLo() { PinSetLo((GPIO_TypeDef*)CSGpio, Cs); }
    // General
    int8_t RSSI_dBm(uint8_t ARawRSSI);
    // Registers and buffers
    uint8_t WriteRegister(const uint8_t Addr, const uint8_t AData);
    uint8_t ReadRegister(const uint8_t Addr, uint8_t *PData);
    uint8_t WriteStrobe(uint8_t AStrobe);
    uint8_t WriteTX(uint8_t* Ptr, uint8_t Length);
    // Strobes
    uint8_t Reset()       { return WriteStrobe(CC_SRES); }
    uint8_t EnterTX()     { return WriteStrobe(CC_STX);  }
    uint8_t EnterRX()     { return WriteStrobe(CC_SRX);  }
    uint8_t FlushRxFIFO() { return WriteStrobe(CC_SFRX); }
    uint8_t FlushTxFIFO() { return WriteStrobe(CC_SFTX); }
    uint8_t GetStatus()   { return WriteStrobe(CC_SNOP); }
public:
    uint8_t Init();
    uint8_t EnterIdle()    { return WriteStrobe(CC_SIDLE); }
    uint8_t EnterPwrDown() { return WriteStrobe(CC_SPWD);  }
    void SetChannel(uint8_t AChannel);
    void SetTxPower(uint8_t APwr)  { WriteRegister(CC_PATABLE, APwr); }
    void SetPktSize(uint8_t ASize) { WriteRegister(CC_PKTLEN, ASize); }
    void SetBitrate(const CCRegValue_t* BRSetup);
    // State change
    void Transmit(void *Ptr, uint8_t Len);
    uint8_t Receive(uint32_t Timeout_ms, void *Ptr, uint8_t Len,  int8_t *PRssi=nullptr);
    uint8_t Receive_st(sysinterval_t Timeout_st, void *Ptr, uint8_t Len,  int8_t *PRssi=nullptr);
    void ReceiveAsync(ftVoidVoid Callback);

    uint8_t RxCcaTx_st(void *PtrTx, uint8_t Len,  int8_t *PRssi=nullptr);
    uint8_t RxIfNotYet_st(sysinterval_t RxTimeout_st, void *Ptr, uint8_t Len,  int8_t *PRssi=nullptr);

    void PowerOff();
    uint8_t Recalibrate() {
        while(IState != CC_STB_IDLE) {
            if(EnterIdle() != retvOk) return retvFail;
        }
        if(WriteStrobe(CC_SCAL) != retvOk) return retvFail;
        do {
            GetStatus();
        } while(IState != CC_STB_IDLE);
        return retvOk;
    }
    // Setup
//    void DoRxAfterRxAndRxAfterTx()   { WriteRegister(CC_MCSM1, (CC_MCSM1_VALUE | 0x0F)); }
//    void DoRxAfterRxAndIdleAfterTx() { WriteRegister(CC_MCSM1, ((CC_MCSM1_VALUE | 0x0C) & 0xFC)); }
    void DoRxAfterTx()   { WriteRegister(CC_MCSM1, (CC_MCSM1_VALUE | 0x03)); }
    void DoIdleAfterTx() { WriteRegister(CC_MCSM1, CC_MCSM1_VALUE); }

    uint8_t ReadFIFO(void *Ptr, int8_t *PRssi, uint8_t Len);

    void IIrqHandler();

    cc1101_t(
            SPI_TypeDef *ASpi, GPIO_TypeDef *ASpiGpio,
            uint16_t ASck, uint16_t AMiso, uint16_t AMosi,
            GPIO_TypeDef *ACSGpio, uint16_t ACs,
            GPIO_TypeDef *AGd0Gpio, uint16_t AGdo0):
        ISpi(ASpi), SpiGpio(ASpiGpio), CSGpio(ACSGpio),
        Sck(ASck), Miso(AMiso), Mosi(AMosi), Cs(ACs),
        IGdo0(AGd0Gpio, AGdo0, pudNone, CCIrqHandler),
        IState(0), ThdRef(nullptr) {}
};

#endif //CC1101_H__