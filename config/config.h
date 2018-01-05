#pragma once

#include <iostream>

#define MAX_LISTENING_CLIENTS (100)

#define MAX_BUFFER_SIZE       (8192)

#define NO_BLOCKING_POLL      (0)      //non-blocking poll
#define INFTIM                (-1)     //blocking poll to infinity time
#define MAX_TIMEOUT_POLL      (INFTIM) //in ms or above constants

#define FILE_MIME_TYPES       ("/etc/mime.types")

struct parse_info{
  std::string     config_path;
  std::string     ip;
  std::string     root_path;
  std::uint32_t   number_workers;
  std::uint16_t   port;
  bool            is_ipv4;
};