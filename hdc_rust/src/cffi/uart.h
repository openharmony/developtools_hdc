/*
 * Copyright (C) 2021 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef HDC_UART_H
#define HDC_UART_H

#include <cassert>
#include <chrono>
#include <cinttypes>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fcntl.h>
#include <functional>
#include <numeric>
#include <sstream>
#include <sys/types.h>
#include <unordered_set>
#include <unistd.h>
#include <vector>

#ifdef HOST_MINGW
#include "tchar.h"
#include "windows.h"
#include <setupapi.h>
#include <winnt.h>
#include <termios.h>
#else
#include <fcntl.h> // open close
#include <pthread.h>
#include <termios.h> // truct termios
#endif

enum UartTimeConst {
    UV_TIMEOUT = 10,
    UV_REPEAT = 100,
    TIMEOUTS_R_INTERALTIMEOUT = 1000,
    TIMEOUTS_R_TOALTIMEOUTMULTIPLIER = 500,
    TIMEOUTS_R_TIMEOUTCONSTANT = 5000
};
enum UartSetSerialNBits {
    UART_BIT1 = 7,
    UART_BIT2 = 8
};
enum UartSetSerialNSpeed {
    UART_SPEED2400 = 2400,
    UART_SPEED4800 = 4800,
    UART_SPEED9600 = 9600,
    UART_SPEED115200 = 115200,
    UART_SPEED921600 = 921600,
    UART_SPEED1500000 = 1500000
};
enum UartSetSerialNStop {
    UART_STOP1 = 1,
    UART_STOP2 = 2
};

const std::string CMDSTR_TMODE_UART = "uart";
const std::string UART_HDC_NODE = "/dev/ttyS4";
const std::string CONSOLE_ACTIVE_NODE = "/sys/class/tty/console/active";
constexpr int UART_IO_WAIT_TIME_100 = 100;
constexpr int UART_IO_WAIT_TIME = 1000;

const int ERR_GENERIC = -1;
const int ERR_SUCCESS = 0;
constexpr uint16_t MAX_UART_SIZE_IOBUF = 4096; // MAX_SIZE_IOBUF;
constexpr size_t MAX_READ_BUFFER = MAX_UART_SIZE_IOBUF * 10;
constexpr int WAIT_RESPONSE_TIME_OUT_MS = 1000; // 1000ms
constexpr int READ_GIVE_UP_TIME_OUT_TIME_MS = 500; // 500ms
constexpr uint16_t BUF_SIZE_DEFAULT = 1024;
constexpr uint32_t DEFAULT_BAUD_RATE_VALUE = 1500000;

#ifdef HOST_MINGW
HANDLE WinOpenSerialPort(std::string portName);
bool WinSetSerialPort(HANDLE devUartHandle, std::string serialport, int byteSize, int baudRate);
bool WinCloseSerialPort(HANDLE& handle);
ssize_t WinReadUartDev(HANDLE handle, std::vector<uint8_t> &readBuf, size_t expectedSize, OVERLAPPED &overRead);
ssize_t WinWriteUartDev(HANDLE handle, uint8_t *data, const size_t length, OVERLAPPED &ovWrite);

#else
int GetUartSpeed(int speed);
int GetUartBits(int bits);
int OpenSerialPort(std::string portName);
int SetSerial(int fd, int nSpeed, int nBits, char nEvent, int nStop);
#endif

ssize_t ReadUartDev(int handle, std::vector<uint8_t> &readBuf, size_t expectedSize);
ssize_t WriteUartDev(int handle, uint8_t *data, const size_t length);
bool CloseSerialPort(int& handle);
int CloseFd(int &fd);

#endif
