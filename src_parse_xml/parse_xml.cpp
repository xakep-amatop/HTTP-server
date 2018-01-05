#include <parse_xml.h>

ParseXmlConfig::ParseXmlConfig(std::string &  pathname, parse_info & _info){
  Init(pathname.c_str());
  StartParsing(_info);
  check_info(_info);
}

ParseXmlConfig::ParseXmlConfig(const char * pathname,   parse_info & _info){
  Init(pathname);
  StartParsing(_info);
  check_info(_info);
}

void ParseXmlConfig::Init(const char *  pathname){

  std::cout << "\nStart parsing file: " << pathname << " ...\n\n";

  doc = xmlParseFile(pathname);
  if (doc == NULL ) {
    std::cerr << "\nDocument not parsed successfully.\n";
    exit(EXIT_FAILURE);
  }

  cur = xmlDocGetRootElement(doc);
  if (cur == NULL) {
    std::cerr << "\nError!!! Configuration xml-file is empty!\n";
    xmlFreeDoc(doc);
    exit(EXIT_FAILURE);
  }
}

void ParseXmlConfig::StartParsing(parse_info & _info){
  cur = cur->xmlChildrenNode;

  is_root_configuration();

  _info.port           = 0;
  _info.number_workers = 0;

  while (cur != NULL) {
    std::string name_branch(reinterpret_cast <const char *> (cur->name));

    if (name_branch == "text"){
      cur = cur->next;
      continue;
    }

    auto it_field = xml_fields.find(name_branch);
    if(it_field == xml_fields.end()){
      std::cerr << "Warning! No handler to branch: " << name_branch << std::endl;
    }
    else{
      MFP function = it_field->second;
      if (function){
        (this->*function)(_info);
      }
      else{
        std::cerr << "\nError!!! Defined branch: " << name_branch << ", but not defined handler!\n";
        return;
      }
    }
    cur = cur->next;
  }
}


void ParseXmlConfig::is_root_configuration(){
  if (xmlStrEqual(cur->name, root_branch)){
    std::cerr << "\nDocument of the wrong type, root node != configuration\n";
    exit(EXIT_FAILURE);
  }
}

void ParseXmlConfig::ParseIP        (parse_info & info){
  xmlChar * str_value = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);

  if (!str_value){
    std::cerr << "\nError!!! IP address was not type in configuration file!\n";
    return;
  }

  info.is_ipv4 = false;
  info.is_ipv4 = is_ipv4_address(reinterpret_cast<const char *> (str_value));
  bool is_ipv6 = is_ipv6_address(reinterpret_cast<const char *> (str_value));
  info.ip      = reinterpret_cast<const char *> (str_value);
  xmlFree(str_value);

  if(!info.is_ipv4 && !is_ipv6){
    std::cerr << "\nError!!! Bad IP address: " << info.ip << std::endl;
    info.ip.erase();
    return;
  }

  std::cout << "Server IP address set to: " << info.ip << std::endl;
}

void ParseXmlConfig::ParsePort      (parse_info & info){
  xmlChar * str_value = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);

  if (!str_value){
    std::cerr << "\nPort number was not type in configuration file!\n";
    return;
  }

  int port = atoi(reinterpret_cast<char *> (str_value));

  if (port <= 0 && port > 65535){
    std::cerr << "\nError!!! Not valid data in field with number port in configuration file!\n";
    info.number_workers = sysconf(_SC_NPROCESSORS_ONLN);
    return;
  }

  info.port = static_cast<std::uint16_t>(port);

  std::cout << "Port number set to: " << info.port << std::endl;

  xmlFree(str_value);
}

void ParseXmlConfig::ParseRootPath  (parse_info & info){
  xmlChar * str_value = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);

  if(!str_value){
    std::cerr << "\nRoot directory was not type in configuration file!\n";
    return;
  }

  info.root_path = reinterpret_cast< char * > (str_value);

  if (info.root_path[info.root_path.size() - 1] != '/'){
      info.root_path.push_back('/');
  }

  struct stat _stat = {0};
  if (stat(info.root_path.c_str(), &_stat) < 0){
    std::string path = "\nError!!! Field root directory!\n`"+info.root_path + '`';
    std::perror(path.c_str());
    info.root_path.erase();
  }
  else{
    std::cout << "Root directory set to: " << info.root_path << std::endl;
  }

  xmlFree(str_value);
}

void ParseXmlConfig::ParseNumWorker (parse_info & info){
  xmlChar * str_value = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);

  if (!str_value){
    std::cerr << "\nNumber workers was not type in configuration file!\n";
    return;
  }

  info.number_workers = atoi(reinterpret_cast<char *> (str_value));

  if (static_cast<std::int32_t>(info.number_workers) <= 0){
    std::cerr << "\nError!!! Not valid data in field number workers in configuration file!\n";
    info.number_workers = sysconf(_SC_NPROCESSORS_ONLN);
    if (static_cast<std::int32_t>(info.number_workers) == -1){
      perror("\nError!!! Can not get number cores!");
      info.number_workers = 4;
    }
  }

  xmlFree(str_value);

  std::cout << "Number workers set to: " << info.number_workers << std::endl;
}

bool ParseXmlConfig::is_ipv4_address(const char * address){
  struct sockaddr_in sa;
  return inet_pton(AF_INET, address, &(sa.sin_addr))!=0;
}

bool ParseXmlConfig::is_ipv6_address(const char * address){
  struct sockaddr_in6 sa;
  return inet_pton(AF_INET6, address, &(sa.sin6_addr))!=0;
}

void ParseXmlConfig::check_info(parse_info & info){

  bool must_exit = false;

  if (info.ip.empty()){
    std::cerr << "\nIP address was not parsed!\n";
    must_exit = true;
  }

  if (info.root_path.empty()){
    std::cerr << "\nRoot directory was not parsed!\n";
    must_exit = true;
  }

  if (!info.port){
    std::cerr << "\nPort was not parsed\n";
    must_exit = true;
  }

  if(must_exit){
    this->~ParseXmlConfig();
    exit(EXIT_FAILURE);
  }

  std::cout << "\nParsing the file completed successfully!\n\n";
}

ParseXmlConfig::~ParseXmlConfig(){
  if(doc)
    xmlFreeDoc(doc);
  xmlCleanupParser();
}
