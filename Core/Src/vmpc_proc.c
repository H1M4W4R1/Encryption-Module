/*
 * vmpc_proc.c
 *
 *  Created on: Jan 4, 2021
 *      Author: Patryk Pastuszka
 */
#include "main.h"
#include "usbd_cdc_if.h"
#include "vmpc.h"
#include "usbd_cdc.h"

// Iterator for running around password
uint8_t iterator = 0;

// Currently running command, 0x0 - none
uint8_t currentCommand = 0x0;

// Defines if length of password is set
uint8_t isLengthSet = 0;

// Password, max 256 bytes
uint8_t Password[256];

// Password length, max 256
uint8_t PasswordLength = 0;

// Length of chunk, max 16384
uint16_t ChunkLength;

// Iterator running around chunk
uint16_t chunkIterator = 0;

// Defines if chunk length is set
uint8_t isChunkLenSet = 0;

// Dumped data loading iterator
uint16_t loadIterator;

// Command Do Nothing
const uint8_t CMD_DOE_NOT = 0x0;

// Command Set Password
const uint8_t CMD_SET_PWD = 0x4;

// Command Initialize Algorithm
const uint8_t CMD_INI_ALG = 0x5;

// Command Encrypt Stream
const uint8_t CMD_ENC_STR = 0x6;

// Command Encrypt Sequence
const uint8_t CMD_ENC_SEQ = 0x7;

// Command Dump Data
const uint8_t CMD_DMP_DAT = 0x50;

// Command Load Data
const uint8_t CMD_LOA_DAT = 0x51;

// Size of stream chunk. Will return data after this value is reached. Equal to amount of bytes returned.
// MAX (recommended): 32767.
const uint16_t STREAM_CHUNK_SIZE = 0x1;

uint8_t buffer[16384];

// Current length of data in buffer
uint16_t len = 0;
uint16_t stream_len = 0x0;

// Writes data to USB buffer
void sw(uint8_t b){
	*(buffer + len) = b;
	len++;
}

// Sends USB buffer onto CDC port
void Send(){
	if(len > 0){
		CDC_Transmit_FS(buffer, len);
		len = 0;
	}
}

void OnPacketReceived(uint8_t recv) {

	// If command is nothing check if it's new command
    if (currentCommand == CMD_DOE_NOT) {
        if (recv == CMD_DOE_NOT || recv == CMD_SET_PWD || recv == CMD_INI_ALG || recv == CMD_ENC_SEQ
        		|| recv == CMD_LOA_DAT || recv == CMD_DMP_DAT || recv == CMD_ENC_STR) {
            currentCommand = recv;
        }

    }
    // If set password
    else if (currentCommand == CMD_SET_PWD) {
    	// Set information about password length
        if (isLengthSet == 0) {
            PasswordLength = recv;
            iterator = 0;
            isLengthSet = 1;
        }
        // Load password bytes
        else {
            Password[iterator] = recv;
            iterator++;

            if (iterator >= PasswordLength) {
                currentCommand = 0x0;
                isLengthSet = 0;
            }
        }
    }
    // If initialize algorithm, do so
    else if (currentCommand == CMD_INI_ALG)
    {
        ResetVMPC();
        VMPCInitKey(Password, InitVector, PasswordLength, 64);
        currentCommand = 0x0;
    }
    else if (currentCommand == CMD_ENC_STR)
    {
    	// Encrypt data and store it in return buffer.
    	uint8_t enc1 = VMPCEncrypt(recv);
    	sw(enc1);
    	stream_len++;

    	// If reached end of packet then send it and create new packet.
    	if(stream_len == STREAM_CHUNK_SIZE){
    		stream_len = 0;
    		Send();
    	}
    }
    // Encrypt sequence of bytes
    else if (currentCommand == CMD_ENC_SEQ) {
    	// Loading length, max 2 bytes, MSB first
        if (isChunkLenSet < 2) {
            if (isChunkLenSet > 0) {
                ChunkLength *= 256;
            } else {
                ChunkLength = 0;
            }

            ChunkLength += recv;
            isChunkLenSet++;
            chunkIterator = 0;
        }
        // Sequence data encryption
        else {
            uint8_t enc1 = VMPCEncrypt(recv);
            sw(enc1);
            chunkIterator++;
            if (chunkIterator >= ChunkLength) {
                currentCommand = 0x0;
                // Send encrypted data back over USB
                Send();
                isChunkLenSet = 0;
            }
        }
    } else if (currentCommand == CMD_DMP_DAT) {
    	// Dump encryption data, P, s, n
        for (uint16_t q = 0; q < 256; q++)
            sw(P[q]);
        sw(s);
        sw(n);
        Send();
        currentCommand = 0x0;
    }
    // Load encryption data P, s, n
    else if (currentCommand == CMD_LOA_DAT) {
        if (loadIterator < 256) {
            P[loadIterator] = recv;
            loadIterator++;
        } else if (loadIterator == 256) {
            s = recv;
            loadIterator++;
        } else if (loadIterator == 257) {
            n = recv;
            loadIterator = 0;
            currentCommand = 0x0;
        }
    }

}
