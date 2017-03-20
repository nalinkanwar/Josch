#include <iostream>
#include <mutex>
#include <thread>
#include <chrono>
#include "josch.h"

#define NUMTHREADS 4

//template <typename clock>
Josch::Josch(): tpvec(NUMTHREADS)
{

}

bool Josch::init() {

    /* @FIXME if alraedy inited, don't do it again! */
    /* initialize our thread pool first */
    LOG<<"Josch init start"<<std::endl;
    for(auto& t: this->tpvec) {
        t = std::thread(&Josch::thread_loop, this);
    }

    LOG<<"Josch inited"<<std::endl;
}

void Josch::thread_loop() {
    /* start picking up from workqueue in threads */

    LOG<<"Thread loop started"<<std::endl;

    while(true) {

        std::unique_lock<std::mutex> lock(this->mut_wq);
        while(this->workqueue.empty()) {
            //std::cout<<"Going to wait for workqueue to get full"<<std::endl;
            this->cv_wq.wait(lock);
        }
        //std::cout<<"["<< std::this_thread::get_id() << "] Woke up!"<<std::endl;
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

    //@FIXME
    //std::find()

}

///template <typename clock>
void Josch::list_jobs() {

}

void Josch::handle_jobs() {

    /* main JobScheduler loop */
    while (true) {

        std::unique_lock<std::mutex> lock(this->mut_wq);

        for(auto &next_task: this->jlist) {

            /* if we don't have enough scheduled jobs, schedule them */
            if((this->workqueue.size() < NUMTHREADS) && (next_task.nextRun() == true)) {
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
