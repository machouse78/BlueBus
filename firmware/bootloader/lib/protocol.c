/*
 * File: protocol.c
 * Author: Ted Salmon <tass2001@gmail.com>
 * Description:
 *     Implement a simple data protocol for the bootloader
 */
#include "protocol.h"

/**
 * ProtocolBC127Mode()
 *     Description:
 *         In order to allow the BC127 to be managed directly (including firmware
 *         upgrades), we set the tri-state buffer mode to pass UART data from the
 *         BC127 to the FT232R, rather than the MCU. It is not possible to exit
 *         this mode gracefully, so a hard reset of the device will be required
 *         in order to return to the regular system.
 *     Params:
 *         None
 *     Returns:
 *         void
 */
void ProtocolBC127Mode()
{
    // Release the UART
    UARTDestroy(BC127_UART_MODULE);
    // Set the UART mode to BC127 after disabling the MCU UART
    UART_SEL = UART_SEL_BT;
}

/**
 * ProtocolFlashErase()
 *     Description:
 *         Iterate through the NVM space and clear all pages of memory.
 *         This should be done prior to any write operations.
 *     Params:
 *         void
 *     Returns:
 *         void
 */
void ProtocolFlashErase()
{
    uint32_t address = BOOTLOADER_APPLICATION_START;
    while (address >= BOOTLOADER_APPLICATION_START &&
           address < BOOTLOADER_APPLICATION_END
    ) {
        FlashErasePage(address);
        // Pages are erased in 1024 instruction blocks
        address += _FLASH_ROW * 16;
    }
}

/**
 * ProtocolFlashWrite()
 *     Description:
 *         Take a Flash Write packet and write it out to NVM
 *     Params:
 *         ProtocolPacket_t *packet - The data packet structure
 *     Returns:
 *         uint8_t - The result of the write operation
 */
uint8_t ProtocolFlashWrite(ProtocolPacket_t *packet)
{
    uint32_t address = (
        ((uint32_t) 0 << 24) +
        ((uint32_t)packet->data[0] << 16) +
        ((uint32_t)packet->data[1] << 8) +
        ((uint32_t)packet->data[2])
    );
    uint8_t index = 3;
    uint8_t flashRes = 1;
    while (index < packet->dataSize && flashRes == 1) {
        // Do not allow the Bootloader to be overwritten
        if (address < BOOTLOADER_APPLICATION_START) {
            // Skip the current DWORD, since it overwrites protected memory
            address += 0x2;
            index += 3;
        } else {
            uint32_t data = (
                ((uint32_t)0 << 24) + // "Phantom" Byte
                ((uint32_t)packet->data[index] << 16) +
                ((uint32_t)packet->data[index + 1] << 8) + 
                ((uint32_t)packet->data[index + 2])
            );
            uint32_t data2 = (
                ((uint32_t)0 << 24) + // "Phantom" Byte
                ((uint32_t)packet->data[index + 3] << 16) +
                ((uint32_t)packet->data[index + 4] << 8) + 
                ((uint32_t)packet->data[index + 5])
            );
            // We write two WORDs at a time, so jump the necessary
            // number of indices and addresses
            flashRes = FlashWriteDWORDAddress(address, data, data2);
            index += 6;
            address += 0x04;
        }
    }
    return flashRes;
}

/**
 * ProtocolProcessMessage()
 *     Description:
 *         Execute the packet data
 *     Params:
 *         UART_t *uart - The UART struct to use for communication
 *         uint8_t *BOOT_MODE - The bootload mode flag
 *     Returns:
 *         ProtocolPacket_t
 */
void ProtocolProcessMessage(
    UART_t *uart,
    uint8_t *BOOT_MODE
) {
    if ((TimerGetMillis() - uart->rxLastTimestamp) > UART_RX_QUEUE_TIMEOUT) {
        UARTResetRxQueue(uart);
        ProtocolSendPacket(
            uart,
            (unsigned char) PROTOCOL_BAD_PACKET_RESPONSE,
            0,
            0
        );
    }
    // Process Message
    struct ProtocolPacket_t packet;
    if (uart->moduleIndex == (BC127_UART_MODULE - 1)) {
        packet = ProtocolProcessPacketBC127(uart);
    } else {
        packet = ProtocolProcessPacket(uart);
    }
    if (packet.status == PROTOCOL_PACKET_STATUS_OK) {
        // Lock the device in the bootloader since we have a good packet
        if (*BOOT_MODE == 0) {
            ON_LED = 1;
            *BOOT_MODE = BOOT_MODE_BOOTLOADER;
        }
        if (packet.command == PROTOCOL_CMD_PLATFORM_REQUEST) {
            ProtocolSendStringPacket(
                uart,
                (unsigned char) PROTOCOL_CMD_PLATFORM_RESPONSE,
                (char *) BOOTLOADER_PLATFORM
            );
        } else if (packet.command == PROTOCOL_CMD_ERASE_FLASH_REQUEST) {
            ProtocolFlashErase();
            ProtocolSendPacket(
                uart,
                (unsigned char) PROTOCOL_CMD_ERASE_FLASH_RESPONSE,
                0,
                0
            );
        } else if (packet.command == PROTOCOL_CMD_WRITE_DATA_REQUEST) {
            uint8_t writeResult = ProtocolFlashWrite(&packet);
            if (writeResult == 1) {
                ProtocolSendPacket(
                    uart,
                    PROTOCOL_CMD_WRITE_DATA_RESPONSE_OK,
                    0,
                    0
                );
            } else {
                ProtocolSendPacket(
                    uart,
                    PROTOCOL_CMD_WRITE_DATA_RESPONSE_ERR,
                    0,
                    0
                );
            }
        } else if (packet.command == PROTOCOL_CMD_BC127_MODE_REQUEST) {
            ProtocolSendPacket(
                uart,
                (unsigned char) PROTOCOL_CMD_BC127_MODE_RESPONSE,
                0,
                0
            );
            // Nop() So the packet makes it to the receiver
            uint16_t i = 0;
            while (i < NOP_COUNT) {
                Nop();
                i++;
            }
            ProtocolBC127Mode();
        } else if (packet.command == PROTOCOL_CMD_START_APP_REQUEST) {
            ProtocolSendPacket(
                uart,
                (unsigned char) PROTOCOL_CMD_START_APP_RESPONSE,
                0,
                0
            );
            // Nop() So the packet makes it to the receiver
            uint16_t i = 0;
            while (i < NOP_COUNT) {
                Nop();
                i++;
            }
            *BOOT_MODE = BOOT_MODE_NOW;
        } else if (packet.command == PROTOCOL_CMD_WRITE_SN_REQUEST) {
            ProtocolWriteSerialNumber(uart, &packet);
        }
    } else if (packet.status == PROTOCOL_PACKET_STATUS_BAD) {
        UARTResetRxQueue(uart);
        ProtocolSendPacket(
            uart,
            (unsigned char) PROTOCOL_BAD_PACKET_RESPONSE,
            0,
            0
        );
    }
}

/**
 * ProtocolProcessPacket()
 *     Description:
 *         Attempt to create and verify a packet. It will return a bad
 *         error status if the data in the UART buffer does not align with
 *         the protocol.
 *     Params:
 *         UART_t *uart - The UART struct to use for communication
 *     Returns:
 *         ProtocolPacket_t
 */
ProtocolPacket_t ProtocolProcessPacket(UART_t *uart)
{
    struct ProtocolPacket_t packet;
    packet.status = PROTOCOL_PACKET_STATUS_INCOMPLETE;
    if (uart->rxQueueSize >= 2) {
        if (uart->rxQueueSize == (uint8_t) UARTGetOffsetByte(uart, 1)) {
            packet.command = UARTGetNextByte(uart);
            packet.dataSize = (uint8_t) UARTGetNextByte(uart) - PROTOCOL_CONTROL_PACKET_SIZE;
            uint8_t i;
            for (i = 0; i < packet.dataSize; i++) {
                if (uart->rxQueueSize > 0) {
                    packet.data[i] = UARTGetNextByte(uart);
                } else {
                    packet.data[i] = 0x00;
                }
            }
            unsigned char validation = UARTGetNextByte(uart);
            packet.status = ProtocolValidatePacket(&packet, validation);
        }
    }
    return packet;
}

ProtocolPacket_t ProtocolProcessPacketBC127(UART_t *uart)
{
    struct ProtocolPacket_t packet;
    packet.status = PROTOCOL_PACKET_STATUS_INCOMPLETE;
    uint8_t messageLength = UARTRxQueueSeek(uart, PROTOCOL_BC127_MSG_END_CHAR);
    if (messageLength > 0) {
        char msg[messageLength];
        uint8_t i;
        uint8_t delimCount = 1;
        for (i = 0; i < messageLength; i++) {
            char c = UARTGetNextByte(uart);
            if (c == PROTOCOL_BC127_MSG_DELIMETER) {
                delimCount++;
            }
            if (c != PROTOCOL_BC127_MSG_END_CHAR) {
                msg[i] = c;
            } else {
                // The protocol states that 0x0D delimits messages,
                // so we change it to a null terminator instead
                msg[i] = '\0';
            }
        }
        // Copy the message, since strtok adds a null terminator after the first
        // occurence of the delimiter, causes issues with any functions used going forward
        char tmpMsg[messageLength];
        strcpy(tmpMsg, msg);
        char *msgBuf[delimCount];
        char *p = strtok(tmpMsg, " ");
        i = 0;
        while (p != NULL) {
            msgBuf[i++] = p;
            p = strtok(NULL, " ");
        }
        if (strcmp(msgBuf[0], "RECV") == 0) {
            delimCount = 0;
            uint16_t j = 0;
            uint16_t k = 0;
            unsigned char data[PROTOCOL_MAX_DATA_SIZE];
            // Extract our packet from the message
            while (j < messageLength - 1) {
                if (msg[j] == 0x20) {
                    delimCount++;
                }
                if (delimCount >= 3) {
                    data[k] = msg[j];
                    k++;
                }
                j++;
            }
            packet.command = data[0];
            packet.dataSize = (uint8_t) data[1] - PROTOCOL_CONTROL_PACKET_SIZE;
            uint8_t i;
            for (i = 0; i < packet.dataSize; i++) {
                if (i < sizeof(data)) {
                    packet.data[i] = data[i + 2];
                } else {
                    packet.data[i] = 0x00;
                }
            }
            unsigned char validation = data[i + 2];
            packet.status = ProtocolValidatePacket(&packet, validation);
        }
    }
    return packet;
}

/**
 * ProtocolSendPacket()
 *     Description:
 *         Generated a packet and send if over UART
 *     Params:
 *         UART_t *uart - The UART struct to use for communication
 *         unsigned char command - The protocol command
 *         unsigned char *data - The data to send in the packet. Must be at least
 *             one byte per protocol
 *         uint8_t dataSize - The number of bytes in the data pointer
 *     Returns:
 *         void
 */
void ProtocolSendPacket(
    UART_t *uart, 
    unsigned char command, 
    unsigned char *data,
    uint8_t dataSize
) {
    uint8_t nullData = 0;
    if (dataSize == 0) {
        dataSize = 1;
        nullData = 1;
    }
    uint8_t crc = 0;
    uint8_t length = dataSize + PROTOCOL_DATA_INDEX_BEGIN;
    unsigned char packet[length];

    crc = crc ^ (uint8_t) command;
    crc = crc ^ (uint8_t) length;
    packet[0] = command;
    packet[1] = length;
    if (nullData == 1) {
        // The protocol requires at least one data byte, so throw in a NULL
        packet[2] = 0x00;
    } else {
        uint8_t i;
        for (i = 2; i < length; i++) {
            packet[i] = data[i - 2];
            crc = crc ^ (uint8_t) data[i - 2];
        }
    }
    packet[length - 1] = (unsigned char) crc;
    UARTSendData(uart, packet, length);
}

/**
 * ProtocolSendStringPacket()
 *     Description:
 *         Send a given string in a packet with the given command.
 *     Params:
 *         UART_t *uart - The UART struct to use for communication
 *         unsigned char command - The protocol command
 *         char *string - The string to send
 *     Returns:
 *         void
 */
void ProtocolSendStringPacket(UART_t *uart, unsigned char command, char *string)
{
    uint8_t len = 0;
    while (string[len] != 0) {
        len++;
    }
    len++;
    ProtocolSendPacket(uart, command, (unsigned char *) string, len);
}

/**
 * ProtocolValidatePacket()
 *     Description:
 *         Use the given checksum to validate the data in a given packet
 *     Params:
 *         ProtocolPacket_t *packet - The data packet structure
 *         unsigned char validation - the XOR checksum of the data in packet
 *     Returns:
 *         uint8_t - If the packet is valid or not
 */
uint8_t ProtocolValidatePacket(ProtocolPacket_t *packet, unsigned char validation)
{
    uint8_t chk = packet->command;
    chk = chk ^ (packet->dataSize + PROTOCOL_CONTROL_PACKET_SIZE);
    uint8_t msgSize = packet->dataSize;
    uint8_t idx;
    for (idx = 0; idx < msgSize; idx++) {
        chk = chk ^ packet->data[idx];
    }
    chk = chk ^ validation;
    if (chk == 0) {
        return PROTOCOL_PACKET_STATUS_OK;
    } else {
        return PROTOCOL_PACKET_STATUS_BAD;
    }
}

/**
 * ProtocolWriteSerialNumber()
 *     Description:
 *         Write the serial number to EEPROM if it's not already set
 *     Params:
 *         UART_t *uart - The UART struct to use for communication
 *         ProtocolPacket_t *packet - The data packet structure
 *     Returns:
 *         void
 */
void ProtocolWriteSerialNumber(UART_t *uart, ProtocolPacket_t *packet)
{
    uint16_t serialNumber = (
            (EEPROMReadByte(CONFIG_SN_MSB) << 8) | 
            (EEPROMReadByte(CONFIG_SN_LSB) & 0xFF)
    );
    if (serialNumber == 0x0) {
        EEPROMWriteByte(CONFIG_SN_MSB, packet->data[0]);
        EEPROMWriteByte(CONFIG_SN_LSB, packet->data[1]);
        ProtocolSendPacket(uart, PROTOCOL_CMD_WRITE_SN_RESPONSE_OK, 0, 0);
    } else {
        ProtocolSendPacket(uart, PROTOCOL_CMD_WRITE_SN_RESPONSE_ERR, 0, 0);
    }
}
