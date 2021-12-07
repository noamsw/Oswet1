#include <iostream>
#include <signal.h>
#include "signals.h"
#include "Commands.h"

using namespace std;

void ctrlZHandler(int sig_num) {
    cout << "smash: got ctrl-Z" << endl;
    if(SmallShell::getInstance().p_running){
        pid_t pid_to_kill = SmallShell::getInstance().cur_pid;
        SmallShell::getInstance().jobslist.addJob(SmallShell::getInstance().cmd_running, true);
        int return_val =  kill(SmallShell::getInstance().cur_pid, 19);
        cout << "smash: process " << pid_to_kill <<  " was stopped";
        SmallShell::getInstance().p_running = false;
        if (return_val){
            std::cout << "houston we have a problem" << endl; //this is just a check if killing a proccess doesnt work will remove before final issue.
        }
    }
}

void ctrlCHandler(int sig_num) {
    cout << "smash: got ctrl-C" << endl;
    if(SmallShell::getInstance().p_running){
        pid_t pid_to_kill = SmallShell::getInstance().cur_pid;
        int return_val =  kill(SmallShell::getInstance().cur_pid, 9);
        cout << "smash: process " << pid_to_kill <<  " was killed";
        SmallShell::getInstance().p_running = false;
        if (return_val){
            std::cout << "houston we have a problem" << endl; //this is just a check if killing a proccess doesnt work will remove before final issue.
        }
    }
}

void alarmHandler(int sig_num) {
  // TODO: Add your implementation
}

//