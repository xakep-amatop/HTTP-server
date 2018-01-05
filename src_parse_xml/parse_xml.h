#pragma once

#include <iostream>
#include <fstream>
#include <map>

#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <libxml/parser.h>
#include <arpa/inet.h>

#include <config.h>

class ParseXmlConfig{

    typedef void (ParseXmlConfig::*MFP)(parse_info & );

    std::ifstream xml_file;

    xmlDocPtr   doc;
    xmlNodePtr  cur;

    const xmlChar * root_branch = reinterpret_cast< const xmlChar * >("configuration");

    inline void is_root_configuration();

    inline void Init(const char * pathname);

    inline void StartParsing(parse_info & _info);

    void ParseIP        (parse_info & info);
    void ParsePort      (parse_info & info);
    void ParseRootPath  (parse_info & info);
    void ParseNumWorker (parse_info & info);

    std::map<std::string, MFP> xml_fields =
                                        {
                                          {"IP-address",      & ParseXmlConfig::ParseIP         },
                                          {"TCP-port",        & ParseXmlConfig::ParsePort       },
                                          {"root-path",       & ParseXmlConfig::ParseRootPath   },
                                          {"number-workers",  & ParseXmlConfig::ParseNumWorker  },
                                        };

    inline bool is_ipv4_address(const char * address);
    inline bool is_ipv6_address(const char * address);

    inline void check_info(parse_info & info);

  public:
    ParseXmlConfig() = delete;

    ParseXmlConfig(std::string &  pathname, parse_info & _info);
    ParseXmlConfig(const char *   pathname, parse_info & _info);

    ~ParseXmlConfig();
};