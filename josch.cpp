#include <iostream>
#include <algorithm>
#include <mutex>
#include <thread>
#include <chrono>

#include "josch.h"

extern int main_quit;

//template <typename clock>
Josch::Josch(): tpvec(MINTHREADS) {  this->quit = false; }
Josch::Josch(unsigned numthreads): tpvec(numthreads) { this->quit = false; }

Josch::~Josch() {
    this->quit = true;

    /* make sure all threads exit before killing the main thread */
    for(auto& t: this->tpvec) {
        if(t.joinable()) {
            t.join();
        }
    }
}

bool Josch::spawn_extra_threads(unsigned n) {

    unsigned maxthreads = std::thread::hardware_concurrency();

    if(maxthreads == 0) {
        maxthreads = MINTHREADS;
    }

    if(this->tpvec.size() > maxthreads) {
        return false;
    }
    for(int ctr = 0; ctr < n; ctr++) {
        this->tpvec.push_back(std::thread(&Josch::thread_loop, this, std::ref(this->quit)));
    }

    return true;
}

void Josch::die() {
    this->quit = true;
}

void Josch::thread_loop(std::atomic<bool> &quit_flag) {
    /* start picking up from workqueue in threads */

    LOG<<"Thread started"<<std::endl;

    while(this->quit == false) {

        std::unique_lock<std::mutex> lock(this->mut_wq);
        while(this->workqueue.empty()) {
            //LOG<<"Going to wait for workqueue to get filled"<<std::endl;
            if(this->quit == true) {
                break;
            }
            this->cv_wq.wait(lock);
        }

        if(this->quit == true) {
            break;
        }
        //LOG<<"Woke up!"<<std::endl;
        const class Job& job = this->workqueue.top();        
        job.spawn_process();

        this->workqueue.pop();
    }
    LOG<<"Thread finished"<<std::endl;
}

//template <typename clock>
bool Josch::register_job(std::string &cmd, int ival) {
    try {
        std::lock_guard<std::mutex> lock(this->mut_jl);

        LOG<<"Registering Job : "<<cmd<<" with interval "<<ival<<std::endl;
        this->jlist.push_back(Job(cmd, ival));
    } catch (std::exception &e) {
        LOG<<"Couldn't register job: "<<e.what()<<std::endl;
        return false;
    }
    return true;
}

//template <typename clock>
bool Josch::unregister_job(uint64_t tmpjobid) {
    try {
        std::lock_guard<std::mutex> lock(this->mut_jl);

        auto it = std::remove_if(this->jlist.begin(), this->jlist.end(), [&tmpjobid](const class Job& j) {
            if(j.get_job_id() == tmpjobid) {
                LOG<<"Unregistering Job "<<j.get_job_id()<<" : "<<j.get_command()<<" with interval "<<j.get_interval()<<std::endl;
                return true;
            }
            return false;
        });

        if(it == this->jlist.end()) {
            /* didnt find the job */
            return false;
        }

        this->jlist.erase(it, this->jlist.end());
    } catch (std::exception &e) {
        LOG<<"Couldn't unregister job: "<<e.what()<<std::endl;
        return false;
    }
    return true;
}

bool Josch::unregister_job(std::string &cmd, int ival) {

    try {
        std::lock_guard<std::mutex> lock(this->mut_jl);

        auto it = std::remove_if(this->jlist.begin(), this->jlist.end(), [&cmd, &ival] (const class Job& j) {
            if(cmd != j.get_command()) {
                return false;
            }
            if(ival != j.get_interval()) {
                return false;
            }
            LOG<<"Removing "<<j.get_command()<<" with interval "<<j.get_interval()<<std::endl;
            return true;
        });

        if(it == this->jlist.end()) {
            /* didnt find the job */
            return false;
        }

        this->jlist.erase(it, this->jlist.end());
    } catch (std::exception &e) {
        LOG<<"Couldn't unregister job: "<<e.what()<<std::endl;
        return false;
    }
    return true;
}

///template <typename clock>
std::string Josch::list_jobs() {

    LOG<<std::string(80, '=')<<std::endl;

    std::string s;

    for(auto& job: this->jlist) {
        LOG<<" Job["<<job.get_job_id()<<"]: '"<<job.get_command()<<"' ival: "<<job.get_interval()<<std::endl;
        s.append(" Job [" + std::to_string(job.get_job_id()) + "]: " + job.get_command() + " ival: " + std::to_string(job.get_interval()) + "\n");
    }

    LOG<<std::string(80, '=')<<std::endl;

    return s;
}

void Josch::handle_jobs() {

    for(auto& t: this->tpvec) {
        t = std::thread(&Josch::thread_loop, this, std::ref(this->quit));
    }

    /* main JobScheduler loop */
    while (this->quit == false) {

        std::unique_lock<std::mutex> lock(this->mut_wq);
        std::unique_lock<std::mutex> lock2(this->mut_jl);

        for(auto &next_task: this->jlist) {

//            if(next_task.getOverruns() > 100) {
//                next_task.resetOverruns();
//                this->spawnExtraThreads(MINTHREADS);
//            }            
            if(/*(this->workqueue.size() < NUMTHREADS) &&*/ (next_task.next_run() == true)) {
                this->workqueue.push(next_task);
            }
        }
        lock2.unlock();

        this->cv_wq.notify_all();
        lock.unlock();
        /* this happens in main thread; so we are only accessing
         * this variable in one thread and doesn't need mutex */
        if(main_quit == true) {
            this->quit = true;
        }
    }

    /* wake up all threads before exiting */
    this->cv_wq.notify_all();
    LOG<<"Main thread done : "<<this->quit<<std::endl;

    /* make sure all threads exit before killing the main thread */
    for(auto& t: this->tpvec) {
        if(t.joinable()) {
            t.join();
        }
    }
}
