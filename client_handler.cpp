#include <iostream>
#include <algorithm>
#include <cstring>
#include <cassert>

#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/epoll.h>
#include <fcntl.h>

#include "client_handler.h"

extern std::atomic<bool> main_quit;

client_handler::client_handler(Josch *tmpj) {
    this->j = tmpj;
    this->quit = false;
    this->lfd = -1;
    this->epfd = -1;
}

client_handler::~client_handler() {

    if(!this->fpath.empty()) {
        unlink(this->fpath.c_str());
    }

    this->die();
    if(this->th.joinable()) {
        this->th.join();
    }

    if(this->lfd != -1) {
        close(this->lfd);
    }

    if(this->epfd != -1) {
        close(this->epfd);
    }
}

void client_handler::join() {
    if(this->th.joinable()) {
        this->th.join();
    }
}

void client_handler::die() {
    this->quit = true;
}

bool client_handler::init(const std::string &fpath) {
    struct sockaddr_un sun;

    this->fpath = fpath;

    this->lfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if(this->lfd == -1) {
        LOG<<"Socket creation failed because "<<std::strerror(errno)<<std::endl;        
        return false;
    }

    sun.sun_family = AF_UNIX;
    memcpy(sun.sun_path, fpath.c_str(), fpath.length() + 1);

    if(bind(this->lfd, (sockaddr *)&sun, sizeof(sun)) == -1) {
        LOG<<"Failed to bind because "<<std::strerror(errno)<<std::endl;
        close(this->lfd);
        return false;
    }

    if(listen(this->lfd, 10) == -1) {
        LOG<<"Failed to start listener because "<<std::strerror(errno)<<std::endl;
        close(this->lfd);
        return false;
    }

    this->epfd = epoll_create1(0);
    if(this->epfd == -1) {
        LOG<<"Failed to initialize epoll because "<<std::strerror(errno)<<std::endl;
        close(this->lfd);
        return false;
    }

    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = this->lfd;
    epoll_ctl(this->epfd, EPOLL_CTL_ADD, this->lfd, &ev);

    LOG<<"Listening on "<<this->fpath<<" with fd: "<<this->lfd<<std::endl;

    this->th = std::thread(&client_handler::handle_conns, this);
}

bool client_handler::handle_listener(epoll_event & ev) {

    int new_cfd;

    if(ev.events || EPOLLIN) {
        new_cfd = accept(this->lfd, NULL, 0);
        if(new_cfd == -1) {
            LOG<<"Accept failed because"<<std::strerror(errno)<<std::endl;
            return false;
        }
        LOG<<"New client connected: "<<new_cfd<<std::endl;

        /* set O_NONBLOCK on fd */
        int flags = fcntl(new_cfd, F_GETFL, 0);
        fcntl(new_cfd, F_SETFL, flags | O_NONBLOCK);

        struct epoll_event ev;
        ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
        ev.data.fd = new_cfd;

        epoll_ctl(this->epfd, EPOLL_CTL_ADD, new_cfd, &ev);

        struct client c;
        c.fd = new_cfd;
        c.filled = c.processed = 0;
        c.size = BUFFSIZE;
        c.state = STATE_RECVING;

        this->clist.push_back(c);
    }
}

void client_handler::close_client(int fd) {

    epoll_event ev;
    ev.data.fd = fd;
    epoll_ctl(this->epfd, EPOLL_CTL_DEL, fd, &ev);

    close(fd);
    this->clist.erase((std::remove_if(this->clist.begin(), this->clist.end(), [&fd] (const struct client &tc) {
        if (fd == tc.fd) {
            return true;
        }
        return false;
    })), this->clist.end());
}

bool client_handler::process_data(struct tlv *tptr, struct client& cl) {

    cl.cmd.append(((char*)tptr + sizeof(struct tlv)), tptr->length);
    cl.processed += (sizeof(struct tlv) + tptr->length);
    if(cl.filled == cl.processed) {
        cl.filled = cl.processed = 0;
    }

    //LOG<<"Got string '"<<cl.cmd<<"' "<<std::endl;
    /* wait for whole command */
    if(tptr->last != 1) {
        return false;
    }

    size_t comma = cl.cmd.find_last_of(',');

    cl.interval = std::strtol(cl.cmd.substr(comma + 1).c_str(), NULL, 10);
    cl.cmd.erase(comma);

    //LOG<<"Got cmd "<<cl.cmd<<" with ival "<<cl.interval<<std::endl;
    return true;
}

void client_handler::set_state_sending(struct tlv *tptr, struct client &cl, int type) {

   /* Give back response */
    switch(type) {
        case TLV_LIST_JOBS:
            cl.state = STATE_SEND_LIST;
            /* reuse cl.cmd for saving job list to be sent */
            cl.cmd = this->j->list_jobs();
            cl.filled = cl.cmd.length();
            cl.processed = 0;
            break;
        case TLV_SUCCESS:
        case TLV_FAILURE:
            cl.state = STATE_SEND_RESP;
            tptr = (struct tlv*)cl.iobuf;
            tptr->type = type;
            tptr->length = 0;

            cl.filled = sizeof(struct tlv);
            cl.processed = 0;
            break;
    }
}


bool client_handler::send_list_handler(struct client &cl) {
    assert(cl.state == STATE_SEND_LIST);

    int ret;
    errno = 0;
    const char *sptr;

    LOG<<"Send list handler "<<std::endl;

    sptr = cl.cmd.c_str();

    ret = write(cl.fd, sptr + cl.processed, cl.filled - cl.processed);
    switch(ret) {
        case 0:
            break;
        case -1:
            if(errno == EAGAIN) {
                //LOG<<"EAGAIN on write";
                cl.write_ready = false;
                break;
            }
            LOG<<"Write failed due to :"<<std::strerror(errno)<<std::endl;
            this->close_client(cl.fd);
            break;
        default:
            LOG<<"Wrote "<<ret<<" bytes"<<std::endl;
            cl.processed += ret;
            if(cl.processed == cl.filled) {
                this->close_client(cl.fd);
                return true;
            }
            break;
    }

    return true;
}

bool client_handler::write_handler(struct client &cl) {

    int ret;


    //LOG<<"OUT EVENT"<<std::endl;
    if(cl.state != STATE_SEND_RESP) {
        return false;
    }

    if(cl.filled <= cl.processed) {
        return true;
    }

    errno = 0;
    ret = write(cl.fd, cl.iobuf + cl.processed, cl.filled - cl.processed);
    switch(ret) {
        case 0:
            break;
        case -1:
            if(errno == EAGAIN) {
                //LOG<<"EAGAIN on write";
                cl.write_ready = false;
                break;
            }
            LOG<<"Write failed due to :"<<std::strerror(errno)<<std::endl;
            this->close_client(cl.fd);
            break;
        default:
            //LOG<<"Wrote "<<ret<<" bytes"<<std::endl;
            cl.processed += ret;
            if(cl.processed == cl.filled) {
                this->close_client(cl.fd);
                return true;
            }
            break;
    }
}

bool client_handler::read_handler(struct client &cl) {

    int ret;
    struct tlv* tptr = NULL;

    //LOG<<"IN EVENT"<<std::endl;

    if(cl.state != STATE_RECVING) {
        LOG<<"Client not following protocol!"<<std::endl;
        this->close_client(cl.fd);
        return false;
    }

    /* read till eagain */
    while(1) {
        errno = 0;
        ret = read(cl.fd, (cl.iobuf + cl.filled), (cl.size - cl.filled));
        switch(ret) {
            case 0:
                LOG<<"Client closed connection while reading to it"<<std::endl;
                this->close_client(cl.fd);
                return false;
                break;
            case -1:
                if(errno == EAGAIN) {
                    //LOG<<"EAGAIN on read"<<std::endl;
                    return true;
                }
                this->close_client(cl.fd);
                return false;
                break;
            default:
                {
                    //LOG<<"Got "<<ret<<" bytes "<<std::endl;
                    cl.filled += ret;

                    while((cl.filled - cl.processed) >= sizeof(struct tlv)) {

                        if(tptr == NULL) {
                            tptr = (struct tlv*)(cl.iobuf + cl.processed);
                            //LOG<<"Got t: "<<tptr->type<<" l: "<<tptr->length<<std::endl;
                        }

                        if((cl.filled - cl.processed) >= (sizeof(struct tlv) + tptr->length)) {
                            switch (tptr->type) {
                                case TLV_REGISTER_JOB:
                                    if(this->process_data(tptr, cl) == false) {
                                        //LOG<<"pending data :'"<<cl.cmd<<"'"<<std::endl;
                                        tptr = NULL;
                                        break;
                                    }

                                    /* do the tlv job */
                                    if(this->j->register_job(cl.cmd, cl.interval) == true) {
                                        this->set_state_sending(tptr, cl, TLV_SUCCESS);
                                    } else {
                                        this->set_state_sending(tptr, cl, TLV_FAILURE);
                                    }
                                    return true;

                                    break;
                                case TLV_UNREGISTER_JOB:
                                    if(this->process_data(tptr, cl) == false) {
                                        //LOG<<"pending data :'"<<cl.cmd<<"'"<<std::endl;
                                        tptr = NULL;
                                        break;
                                    }

                                    /* do the tlv job */
                                    if(this->j->unregister_job(cl.cmd, cl.interval) == true) {
                                        this->set_state_sending(tptr, cl, TLV_SUCCESS);
                                    } else {
                                        this->set_state_sending(tptr, cl, TLV_FAILURE);
                                    }
                                    return true;
                                    break;
                                case TLV_LIST_JOBS:
                                    //std::string joblist = this->j->list_jobs();
                                    this->set_state_sending(tptr, cl, TLV_LIST_JOBS);
                                    return true;
                                    break;
                                default:
                                    break;
                            }
                        } else {
                            //LOG<<"Not enough data!"<<std::endl;
                            break;
                        }
                    }
                }
                break;
        }
    }


}

bool client_handler::handle_client(epoll_event &ev) {

    int ret;

    if((ev.events & EPOLLERR) || (ev.events & EPOLLHUP)) {
        LOG<<"Client "<<ev.data.fd<<" closed connection"<<std::endl;
        this->close_client(ev.data.fd);
        return false;
    }

    /* bad sequential lookup; @FIXME */
    auto ait = this->clist.begin();
    while(ait != this->clist.end()) {
        if((*ait).fd == ev.data.fd) {
            break;
        }
        ait++;
    }

    if(ait == this->clist.end()) {
        LOG<<"Client doesn't exist!; shouldn't happen!"<<std::endl;
        return false;
    }

    auto &cl = (*ait);


    if(ev.events & EPOLLIN) {
        this->read_handler(cl);
    }

    if((ev.events & EPOLLOUT) || (cl.write_ready == true)) {
        cl.write_ready = true;
        switch(cl.state) {
            case STATE_SEND_RESP:
                this->write_handler(cl);
                break;
            case STATE_SEND_LIST:
                this->send_list_handler(cl);
                break;
            default:
                break;
        }
    }

    //LOG<<"Done with client handler"<<std::endl;
    return true;
}

bool client_handler::handle_conns() {

    int nevents;
    epoll_event evs[10];

    //LOG<<"Start client_handler"<<std::endl;
    while(this->quit == false) {
        //LOG<<"Waiting for event"<<std::endl;
        nevents = epoll_wait(this->epfd, evs, 10, 1000);
        if(nevents == -1) {
            LOG<<"epoll_wait failed due to "<<std::strerror(errno)<<std::endl;
            continue;
        }

        for(int i = 0; i < nevents; i++) {
            //LOG<<"Handling event for client "<<evs[i].data.fd<<std::endl;

            if(evs[i].data.fd == this->lfd) {
                this->handle_listener(evs[i]);
            } else {
                this->handle_client(evs[i]);
            }
        }
    }
    //LOG<<"End client_handler"<<std::endl;
}
