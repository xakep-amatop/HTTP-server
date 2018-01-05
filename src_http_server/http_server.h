#pragma once

#include <iostream>
#include <iterator>
#include <algorithm>
#include <vector>
#include <map>

#include <condition_variable>
#include <mutex>
#include <queue>

#include <regex>

#include <thread>

#include <unistd.h>
#include <cstring>
#include <termios.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <signal.h>

#include <config.h>
#include <parse_xml.h>


struct membuf : std::streambuf{
    membuf(char* begin, char* end) {
        this->setg(begin, begin, end);
    }
};

class HTTP_Server{

  typedef void (HTTP_Server::*MFP)(std::istream & , std::vector<uint8_t> &);

  public:

    HTTP_Server() = delete;
    HTTP_Server(const char  *  pathname_config);
    HTTP_Server(std::string &  pathname_config);

    void Run();

    ~HTTP_Server();

  private:

    parse_info                  info;

    std::condition_variable     cv;
    std::mutex                  mtx;
    std::queue<int>             tasks;
    std::uint32_t               number_workers;
    std::thread *               workers;

    union{
        sockaddr_in                 v4;
        sockaddr_in6                v6;
    } socket_addr;

    int                         socket_fd;
    std::vector<struct pollfd>  cli_poll;

    char                        buff[MAX_BUFFER_SIZE];


    const wchar_t HEX2DEC[256] =
      {
        /*       0  1  2  3   4  5  6  7   8  9  A  B   C  D  E  F */
        /* 0 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
        /* 1 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
        /* 2 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
        /* 3 */  0, 1, 2, 3,  4, 5, 6, 7,  8, 9,-1,-1, -1,-1,-1,-1,

        /* 4 */ -1,10,11,12, 13,14,15,-1, -1,-1,-1,-1, -1,-1,-1,-1,
        /* 5 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
        /* 6 */ -1,10,11,12, 13,14,15,-1, -1,-1,-1,-1, -1,-1,-1,-1,
        /* 7 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,

        /* 8 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
        /* 9 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
        /* A */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
        /* B */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,

        /* C */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
        /* D */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
        /* E */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
        /* F */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1
      };

    std::map<std::string, std::string> extension_mime;

    const std::map<std::string, MFP> requests =
      {
        {"GET",       & HTTP_Server::GET_Handler    }, /* The GET method is used to retrieve information from the given server using a given URI. Requests using GET should only retrieve data and should have no other effect on the data.*/
        {"HEAD",      nullptr                       }, /* Same as GET, but it transfers the status line and the header section only.*/
        {"POST",      & HTTP_Server::POST_Handler   }, /* A POST request is used to send data to the server, for example, customer information, file upload, etc. using HTML forms.*/
        {"PUT",       nullptr                       }, /* Replaces all the current representations of the target resource with the uploaded content.*/
        {"DELETE",    nullptr                       }, /* Removes all the current representations of the target resource given by URI.*/
        {"CONNECT",   nullptr                       }, /* Establishes a tunnel to the server identified by a given URI.*/
        {"OPTIONS",   nullptr                       }, /* Describe the communication options for the target resource.*/
        {"TRACE",     nullptr                       }, /* Performs a message loop back test along with the path to the target resource.*/
      };

    std::map<uint16_t, std::string> response_status =
      {
        {200, "200 OK\n"                          },

        {400, "400 Bad Request\n"                 },
        {403, "403 Forbidden\n"                   },
        {404, "404 Not Found\n"                   },
        {405, "405 Method Not Allowed\n"          },

        {500, "500 Internal Server Error\n"       },
        {505, "505 HTTP Version Not Supported\n"  },
      };

    std::string mime_types;

    std::string   UriDecode               (const std::string & sSrc);

    void          RequestHandler          ();

    void          GET_POST_Header_Handler (std::istream & request, std::vector<uint8_t> & respond, bool is_get = true);
    void          GET_Handler             (std::istream & request, std::vector<uint8_t> & respond);
    void          POST_Handler            (std::istream & request, std::vector<uint8_t> & respond);

    inline void   init                    (const char * pathname_congig);

    inline void   readFile                (const char* filename,      std::vector<uint8_t> & dst);

    inline void   PutStatus               (uint16_t status,           std::vector<uint8_t> & dst);
    inline void   PutDateTime             (                           std::vector<uint8_t> & dst);
    inline void   PutLastModified         (timespec &ts,              std::vector<uint8_t> & dst);
    inline void   PutContentLenth         (int32_t lenth,             std::vector<uint8_t> & dst);
    inline void   PutServerName           (                           std::vector<uint8_t> & dst);
    inline void   PutContentType          (std::string & filename,    std::vector<uint8_t> & dst);
    inline void   PutConnection           (                           std::vector<uint8_t> & dst);

    inline void   GetCurrentDateTime      (std::string & buffer, tm * _tstruct);
    inline void   timespec2str            (std::string & buffer, timespec & ts);

    static void   SIGUSR1_Handler         (int signum);

    inline void   RequestKillWorkers      ();
    inline void   join_all_workers        ();
    inline void   clear_tasks             ();
    inline void   additional_tools        ();
};

extern HTTP_Server * server;
