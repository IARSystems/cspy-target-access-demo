/**************************************************
 *
 * ITM Logger example (itm-logger.cpp)
 *
 * Copyright (c) 2023 IAR Systems AB.
 *
 * See LICENSE for detailed license information.
 *
 **************************************************/

#include "TargetAccessClientSDK.h"

#include <iostream>
#include <iomanip>
#include <string>
#include <thread>
#include <chrono>

#if defined(_WIN32) || defined(_WIN64)
#include <Windows.h>
BOOL WINAPI ConsoleCloseHandler(DWORD dwCtrlType)
{
  switch (dwCtrlType)
  {
    case CTRL_C_EVENT:
      std::cout << "Ctrl-C detected." << std::setfill(' ') << 
        std::setw(50) << std::endl;
      ::TargetAccessShutdown();
      return TRUE;
      
    case CTRL_CLOSE_EVENT:
      std::cout << "TargetAccessClientITMdemo is shutting down" << 
        std::setfill(' ') << std::setw(50) << "" << std::endl;
      ::TargetAccessShutdown();
      return TRUE;
      
    default:
      return FALSE;
  }
}
#endif

class DemoException : public std::exception
{
public:
  DemoException()
  {
  }
};

void ReportITMStatus()
{
  ITMListenerStatus status;
  
  status.listening = 0;
  status.channels = 0;
  status.bufferCapacity = 0;
  status.bufferSize = 0;
  ::TargetAccessGetItmListenerStatus(&status);
  
  std::cout << "ITM Listener: " << (status.listening != 0 ? "on" : "off") << " ";
  std::cout << "Channels: 0x" << std::hex << std::setfill('0') << std::setw(8) << status.channels << std::dec << " ";
  std::cout << "Buffer capacity: " << status.bufferCapacity << " ";
  std::cout << "Buffer size: " << status.bufferSize << "\r";
}

void ReportError()
{
  std::string errorMsg;
  
  errorMsg.resize(512);
  ::TargetAccessGetLastErrorMsg(const_cast<char *>(errorMsg.data()), errorMsg.size()); 
  std::cout << "Error: " << errorMsg << std::endl; 
  std::cout << "Client protocol version: " << ::TargetAccessGetClientProtocolVersion() << std::endl;
  std::cout << "Server protocol version: " << ::TargetAccessGetServerProtocolVersion() << std::endl;
}

int RunItmLogging()
{
  if (::TargetAccessConfigureItmListener(0xffffffff, 1000) != 0)
    throw DemoException();
  
  if (::TargetAccessStartItmListener() != 0)
    throw DemoException();
  
  auto defaultDuration = std::chrono::seconds(1);
  auto duration = defaultDuration;
  bool isValid = false;
  
  while (1)
  {
    ITMEvent itmEvent;
    if (::TargetAccessReadItmEvent(&itmEvent, &isValid) != TARGET_ACCESS_STATUS_OK)
        throw DemoException();
  
    if (isValid)
    {
      std::cout << "[" << std::hex << itmEvent.timeStamp << std::dec <<
      "] ITM port " << (int)itmEvent.portId << ": ";
      uint32_t value = 0;
      
      switch (itmEvent.dataLength)
      {
      case 1:
        value = itmEvent.data[0];
        break;
      case 2:
        value = *reinterpret_cast<uint16_t *>(itmEvent.data);
        break;
      case 4:
        value = *reinterpret_cast<uint32_t *>(itmEvent.data);
        break;
      }
      std::cout << std::hex << std::setfill('0') << std::setw(8) << value << std::dec;  
      std::cout << " (" << itmEvent.dataLength << " byte" << (itmEvent.dataLength > 1 ? "s" : "") << " length)";
      std::cout << std::setw(50) << std::setfill(' ') << "" << std::endl;
      duration = std::chrono::seconds(0); // Keep reading events until the buffer is empty
    }
    else
    {
      duration = defaultDuration; // Pause with 1 second delays when no data is recorded
      ::ReportITMStatus();
    }
    std::this_thread::sleep_for(duration);
  }
  return 0;
}

int main(int argc, const char **argv)
{
#if defined(_WIN32) || defined(_WIN64)
  ::SetConsoleCtrlHandler(ConsoleCloseHandler, TRUE);
#endif

  char *portStr = nullptr;
  size_t sz = 0;
  int port = 9931; // server's default UDP port
  if (_dupenv_s(&portStr, &sz, "TARGET_ACCESS_PORT") == 0 && portStr != nullptr)
  {
    port = std::stoi(portStr);
    free(portStr);
  }
  try
  {
    // Initialize client
    if (::TargetAccessInitializeWithPort("localhost", port) != TARGET_ACCESS_STATUS_OK)
    {
      throw DemoException();
      ::TargetAccessShutdown();
      return 1;
    }
    ::RunItmLogging();
  }
  catch (const DemoException &)
  {
    ::ReportError();
  }
  ::TargetAccessShutdown();
  return 0;
}
