#include <iostream>
#include <algorithm>
#include <mutex>
#include <thread>
#include <chrono>
#include "josch.h"

//template <typename clock>
Josch::Josch(): tpvec(MINTHREADS) { }
Josch::Josch(unsigned numthreads): tpvec(numthreads) { }

bool Josch::spawnExtraThreads(unsigned n) {

    unsigned maxthreads = std::thread::hardware_concurrency();

    if(maxthreads == 0) {
        maxthreads = MINTHREADS;
    }

    if(this->tpvec.size() > maxthreads) {
        return false;
    }
    for(int ctr = 0; ctr < n; ctr++) {
        this->tpvec.push_back(std::thread(&Josch::thread_loop, this));
    }

    return true;

}

void Josch::thread_loop() {
    /* start picking up from workqueue in threads */

    //LOG<<"Thread loop started"<<std::endl;

    while(true) {

        std::unique_lock<std::mutex> lock(this->mut_wq);
        while(this->workqueue.empty()) {
            //LOG<<"Going to wait for workqueue to get filled"<<std::endl;
            this->cv_wq.wait(lock);
        }
        //LOG<<"Woke up!"<<std::endl;
        const class Job& job = this->workqueue.top();
        this->workqueue.pop();
        job.spawnProcess();
    }
}

//template <typename clock>
bool Josch::register_job(Job &newj) {

    std::lock_guard<std::mutex> lock(this->mut_jl);

    LOG<<"Registering Job : "<<newj.getJobId()<<std::endl;
    this->jlist.push_back(newj);
}

//template <typename clock>
bool Josch::unregister_job(uint64_t tmpjobid) {

    std::lock_guard<std::mutex> lock(this->mut_jl);

    std::remove_if(this->jlist.begin(), this->jlist.end(), [&tmpjobid](const class Job& j) {
        if(j.getJobId() == tmpjobid) {
            return true;
        }
    });
}

bool Josch::unregister_job(Job& oldj) {
    std::lock_guard<std::mutex> lock(this->mut_jl);

    this->jlist.erase(std::remove_if(this->jlist.begin(), this->jlist.end(), [&oldj] (const class Job& j) {
        if(oldj.getCommand() != j.getCommand()) {
            return false;
        }
        if(oldj.getInterval() != j.getInterval()) {
            return false;
        }
        LOG<<"Removing "<<j.getCommand()<<" with interval "<<j.getInterval()<<std::endl;
        return true;
    }), this->jlist.end());
}

///template <typename clock>
void Josch::list_jobs() {

    LOG<<std::string(80, '=')<<std::endl;

    for(auto& job: this->jlist) {
        LOG<<" Job["<<job.getJobId()<<"]: '"<<job.getCommand()<<"' ival: "<<job.getInterval()<<std::endl;
    }

    LOG<<std::string(80, '=')<<std::endl;
}

void Josch::handle_jobs() {

    for(auto& t: this->tpvec) {
        t = std::thread(&Josch::thread_loop, this);
    }

    /* main JobScheduler loop */
    while (true) {

        std::unique_lock<std::mutex> lock(this->mut_wq);

        for(auto &next_task: this->jlist) {

//            if(next_task.getOverruns() > 100) {
//                next_task.resetOverruns();
//                this->spawnExtraThreads(MINTHREADS);
//            }

            if(/*(this->workqueue.size() < NUMTHREADS) &&*/ (next_task.nextRun() == true)) {
                this->workqueue.push(next_task);
            }
        }

        this->cv_wq.notify_all();
    }

    /* make sure all threads exit before killing the main thread */
    for(auto& t: this->tpvec) {
        t.join();
    }
}
