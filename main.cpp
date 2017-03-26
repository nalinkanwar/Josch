#include <iostream>
#include <thread>
#include <csignal>
#include <sys/wait.h>
#include <unistd.h>
#include <getopt.h>

#include "job.h"
#include "josch.h"
#include "tlv_client.h"
#include "client_handler.h"

using namespace std;

uint64_t JobId::jobid_counter = 0;

std::atomic<bool> main_quit;

/* reap the zombie childrens */
void proc_exit(int signal) {
    int wstatus, ret;
    while(1) {
        ret = waitpid(0, &wstatus, WNOHANG);
        if(ret == 0 || ret == -1) {
            break;
        }
    }
}

void handle_quit(int signal) {
    main_quit = true;
}

/* Pending Tasks
 * 1. get number of hw threads (with hw_concurrency/sysinfo?) and set no. of threads to that value - DONE
 * 2. command line options handler - DONE
 * 3. Unix socket listener for registering/unregistering jobs; probably good if separate thread
 * 4. Overrun mitigation?
 */

void print_usage(char *binname) {
    std::cout<<"Usage: "<<binname<<" [ -ml -i<tag> -r \"<job>, <interval>\" -u \"<job>, <interval>\" -d <jobid> ] "<<std::endl<<
               "\t\t -i <tag>: will spawn a new Josch instance with ID <tag>; use before other commands to send to that Josch instance"<<std::endl<<
               "\t\t -r \"job, interval\": registers a <job> that is to be repeated at every <interval> intervals"<<std::endl<<
               "\t\t -u \"job, interval\": unregisters a previously registered <job>"<<std::endl<<
               "\t\t -d \"jobid\": unregisters a previously registered job with <jobid>"<<std::endl<<
               "\t\t -l : lists all jobs currently registered"<<std::endl;
}

int main(int argc, char *argv[])
{

    int cli_index = 0, cliopt, log_level = 1;
    std::list<class Job> regjobs, unregjobs;
    std::string fpath;

    main_quit = false;
    static struct option cli[] = {
        {"register",    required_argument,  NULL,       'r'},
        {"unregister",  required_argument,  NULL,       'u'},
        {"deregister",  required_argument,  NULL,       'd'},
        {"id",          required_argument,  NULL,       'i'},
        {"version",     no_argument,        NULL,       'v'},
        {"listjobs",    no_argument,        NULL,       'l'},
        {"mute",        no_argument,        NULL,       'm'},
        {NULL,          0,                  NULL,       0}
    };

    while((cliopt = getopt_long(argc, argv, ":r:u:i:ld:vm", cli, &cli_index)) != -1) {
        switch(cliopt) {
            case ':':
                std::cout<<"Invalid or missing arguments"<<std::endl;
            case 0:
                print_usage(argv[0]);
                exit(0);
                break;
            case 'v':
                std::cout<<"Josch v1.0"<<std::endl;
                std::cout<<"Written by Nalin Kanwar."<<std::endl;
                exit(0);
                break;
            case 'm':
                log_level = 0;
                std::cout<<"Setting log level to mute"<<std::endl;
                break;
            case 'r':
            {
                //temporary; we'll take last argument as job later on so it works like 'watch'
                std::string arg = optarg;
                auto const pos = arg.find_last_of(",");

                if(pos != std::string::npos) {
                    std::string jobcmd = arg.substr(0, pos);
                    int interval = std::stol(arg.substr(pos+1));

                    if(interval == 0) {
                        LOG<<"Invalid interval "<<interval<<std::endl;
                    }

                    LOG<<"Registering "<<jobcmd<<" with interval "<<std::endl;

                    class tlv_client t;

                    if(fpath.empty()) {
                        if(t.init(std::string(DEFAULT_FPATH)) == false) {
                            LOG<<"Failed to init tlv client"<<std::endl;
                            exit(-1);
                        }
                    } else {
                        if(t.init(std::string(fpath)) == false) {
                            LOG<<"Failed to init tlv client"<<std::endl;
                            exit(-1);
                        }
                    }
                    if(t.sendcmd(TLV_REGISTER_JOB, jobcmd, interval) == false) {
                        LOG<<"Failed to send command to josch"<<std::endl;
                    }
                    LOG<<"Registered '"<<jobcmd<<"' successfully"<<std::endl;
                } else {
                    print_usage(argv[0]);
                }
                exit(0);
            }
                break;
            case 'u':
            {
                //temporary; we'll take last argument as job later on so it works like 'watch'
                std::string arg = optarg;
                auto const pos = arg.find_last_of(",");

                if(pos != std::string::npos) {
                    std::string jobcmd = arg.substr(0, pos);
                    int interval = std::stol(arg.substr(pos+1));

                    if(interval == 0) {
                        LOG<<"Invalid interval "<<interval<<std::endl;
                    }

                    LOG<<"Unregistering "<<jobcmd<<" with interval "<<std::endl;

                    class tlv_client t;
                    if(fpath.empty()) {
                        if(t.init(std::string(DEFAULT_FPATH)) == false) {
                            LOG<<"Failed to init tlv client"<<std::endl;
                            exit(-1);
                        }
                    } else {
                        if(t.init(std::string(fpath)) == false) {
                            LOG<<"Failed to init tlv client"<<std::endl;
                            exit(-1);
                        }
                    }
                    if(t.sendcmd(TLV_UNREGISTER_JOB, jobcmd, interval) == false) {
                        LOG<<"Failed to send command to josch"<<std::endl;
                    }
                    LOG<<"Unregistered '"<<jobcmd<<"' successfully"<<std::endl;
                } else {
                    print_usage(argv[0]);
                }
                exit(0);
            }
                break;
            case 'd':
            {
                //temporary; we'll take last argument as job later on so it works like 'watch'
                std::string arg = optarg;

                LOG<<"Unregistering Job with id"<<arg<<std::endl;

                class tlv_client t;
                if(fpath.empty()) {
                    if(t.init(std::string(DEFAULT_FPATH)) == false) {
                        LOG<<"Failed to init tlv client"<<std::endl;
                        exit(-1);
                    }
                } else {
                    if(t.init(std::string(fpath)) == false) {
                        LOG<<"Failed to init tlv client"<<std::endl;
                        exit(-1);
                    }
                }
                if(t.sendcmd(TLV_UNREGISTER_JOB_BY_ID, arg) == false) {
                    LOG<<"Failed to send command to josch"<<std::endl;
                }
                LOG<<"Unregistered Job with id'"<<arg<<"' successfully"<<std::endl;
                exit(0);
            }
                break;
            case 'l':
            {
                class tlv_client t;
                std::string s;
                if(fpath.empty()) {
                    if(t.init(std::string(DEFAULT_FPATH)) == false) {
                        LOG<<"Failed to init tlv client"<<std::endl;
                        exit(-1);
                    }
                } else {
                    if(t.init(std::string(fpath)) == false) {
                        LOG<<"Failed to init tlv client"<<std::endl;
                        exit(-1);
                    }
                }
                if(t.sendcmd(TLV_LIST_JOBS) == false) {
                    LOG<<"Failed to send command to josch"<<std::endl;
                }
                exit(0);
            }
                break;
            case 'i':
            {
                fpath = std::string(DEFAULT_FPATH).append(optarg);
                break;
            }
            default:
                print_usage(argv[0]);
                exit(0);
                break;
        }
    }

    if(log_level == 0) {
        close(1);
    }
    /* set child handler to reap zombie childs */
    signal(SIGCHLD, proc_exit);
    signal(SIGINT, handle_quit);
    signal(SIGPIPE, SIG_IGN);

    unsigned nHWthreads = std::thread::hardware_concurrency();
    //LOG<<"HWTheads: "<<nHWthreads<<std::endl;
    try {

        class Josch js(nHWthreads ? nHWthreads: MINTHREADS);
        class client_handler ch(&js);

        if(fpath.empty()) {
            ch.init(std::string(DEFAULT_FPATH));
        } else {
            ch.init(std::string(fpath));
        }

        js.handle_jobs();
        LOG<<"Finished handling jobs"<<endl;

        /* Make sure our TLV client handler thread also terminates before main thread */
        ch.die();
        ch.join();
    } catch (std::exception &e) {
        LOG<<"Exception :"<<e.what()<<std::endl;
        main_quit = true;
    }

    LOG<<"Exiting main thread"<<endl;

    return 0;
}
