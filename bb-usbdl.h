/*
 * Copyright (C) 2024 Ivaylo Ivanov <ivo.ivanov.ivanov1@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.0.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License 3.0 for more details.
 *
 * A copy of the GPL 3.0 should have been included with the program.
 * If not, see http://www.gnu.org/licenses/
 *
 * This file includes small portions of code derived from the Sachesi
 * project:
 * Copyright (C) 2014 Sacha Refshauge
 * Repository: http://github.com/xsacha/Sachesi
 * Licensed under the GNU General Public License v3.0
 */

#ifndef __BB_USBDL
#define __BB_USBDL

#include <libusb-1.0/libusb.h>

#define VENDOR_ID	0x0fca
#define PRODUCT_ID	0x0001
#define TIMEOUT		5000

#define nullptr ((void*)0)

#define DEBUG 1
void print_hex(const char *label, unsigned char *data, int length)
{
#ifdef DEBUG
	printf("%s (%d bytes):\n", label, length);

	for (int i = 0; i < length; i++) {
		printf("%02x ", data[i]);
		if ((i + 1) % 16 == 0)
			printf("\n");
	}
	printf("\n");
#endif
}

typedef struct ControlMessageHeader {
	unsigned short type;
	unsigned short packetSize;
	unsigned char command;
	unsigned char mode;
	unsigned short packetId;
} __attribute__((packed)) ControlMessageHeader;

enum BootloaderMode {
	kBootloaderRimReinit			= 0x0,
	kBootloaderRimBoot			= 0x1,
	kBootloaderRamBoot			= 0x2,
	kBootloaderUPL				= 0x3,
	kBootloaderRimBootNuke			= 0x4
};

enum ControlMessageCommand {
	kCommandPing				= 0x01, // OUT
	kCommandPingResponse			= 0x02, // IN
	kCommandReboot				= 0x03, // OUT
	kCommandRebootResponse			= 0x04, // IN
	kCommandGetVariable			= 0x05, // OUT
	kCommandGetVariableResponse		= 0x06, // IN
	kCommandSetMode				= 0x07, // OUT
	kCommandSetModeSuccess			= 0x08, // IN
	kCommandSetModeFailure			= 0x09, // IN
	kCommandGetPasswordInfo			= 0x0A, // OUT
	kCommandLoadTransferredData		= 0x0B, // IN/OUT
	kCommandLoadTransferredDataSuccess	= 0x0C, // IN/OUT
	kCommandLoadTransferredDataFailure	= 0x0D, // IN/OUT
	kCommandGetPasswordInfoResponse		= 0x0E, // IN
	kCommandSendPassword			= 0x0F, // OUT
	kCommandSendPasswordCorrect		= 0x10, // IN
	kCommandUnkn1				= 0x11, // ?
	kCommandUnkn1Response			= 0x12, // OUT
	kCommandReadyForDataTransfer		= 0x13 // IN
};

#endif
