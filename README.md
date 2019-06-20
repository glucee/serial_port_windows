# Serial_Port_Win
Serial Port Read/Write Library for Windows

The library contains the following functions:

	SerialPort(LPCSTR port_name, DWORD baud_rate); // Open a port
	~SerialPort(); // disconnect a port
	int bytesInBuffer(); // detect the data in the read buffer
	int read(uint8_t* buffer, uint8_t buffer_size); // read data
	int write(uint8_t* buffer, uint8_t buffer_size); // wirte data 
	int clear(); // clear the read buffer
	int resetTimeouts(); // set the timeout
	bool isConnected(); // check if the port is connected
