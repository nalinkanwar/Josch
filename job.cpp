#include <iostream>
#include <cstring>
#include <unistd.h>

#include <sys/wait.h>

#include "job.h"

Job::Job() { }

Job::~Job() {
    //LOG<<"Destructing job with jobid '"<<this->jobid<<"' and command "<<this->command<<std::endl;
}

Job::Job(const std::string cmd, int ival)
{
    this->command = cmd;
    this->jobid = JobId::nextJobId();
    this->interval = std::chrono::milliseconds(ival);
    this->lastrun = std::chrono::steady_clock::now();

    LOG<<"Created new job with jobid: '"<<this->jobid<<"' with "<<this->interval.count()<<" ms duration"<<std::endl;
}

Job::Job(const class Job& j)
{
    //LOG<<"Copying const job with jobid '"<<j.jobid<<"' and command "<<j.command<<std::endl;

    //this->command = j.command;
    this->command.clear();
    this->command.append(j.command);

    this->jobid = j.jobid;
    this->interval = j.interval;
    this->lastrun = j.lastrun;
}

uint64_t Job::get_job_id() const {
    return this->jobid;
}

const char* Job::get_command() const {
    return (this->command.c_str());
}

int64_t Job::get_interval() const {
    return this->interval.count();
}

void Job::set_scheduled() {
    LOG<<"Scheduling job..."<<std::endl;
    this->scheduled = true;
}

void Job::reset_overruns() {
    this->overruns = 0;
}

int64_t Job::get_overruns() {
    return this->overruns;
}

/* This will be used to see which job should be run first */
bool Job::operator<(const class Job& jright)
{
    using namespace std::chrono;

   microseconds us;

   duration<float> dl = steady_clock::now() - this->lastrun;
   duration<float> dr = steady_clock::now() - jright.lastrun;

   return (this->interval.count() - dl.count()) < (this->interval.count() - dr.count());
}

/* schedule the next time to run for the Job */
bool Job::next_run() {

    using namespace std::chrono;

    duration<int, std::milli> dl = duration_cast<milliseconds>(steady_clock::now() - this->lastrun);

    if(dl.count() < this->interval.count()) {
        return false;
    }

    if(dl.count() > this->interval.count()) {
        this->overruns++;
    }

    LOG<<"Scheduling a process: "<<this->command<<"' with "<<dl.count()<<"ms duration elapsed"<<std::endl;
    this->lastrun = steady_clock::now();
    return true;
}

bool Job::spawn_process() const {

    LOG<<"Spawning a process: '"<<this->command<<"'"<<std::endl;
    pid_t child_pid = fork();

    switch(child_pid) {
        case 0:
            /* child process; Only call async signal safe commands after this! */
            close(0);
            close(1);
            close(2);
            if(execlp("/bin/sh","sh","-c", this->command.c_str(), NULL) == -1) {
                LOG<<"Child failed to exec"<<std::endl;
                exit(0);
            }
            /* child process gets replaced by execlp */
            break;
        case -1:
            /* error creating process */
            LOG<<"Fork failed to spawn process"<<std::strerror(errno)<<std::endl;
            break;
        default:
            /* parent process (thread here)
               Will reap child with SIGCHLD handler;
               no need to do anything here */
            break;
    }
}

