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
#include "securec.h"
#include "uart.h"
#include "fcntl.h"
#include <dirent.h>
#include <cstring>
#include "log.h"

using namespace std;
using namespace Hdc;

bool g_ioCancel = false;

// review why not use QueryDosDevice ?
bool EnumSerialPort(bool &portChange)
{
    std::vector<string> newPortInfo;
    std::vector<string> serialPortInfo;
    std::vector<string> serialPortRemoved;
    serialPortRemoved.clear();
    bool bRet = true;

#ifdef HOST_MINGW
    constexpr int MAX_KEY_LENGTH = 255;
    constexpr int MAX_VALUE_NAME = 16383;
    HKEY hKey;
    TCHAR achValue[MAX_VALUE_NAME];    // buffer for subkey name
    DWORD cchValue = MAX_VALUE_NAME;   // size of name string
    TCHAR achClass[MAX_PATH] = _T(""); // buffer for class name
    DWORD cchClassName = MAX_PATH;     // size of class string
    DWORD cSubKeys = 0;                // number of subkeys
    DWORD cbMaxSubKey;                 // longest subkey size
    DWORD cchMaxClass;                 // longest class string
    DWORD cKeyNum;                     // number of values for key
    DWORD cchMaxValue;                 // longest value name
    DWORD cbMaxValueData;              // longest value data
    DWORD cbSecurityDescriptor;        // size of security descriptor
    FILETIME ftLastWriteTime;          // last write time
    LSTATUS iRet = -1;
    std::string port;
    TCHAR strDSName[MAX_VALUE_NAME];
    errno_t nRet = 0;
    nRet = memset_s(strDSName, sizeof(TCHAR) * MAX_VALUE_NAME, 0, sizeof(TCHAR) * MAX_VALUE_NAME);
    if (nRet != EOK) {
        return false;
    }
    DWORD nBuffLen = 10;
    if (ERROR_SUCCESS == RegOpenKeyEx(HKEY_LOCAL_MACHINE, _T("HARDWARE\\DEVICEMAP\\SERIALCOMM"), 0,
                                      KEY_READ, &hKey)) {
        // Get the class name and the value count.
        iRet = RegQueryInfoKey(hKey, achClass, &cchClassName, NULL, &cSubKeys, &cbMaxSubKey,
                               &cchMaxClass, &cKeyNum, &cchMaxValue, &cbMaxValueData,
                               &cbSecurityDescriptor, &ftLastWriteTime);
        // Enumerate the key values.
        if (ERROR_SUCCESS == iRet) {
            for (DWORD i = 0; i < cKeyNum; i++) {
                cchValue = MAX_VALUE_NAME;
                achValue[0] = '\0';
                nBuffLen = MAX_KEY_LENGTH;
                if (ERROR_SUCCESS == RegEnumValue(hKey, i, achValue, &cchValue, NULL, NULL,
                                                  (LPBYTE)strDSName, &nBuffLen)) {
#ifdef UNICODE
                    strPortName = WstringToString(strDSName);
#else
                    port = std::string(strDSName);
#endif
                    newPortInfo.push_back(port);
                    auto it = std::find(serialPortInfo.begin(), serialPortInfo.end(), port);
                    if (it == serialPortInfo.end()) {
                        portChange = true;
                    }
                } else {
                    bRet = false;
                }
            }
        } else {
            bRet = false;
        }
    } else {
        bRet = false;
    }
    RegCloseKey(hKey);
#else
    DIR *dir = opendir("/dev");
    dirent *p = nullptr;
    while (dir != nullptr && ((p = readdir(dir)) != nullptr)) {
#ifdef HOST_LINUX
        if (p->d_name[0] != '.' && string(p->d_name).find("tty") != std::string::npos) {
#else
        if (p->d_name[0] != '.' && string(p->d_name).find("serial") != std::string::npos) {
#endif
            string port = "/dev/" + string(p->d_name);
            if (port.find("/dev/ttyUSB") == 0 || port.find("/dev/ttySerial") == 0 || port.find("/dev/cu.") == 0) {
                newPortInfo.push_back(port);
                auto it = std::find(serialPortInfo.begin(), serialPortInfo.end(), port);
                if (it == serialPortInfo.end()) {
                    portChange = true;
                    WRITE_LOG(LOG_INFO, "new port:%s", port.c_str());
                }
            }
        }
    }
    if (dir != nullptr) {
        closedir(dir);
    }
#endif
    for (auto &oldPort : serialPortInfo) {
        auto it = std::find(newPortInfo.begin(), newPortInfo.end(), oldPort);
        if (it == newPortInfo.end()) {
            // not found in new port list
            // we need remove the connect info
            serialPortRemoved.emplace_back(oldPort);
        }
    }

    if (!portChange) {
        // new scan empty , same as port changed
        if (serialPortInfo.size() != newPortInfo.size()) {
            portChange = true;
        }
    }
    if (portChange) {
        serialPortInfo.swap(newPortInfo);
    }
    return bRet;
}

std::string CanonicalizeSpecPath(std::string &src) {
    char resolvedPath[PATH_MAX] = { 0 };
#ifdef HOST_MINGW
    if (!_fullpath(resolvedPath, src.c_str(), PATH_MAX)) {
        return "";
    }
#else
    if (realpath(src.c_str(), resolvedPath) == nullptr) {
        return "";
    }
#endif
    std::string res(resolvedPath);
    return res;
}

#ifdef HOST_MINGW

static constexpr int PORT_NAME_LEN = 10;
static constexpr int NUM = 2;

HANDLE WinOpenSerialPort(std::string portName) {
    printf("WinOpenSerialPort start\n");
    TCHAR buf[PORT_NAME_LEN * NUM];
    #ifdef UNICODE
        _stprintf_s(buf, MAX_PATH, _T("\\\\.\\%S"), portName.c_str());
    #else
        _stprintf_s(buf, MAX_PATH, _T("\\\\.\\%s"), portName.c_str());
    #endif // UNICODE
    DWORD dwFlagsAndAttributes = FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED;
    HANDLE devUartHandle = CreateFile(buf, GENERIC_READ | GENERIC_WRITE, 0, NULL,
                                        OPEN_EXISTING, dwFlagsAndAttributes, NULL);
    if (devUartHandle == INVALID_HANDLE_VALUE)
    {
        printf("CreateFile, open handle ok\n");
    } else {
        printf("CreateFile open failed\n");
    }

    return devUartHandle;
}

bool WinSetSerialPort(HANDLE devUartHandle, string serialport, int byteSize, int baudRate) {
    bool winRet = true;
    COMMTIMEOUTS timeouts;
    GetCommTimeouts(devUartHandle, &timeouts);
    int interTimeout = 5;
    timeouts.ReadIntervalTimeout = interTimeout;
    timeouts.ReadTotalTimeoutMultiplier = 0;
    timeouts.ReadTotalTimeoutConstant = 0;
    timeouts.WriteTotalTimeoutMultiplier = 0;
    timeouts.WriteTotalTimeoutConstant = 0;
    SetCommTimeouts(devUartHandle, &timeouts);
    constexpr int max = DEFAULT_BAUD_RATE_VALUE / 8 * 2; // 2 second buffer size
    do {
        if (!SetupComm(devUartHandle, max, max)) {
            printf("SetupComm %s fail, err:%lu.", serialport.c_str(), GetLastError());
            winRet = false;
            break;
        }
        DCB dcb;
        if (!GetCommState(devUartHandle, &dcb)) {
            printf("GetCommState %s fail, err:%lu.", serialport.c_str(),
                      GetLastError());
            winRet = false;
        }
        dcb.DCBlength = sizeof(DCB);
        dcb.BaudRate = baudRate;
        dcb.Parity = 0;
        dcb.ByteSize = byteSize;
        dcb.StopBits = ONESTOPBIT;
        if (!SetCommState(devUartHandle, &dcb)) {
            printf("SetCommState %s fail, err:%lu.", serialport.c_str(),
                      GetLastError());
            winRet = false;
            break;
        }
        if (!PurgeComm(devUartHandle,
                       PURGE_RXCLEAR | PURGE_TXCLEAR | PURGE_RXABORT | PURGE_TXABORT)) {
            printf("PurgeComm  %s fail, err:%lu.", serialport.c_str(), GetLastError());
            winRet = false;
            break;
        }
        DWORD dwError;
        COMSTAT cs;
        if (!ClearCommError(devUartHandle, &dwError, &cs)) {
            printf("ClearCommError %s fail, err:%lu.", serialport.c_str(),
                      GetLastError());
            winRet = false;
            break;
        }
    } while (false);
    printf("WinSetSerialPort ret %d\n", winRet);
    if (!winRet) {
        WinCloseSerialPort(devUartHandle);
    }
    return winRet;
}

bool WinCloseSerialPort(HANDLE &handle) {
    printf("CloseSerialPort\n");
    if (handle != INVALID_HANDLE_VALUE) {
        CloseHandle(handle);
        handle = INVALID_HANDLE_VALUE;
    }
    return true;
}

ssize_t WinReadUartDev(HANDLE handle, std::vector<uint8_t> &readBuf, size_t expectedSize, OVERLAPPED &overRead) {
    ssize_t totalBytesRead = 0;
    uint8_t uartReadBuffer[MAX_UART_SIZE_IOBUF];
    DWORD bytesRead = 0;

    do {
        bytesRead = 0;
        BOOL bReadStatus = ReadFile(handle, uartReadBuffer, sizeof(uartReadBuffer), &bytesRead, &overRead);
        if (!bReadStatus) {
            if (GetLastError() == ERROR_IO_PENDING) {
                bytesRead = 0;
                DWORD dwMilliseconds = READ_GIVE_UP_TIME_OUT_TIME_MS;
                if (expectedSize == 0) {
                    dwMilliseconds = INFINITE;
                }
                if (!GetOverlappedResultEx(handle, &overRead, &bytesRead,
                                           dwMilliseconds, FALSE)) {
                    // wait io failed
                    DWORD error = GetLastError();
                    if (error == ERROR_OPERATION_ABORTED) {
                        totalBytesRead += bytesRead;
                        printf("%s error cancel read. %lu %zd", __FUNCTION__,
                                  bytesRead, totalBytesRead);
                        return totalBytesRead;
                    } else if (error == WAIT_TIMEOUT) {
                        totalBytesRead += bytesRead;
                        printf("%s error timeout. %lu %zd", __FUNCTION__, bytesRead,
                                  totalBytesRead);
                        return totalBytesRead;
                    } else {
                        printf("%s error wait io:%lu.", __FUNCTION__, GetLastError());
                    }
                    return -1;
                }
            } else {
                // not ERROR_IO_PENDING
                printf("%s  err:%lu. ", __FUNCTION__, GetLastError());
                return -1;
            }
        }
        if (bytesRead > 0) {
            readBuf.insert(readBuf.end(), uartReadBuffer, uartReadBuffer + bytesRead);
            totalBytesRead += bytesRead;
        }
    } while (readBuf.size() < expectedSize || bytesRead == 0);
    return totalBytesRead;
}

ssize_t WinWriteUartDev(HANDLE handle, uint8_t *data, const size_t length, OVERLAPPED &ovWrite) {
    ssize_t totalBytesWrite = 0;
    do {
        DWORD bytesWrite = 0;
        BOOL bWriteStat = WriteFile(handle, data + totalBytesWrite, length - totalBytesWrite, &bytesWrite, &ovWrite);
        if (!bWriteStat) {
            if (GetLastError() == ERROR_IO_PENDING) {
                if (!GetOverlappedResult(handle, &ovWrite, &bytesWrite, TRUE)) {
                    printf("%s error wait io:%lu. bytesWrite %lu", __FUNCTION__,
                           GetLastError(), bytesWrite);
                    return -1;
                }
            } else {
                printf("%s err:%lu. bytesWrite %lu", __FUNCTION__, GetLastError(),
                       bytesWrite);
                return -1;
            }
        }
        totalBytesWrite += bytesWrite;
    } while (totalBytesWrite < signed(length));
    return totalBytesWrite;
}

#else

int GetUartSpeed(int speed) {
    switch (speed) {
        case UART_SPEED2400:
            return (B2400);
        case UART_SPEED4800:
            return (B4800);
        case UART_SPEED9600:
            return (B9600);
        case UART_SPEED115200:
            return (B115200);
        case UART_SPEED921600:
            return (B921600);
        case UART_SPEED1500000:
            return (B1500000);
        default:
            return (B921600);
    }
}

int GetUartBits(int bits) {
    switch (bits) {
        case UART_BIT1:
            return (CS7);
        case UART_BIT2:
            return (CS8);
        default:
            return (CS8);
    }
}

int OpenSerialPort(std::string portName) {
    int uartHandle = -1;
    if ((uartHandle = open(portName.c_str(), O_RDWR | O_NOCTTY | O_NDELAY)) < 0) {
        WRITE_LOG(LOG_WARN, "%s: cannot open uartHandle: errno=%d\n", portName.c_str(), errno);
        return -1;
    }
    usleep(UART_IO_WAIT_TIME_100);
    // cannot open with O_CLOEXEC, must fcntl
    fcntl(uartHandle, F_SETFD, FD_CLOEXEC);
    int flag = fcntl(uartHandle, F_GETFL);
    flag &= ~O_NONBLOCK;
    fcntl(uartHandle, F_SETFL, flag);

    return uartHandle;
}

#ifdef HOST_MAC
int SetSerial(int fd, int nSpeed, int nBits, char nEvent, int nStop) {
    struct termios options, oldttys1;
    if (tcgetattr(fd, &oldttys1) != 0) {
        constexpr int buf_size = 1024;
        char buf[buf_size] = { 0 };
        strerror_r(errno, buf, buf_size);
        return ERR_GENERIC;
    }

    errno_t nRet = 0;
    nRet = memcpy_s(&options, sizeof(options), &oldttys1, sizeof(options));
    if (nRet != EOK) {
        return ERR_GENERIC;
    }
    cfmakeraw(&options);
    options.c_cc[VMIN] = 0;
    options.c_cc[VTIME] = 10; // 10 * 1/10 sec : 1 sec

    cfsetspeed(&options, B19200);
    options.c_cflag |= GetUartBits(nBits); // Use 8 bit words
    options.c_cflag &= ~PARENB;

    speed_t speed = nSpeed;
    if (ioctl(fd, IOSSIOSPEED, &speed) == -1) {
    }
    if ((tcsetattr(fd, TCSANOW, &options)) != 0) {
        return ERR_GENERIC;
    }
    if (ioctl(fd, IOSSIOSPEED, &speed) == -1) {
    }
    return RET_SUCCESS;
}
#else
int SetSerial(int fd, int nSpeed, int nBits, char nEvent, int nStop) {
    struct termios newttys1, oldttys1;
    if (tcgetattr(fd, &oldttys1) != 0) {
        constexpr int buf_size = 1024;
        char buf[buf_size] = { 0 };
        strerror_r(errno, buf, buf_size);
        return ERR_GENERIC;
    }
    bzero(&newttys1, sizeof(newttys1));
    newttys1.c_cflag = GetUartSpeed(nSpeed);
    newttys1.c_cflag |= (CLOCAL | CREAD);
    newttys1.c_cflag &= ~CSIZE;
    newttys1.c_lflag &= ~ICANON;
    newttys1.c_cflag |= GetUartBits(nBits);
    switch (nEvent) {
        case 'O':
            newttys1.c_cflag |= PARENB;
            newttys1.c_iflag |= (INPCK | ISTRIP);
            newttys1.c_cflag |= PARODD;
            break;
        case 'E':
            newttys1.c_cflag |= PARENB;
            newttys1.c_iflag |= (INPCK | ISTRIP);
            newttys1.c_cflag &= ~PARODD;
            break;
        case 'N':
            newttys1.c_cflag &= ~PARENB;
            break;
        default:
            break;
    }
    if (nStop == UART_STOP1) {
        newttys1.c_cflag &= ~CSTOPB;
    } else if (nStop == UART_STOP2) {
        newttys1.c_cflag |= CSTOPB;
    }
    newttys1.c_cc[VTIME] = 0;
    newttys1.c_cc[VMIN] = 0;
    if (tcflush(fd, TCIOFLUSH)) {
        return ERR_GENERIC;
    }
    if ((tcsetattr(fd, TCSANOW, &newttys1)) != 0) {
        return ERR_GENERIC;
    }
    return ERR_SUCCESS;
}
#endif

ssize_t ReadUartDev(int handle, std::vector<uint8_t> &readBuf, size_t expectedSize) {
    ssize_t totalBytesRead = 0;
    uint8_t uartReadBuffer[MAX_UART_SIZE_IOBUF];
    ssize_t bytesRead = 0;

    do {
        bytesRead = 0;
        int ret = 0;
        fd_set readFds;
        FD_ZERO(&readFds);
        FD_SET(handle, &readFds);
        const constexpr int msTous = 1000;
        const constexpr int sTous = 1000 * msTous;
        struct timeval tv;
        tv.tv_sec = 0;

        if (expectedSize == 0) {
            tv.tv_usec = WAIT_RESPONSE_TIME_OUT_MS * msTous;
            tv.tv_sec = tv.tv_usec / sTous;
            tv.tv_usec = tv.tv_usec % sTous;
#ifdef HDC_HOST
            // only host side need this
            // in this caes
            // We need a way to exit from the select for the destruction and recovery of the
            // serial port read thread.
            ret = select(handle + 1, &readFds, nullptr, nullptr, &tv);
#else
            ret = select(handle + 1, &readFds, nullptr, nullptr, nullptr);
#endif
        } else {
            // when we have expect size , we need timeout for link data drop issue
            tv.tv_usec = READ_GIVE_UP_TIME_OUT_TIME_MS * msTous;
            tv.tv_sec = tv.tv_usec / sTous;
            tv.tv_usec = tv.tv_usec % sTous;
            ret = select(handle + 1, &readFds, nullptr, nullptr, &tv);
        }
        if (ret == 0 and expectedSize == 0) {
            // no expect but timeout
            if (g_ioCancel) {
                g_ioCancel = true;
                return totalBytesRead;
            } else {
                continue;
            }
        } else if (ret == 0) {
            // we expected some byte , but not arrive before timeout
            return totalBytesRead;
        } else if (ret < 0) {
            return -1; // wait failed.
        } else {
            // select > 0
            size_t maxReadSize = expectedSize - totalBytesRead;
            if (maxReadSize > MAX_UART_SIZE_IOBUF) {
                maxReadSize = MAX_UART_SIZE_IOBUF;
            }
            bytesRead = read(handle, uartReadBuffer, maxReadSize);
            if (bytesRead <= 0) {
                // read failed !
                return -1;
            }
        }
        if (bytesRead > 0) {
            readBuf.insert(readBuf.end(), uartReadBuffer, uartReadBuffer + bytesRead);
            totalBytesRead += bytesRead;
        }
    } while (readBuf.size() < expectedSize or bytesRead == 0); // if caller know how many bytes it want
    return totalBytesRead;
}

ssize_t WriteUartDev(int handle, uint8_t *data, const size_t length) {
    ssize_t totalBytesWrite = 0;
    do {
        ssize_t bytesWrite = 0;
        bytesWrite = write(handle, data + totalBytesWrite, length - totalBytesWrite);
        if (bytesWrite < 0) {
            if (errno == EINTR or errno == EAGAIN) {
                continue;
            } else {
                // we don't know how to recory in this function
                // need reopen device ?
                constexpr int buf_size = 1024;
                char buf[buf_size] = { 0 };
                strerror_r(errno, buf, buf_size);
                return -1;
            }
        } else {
            // waits until all output written to the object referred to by fd has been transmitted.
            tcdrain(handle);
        }
        totalBytesWrite += bytesWrite;
    } while (totalBytesWrite < signed(length));

    return totalBytesWrite;
}

bool CloseSerialPort(int &handle) {
    if (handle != -1)
    {
        return CloseFd(handle) >= 0;
    } else {
        return true;
    }
}

int CloseFd(int &fd) {
    int rc = 0;
#ifndef HDC_HOST
#endif
    if (fd > 0) {
        rc = close(fd);
        if (rc < 0) {
            char buffer[Hdc::BUF_SIZE_DEFAULT] = { 0 };
#ifdef _WIN32
            strerror_s(buffer, Hdc::BUF_SIZE_DEFAULT, errno);
#else
            strerror_r(errno, buffer, Hdc::BUF_SIZE_DEFAULT);
#endif
        } else {
            fd = -1;
        }
    }
    return rc;
}

#endif
