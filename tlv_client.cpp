#include <string>
#include <unistd.h>

#include <cassert>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "tlv_client.h"

#define TLV_MAX_LEN 8096

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
        //@FIXME error
        LOG<<"Failed to connect due to "<<std::strerror(errno)<<std::endl;
        close(this->fd);
        this->fd = -1;
        return false;
    }
    LOG<<"Connected to "<<f.c_str()<<std::endl;

    return true;
}

int generic_writer(int fd, char *buff, int len) {

    int ret;
    ret = write(fd, buff, len);
    switch(ret) {
        case -1:
            LOG<<"write failed due to "<<std::strerror(errno)<<std::endl;
            return -1;
            break;
        case 0:
            LOG<<"Server closed connection"<<std::endl;
            return -1;
            break;
        default:
            //LOG<<"Wrote "<<ret<<" bytes"<<std::endl;
            break;
    }
    return ret;
}

int generic_reader(int fd, char *buff, int len) {

    int ret;
    ret = read(fd, buff, len);
    switch(ret) {
        case -1:
            LOG<<"write failed due to "<<std::strerror(errno)<<std::endl;
            return -1;
            break;
        case 0:
            LOG<<"Server closed connection"<<std::endl;
            return -1;
            break;
        default:
            //LOG<<"Wrote "<<ret<<" bytes"<<std::endl;
            break;
    }
    return ret;
}

bool tlv_client::handle_tlv_list_jobs(int length) {
    //@FIXME

    int nread = 0, ret;
    char buff[4096];

    while(nread != length) {
        ret = read(this->fd, buff, 4096);
        switch(ret) {
            case -1:
                LOG<<std::endl<<"Read failed due to "<<std::strerror(errno)<<std::endl;
                break;
            case 0:
                LOG<<std::endl<<"Server abruptly closed connection"<<std::endl;
                break;
            default:
                LOG<<"Got "<<ret<<" bytes"<<std::endl;
                nread += ret;
                std::cout.write(buff, ret);
                std::cout<<std::endl;
                break;
        }
    }

    return true;
}

bool tlv_client::sendcmd(tlv_types type, std::string& cmd, int interval) {

    enum {
        SENDING_NONE = 0,
        SENDING_TLV,
        SENDING_CMD,
        RECVING_RESP,
    };

    std::string s;
    struct tlv t;

    bool finished = false;
    char* sptr;
    int bytes = 0, tosend = 0, ret;
    int state = SENDING_TLV;

    /* prep the cmd to be sent */
    if(interval > 0) {
        s.append(cmd);
        s.append(",");
        s.append(std::to_string(interval));
    }

    //LOG<<"COMMAND: '"<<s<<"'"<<std::endl;

    t.type = type;
    if(s.length() > TLV_MAX_LEN) {
        t.length = TLV_MAX_LEN;
        t.last = 0;

        tosend = s.length() + (sizeof(struct tlv) * (s.length()/TLV_MAX_LEN));
    } else {
        t.length = s.length();
        t.last = 1;

        tosend = s.length() + sizeof(struct tlv);
    }

    while(tosend != 0) {
        //LOG<<" bytes "<<bytes<<" tosend "<<tosend<<std::endl;
        switch(state) {
            case SENDING_TLV:                
                sptr = ((char *)&t + bytes);

                ret = generic_writer(this->fd, sptr, sizeof(struct tlv) - bytes);
                switch(ret) {
                    case -1:
                        close(this->fd);
                        this->fd = -1;
                        return false;
                        break;
                    default:
                        bytes += ret;                        
                        if(bytes == sizeof(struct tlv)) {
                            if(t.type == TLV_LIST_JOBS) {
                                state = RECVING_RESP;
                                tosend = 0;
                                LOG<<" recving list"<<std::endl;
                            } else {
                                state = SENDING_CMD;
                                sptr = (char *)(s.c_str());
                                bytes = 0;
                                tosend -= sizeof(struct tlv);
                                LOG<<" sending cmd "<<sptr<<std::endl;
                            }
                        }
                        break;
                }
                break;
            case SENDING_CMD:
                sptr = ((char *)s.c_str() + bytes);

                ret = generic_writer(this->fd, sptr, t.length - bytes);
                switch(ret) {
                    case -1:
                        close(this->fd);
                        this->fd = -1;
                        return false;
                        break;
                    default:
                        bytes += ret;
                        if(bytes == t.length) {
                            tosend -= t.length;

                            if(tosend == 0) {
                                state = RECVING_RESP;
                                bytes = 0;
                                LOG<<" awaiting response from server "<<std::endl;
                            } else {
                                s.erase(0, t.length);

                                t.type = type;
                                if(tosend > TLV_MAX_LEN) {
                                    t.length = TLV_MAX_LEN;
                                    t.last = 0;
                                    LOG<<" sending next chunk with "<<t.length<<" bytes"<<std::endl;
                                } else {
                                    t.length = s.length();
                                    t.last = 1;
                                    LOG<<" sending last chunk with "<<t.length<<" bytes"<<std::endl;
                                }

                                state = SENDING_TLV;
                                bytes = 0;
                            }
                        }
                        break;
                    }
                break;
            default:
                break;
        }
    }


    if(state != RECVING_RESP) {
        LOG<<"STATE NOT RECVING RESP"<<std::endl;
        return false;
    }

    sptr = (char*)&t;
    bytes = 0;

    ret = generic_reader(this->fd, sptr, sizeof(struct tlv) - bytes);
    switch(ret) {
        case -1:
            close(this->fd);
            this->fd = -1;
            return false;
            break;
        default:
            bytes += ret;
            if(bytes == sizeof(struct tlv)) {
                switch(t.type) {
                    case  TLV_SUCCESS:
                        if(type == TLV_REGISTER_JOB) {
                            LOG<<"Job '"<<cmd<<"' with interval "<<interval<<" registered successfully"<<std::endl;
                        } else if (type == TLV_UNREGISTER_JOB) {
                            LOG<<"Job '"<<cmd<<"' with interval "<<interval<<" unregistered successfully"<<std::endl;
                        }
                        exit(0);
                        break;
                    case TLV_FAILURE:

                        if(type == TLV_REGISTER_JOB) {
                            LOG<<"Job registration failed"<<std::endl;
                        } else if (type == TLV_UNREGISTER_JOB) {
                            LOG<<"Job unregistration failed"<<std::endl;
                        }
                        exit(0);
                        break;
                    case TLV_LIST_JOBS:
                        state = RECVING_RESP;
                        handle_tlv_list_jobs(t.length);
                        break;
                }
            }
            break;
    }
    return true;
}
