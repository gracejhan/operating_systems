/* Spring 2018 Operating Systems
   Lab2: Scheduler
   Jungwoo(Grace) Han
   jh5990@nyu.edu */

#include <cstdio>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <queue>
#include <stack>
#include <list>
#include <iomanip>

#define QUANTUM 2

using namespace std;

// StateTypes is used for getting strings for ProcStates
std::string StateTypes[] = { "unstarted", "ready", "running", "blocked", "terminated" };
typedef enum { UNSTARTED, READY, RUNNING, BLOCKED, TERMINATED } ProcStates;
std::string ModelTypes[] = {"First Come First Served", "Round Robin", "Uniprocessor", "Preemptive Shortest Job First"};
enum  Models { FCFS, RR, UNIPROG, SRTN, LAST } ;

static string input_file;
static string rand_file;

int timer = 0;
int num_processes = 0;
int randnum_cnt = 0;
int io_bool = false;
int io_cnt = 0;

std::vector<int> randVec;

class Process {
public:
    Process();

    int process_id;
    int arrival_time;               // A
    int proc_fixed_cpuburst;        // B
    int proc_fix_totalcpu;          // C
    int io_burst;                   // IO

    bool io_now;                    // true if in BLOCKED state
    bool io_last;
    bool wait_now;                  // true if in READY state
    bool wait_last;
    bool preempted;                 // true if preempted
    bool returning_to_wait;
    int remain_cpu_burst;           // randomOS - consumed
    int remain_io_burst;
    int dur_io_burst;               // IO burst time interval
    int total_io_burst;             // sum of IO burst time interval
    int io_start_time;
    int io_end_time;
    int wait_start_time_saved;
    int dur_wait;                   // waiting time interval
    int total_wait;                 // sum of waiting time intervals
    int wait_start_time;
    int wait_end_time;
    int proc_left_totalcpu;         // proc_fix_totalcpu - consumed
    int finish_time;
    float added_to_ready_time;
    int qt;                         // quantum time

    string getMode() const;
    int ioDuration();
    int waitDuration();
    ProcStates currentState;
};

Process::Process(){
    io_now = false;
    io_last = false;
    wait_now = false;
    wait_last = false;
    preempted = false;
    returning_to_wait = false;
    qt = QUANTUM;

    remain_cpu_burst = -1;

    total_io_burst = 0;
    total_wait = 0;
}

int Process::ioDuration() {
    return io_end_time - io_start_time;
}

int Process::waitDuration() {
    return wait_end_time - wait_start_time;
}

vector<Process*> procList;  // All processes are stored in order of arrival_time
vector<Process*> procList_unsorted;
vector<Process*> list_unstarted;
vector<Process*> list_blocked;
vector<Process*> list_running;   // process in running
//vector<Process*> list_terminated;
queue<Process*> list_notTerminated; // process not terminated

Process * currentRunning = NULL;

string Process::getMode() const {
    return StateTypes[int(currentState)];
}

/* give higher priority to process with smaller id */
struct less_than_id {
    bool operator()(Process *process1, Process *process2)
    {
        return (process1->process_id > process2->process_id);
    }
};

struct less_than_id_reverse {
    bool operator()(Process *process1, Process *process2)
    {
        return (process1->process_id < process2->process_id);
    }
} less_than_id1;

/* SRTN queue priority logic */
struct less_than_work {
    bool operator()(Process *process1, Process *process2)
    {
        if(process1->proc_left_totalcpu != process2->proc_left_totalcpu)
            return (process1->proc_left_totalcpu > process2->proc_left_totalcpu);
        else
            return process1->process_id > process2->process_id;
    }
};

struct ready_insert_conflict {
    bool operator()(Process *process1, Process *process2)
    {
        if(process1->added_to_ready_time != process2->added_to_ready_time)
            return (process1->added_to_ready_time > process2->added_to_ready_time);
        else
            return process1->process_id > process2->process_id;
    }
};

/*** Scheduler Class ***/
class Scheduler {
public:
    virtual void addProcess(Process* process) = 0;
    virtual Process* getProcess() = 0;
    virtual bool isEmpty() = 0;
private:
};

Scheduler* current_scheduler;

/* FCFS */
class FCFS_Scheduler: public Scheduler {
public:
    void addProcess(Process* process);
    Process* getProcess();
    bool isEmpty();
private:
    queue<Process*> list_ready;
}fcfs_scheduler;

void FCFS_Scheduler::addProcess(Process* process) {
    process->currentState = READY;
    process->wait_now = true;
    process->wait_last = true;
    process->wait_start_time = timer;

    list_ready.push(process);
}

Process* FCFS_Scheduler::getProcess() {
    if(list_ready.empty())
        return NULL;
    Process* p = list_ready.front();
    p->wait_now = false;
    p->wait_end_time = timer;
    list_ready.pop();
    return p;
}

bool FCFS_Scheduler::isEmpty() {
    return list_ready.size() == 0;
}

/* UNIPROG */
class UNIPROG_Scheduler: public Scheduler {
public:
    void addProcess(Process* process);
    Process* getProcess();
    bool isEmpty();
private:
    std::priority_queue <Process*, vector<Process*>, less_than_id> list_ready;
} uniprog_scheduler;

void UNIPROG_Scheduler::addProcess(Process* process) {
    process->currentState = READY;
    process->wait_now = true;
    process->wait_last = true;
    process->wait_start_time = timer;

    list_ready.push(process);
}

Process* UNIPROG_Scheduler::getProcess() {
    if(list_notTerminated.empty()) return NULL;
    Process* p = list_notTerminated.front();
    if(p->currentState == BLOCKED) return NULL;
    else if(p->currentState == READY) {
        p->wait_now = false;
        p->wait_end_time = timer;
        return p;
    }
    else return p;
}

bool UNIPROG_Scheduler::isEmpty() {
    return list_ready.size() == 0;
}

/* SRTN */
class SRTN_Scheduler: public Scheduler {
public:
    void addProcess(Process* process);
    Process* getProcess();
    Process* preemptRunning();
    bool isEmpty();
//private:
    std::priority_queue <Process*, vector<Process*>, less_than_work> list_ready;
} srtn_scheduler;


void SRTN_Scheduler::addProcess(Process* process) {
    process->currentState = READY;
    process->wait_now = true;
    process->wait_last = true;
    if(process->returning_to_wait == true)
        process->wait_start_time = process->wait_start_time_saved;
    else
        process->wait_start_time = timer;

    list_ready.push(process);
}

Process* SRTN_Scheduler::getProcess() {
    if(list_ready.empty())
        return NULL;
    Process* p = list_ready.top();
    p->wait_now = false;
    p->wait_end_time = timer;
    list_ready.pop();
    return p;
}

bool SRTN_Scheduler::isEmpty() {
    return list_ready.size() == 0;
}

int RandomOS(int randnum, int modulo) {
    int result;
    if(modulo == 0) {
        result = 0;
    }
    else {
        result = (randnum % modulo) + 1;
    }
    return result;
}


Process* SRTN_Scheduler::preemptRunning() {
    Process* cmp = srtn_scheduler.getProcess();
    // If a process was pulled out from list_ready
    if(cmp != NULL) {

        // If there's a process with same or shorter burst left, use the new one
        if (
                ((cmp->proc_left_totalcpu == currentRunning->proc_left_totalcpu) &&
                 (cmp->process_id < currentRunning->process_id))
                || (cmp->proc_left_totalcpu < currentRunning->proc_left_totalcpu)
                )
        {
            cmp->currentState = RUNNING;
            // If currentRunning has enough cpu burst left, just add it to the queue
            if(currentRunning->remain_cpu_burst > 0){
                srtn_scheduler.addProcess(currentRunning);
            }
            // If currentRunning doesn't have enough cpu burst left, block it
            else {
                currentRunning->currentState = BLOCKED;
                currentRunning->io_last = true;
                currentRunning->io_now = true;
                currentRunning->io_start_time = timer;
                currentRunning->remain_io_burst = RandomOS(randVec[randnum_cnt], currentRunning->io_burst);
                randnum_cnt++;
                list_blocked.push_back(currentRunning);
            }
            // If cmp doesn't have enough cpu_burst, get a new cpu burst
            if (cmp->remain_cpu_burst <= 1) {
                cmp->remain_cpu_burst = RandomOS(randVec[randnum_cnt], cmp->proc_fixed_cpuburst);
                randnum_cnt++;
            }
            return cmp;
        }
        // If there's a process with same or shorter burst left, continue with a current one
        else
        {
            cmp->returning_to_wait = true;
            cmp->wait_start_time_saved = cmp->wait_start_time;
            srtn_scheduler.addProcess(cmp);
            return currentRunning;
        }
    }
    // If a process was NOT pulled out from list_ready
    else {
        return currentRunning;
    }
}

class RR_Scheduler: public Scheduler {
public:
    void addProcess(Process* process);
    Process* getProcess();
    bool isEmpty();
//private:
    std::priority_queue <Process*, vector<Process*>, ready_insert_conflict> list_ready;

} rr_scheduler;

void RR_Scheduler::addProcess(Process* process) {
    process->currentState = READY;
    process->wait_now = true;
    process->wait_last = true;
    process->wait_start_time = timer;
    process->added_to_ready_time = timer;

    list_ready.push(process);
}

Process* RR_Scheduler::getProcess() {
    if(list_ready.empty())
        return NULL;
    Process* p = list_ready.top();
    p->wait_now = false;
    p->wait_end_time = timer;
    p->qt = QUANTUM;
    list_ready.pop();
    return p;
}

bool RR_Scheduler::isEmpty() {
    return list_ready.size() == 0;
}

void ReadInputFile() {
    ifstream infile1;
    infile1.open(input_file.c_str(), ifstream::in);

    if (infile1.is_open()) {
        procList.clear();
        list_unstarted.clear();
        // get the total number of processes
        infile1 >> num_processes;

        for(int i=0; i<num_processes; i++){
            // create a new process
            Process* newProcess = new Process();

            infile1 >> newProcess->arrival_time >> newProcess->proc_fixed_cpuburst
                    >> newProcess->proc_fix_totalcpu >> newProcess->io_burst;
            newProcess->process_id = i;
            newProcess->currentState = UNSTARTED;
            newProcess->proc_left_totalcpu = newProcess->proc_fix_totalcpu;

            procList.push_back(newProcess);
            list_unstarted.push_back(newProcess);
        }
        procList_unsorted = procList;

        struct less_than_arrivals{
            bool operator() (Process *process1, Process *process2)
            {
                return (process1->arrival_time < process2->arrival_time);
            }
        }  less_than_arrival_obj;
        std::sort(procList.begin(), procList.end(), less_than_arrival_obj);

        // assign the process ID as the position inside the vector
        for(int j=0; j<procList.size(); j++) {
            procList[j]->process_id = j;
        }

    }
}

// Read random numbers into a vector randVec (modulo not applied yet)
void ReadRandFile() {
    ifstream infile2;
    infile2.open(rand_file.c_str(), ifstream::in);

    if (infile2.is_open())
    {
        while (!infile2.eof()) {
            int x;
            infile2 >> x;
            randVec.push_back(x);
        }
    }
}

int main(int argc, char* argv[]) {

    bool verbose;
    verbose = false;

    if( strcmp(argv[1], "--verbose") == 0 ){
        verbose = true;
        input_file = argv[2];
        rand_file = argv[3];
    }
    else {
        input_file = argv[1];
        rand_file = argv[2];
    }

    for(int fooInt=FCFS; fooInt != LAST; fooInt++)
    {
        Models m = static_cast<Models>(fooInt);
        ReadInputFile();
        ReadRandFile();
        timer = 0;
        randnum_cnt = 0;
        // io_bool = false;
        io_cnt = 0;

        vector<Process*> list_terminated;

        switch (m)
        {
            case FCFS:
                current_scheduler = &fcfs_scheduler;
                break;
            case UNIPROG:
                current_scheduler = &uniprog_scheduler;
                break;
            case SRTN:
                current_scheduler = &srtn_scheduler;
                break;
            case RR:
                current_scheduler = &rr_scheduler;
                break;
            case LAST:
                current_scheduler = &fcfs_scheduler;
        }
        cout<< setw(25) << "The original input was: " << setw(4) << num_processes << "    ";
        for (int i=0; i< procList_unsorted.size(); i++) {
            cout << procList_unsorted[i]->arrival_time << " " << procList_unsorted[i]->proc_fixed_cpuburst << " "
                 << procList_unsorted[i]->proc_fix_totalcpu << " " << procList_unsorted[i]->io_burst << "    ";
        }
        cout << endl << setw(25) << "The (sorted) input is: " << setw(4) << num_processes << "    ";
        for (int i=0; i< procList.size(); i++) {
            cout << procList[i]->arrival_time << " " << procList[i]->proc_fixed_cpuburst << " "
                 << procList[i]->proc_fix_totalcpu << " " << procList[i]->io_burst << "    ";
        }
        if (verbose == true){
            cout << endl;
            cout << endl << "This detailed printout gives the state and remaining burst for each process" << endl;
            cout << endl;
        }

        randnum_cnt = 0;
        currentRunning = NULL;
        queue<Process*> list_empty;
        list_notTerminated = list_empty;
        // initialize list_notterminated with all processes in procList
        for(int i=0; i< procList.size(); i++) {
            list_notTerminated.push(procList[i]);
        }

        // run while loop until all processes terminate
        while(list_terminated.size() < procList.size()) {
            if(verbose == true) {
                cout << " Before Cycle " << setw(4) << timer << ":   ";
            }

            io_bool = false;
            for(int k=0; k<procList.size(); k++)
            {
                // Initialize boolean
                procList[k]->returning_to_wait = false;

                if(verbose == true){
                    // Print the state of all processes
                    std::string curr_state = procList[k]->getMode();
                    cout << setw(12) << curr_state << setw(2) << "  ";
                    if(curr_state == "ready") cout << setw(2) << "0" << "  ";
                    else if(curr_state == "blocked") {
                        io_bool = true;
                        cout << setw(2) << procList[k]->remain_io_burst + 1 << "  ";
                    }
                    else if(curr_state == "running") cout << setw(2) << procList[k]->remain_cpu_burst + 1 << "  ";
                    else if(curr_state == "terminated") cout << setw(2) << "0" << "  ";
                    else if(curr_state == "unstarted") cout << setw(2) << "0" << "  ";
                }
            }
            for(int l=0; l<procList.size(); l++) {
                std::string curr_state = procList[l]->getMode();
                if(curr_state == "blocked")
                    io_bool = true;
            }
            if(io_bool == true) io_cnt++;
            std::sort(list_blocked.begin(), list_blocked.end(), less_than_id1);

            /* BLOCKED => READY */
            //    Go through list_blocked and check if anything is done with IO
            //    Pass unblocked processes to the Scheduler
            int size2 = list_blocked.size();
            for(int j=0; j<size2; j++) {
                if(list_blocked[j]->remain_io_burst == 0)
                {
                    current_scheduler->addProcess(list_blocked[j]);
                    list_blocked[j]->io_now = false;
                    list_blocked.erase(list_blocked.begin() + j);
                    j--;
                    size2--;
                }
            }

            /* UNSTARTED => READY */
            int size1 = list_unstarted.size();
            for(int i=0; i<size1; i++) {
                if(list_unstarted[i]->arrival_time == timer){
                    current_scheduler->addProcess(list_unstarted[i]);
                    list_unstarted.erase(list_unstarted.begin() + i);
                    i--;
                    size1--;
                }
            }

            // SRTN: preempt the running process
            if(m == SRTN) {
                if((currentRunning != NULL) && (currentRunning->currentState == RUNNING)
                   && (currentRunning->proc_left_totalcpu > 0)) {
                    currentRunning = srtn_scheduler.preemptRunning();
                }
            }
            // If there is nothing running
            if (currentRunning == NULL) {
                // Try getting a new process to run

                Process* get_proc = current_scheduler->getProcess();

                // If there is a process in Ready
                // Compute and assign the random cpu burst
                // Add the process to list_running
                if (get_proc != NULL) {
                    if((m == FCFS)||(m == UNIPROG)) {
                        get_proc->remain_cpu_burst = RandomOS(randVec[randnum_cnt], get_proc->proc_fixed_cpuburst);
                        randnum_cnt++;
                    }
                    else {
                        if(get_proc->remain_cpu_burst <= 0 ) {
                            get_proc->remain_cpu_burst = RandomOS(randVec[randnum_cnt], get_proc->proc_fixed_cpuburst);
                            randnum_cnt++;
                        }
                    }
                    // READY => RUNNING
                    // add it to the running list
                    list_running.push_back(get_proc);
                    get_proc->currentState = RUNNING;
                    get_proc->proc_left_totalcpu -= 1;
                    get_proc->remain_cpu_burst -= 1;
                    if(m == RR){
                        get_proc->qt -= 1;
                    }
                    currentRunning = get_proc;
                }
                // If there's no process in Ready currentRunning stays NULL
            }

            // If there is a process running
            else if((currentRunning != NULL)&&(currentRunning->proc_left_totalcpu != 0)) {
                // If there is no more left_cpuburst
                if((currentRunning->remain_cpu_burst) <= 0) {
                    // If total work is not done, get io_burst and put into Blocked state
                    // RUNNING => BLOCKED
                    currentRunning->remain_io_burst
                            = RandomOS(randVec[randnum_cnt], currentRunning->io_burst);
                    randnum_cnt++;
                    list_blocked.push_back(currentRunning);
                    currentRunning->currentState = BLOCKED;
                    currentRunning->io_last = true;
                    currentRunning->io_now = true;
                    currentRunning->io_start_time = timer;
                    currentRunning = NULL;

                    // 3A-0. Try getting a new process to run
                    Process* get_proc = current_scheduler->getProcess();
                    if (get_proc != NULL) {
                        if((m == FCFS)||(m == UNIPROG)){
                            get_proc->remain_cpu_burst = RandomOS(randVec[randnum_cnt], get_proc->proc_fixed_cpuburst);
                            randnum_cnt++;
                        }
                        else {
                            if(get_proc->remain_cpu_burst <= 0) {
                                get_proc->remain_cpu_burst = RandomOS(randVec[randnum_cnt], get_proc->proc_fixed_cpuburst);
                                randnum_cnt++;
                            }
                        }

                        // add it to the running list
                        list_running.push_back(get_proc);
                        get_proc->currentState = RUNNING;
                        get_proc->proc_left_totalcpu -= 1;
                        get_proc->remain_cpu_burst -= 1;
                        if(m == RR){
                            get_proc->qt -= 1;
                        }
                        currentRunning = get_proc;
                    }
                }

                // CPU burst is left, but quantum = 0
                // RUNNING => READY
                else if((currentRunning->remain_cpu_burst) > 0 && (currentRunning->qt == 0)) {
                    rr_scheduler.addProcess(currentRunning);
                    currentRunning = NULL;

                    // Try getting a new process to run
                    Process* get_proc = rr_scheduler.getProcess();
                    if (get_proc != NULL) {
                        if((m == FCFS)||(m == UNIPROG)){
                            get_proc->remain_cpu_burst = RandomOS(randVec[randnum_cnt], get_proc->proc_fixed_cpuburst);
                            randnum_cnt++;
                        }
                        else {
                            if(get_proc->remain_cpu_burst <= 0) {
                                get_proc->remain_cpu_burst = RandomOS(randVec[randnum_cnt], get_proc->proc_fixed_cpuburst);
                                randnum_cnt++;
                            }
                        }

                        // add it to the running list
                        list_running.push_back(get_proc);
                        get_proc->currentState = RUNNING;
                        get_proc->proc_left_totalcpu -= 1;
                        get_proc->remain_cpu_burst -= 1;
                        get_proc->qt -= 1;
                        currentRunning = get_proc;
                    }
                }

                /* 3 */
                // 3B-2. If there is still cycle_left_cpuburst
                //       RUNNING => RUNNING
                else if((currentRunning->remain_cpu_burst > 0) && (currentRunning->qt > 0)) {
                    currentRunning->remain_cpu_burst -= 1;
                    currentRunning->proc_left_totalcpu -= 1;
                    if(m == RR) {
                        currentRunning->qt -= 1;
                    }
                }
            }

            else if((currentRunning != NULL)&&(currentRunning->proc_left_totalcpu == 0))
            {
                currentRunning->currentState = TERMINATED;
                currentRunning->finish_time = timer;
                list_terminated.push_back(currentRunning);
                list_notTerminated.pop();

                // 3A-0. Try getting a new process to run
                Process* get_proc = current_scheduler->getProcess();
                if (get_proc != NULL) {
                    if((m == FCFS)||(m == UNIPROG)){
                        get_proc->remain_cpu_burst = RandomOS(randVec[randnum_cnt], get_proc->proc_fixed_cpuburst);
                        randnum_cnt++;
                    }
                    else{
                        if(get_proc->remain_cpu_burst <= 0) {
                            get_proc->remain_cpu_burst = RandomOS(randVec[randnum_cnt], get_proc->proc_fixed_cpuburst);
                            randnum_cnt++;
                        }
                    }

                    list_running.push_back(get_proc);
                    get_proc->currentState = RUNNING;
                    get_proc->proc_left_totalcpu -= 1;
                    get_proc->remain_cpu_burst -= 1;
                    if(m == RR){
                        get_proc->qt -= 1;
                    }
                    currentRunning = get_proc;
                }
            }

            // 4. Decrease IO burst time for all Blocked processes
            for(int b=0; b<list_blocked.size(); b++) {
                if(list_blocked[b]->proc_left_totalcpu == 0){
                    list_blocked[b]->currentState = TERMINATED;
                    list_blocked[b]->finish_time = timer;
                    list_terminated.push_back(list_blocked[b]);
                    list_notTerminated.pop();
                    list_blocked.erase(list_blocked.begin() + b);
                    b--;
                } else {
                    list_blocked[b]->remain_io_burst -= 1;
                }
            }

            // If I/O has been unblocked, save the timer
            for(int i=0; i<procList.size(); i++) {
                if((procList[i]->io_last == true)&&(procList[i]->io_now == false)) {
                    procList[i]->io_end_time = timer;
                    procList[i]->dur_io_burst = procList[i]->ioDuration();
                    procList[i]->total_io_burst += procList[i]->dur_io_burst;
                    procList[i]->io_last = false;
                }
                if((procList[i]->wait_last == true)&&(procList[i]->wait_now == false)) {
                    procList[i]->dur_wait = procList[i]->waitDuration();
                    procList[i]->total_wait += procList[i]->dur_wait;
                    procList[i]->wait_last = false;
                }
            }
            if(verbose == true) cout << endl;
            timer += 1;
        }

        /* Summary */
        cout << endl;
        cout << endl << "The scheduling algorithm used was " << ModelTypes[int(m)] << endl;
        cout << endl;

        int sum_cpu_bursts = 0;
        int sum_io_bursts = 0;
        int sum_turnarounds = 0;
        int sum_waits = 0;
        for(int i=0; i<procList.size(); i++){
            cout << "Process " << i << ":" << endl;
            cout << "        " << "(A,B,C,IO) = (" << procList[i]->arrival_time << ","
                 << procList[i]->proc_fixed_cpuburst << "," << procList[i]->proc_fix_totalcpu
                 << "," << procList[i]->io_burst << ")" << endl;
            cout << "        "<< "Finishing time: " << procList[i]->finish_time << endl;
            cout << "        "<< "Turnaround time: " << procList[i]->finish_time - procList[i]->arrival_time
                 << endl;
            cout << "        "<< "I/O time: " << procList[i]-> total_io_burst << endl;
            cout << "        "<< "Waiting time: " << procList[i]->total_wait << endl;
            cout << endl;

            sum_cpu_bursts += procList[i]->proc_fix_totalcpu;
            sum_io_bursts += procList[i]->total_io_burst;
            sum_turnarounds += (procList[i]->finish_time - procList[i]->arrival_time);
            sum_waits += procList[i]->total_wait;
        }

        std::cout.setf(std::ios_base::fixed, std::ios_base::floatfield);
        std::cout.precision(6);
        cout << "Summary Data: " << endl;
        cout << "        " << "Finishing time: " << timer -1 << endl;
        cout << "        " << "CPU Utilization: "<< sum_cpu_bursts/float(timer-1)<< endl;
        cout << "        " << "I/O Utilization: " << io_cnt/float(timer-1) << endl;
        cout << "        " << "Throughput: " << procList.size()/float(timer-1)*100 << " processes per hundred cycles" << endl;
        cout << "        " << "Average turnaround time: " << sum_turnarounds/float(procList.size()) << endl;
        cout << "        " << "Average waiting time: " << sum_waits/float(procList.size()) << endl;
        cout << endl;
        cout << "====================================================================================================="<< endl;
    }
    return 0;
}
