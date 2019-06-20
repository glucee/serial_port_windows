#include "serial_port.h"
#include <algorithm>
#include <iostream>

const std::vector<uint32_t> STANDARD_BAUD_RATES{ CBR_110, CBR_300, CBR_600, CBR_1200, CBR_2400, CBR_4800, CBR_9600, CBR_14400, CBR_19200, CBR_38400, CBR_57600, CBR_115200 };

SerialPort::SerialPort(LPCSTR port_name, DWORD baud_rate) :
	port_name(port_name)
{

	if (checkBaudRate(baud_rate)) {
		this->baud_rate = baud_rate;
	}
	else {
		printf("Not a standard baud rate\n");
		//printf("ERROR: please use one of the following standard baud rates: ");
		for (auto &elem : STANDARD_BAUD_RATES) {
			std::cout << elem << std::endl;
		}
	}

	portHandle = CreateFileA(port_name, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	if (portHandle == INVALID_HANDLE_VALUE) {
		last_error = GetLastError();
		if (last_error == ERROR_FILE_NOT_FOUND) {
			printf("Unable to open port %s \n", port_name);
			//std::cout << "ERROR: Unable to open port " << port_name << std::endl;
		}
		else {
			printf("Error creating port handle %s \n", port_name);
			//std::cout << "ERROR creating port handle" << std::endl;
		}
		//system("pause");
		return;
	}

	/* Setup Serial-specific port settings */

	DCB serialConnection;

	if (!(GetCommState(portHandle, &serialConnection))) {
		printf("Couldn't get port settings\n");
		return;
	}

	serialConnection.BaudRate = baud_rate;
	serialConnection.ByteSize = 8;
	serialConnection.StopBits = ONESTOPBIT;
	serialConnection.Parity = NOPARITY;
	serialConnection.fDtrControl = DTR_CONTROL_DISABLE;

	if (!(SetCommState(portHandle, &serialConnection))) {
		printf("Couldn't apply settings\n");
		return;
	}

	/* Setup timeouts (don't appear to have much effect) */

	COMMTIMEOUTS timeouts;

	if (!(GetCommTimeouts(portHandle, &timeouts))) {
		printf("Couldn't get timeouts\n");
		return;
	}

	timeouts.ReadIntervalTimeout = 10000;
	timeouts.ReadTotalTimeoutMultiplier = 1;

	/*
	printf("ReadIntervalTimeout: %d\n", timeouts.ReadIntervalTimeout);
	printf("ReadTotalTimeoutMultiplier: %d\n", timeouts.ReadTotalTimeoutMultiplier);
	printf("ReadTotalTimeoutConstant: %d\n", timeouts.ReadTotalTimeoutConstant);
	printf("WriteTotalTimeoutMultiplier: %d\n", timeouts.WriteTotalTimeoutMultiplier);
	printf("WriteTotalTimeoutConstant: %d\n", timeouts.WriteTotalTimeoutConstant);
	*/

	if (!(SetCommTimeouts(portHandle, &timeouts))) {
		printf("Couldn't apply timeouts\n");
		return;
	}

	/* Set RTS and DTR bits to high, so the Arduino will know when to start sending */

	if (!EscapeCommFunction(portHandle, SETRTS)) {
		printf("Failed to set RTS pin\n");
		return;
	}
	else {
		printf("Set RTS pin successfully\n");
	}

	if (!EscapeCommFunction(portHandle, SETDTR)) {
		printf("Failed to set DTR pin\n");
		return;
	}
	else {
		printf("Set DTR pin successfully\n");
	}

	this->connected = true;
	PurgeComm(portHandle, PURGE_RXCLEAR | PURGE_TXCLEAR);
	Sleep(INIT_TIME_MS);

}

SerialPort::~SerialPort() {
	CloseHandle(portHandle);
	connected = false;
}

int SerialPort::bytesInBuffer() {
	ClearCommError(portHandle, &last_error, &status);

	return status.cbInQue;
}

int SerialPort::resetTimeouts() {
	COMMTIMEOUTS timeouts;

	if (!(GetCommTimeouts(portHandle, &timeouts))) {
		printf("Couldn't get timeouts\n");
		return 1;
	}

	timeouts.ReadIntervalTimeout = 0;
	timeouts.ReadTotalTimeoutMultiplier = 0;

	/*
	printf("ReadIntervalTimeout: %d\n", timeouts.ReadIntervalTimeout);
	printf("ReadTotalTimeoutMultiplier: %d\n", timeouts.ReadTotalTimeoutMultiplier);
	printf("ReadTotalTimeoutConstant: %d\n", timeouts.ReadTotalTimeoutConstant);
	printf("WriteTotalTimeoutMultiplier: %d\n", timeouts.WriteTotalTimeoutMultiplier);
	printf("WriteTotalTimeoutConstant: %d\n", timeouts.WriteTotalTimeoutConstant);
	*/

	if (!(SetCommTimeouts(portHandle, &timeouts))) {
		printf("Couldn't apply timeouts\n");
		return 1;
	}
	return 0;
}

int SerialPort::clear() {
	uint8_t *buf = { 0 };
	int totalRead = 0;
	DWORD bytesRead = 0;
	while (status.cbInQue > 0) {
		if (ReadFile(portHandle, buf, 1, &bytesRead, NULL)) {
			printf("Read failed after %d bytes\n", bytesRead);
			return 1;
		}
		totalRead += bytesRead;
	}
	printf("Cleared buffer after reading %d bytes\n", totalRead);
	return 0;
}

int SerialPort::read(uint8_t* buffer, uint8_t buffer_size) {
	DWORD bytesRead = 0;
	unsigned int toRead = 0;

	if (!ClearCommError(portHandle, &last_error, &status)) {
		printf("CCE failed with error %d\n", last_error);
	}

	if (status.cbInQue > 0) {
		if (status.cbInQue > buffer_size) {
			toRead = buffer_size;
		}
		else toRead = status.cbInQue;

		if (!ReadFile(portHandle, buffer, toRead, &bytesRead, NULL)) {
			printf("Read failed after %d bytes\n", bytesRead);
			return bytesRead;
		}
	}
	//printf("Read %d bytes\n", bytesRead);

	return bytesRead;
}

int SerialPort::write(uint8_t *buffer, uint8_t buffer_size) {
	DWORD bytesWritten = 0;
	//printf("Clearing previous errors before writing\n");
	ClearCommError(portHandle, &last_error, &status);
	if (!WriteFile(portHandle, buffer, buffer_size, &bytesWritten, NULL)) {
		DWORD dw = GetLastError();
		printf("WriteFile failed after writing %d bytes with error code %d \n", bytesWritten, dw);
		DWORD spr = SetFilePointer(portHandle, 0, nullptr, FILE_BEGIN);
		if (spr == INVALID_SET_FILE_POINTER) {
			printf("SetFilePointer failed\n");
		}
		return int(dw);
	}
	printf("Wrote %d bytes to file\n", bytesWritten);
	DWORD spr = SetFilePointer(portHandle, 0, nullptr, FILE_BEGIN);
	if (spr == INVALID_SET_FILE_POINTER) {
		printf("SetFilePointer failed\n");
	}
	return 0;
}

bool SerialPort::isConnected() {
	return this->connected;
}

bool SerialPort::checkBaudRate(DWORD baud_rate) {
	return (std::find(STANDARD_BAUD_RATES.begin(), STANDARD_BAUD_RATES.end(), baud_rate) != STANDARD_BAUD_RATES.end());
}