#include <http_server.h>

void HTTP_Server::additional_tools(){
  {
    struct termios old, _new;
    /* Turn echoing off and fail if we can't. */
    if (!tcgetattr (fileno (stdin), &old)){
      _new = old;
      _new.c_lflag &= ~ECHO;
      tcsetattr (fileno(stdin), TCSAFLUSH, & _new);
    }
  }

  //form map: extension, MIME type
  {
    std::ifstream types(FILE_MIME_TYPES);
    mime_types.insert(mime_types.begin(), std::istreambuf_iterator<char>(types),
                     std::istreambuf_iterator<char>());

    std::regex                  type("(\\b[\\w\\-\\.]+/[\\w\\-\\.\\+]+)\\s+(([\\w\\+\\-]+\\s)+)");
    std::string::const_iterator searchStart( mime_types.cbegin() );
    std::smatch                 result;

    while ( std::regex_search( searchStart, mime_types.cend(), result, type ) ){
      std::istringstream  types(result[2].str());
      while(!types.eof()){
        std::string ext;
        types >> ext;
        extension_mime[ext] = result[1].str();
      }
      searchStart += result.position() + result.length();
    }
  }

  // set handler to signal SIGUSR1
  if (signal(SIGUSR1, SIGUSR1_Handler) == SIG_ERR) {
    std::cerr << "\nAn error occurred while setting a signal handler.\n\n";
  }
}

void HTTP_Server::init(const char * pathname_config){

  // load server configuration from xml file
  {
    ParseXmlConfig * config = new ParseXmlConfig(pathname_config, info);
    delete config;
  }

  info.config_path = pathname_config; // for next reloading by SIGUSR1 handler

  std::cout << "\n\033[1;35mInitializing server...\033[0m\n\n";

  memset(&socket_addr, 0, sizeof(socket_addr));

  socket_fd = socket(info.is_ipv4 ? AF_INET : AF_INET6, SOCK_STREAM, IPPROTO_TCP);

  if (socket_fd < 0) {
    std::cerr << "\n\033[1;31mError!!! Cannot create socket! \033[0m\033[1;35" << strerror(errno) << "\033[0m\n\n";
    exit(EXIT_FAILURE);
  }
  int rc, on = 1;

  rc = setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on));
  if (rc < 0){
    std::cerr << "\n\033[1;31mError!!! Function setsockopt! \033[0m\033[1;35" << strerror(errno) << "\033[0m\n\n";
    close(socket_fd);
    exit(EXIT_FAILURE);
  }

  rc = ioctl(socket_fd, FIONBIO, (char *)&on);
  if (rc < 0){
    std::cerr << "\n\033[1;31mError!!! Function ioctl! \033[0m\033[1;35" << strerror(errno) << "\033[0m\n\n";
    close(socket_fd);
    exit(EXIT_FAILURE);
  }

  if(info.is_ipv4){
    socket_addr.v4.sin_family = AF_INET;
    socket_addr.v4.sin_port   = htons(info.port);
    rc                        = inet_pton(AF_INET, info.ip.c_str(), &socket_addr.v4.sin_addr);
  }
  else{
    socket_addr.v6.sin6_family = AF_INET6;
    socket_addr.v6.sin6_port   = htons(info.port);
    rc                         = inet_pton(AF_INET6, info.ip.c_str(), &socket_addr.v6.sin6_addr);
  }
  if (rc <= 0 ){
    std::cerr << "\n\033[1;31mError!!! Function inet_pton! \033[0m\033[1;35" << strerror(errno) << "\033[0m\n\n";
    close(socket_fd);
    exit(EXIT_FAILURE);
  }

  if (bind(socket_fd, (sockaddr *)&socket_addr, info.is_ipv4 ? sizeof(socket_addr.v4) : sizeof(socket_addr.v6)) == -1) {
    std::cerr << "\n\033[1;31mError!!! Bind failed! \033[0m\033[1;35m" << strerror(errno) << "\033[0m\n\n";
    close(socket_fd);
    exit(EXIT_FAILURE);
  }

  if (listen(socket_fd, MAX_LISTENING_CLIENTS) == -1) {
    std::cerr << "\n\033[1;31mError!!! Listen failed! \033[0m\033[1;35m" << strerror(errno) << "\033[0m\n\n";
    close(socket_fd);
    exit(EXIT_FAILURE);
  }

  { // add socket that receive new connection to vector of pollfd structures
    pollfd tmp;
    tmp.fd        = socket_fd;
    tmp.events    = POLLIN;

    cli_poll.push_back(tmp);
  }

  // create "pool" of thread, and start each thread
  workers = new std::thread[info.number_workers];

  for(std::size_t i = 0; i < info.number_workers; ++i){
    workers[i] = std::thread([this](){this->RequestHandler();});
  }
}

HTTP_Server::HTTP_Server(std::string & pathname_config){
  init(pathname_config.c_str());
  additional_tools();
}

HTTP_Server::HTTP_Server(const char * pathname_config){
  init(pathname_config);
  additional_tools();
}

void HTTP_Server::readFile(const char* filename, std::vector<uint8_t> & data){
  PutServerName(data);

  // open the file:
  std::string _filename = filename;
  std::ifstream file(_filename, std::ios::binary);

  // Stop eating new lines in binary mode!!!
  file.unsetf(std::ios::skipws);

  // get its size:
  int fileSize;

  file.seekg(0, std::ios::end);
  fileSize = file.tellg();

  file.seekg(0, std::ios::beg);

  PutContentLenth(fileSize,  data);
  PutContentType(_filename, data);

  data.push_back('\n'); // payload separation from the header

  // read the data:
  data.insert(data.end(),
             std::istream_iterator<uint8_t>(file),
             std::istream_iterator<uint8_t>());
}

// Get current date/time, example: Date: Tue, 15 Nov 1994 08:12:31 GMT
// rfc7232
void HTTP_Server::GetCurrentDateTime(std::string & buffer, tm * _tstruct) {
  time_t     now = time(0);
  tm         tstruct;
  char       local_buf[80];

  if(_tstruct){
    tstruct = *_tstruct;
  }
  else{
    tstruct = *localtime(&now);
  }
  strftime(local_buf, sizeof(local_buf), "%a, %d %b %Y %H:%M:%S %Z\n", &tstruct);

  buffer.append(local_buf);
}

void HTTP_Server::timespec2str(std::string & buffer, timespec & ts) {
  struct tm t;
  tzset();
  if (localtime_r(&(ts.tv_sec), &t) == NULL){
    // for future handler
  }

  GetCurrentDateTime(buffer, & t);
}

void HTTP_Server::PutStatus(uint16_t status, std::vector<uint8_t> & dst){
  std::copy(response_status[status].begin(), response_status[status].end(), std::back_inserter(dst));
}

void HTTP_Server::PutDateTime(std::vector<uint8_t> & dst){
  std::string cur_datetime = "Date: ";
  GetCurrentDateTime(cur_datetime, nullptr);
  std::copy(cur_datetime.begin(), cur_datetime.end(), std::back_inserter(dst));
}

void HTTP_Server::PutContentLenth(int32_t lenth, std::vector<uint8_t> & dst){
  std::string str_cl = "Content-Length: " + std::to_string(lenth) + "\n";
  std::copy(str_cl.begin(), str_cl.end(), std::back_inserter(dst));
}

void HTTP_Server::PutLastModified(timespec & ts, std::vector<uint8_t> & dst){
  std::string cur_datetime = "Last-Modified: ";
  timespec2str(cur_datetime, ts);
  std::copy(cur_datetime.begin(), cur_datetime.end(), std::back_inserter(dst));
}

void HTTP_Server::PutServerName(std::vector<uint8_t> & dst){
  std::string str = "Server: YP\n";
  std::copy(str.begin(), str.end(), std::back_inserter(dst));
}

void HTTP_Server::PutConnection(std::vector<uint8_t> & dst){
  std::string str = "Connection: close\n";
  std::copy(str.begin(), str.end(), std::back_inserter(dst));
}

void HTTP_Server::PutContentType(std::string & filename, std::vector<uint8_t> & dst){
  std::string str_ct      = "Content-Type: ";

  char *      extension   = std::strrchr(const_cast<char*>(filename.c_str()), '.');

  if(extension && *++extension){
    auto it_type = extension_mime.find(extension);
    if (it_type != extension_mime.end()){
      str_ct += it_type->second;
    }
  }

  str_ct.push_back('\n');
  std::copy(str_ct.begin(), str_ct.end(), std::back_inserter(dst));
}

std::string HTTP_Server::UriDecode(const std::string & sSrc){
  // Note from RFC1630: "Sequences which start with a percent
  // sign but are not followed by two hexadecimal characters
  // (0-9, A-F) are reserved for future extension"

  const unsigned char * pSrc = (const unsigned char *)sSrc.c_str();
  const int SRC_LEN = sSrc.length();
  const unsigned char * const SRC_END = pSrc + SRC_LEN;
  // last decodable '%'
  const unsigned char * const SRC_LAST_DEC = SRC_END - 2;

  char * const pStart = new char[SRC_LEN];
  char * pEnd = pStart;

  while (pSrc < SRC_LAST_DEC){
    if (*pSrc == '%'){
      char dec1, dec2;
      if (-1 != (dec1 = HEX2DEC[*(pSrc + 1)]) &&
          -1 != (dec2 = HEX2DEC[*(pSrc + 2)])){
        *pEnd++ = (dec1 << 4) + dec2;
        pSrc += 3;
        continue;
      }
    }

    *pEnd++ = *pSrc++;
  }

  // the last 2- chars
  while (pSrc < SRC_END)
    *pEnd++ = *pSrc++;

  std::string sResult(pStart, pEnd);
  delete [] pStart;
  return sResult;
}

void HTTP_Server::GET_POST_Header_Handler(std::istream & request, std::vector<uint8_t> & respond, bool is_get){
  std::string protocol;
  std::string pathname;

  request >> pathname;
  request >> protocol;

  if (protocol != "HTTP/1.1"){
    std::cerr << "Protocol doesn't support: " << protocol << std::endl;
    PutStatus(505, respond);
    PutDateTime(respond);
    readFile((info.root_path + "505.html").c_str(), respond);
    return;
  }

  pathname = UriDecode(pathname);

  if(is_get){
    const char * get_params = std::strchr(pathname.c_str(), '?');
    pathname                = pathname.substr(0, get_params ? (get_params - pathname.c_str()) : pathname.npos );
  }

  pathname = info.root_path + ((pathname[0] == '/') ? pathname.substr(1, pathname.size() - 1) : pathname);

  struct stat _stat = {0};
  if (stat(pathname.c_str(), &_stat) < 0){
    std::string path = "\nError!!! `" + pathname + "` ";
    std::perror(path.c_str());
    PutStatus(404, respond);
    PutDateTime(respond);
    readFile((info.root_path + "404.html").c_str(), respond);
    return;
  }

  if(_stat.st_mode & S_IFDIR){
    PutStatus(403, respond);
    PutDateTime(respond);
    readFile((info.root_path + "403.html").c_str(), respond);
    return;
  }

  PutStatus(200, respond);
  PutDateTime(respond);

  PutLastModified(_stat.st_mtim, respond);

  readFile(pathname.c_str(), respond);
}

void HTTP_Server::GET_Handler(std::istream & request, std::vector<uint8_t> & respond){
  GET_POST_Header_Handler(request, respond);
}

void HTTP_Server::POST_Handler(std::istream & request, std::vector<uint8_t> & respond){
  GET_POST_Header_Handler(request, respond, false);
}

void HTTP_Server::RequestHandler(){
  //sigset_t signal_mask;  /* signals to block */
  //sigemptyset (&signal_mask);
  //sigaddset (&signal_mask, SIGINT);
  //sigaddset (&signal_mask, SIGTERM);
  //int rc = pthread_sigmask (SIG_BLOCK, &signal_mask, NULL);

  //if(!rc){
  //  return;
  //  }
  while(true){
    int fd = -1;
    std::unique_lock<std::mutex> lk(mtx);
    cv.wait(lk, [this]{return this->tasks.size();});
    fd = tasks.front();
    tasks.pop();
    lk.unlock();

    if(fd == -1){
      return;
    }

    std::vector<uint8_t> msg;
    int rc;
    do{
      rc = recv(fd, buff, sizeof(buff) - 2, 0);
      if (rc <= 0){
        close(fd);
        return;
      }
      else{
        buff[rc] = '\0';
        std::copy(buff, buff +rc, std::back_inserter(msg));
        if ((unsigned) rc < sizeof(buff) - 2){
          break;
        }
      }
    } while(true);

    membuf sbuf(reinterpret_cast<char*>(msg.data()), reinterpret_cast<char*>(msg.data() + msg.size()));
    std::istream in(&sbuf);

    std::string method;
    in >> method;

    std::vector<uint8_t> respond;
    std::string str = "HTTP/1.1 ";
    std::copy(str.begin(), str.end(), std::back_inserter(respond));

    auto it_request = requests.find(method);
    if(it_request == requests.end()){
      std::cerr << "Error!!! Unknown method!\n";
      PutStatus(400, respond);
      PutDateTime(respond);
      readFile((info.root_path + "400.html").c_str(), respond);
    }
    else{
      MFP function = it_request->second;
      if (function){
        (this->*function)(in, respond);
      }
      else{
        std::cerr << "\nError!!! Defined method: " << method << ", but not defined him handler!\n";
        PutStatus(405, respond);
        PutDateTime(respond);
        readFile((info.root_path + "405.html").c_str(), respond);
      }
    }

    rc = send(fd, respond.data(), respond.size(), 0);
    close(fd);
  }
}

void HTTP_Server::join_all_workers(){
  for(std::size_t i = 0; i < info.number_workers; ++i){
    if(workers[i].joinable()){
      workers[i].join();
    }
  }
}

void HTTP_Server::clear_tasks(){
  std::unique_lock<std::mutex> lk(mtx);
  std::queue<int> empty;
  std::swap(tasks, empty);
  lk.unlock();
}

void HTTP_Server::RequestKillWorkers(){

  if(!workers) return;

  for(std::size_t i = 0; i < info.number_workers; ++i){
    std::unique_lock<std::mutex> lk(mtx);
    tasks.push(-1);
    lk.unlock();
    cv.notify_one();
   }

  join_all_workers();
}

void HTTP_Server::SIGUSR1_Handler(int signum){
  server->RequestKillWorkers();
  delete [] server->workers;

  for (auto it_poll = server->cli_poll.begin(); it_poll != server->cli_poll.end(); ++it_poll){
   if(it_poll->fd != -1) close(it_poll->fd);
  }

  server->clear_tasks();
  server->cli_poll.clear();

  server->init(server->info.config_path.c_str());

  std::cout << "\033[1;37mStart listening at: \033[0m\033[1;33m" << server->info.ip
            << ":" << server->info.port << "\033[0m\n";
}

void HTTP_Server::Run(){
  std::cout << "\033[1;37mStart listening at: \033[0m\033[1;33m" << info.ip << ":" << info.port << "\033[0m\n";

  int rc;
  bool end_server     = false;
  do{
    rc = poll(& cli_poll[0], cli_poll.size(), MAX_TIMEOUT_POLL);

    if (rc < 0){
      if(errno == EINTR)
        continue;
      std::cerr << "\n\033[1;31mError!!! Function poll failed! \033[0m\033[1;35m" << strerror(errno) << "\033[0m\n";
      close(socket_fd);
      RequestKillWorkers();
      exit(EXIT_FAILURE);
    }

    if (rc == 0){
      std::cerr << "\n\033[1;35mpoll() timed out. End program.\033[0m\n\n";
      close(socket_fd);
      RequestKillWorkers();
      exit(EXIT_FAILURE);
    }

    bool was_new_connect = false;

    for (std::size_t i = 0; i < cli_poll.size(); ++i){

      if(cli_poll[i].revents == 0 || cli_poll[i].revents == POLLNVAL)
        continue;

      if(cli_poll[i].revents & (POLLERR|POLLHUP)){
        std::cerr << "\n\033[1;35mWarning!!! Field revents not equal POLLIN! revents = " << cli_poll[i].revents <<" "<< strerror(errno) <<" \033[0m\n\n";
        close(cli_poll[i].fd);
        cli_poll.erase(cli_poll.begin() + i);
        i -= 2;
        continue;
      }

      if(cli_poll[i].revents != POLLIN)
        continue;

      if (cli_poll[i].fd == socket_fd){
        int new_sd = -1;
        do{
          sockaddr_in client_addr;
          socklen_t     len = sizeof(client_addr);

          new_sd = accept(socket_fd, (sockaddr * )& client_addr, &len);
          if (new_sd < 0){
            if (errno != EWOULDBLOCK){
              std::cerr << "\n\033[1;31mError!!! Function accept failed! \033[0m\033[1;35m" << strerror(errno) << "\033[0m\n\n";
              end_server = true;
            }
            break;
          }
          {
            pollfd tmp;
            tmp.fd          = new_sd;
            tmp.events      = POLLIN;
            was_new_connect = true;
            cli_poll.push_back(tmp);
          }
        } while (new_sd != -1);
      }
      else{
        std::unique_lock<std::mutex> lk(mtx);
        tasks.push(cli_poll[i].fd);
        cli_poll.erase(cli_poll.begin() + i);
        lk.unlock();
        cv.notify_one();
      }

      if(was_new_connect){
        break;
      }
    }
  } while (!end_server);
}

HTTP_Server::~HTTP_Server(){
  std::cout << "Closing all conections...\n";

  for (auto it_poll = cli_poll.begin(); it_poll != cli_poll.end(); ++it_poll){
    if(it_poll->fd != -1) close(it_poll->fd);
  }

  server->cli_poll.clear();

  RequestKillWorkers();

  delete [] workers;
}
