#include <string>
#include <unistd.h>

#include <cassert>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "tlv_client.h"

tlv_client::tlv_client() {

}

tlv_client::~tlv_client() {
    close(this->fd);
}

bool tlv_client::init(const std::string& f) {
    struct sockaddr_un sun;

    this->fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if(this->fd == -1) {
        LOG<<"Failed to create socket due to "<<std::strerror(errno)<<std::endl;
        return false;
    }

    this->fpath = f;

    sun.sun_family = AF_UNIX;
    memcpy(sun.sun_path, f.c_str(), f.length() + 1);

    if(connect(this->fd, (struct sockaddr *)&sun, sizeof(struct sockaddr_un)) == -1) {
        LOG<<"Failed to connect due to "<<std::strerror(errno)<<std::endl;
        close(this->fd);
        this->fd = -1;
        return false;
    }
    this->conn.set_fd(this->fd);

    LOG<<"Connected to "<<f.c_str()<<std::endl;

    return true;
}

bool dummy_process_data(tlv_client& th, char* tptr, int len) {

    struct tlv* t;
    t = (struct tlv*) tptr;

//    std::cout.write(tptr, len);

    if(t->last == true) {
        return false;
    }
    return true;
}

tlv_types dummy_process_readdata(tlv_client& th, char* tptr, int len) {

    struct tlv* t;

    t = (struct tlv*)tptr;
    switch(t->type) {
        case TLV_SUCCESS:
            return TLV_SUCCESS;
        case TLV_FAILURE:
            //std::cout<<"TLV_FAILURE"<<std::endl;
            return TLV_FAILURE;
        case TLV_LIST_JOBS:

            if(t->length == 0) {
                std::cout<<"No jobs registered"<<std::endl;
            } else {
                std::cout.write(t->value, len);
                std::cout<<std::endl;
            }

            return TLV_SUCCESS;
    }
}


bool tlv_client::sendcmd(tlv_types type) {

    if(type != TLV_LIST_JOBS) {
        return false;
    }

    struct tlv t;
    char *cptr = (char *)&t;

    t.type = type;
    t.length = 0;
    t.last = true;

    if(this->conn.write_to_peer(cptr, sizeof(struct tlv)) == false) {
        LOG<<"error writing tlv "<<std::strerror(errno)<<std::endl;
        return false;
    }

    while(this->conn.writer((*this), dummy_process_data) != true);

    this->conn.reset_iob();

    if(this->conn.reader((*this), dummy_process_readdata) == TLV_FAILURE) {
        return false;
    }

    return true;
}

bool tlv_client::sendcmd(tlv_types type, std::string& cmd, int interval) {

    struct tlv t;
    char *cptr = (char *)&t;

    if(interval != 0) {
        std::string ival = "," + std::to_string(interval);
        cmd.append(ival);
    }

    t.type = type;
    t.length = cmd.length();
    t.last = true;

    if(this->conn.write_to_peer(cptr, sizeof(struct tlv)) == false) {
        LOG<<"error writing tlv "<<std::strerror(errno)<<std::endl;
        return false;
    }

    LOG<<"writing cmd '"<<cmd<<"'"<<std::endl;

    if(this->conn.write_to_peer(cmd) == false) {
        LOG<<"error writing cmd & interval "<<std::strerror(errno)<<std::endl;
        return false;
    }

    while(this->conn.writer((*this), dummy_process_data) != true);

    this->conn.reset_iob();

    if(this->conn.reader((*this), dummy_process_readdata) == false) {
        return false;
    }

    return true;
}

/* dummy; will never be called */
void tlv_client::get_list_of_jobs() {

}

/* dummy; will never be called */
std::string& tlv_client::get_joblist() {
    return this->fpath;
}
