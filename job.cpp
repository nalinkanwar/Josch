#include <iostream>
#include <cstring>
#include <unistd.h>

#include <sys/wait.h>
#include "job.h"

using namespace std;

Job::Job()
{

}

Job::Job(string cmd, int ival)
{
    this->command = cmd;
    this->jobid = JobId::getCurrJobId();
    this->interval = std::chrono::milliseconds(ival);
    this->lastrun = steady_clock::now();

    LOG<<"Created new job with jobid: '"<<this->jobid<<"' with "<<this->interval.count()<<" ms duration"<<endl;
}


Job::Job(class Job& j)
{
    //LOG<<"Copying job with jobid '"<<j.jobid<<"'"<<endl;

    this->command = j.command;
    this->jobid = j.jobid;
    this->interval = j.interval;
    this->lastrun = j.lastrun;
}

Job::Job(const class Job& j)
{
    //LOG<<"Copying const job with jobid '"<<j.jobid<<"'"<<endl;

    this->command = j.command;
    this->jobid = j.jobid;
    this->interval = j.interval;
    this->lastrun = j.lastrun;
}

uint64_t Job::getJobId() const {
    return this->jobid;
}

std::string& Job::getCommand() {
    return this->command;
}

void Job::setScheduled() {
    LOG<<"Scheduling job..."<<std::endl;
    this->scheduled = true;
}

void Job::resetOverruns() {
    this->overruns = 0;
}

int64_t Job::getOverruns() {
    return this->overruns;
}

/* This will be used to see which job should be run first */
bool Job::operator<(const class Job& jright)
{
   microseconds us;

   duration<float> dl = steady_clock::now() - this->lastrun;
   duration<float> dr = steady_clock::now() - jright.lastrun;

   return (this->interval.count() - dl.count()) < (this->interval.count() - dr.count());
}

bool Job::nextRun() {

    duration<int, std::milli> dl = duration_cast<std::chrono::milliseconds>(steady_clock::now() - this->lastrun);

    if(dl.count() < this->interval.count()) {
        return false;
    }

    if(dl.count() > this->interval.count()) {
        this->overruns++;
    }

    LOG<<"Scheduling a process: "<<this->command<<"' with "<<dl.count()<<"ms duration elapsed"<<endl;
    this->lastrun = steady_clock::now();
    return true;
}

bool Job::spawnProcess() const {

    LOG<<"Spawning a process: "<<this->command<<"'"<<endl;
    pid_t child_pid = fork();

    switch(child_pid) {
        case 0:
            /* child process; Only call async signal safe commands after this! */
            close(0);
            close(1);
            close(2);
            if(execlp("/bin/sh","sh","-c", this->command.c_str(), NULL) == -1) {
                LOG<<"Child failed to exec"<<endl;
                exit(0);
            }
            /* child process gets replaced by execlp */
            break;
        case -1:
            /* error creating process */
            LOG<<"Fork failed to spawn process"<<std::strerror(errno)<<endl;
            break;
        default:
            /* parent process (thread here)*/
//            int status;
//            wait(&status);
            break;
    }
}

