# bb-usbdl
A small program that can talk to the RAMLoader recovery protocol of blackberry phones.

## Disclaimer
You will be solely responsible for any damage caused to your hardware/software/warranty/data/cat/etc...

## Description
The BlackBerry download protocol allows booting payloads from USB. This method of boot requires a USB host to send a signed payload via the USB port.

The purpose of this tool is to get far enough in reverse-engineering the protocol in order to be able to sucessfully boot and send signed payloads. In the future, we could try to exploit the signature verification.

## Supported targets
* BBOS 10 devices

## Boot flow
For MSM8960, the current known information is from reverse engineering the boot flow by comparing the flow of typical LA MSM8960 phones with what we currently know for the BlackBerry Passport + autoloader firmware leaks.

![bootflow](https://github.com/ivoszbg/bb-usbdl/blob/main/image/bootflow.jpg?raw=true)

* PBL -> BlackBerry devices seem to have disabled access to QDL.
* BBSS -> Not much information is known about this. The current guess is that it's loaded right after the bootrom and is used for security variables.
* BOOT0 -> A proprietary replacement for the typical SBL1/2/3 combo. It's entirely written by BlackBerry and is responsible for loading firmware for APSS, RPM and TZ. It also contains a restoration protocol called RAMLoader.
* STARTUP.BIN -> This is the first payload that gets executed in the DRAM. It sets us up for loading an ImageFS (IFS) - a combo of a kernel and a ramdisk.
* IFS -> In this stage the QNX kernel and ramdisk get executed and we make our way to the userspace.

## Access to RAMLoader
On boot, the device checks whenever it's connected to a USB host. If it is, you get a 5 to 10 seconds gap where it's waiting for a mode command, and if it doesn't receive any, the device continues with the boot flow. You'll know you're there when the notification LED is red. For testing bb-usbdl, you can just disconnect the battery and unplug/replug the usb cable.

Each command consists of the following header and additional data appended after it (if needed):
```
typedef struct ControlMessageHeader {
	unsigned short type;
	unsigned short packetSize;
	unsigned char command;
	unsigned char mode;
	unsigned short packetId;
} __attribute__((packed)) ControlMessageHeader;
```

For more information about each individual part of the header, check the code.

Once it receives a mode command, the device aborts the normal boot process and waits for next commands. From there, you have three options:

1. Reboot
2. Dump the environment information
3. Send the userspace password, prepare it for loading a payload and send it.

Currently, this tool implements the first two, but still doesn't fully implement sending a payload. Password sending is a bit broken as well, so having no password is recommended. Keep in mind that if you send the wrong password 10 times, it'll count as wrong password attemps in the userspace as well and you'll hit the 10/10 password attempts.

## Usage
```
./b-usbdl <arg1>
	arg1: mode of operation
		info: outputs device environment info to a text file
		reboot: reboots the device
		send: sets us up to send a payload
```

## License
Please see [LICENSE](/LICENSE).
