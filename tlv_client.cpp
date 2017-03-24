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
        return false;
    }

    this->fpath = f;

    sun.sun_family = AF_UNIX;
    memcpy(sun.sun_path, f.c_str(), f.length() + 1);

    if(connect(this->fd, (struct sockaddr *)&sun, sizeof(struct sockaddr_un)) == -1) {
        //@FIXME error
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
            //@FIXME handlerror
            LOG<<"write failed due to"<<std::strerror(errno)<<std::endl;
            return -1;
            break;
        case 0:
            LOG<<"Server closed connection"<<std::endl;
            return -1;
            break;
        default:
            LOG<<"Wrote "<<ret<<" bytes"<<std::endl;
            break;
    }
    return ret;
}

int generic_reader(int fd, char *buff, int len) {

    int ret;
    ret = read(fd, buff, len);
    switch(ret) {
        case -1:
            //@FIXME handlerror
            LOG<<"write failed due to"<<std::strerror(errno)<<std::endl;
            return -1;
            break;
        case 0:
            LOG<<"Server closed connection"<<std::endl;
            return -1;
            break;
        default:
            LOG<<"Wrote "<<ret<<" bytes"<<std::endl;
            break;
    }
    return ret;
}

bool tlv_client::sendcmd(int type, std::string& cmd, int interval) {

    enum {
        SENDING_NONE = 0,
        SENDING_TLV,
        SENDING_CMD,
        RECVING_RESP
    };

    std::string s;
    struct tlv t;

    bool finished = false;
    char* sptr;
    int bytes = 0, tosend = 0, ret;
    int state = SENDING_TLV;

    s.append(cmd);
    s.append(",");
    s.append(std::to_string(interval));
    LOG<<"COMMAND: "<<s<<std::endl;

    tosend = (sizeof (struct tlv)) + s.size();

    t.type = type;
    t.length = s.size();

    tosend = sizeof(struct tlv);
    while(finished == false) {
        LOG<<" bytes "<<bytes<<" tosend "<<tosend<<std::endl;
        switch(state) {
            case SENDING_TLV:                
                sptr = ((char *)&t + bytes);

                ret = generic_writer(this->fd, sptr, tosend - bytes);
                switch(ret) {
                    case -1:
                        close(this->fd);
                        this->fd = -1;
                        return false;
                        break;
                    default:
                        bytes += ret;
                        if(bytes == sizeof(struct tlv)) {
                            state = SENDING_CMD;
                            sptr = (char *)(s.c_str());
                            bytes = 0;
                            tosend = s.length() + 1;
                            LOG<<" sending cmd "<<sptr<<std::endl;
                        }
                        break;
                }

                break;
            case SENDING_CMD:
                sptr = ((char *)s.c_str() + bytes);

                ret = generic_writer(this->fd, sptr, tosend - bytes);
                switch(ret) {
                    case -1:
                        close(this->fd);
                        this->fd = -1;
                        return false;
                        break;
                    default:
                        bytes += ret;
                        if(bytes == s.length() + 1) {
                            state = RECVING_RESP;
                            bytes = 0;
                            LOG<<" awaiting response from server "<<std::endl;
                        }
                        break;
                }
                break;
            case RECVING_RESP:
                sptr = ((char*)&t + bytes);

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
                            if(t.type == TLV_SUCCESS) {
                                if(type == TLV_REGISTER_JOB) {
                                    LOG<<"Job registered successfully"<<std::endl;
                                } else if (type == TLV_UNREGISTER_JOB) {
                                    LOG<<"Job unregistered successfully"<<std::endl;
                                }
                                exit(0);
                            } else {
                                if(type == TLV_REGISTER_JOB) {
                                    LOG<<"Job registration failed"<<std::endl;
                                } else if (type == TLV_UNREGISTER_JOB) {
                                    LOG<<"Job unregistration failed"<<std::endl;
                                }
                                exit(0);
                            }
                        }
                        break;
                }

                break;
            default:
                break;
        }

   //TAG
    }

    sptr = ((char *)&t);
    bytes = 0;

    ret = read(this->fd, (sptr + bytes), (sizeof(struct tlv) - bytes));
    switch(ret) {
        case -1:
            LOG<<"Read failed due to "<<std::strerror(errno)<<std::endl;
            close(this->fd);
            this->fd = -1;
            break;
        case 0:
            LOG<<"Server closed connection"<<std::endl;
            close(this->fd);
            this->fd = -1;
           break;
        default:
            bytes += ret;
            if(bytes == sizeof(tlv)) {
                if(t.type == TLV_SUCCESS) {
                    LOG<<"Command registered successfully"<<std::endl;
                } else {
                    LOG<<"Command registration failed"<<std::endl;
                }
            }
            break;
    }

    return true;
}
