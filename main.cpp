#include <iostream>
#include <thread>
#include <csignal>
#include <sys/wait.h>

#include "job.h"
#include "josch.h"

using namespace std;

uint64_t JobId::jobid_counter = 0;

/* reap the zombie childrens */
void proc_exit(int signal)
{
    int wstatus, ret;
    while(1) {
        ret = waitpid(0, &wstatus, WNOHANG);
        if(ret == 0 || ret == -1) {
            break;
        }
    }
}

/* Pending Tasks
 * 1. get number of hw threads (with sysinfo?) and set no. of threads to that value
 * 2. Unix socket listener for registering/unregistering jobs
 * 3. Overrun mitigation?
 */


int main(int argc, char *argv[])
{
    signal(SIGCHLD, proc_exit);

    class Job j("echo Job1", 10);
    class Job j2("echo Job2", 10);
    class Josch js;

    js.init();
    js.register_job(j);
    js.register_job(j2);

    js.handle_jobs();

    LOG<<"Exiting main thread"<<endl;

    return 0;
}
