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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libusb-1.0/libusb.h>
#include <unistd.h>
#include <openssl/sha.h>
#include "bb-usbdl.h"

unsigned int packetid = 0;
int _bootloaderMode = 0xFF;

int send_ctrlmsg(libusb_device_handle* handle, uint8_t command, unsigned char* response, int length)
{
	ControlMessageHeader header;
	memset(&header, 0, sizeof(header));

	header.type = 0x0000;
	header.packetSize = length + 8;
	header.command = command;
	header.mode = _bootloaderMode;
	header.packetId = packetid++;

	unsigned char result[8 + length];
	int header_size = sizeof(header);

	memcpy(result, &header, header_size);
	if (response)
		memcpy(result + header_size, response, length);

	int transferred = 0;
	int act = libusb_bulk_transfer(handle, 0x02, result, sizeof(result), &transferred, TIMEOUT);
	if (act < 0) {
		fprintf(stderr, "Failed to send control message: %s\n", libusb_error_name(act));
		return act;
	}

	print_hex("Sending", result, transferred);

	return transferred;
}

int receive_ctrlmsg(libusb_device_handle* handle, unsigned char* header, unsigned char* data)
{
	int transferred;
	unsigned char response[2048];

	int result = libusb_bulk_transfer(handle, 0x82, response, sizeof(response), &transferred, TIMEOUT);
	if (result < 0) {
		fprintf(stderr, "Failed to receive response: %s\n", libusb_error_name(result));
		return result;
	}
	print_hex("Response", response, transferred);

	if (header)
		memcpy(header, response, 8);
	if (data)
		memcpy(data, response + 8, transferred - 8);

	return 0;
}

int send_password(libusb_device_handle *handle, const char *password)
{
	ControlMessageHeader pass_header;
	unsigned char challengeData[100] = {0};

	if (send_ctrlmsg(handle, kCommandGetPasswordInfo, NULL, 0) < 0) {
		perror("Failed to send get password info command");
		return 0;
	}

	if (receive_ctrlmsg(handle, &pass_header, challengeData) < 0) {
		perror("Failed to receive challenge data");
		return 0;
	}

	/*
	 * HACK: password sending doesn't completely work yet.
	 * However, you can still disable password in the OS and
	 * then it'll send a response with size 8
	 */
	if (pass_header.packetSize == 8) {
		printf("No password set, continuing..\n");
		return 1;
	} else {
		perror("Please remove your password in the OS!");
		
	};

	unsigned char challenge[4], salt[8];
	memcpy(challenge, challengeData + 4, 4);
	memcpy(salt, challengeData + 12, 8);

	int iterations;
	memcpy(&iterations, challengeData + 20, sizeof(int));

	// Ensure proper endianness
	iterations = le32toh(iterations);

	unsigned char hashedData[68];
	strncpy((char *)hashedData, password, sizeof(hashedData));

	int count = 0;
	int challenger = 1;
	do {
		unsigned char buf[4 + sizeof(salt) + sizeof(hashedData)] = {0};
		memcpy(buf, &count, 4);
		memcpy(buf + 4, salt, sizeof(salt));
		memcpy(buf + 12, hashedData, 64);

		SHA512(buf, 12 + 64, hashedData);

		if (count == iterations - 1 && challenger) {
			count = -1;
			challenger = 0;
			unsigned char temp[68];
			memcpy(temp, challenge, 4);
			memcpy(temp + 4, hashedData, 64);
			memcpy(hashedData, temp, 68);
		}
	} while (++count < iterations);

	// Prepare and send hashed password
	unsigned char finalData[68] = {0};
	finalData[0] = 0x00;
	finalData[1] = 0x00;
	finalData[2] = 0x40;
	finalData[3] = 0x00;
	memcpy(finalData + 4, hashedData, 64);

	if (send_ctrlmsg(handle, kCommandSendPassword, finalData, sizeof(finalData)) < 0) {
		perror("Failed to send hashed password");
		return 0;
	}

	// Step 5: Receive and check response
	unsigned char header[64];
	if (receive_ctrlmsg(handle, header, NULL) < 0) {
		perror("Failed to receive password response");
		return 0;
	}

	if (header[0] == 0x10) {
		printf("Password accepted.\n");
		return 1;
	} else {
		printf("Wrong password.\n");
		return 0;
	}
}

unsigned char rim_bootloader[4][17] = {
	{'R', 'I', 'M', ' ', 'R', 'E', 'I', 'N', 'I', 'T', '\0', '\0', '\0', '\0', '\0', '\0', '\1'},
	{'R', 'I', 'M', '-', 'B', 'o', 'o', 't', 'L', 'o', 'a', 'd', 'e', 'r', '\0', '\0', '\1'},
	{'R', 'I', 'M', '-', 'R', 'A', 'M', 'L', 'o', 'a', 'd', 'e', 'r', '\0', '\0', '\0', '\1'},
	{'R', 'I', 'M', ' ', 'U', 'P', 'L', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\1'},
};

int set_blmode(libusb_device_handle *handle, int bootloader_mode)
{
	int ret = send_ctrlmsg(handle, kCommandSetMode, rim_bootloader[bootloader_mode], 17);
	if (ret < 0)
		return ret;

	ret = receive_ctrlmsg(handle, nullptr, nullptr);
	if (ret < 0)
		return ret;

	_bootloaderMode = bootloader_mode;
	return 0;
}

int reboot_device(libusb_device_handle *handle)
{
	printf("Sending reboot command...\n");
	if (send_ctrlmsg(handle, kCommandReboot, 0, 0) < 0)
		return -1;

	printf("Waiting for reboot response...\n");
	if (receive_ctrlmsg(handle, nullptr, nullptr) < 0)
		return -1;

	return 0;
}

int cmd_getvar(libusb_device_handle* handle, int variable, unsigned char* buffer, int bufferSize)
{
	// Gets a variable from the device
	// Returns the amount of data received in (unsigned char*)buffer
	// 0-1: Buffer size
	// 2-3: Variable
	uint16_t data[2];
	data[0] = bufferSize;
	data[1] = variable;

	unsigned char newBuf[4];
	memcpy(newBuf, data, 4);

	int err = send_ctrlmsg(handle, kCommandGetVariable, newBuf, 4);
	if (err == -7)
		return err;

	err = receive_ctrlmsg(handle, nullptr, buffer);
	if (err == -7)
		return err;

	return err - 8;
}

int processDeviceInfo(libusb_device_handle *handle)
{
	set_blmode(handle, kBootloaderRimBoot);

	unsigned char buffer[2048];
	uint16_t messageSize;
	uint32_t pin, unk1, base_id, br_id;

	int ret = cmd_getvar(handle, 2, buffer, sizeof(buffer));
	if (ret == -7)
		return ret;

	FILE *info = fopen("info.txt", "w");
	if (!info) {
		perror("Failed to open info.txt");
		return 0;
	}

	fprintf(info, "===========================================\n");

	// Little-endian byte order
	memcpy(&messageSize, buffer, sizeof(messageSize));
	messageSize = le16toh(messageSize);

	fprintf(info, "Message Size: %d\n", messageSize);

	memcpy(&pin, buffer + 16, sizeof(pin));
	pin = le32toh(pin);

	fprintf(info, "Hardware ID: 0x%X %s\n", pin, buffer + 20);
	fprintf(info, "Build User: %s\n", buffer + 84);
	fprintf(info, "Build Date: %s\n", buffer + 100);
	fprintf(info, "Build Time: %s\n", buffer + 116);

	memcpy(&unk1, buffer + 132, sizeof(unk1));
	unk1 = le32toh(unk1);

	fprintf(info, "Unknown value: 0x%X\n", unk1);

	memcpy(&base_id, buffer + 188, sizeof(base_id));
	base_id = le32toh(base_id);

	memcpy(&br_id, buffer + 192, sizeof(br_id));
	br_id = le32toh(br_id);

	fprintf(info, "Hardware OS ID: 0x%X\n", base_id);
	fprintf(info, "BR ID: 0x%X\n", br_id);
	fprintf(info, "===========================================\n");

	// Write the remaining buffer data that we can't parse
	fwrite(buffer + 196, 1, ret - 196, info);
	fclose(info);

	return 0;
}

void send_loader(libusb_device_handle *handle)
{
	unsigned char header[8] = {0};
	memset(&header, 0, sizeof(header));

	receive_ctrlmsg(handle, header, nullptr);
	if (header[4] != kCommandReadyForDataTransfer)
		return;

	int transferred = 0;
	unsigned char startSend[] = {0x01, 0x00, 0x10, 0x00, 0x7b, 0x09, 0x2b, 0x96,
				     0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0xf0};
	libusb_bulk_transfer(handle, 0x2, startSend, 16, &transferred, 1000);

	// This drops a lot of info
	receive_ctrlmsg(handle, header, nullptr);
}

int send_loadermsg(libusb_device_handle* handle, uint8_t command, unsigned char* response, int length)
{
	ControlMessageHeader header;
	memset(&header, 0, sizeof(header));

	header.type = 0x1000;
	header.packetSize = length + 8;
	header.command = command;
	header.mode = _bootloaderMode;
	header.packetId = packetid++;

	unsigned char result[8 + length];
	int header_size = sizeof(header) - 2;

	memcpy(result, &header, header_size);

	if (response) {
		memcpy(result + header_size, response, length);
	}

	int transferred = 0;
	int act = libusb_bulk_transfer(handle, 0x02, result, sizeof(result), &transferred, TIMEOUT);
	if (act < 0) {
		fprintf(stderr, "Failed to send control message: %s\n", libusb_error_name(act));
		return act;
	}

	print_hex("Sending", result, transferred);
	return transferred;
}

libusb_device_handle *initialize_device()
{
	libusb_device_handle *handle = NULL;

	printf("Waiting for device 0x%04x:0x%04x to be connected...\n", VENDOR_ID, PRODUCT_ID);
	while (!handle) {
		handle = libusb_open_device_with_vid_pid(NULL, VENDOR_ID, PRODUCT_ID);
		if (!handle)
			sleep(1);
	}
	printf("Device found and opened.\n");

	if (libusb_claim_interface(handle, 0) < 0) {
		fprintf(stderr, "Failed to claim interface.\n");
		libusb_close(handle);
		return NULL;
	}
	printf("Interface 0 claimed.\n");

	return handle;
}

int main(int argc, char *argv[])
{
	libusb_device_handle *handle;
	int ret, task;

	if (argc < 2) {
		printf("Usage: %s arg1\n", argv[0]);
		return 1;
	}

	if (libusb_init(NULL) < 0) {
		fprintf(stderr, "Failed to initialize libusb.\n");
		return EXIT_FAILURE;
	}

	handle = initialize_device();
	if (!handle) {
		libusb_exit(NULL);
		return EXIT_FAILURE;
	}

	// Parse the usbdl arguments
	if (!strcmp(argv[1], "info"))
		task = 1;
	else if (!strcmp(argv[1], "reboot"))
		task = 2;
	else if (!strcmp(argv[1], "send"))
		task = 3;

	switch(task) {
	case 1:
		processDeviceInfo(handle);
		goto cleanup;
		break;
	case 2:
		printf("Setting bootloader mode...\n");
		ret = set_blmode(handle, kBootloaderRimBoot);
		if (ret < 0)
			goto cleanup;

		reboot_device(handle);
		goto cleanup;
	case 0:
	default:
		break;
	}

	printf("Setting bootloader mode...\n");
	ret = set_blmode(handle, kBootloaderRimBoot);
	if (ret < 0)
		goto cleanup;

	// Send password (assuming password is "password123")
	printf("Sending the password...\n");
	if (send_password(handle, "password123")) {
		printf("Password successfully sent.\n");
	} else {
		printf("Failed to send correct password.\n");
	}

	printf("Getting ready to send a loader...\n");
	send_loader(handle);

cleanup:
	libusb_release_interface(handle, 0);
	libusb_close(handle);
	libusb_exit(NULL);

	return EXIT_SUCCESS;
}
