/*
 * File:   ibus.h
 * Author: Ted Salmon <tass2001@gmail.com>
 * Description:
 *     This implements the I-Bus
 */
#ifndef IBUS_H
#define IBUS_H
#include <stdint.h>
#include "uart.h"

#define IBUS_RX_BUFFER_SIZE 255
#define IBUS_TX_BUFFER_SIZE 16
#define IBUS_MAX_MSG_LENGTH 24
#define IBUS_RX_BUFFER_TIMEOUT 50
#define IBUS_TX_BUFFER_WAIT 100

const static unsigned char IBusDevice_GM = 0x00; /* Body module */
const static unsigned char IBusDevice_SHD = 0x08; /* Sunroof Control */
const static unsigned char IBusDevice_CDC = 0x18; /* CD Changer */
const static unsigned char IBusDevice_FUH = 0x28; /* Radio controlled clock */
const static unsigned char IBusDevice_CCM = 0x30; /* Check control module */
const static unsigned char IBusDevice_GT = 0x3B; /* Graphics driver (in navigation system) */
const static unsigned char IBusDevice_DIA = 0x3F; /* Diagnostic */
const static unsigned char IBusDevice_FBZV = 0x40; /* Remote control central locking */
const static unsigned char IBusDevice_GTF = 0x43; /* Graphics driver for rear screen (in navigation system) */
const static unsigned char IBusDevice_EWS = 0x44; /* EWS (Immobileiser) */
const static unsigned char IBusDevice_CID = 0x46; /* Central information display (flip-up LCD screen) */
const static unsigned char IBusDevice_MFL = 0x50; /* Multi function steering wheel */
const static unsigned char IBusDevice_MM0 = 0x51; /* Mirror memory */
const static unsigned char IBusDevice_IHK = 0x5B; /* Integrated heating and air conditioning */
const static unsigned char IBusDevice_PDC = 0x60; /* Park distance control */
const static unsigned char IBusDevice_RAD = 0x68; /* Radio */
const static unsigned char IBusDevice_DSP = 0x6A; /* Digital signal processing audio amplifier */
const static unsigned char IBusDevice_SM0 = 0x72; /* Seat memory */
const static unsigned char IBusDevice_SDRS = 0x73; /* Sirius Radio */
const static unsigned char IBusDevice_CDCD = 0x76; /* CD changer, DIN size. */
const static unsigned char IBusDevice_NAVE = 0x7F; /* Navigation (Europe) */
const static unsigned char IBusDevice_IKE = 0x80; /* Instrument cluster electronics */
const static unsigned char IBusDevice_MM1 = 0x9B; /* Mirror memory */
const static unsigned char IBusDevice_MM2 = 0x9C; /* Mirror memory */
const static unsigned char IBusDevice_FMID = 0xA0; /* Rear multi-info-display */
const static unsigned char IBusDevice_ABM = 0xA4; /* Air bag module */
const static unsigned char IBusDevice_NAVJ = 0xBB; /* Navigation (Japan) */
const static unsigned char IBusDevice_GLO = 0xBF; /* Global, broadcast address */
const static unsigned char IBusDevice_MID = 0xC0; /* Multi-info display */
const static unsigned char IBusDevice_TEL = 0xC8; /* Telephone */
const static unsigned char IBusDevice_TCU = 0xCA; /* BMW Assist */
const static unsigned char IBusDevice_LCM = 0xD0; /* Light control module */
const static unsigned char IBusDevice_GTHL = 0xDA; /* unknown */
const static unsigned char IBusDevice_IRIS = 0xE0; /* Integrated radio information system */
const static unsigned char IBusDevice_ANZV = 0xE7; /* Front display */
const static unsigned char IBusDevice_RLS = 0xE8; /* Rain/Light Sensor */
const static unsigned char IBusDevice_TV = 0xED; /* Television */
const static unsigned char IBusDevice_BMBT = 0xF0; /* On-board monitor operating part */
const static unsigned char IBusDevice_LOC = 0xFF; /* Local */

const static unsigned char IBusAction_CD_STATUS_REQ = 0x38;
const static unsigned char IBusAction_CD_STATUS_REP = 0x39;

/**
 * IBus_t
 *     Description:
 *         This object defines helper functionality to allow us to interact
 *         with the I-Bus
 */
typedef struct IBus_t {
    struct UART_t uart;
    unsigned char cdStatus;
    unsigned char rxBuffer[IBUS_RX_BUFFER_SIZE];
    uint8_t rxBufferIdx;
    uint32_t rxLastStamp;
    unsigned char txBuffer[IBUS_TX_BUFFER_SIZE][IBUS_MAX_MSG_LENGTH];
    uint8_t txBufferReadIdx;
    uint8_t txBufferWriteIdx;
    uint32_t txLastStamp;
    char displayText[200];
    uint8_t displayTextIdx;
    uint32_t displayTextLastStamp;
    /* Temporary */
    uint8_t playbackStatus;
} IBus_t;

struct IBus_t IBusInit();
void IBusProcess(struct IBus_t *);
void IBusSendCommand(struct IBus_t *, const unsigned char, const unsigned char, const unsigned char *, size_t);
void IBusStartup(struct IBus_t *);
void IBusDisplayText(struct IBus_t *, char *);
void IBusDisplayTextClear(struct IBus_t *);
#endif /* IBUS_H */
