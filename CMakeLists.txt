cmake_minimum_required (VERSION 2.6)

set(PROJECT_HTTP_SERVER HTTP_SERV)

include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/src_parse_xml ${CMAKE_CURRENT_SOURCE_DIR}/src_http_server ${CMAKE_CURRENT_SOURCE_DIR}/config ${Readline_INCLUDE_DIR} /usr/include/libxml2 )

set(CMAKE_CXX_FLAGS "-std=c++11 -Wall -g -O0")

set(SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src_parse_xml/parse_xml.cpp ${CMAKE_CURRENT_SOURCE_DIR}/src_http_server/http_server.cpp)

add_executable(${PROJECT_HTTP_SERVER} server.cpp ${SOURCES})

target_link_libraries(${PROJECT_HTTP_SERVER} -lxml2 -lpthread)
