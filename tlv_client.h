#ifndef TLV_CLIENT_H
#define TLV_CLIENT_H

#include <iostream>
#include <string>
#include <thread>
#include <cstring>
#include "tlv.h"

#define DEFAULT_FPATH "/tmp/josch_default.sock"
#define LOG std::cout<<"["<<std::this_thread::get_id()<<"] "

class tlv_client
{
    int fd;
    std::string fpath;
    public:
        tlv_client();
        ~tlv_client();
        bool init(const std::string &f);

        bool sendcmd(int type, std::string& cmd, int interval);

};

#endif // TLV_CLIENT_H
