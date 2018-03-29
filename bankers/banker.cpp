/* Spring 2018 Operating Systems
   Lab3: Banker
   Grace(Jungwoo) Han
   jh5990@nyu.edu */

#include <cstdio>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <set>
#include <map>
#include <algorithm>
#include <queue>
#include <stack>
#include <list>
#include <iomanip>
#include <math.h>

using namespace std;
static string inputFile;
string algorithm;

int numTasks = 0;           // total number of tasks
int numResources = 0;       // total number of resources

void fifo();
void banker();
void printResult();

// Input activities for the tasks will be called works
class Works {
public:
    Works();
    string instruction;     // initiate, request, release, terminate
    int task;
    int delay;
    int resource;
    int unit;
};
Works::Works() { }

// Class for tasks
class Task {
public:
    Task();

    int taskID;
    int work;               // start from 0(initiate) to workList.size()(terminate), follows up which work the task is at
    int delayLeft;          // count down delay left
    int reqUnit;            // currently requested units of resource
    int reqSrcID;           // currently requested ID of resource

    bool blockHandled;      // if block was handled, task will not be processed in the cycle
    bool onblock;
    bool aborted;
    bool willBeAborted;
    bool isTerminated;

    vector<int> initClaim;  // task's initial claim for each resource type ***
    vector<int> resList;    // units of each resource type the task is currently holding ***
//    vector<int> accumList;
    vector<Works> workList; // stores all the work for each task from input

    int timeTakenFIFO;
    int timeWaitingFIFO;
    int timeTakenBanker;
    int timeWaitingBanker;
};

Task::Task() {
    aborted = false;
    isTerminated = false;
    willBeAborted = false;
    blockHandled = false;
    onblock = false;
    delayLeft = 0;
    work = 0;
    reqUnit = 0;
    reqSrcID = -1;
    timeTakenFIFO = 0;
    timeWaitingFIFO =0;
    timeTakenBanker = 0;
    timeWaitingBanker = 0;
}

vector<Task*> taskList;             // vector of all the tasks' pointer

// Class for resource
class Resource {
public:
    Resource();

    int resourceID;
    int totalUnit;
    int availUnit;
};

Resource::Resource() {
    availUnit = totalUnit;
}

vector<Resource*> resourceList;      // vector of all the resources' pointer

// Function to erase a taskID from a list
void removeID(vector<int> &list, int id) {
    list.erase(std::remove(list.begin(), list.end(), id), list.end());
}

// Go through the blocked list and check if any request can be granted
// Return 1 if nothing can be granted
int nothingCanBeGranted (vector<int> blockedList, vector<int> addFuture) {
    int cnt = 0;
    for(int i=0; i<blockedList.size(); i++) {
        int task_id = blockedList[i];
        Task* task = taskList[task_id-1];
        if(task->reqUnit <= (resourceList[(task->reqSrcID)-1]->availUnit + addFuture[(task->reqSrcID)-1])) {
            cnt++;
        }
    }
    if(cnt == 0) {
        return 1;
    }
    else {
        return 0;
    }
}

// Assign delays for next cycle
void assignDelay(Task* tsk, int nextWorkIdx){
    Works nxtWork = (tsk->workList)[nextWorkIdx];
    if(tsk->delayLeft == 0) {
        if((nxtWork.instruction == "request")){
            tsk->delayLeft = nxtWork.delay;
        }
        else if(nxtWork.instruction == "release"){
            tsk->delayLeft = nxtWork.delay;
        }
        else if(nxtWork.instruction == "terminate") {
            tsk->delayLeft = nxtWork.delay;
        }
        else {
            tsk->delayLeft = 0;
        }
    }
}

// First In First Out algorithm
void fifo() {
    algorithm = "fifo";

    // Initialize
    int cycle = 0;
    int terminated = 0;
    int block = 0;

    vector<int> blockTaskList = {};         // stores the taskID of tasks on block

                                            // Add to block
                                            //  - When request cannot be satisfied
                                            // Remove from block
                                            //  - When removed from block

    // Initialize the resList of all tasks with zeros (size == numResources)
    for(int i=0; i<taskList.size(); i++) {
        for(int j=0; j<numResources; j++) {
            (taskList[i]->resList).push_back(0);
        }
    }

    // Initialize the ready list with all the task IDs
    //      pull out from readyList if:
    //              1. a task is put on delay
    //              2. a task terminates/aborts
    vector<int> readyList;
    for(int x=0; x<taskList.size(); x++) {
        readyList.push_back(taskList[x]->taskID);
    }

    // Until all tasks terminate do:
    while(terminated < taskList.size()) {
        bool found = false;                     // marks deadlock state
        vector<int> toRemoveFromblockList;      // stores tasks to be removed from block list at the end of cycle
        vector<int> removeFromReady;            // stores tasks to be removed from ready list at the end of cycle
        vector<int> addToAvail;                 // stores units that will become available at the next cycle

        // Initialize addToAvail
        for(int a=0; a<numResources; a++) {
            addToAvail.push_back(0);
        }

        // Initiate all
        if(cycle < numResources) {
            for (int i=0; i<numResources; i++) {
//                cout << "During " << cycle << "-" << cycle+1 << " each task completes its initiate " << i+1 << endl;
                // move on to the next work on workList
                for(int i=0; i<taskList.size(); i++){
                    taskList[i]->work += 1;
                    assignDelay(taskList[i], taskList[i]->work);
                }
                // increase cycle
                cycle++;
            }
        }

        // For cycles other than initiate do:
        else {
//            cout << endl;
//            cout << "During " << cycle << "-" << cycle+1 << endl;

            // Initialize blockHandled to false;
            for (int i=0; i<taskList.size(); i++) {
                taskList[i]->blockHandled = false;
            }

            // First check if tasks on block can be granted
            if(blockTaskList.size()>0){
//                cout << "    First check blocked tasks." << endl;

                for(int h=0; h<blockTaskList.size(); h++){
                    int tskID = blockTaskList[h];
                    Task* curTsk = taskList[tskID-1];
                    int srcID = curTsk->reqSrcID;

                    // If request can be granted
                    if(curTsk->reqUnit <= resourceList[srcID-1]->availUnit) {
//                        cout << "        Task " << curTsk->taskID << "'s pending request is granted." << endl;
                        // grant the resources
                        // deduct from availUnit of the resource
                        resourceList[srcID-1]->availUnit -= curTsk->reqUnit;
                        // add the requested units to task's resList
                        (curTsk->resList)[srcID-1] += curTsk->reqUnit;
                        // remove units from reqUnit
                        curTsk->reqUnit = 0;
                        curTsk->reqSrcID = -1;
                        curTsk->blockHandled = true;
                        curTsk->onblock = false;
                        curTsk->work++;
                        assignDelay(curTsk, curTsk->work);
                        block--;
                        // remove the task ID from blockTaskList
                        toRemoveFromblockList.push_back(tskID);
                        curTsk->onblock = false;
                    }
                    else{
                        // if request cannot be granted increase the waiting time
                        // leave the task on the block list
                        curTsk->timeWaitingFIFO += 1;
//                        cout << "        Task " << curTsk->taskID << " is on block, but could not be granted." << endl;
                    }
                }
            }

            // Remove tasks from blocked list
            for(int t=0; t<toRemoveFromblockList.size(); t++) {
                removeID(blockTaskList,toRemoveFromblockList[t]);
            }

            // after the tasks on block are handled
            // go through all the tasks in ready that has not been handled for block
            for(int k=0; k<readyList.size(); k++) {
                Task* nowTask = taskList[readyList[k]-1];

                // if the task is on delay, compute for delay
                if((nowTask->delayLeft > 0)&& (nowTask->blockHandled == false)) {
//                    cout << "Task " << nowTask->taskID << " delayed " << nowTask->delayLeft << endl;
                    nowTask->delayLeft -= 1;

                    // if delay is done
                    if(nowTask->delayLeft == 0) {
                        // check if task is ready to terminate
                        if((nowTask->workList[nowTask->work]).instruction == "terminate") {
//                            cout << "    Task " << nowTask->taskID << " terminates at " << cycle+1 << endl;
                            nowTask->isTerminated = true;
                            nowTask->timeTakenFIFO = cycle+1;
                            terminated += 1;
                            removeFromReady.push_back(nowTask->taskID);
                        }
                    }
                }
                // if the task is not on delay
                else {
                    // if the task has not been block-handled and is not blocked
                    if(((nowTask->blockHandled)==false)&&(nowTask->onblock == false)){
                        Works nowWork = nowTask->workList[nowTask->work];

                        // A. if the instruction is "request"
                        if(nowWork.instruction == "request") {
                            nowTask->reqUnit = nowWork.unit;
                            nowTask->reqSrcID = nowWork.resource;
                            int unitsRequested = nowWork.unit;
                            int srcIDRequested = nowWork.resource;
                            // check if resource can be granted
                            // if possible grant it
                            if(unitsRequested <= resourceList[srcIDRequested-1]->availUnit) {
//                                cout << "    Task " << nowTask->taskID << " completes its request (i.e., the request is granted)." << endl;
                                (nowTask->resList)[srcIDRequested-1] += unitsRequested;
                                resourceList[srcIDRequested-1]->availUnit -= unitsRequested;
                                nowTask->work += 1;
                                assignDelay(nowTask, nowTask->work);
                            }
                            // if impossible, put on block
                            else {
                                nowTask->timeWaitingFIFO += 1;
                                nowTask->onblock = true;

                                // add to block list if it is not already there
                                if (std::find(blockTaskList.begin(), blockTaskList.end(),nowTask->taskID)
                                    == blockTaskList.end()) {
                                    blockTaskList.push_back(nowTask->taskID);
                                    nowTask->onblock = true;
                                    block++;
                                }
//                                cout << "    Task " << nowTask->taskID << "'s request cannot be granted" << endl;

                                // check deadlock state
                                if(((block+terminated)==numTasks)&&(toRemoveFromblockList.size()==0)) {
//                                    cout << "   << DEADLOCK >>" << endl;
                                    // abort task
                                    // always choose the task with min taskID
                                    vector<int> sortedList;
                                    vector<int> rmBlockList;
                                    vector<int> rmReadyList;

                                    for(int h=0; h<blockTaskList.size(); h++){
                                        sortedList.push_back(blockTaskList[h]);
                                    }

                                    std::sort(sortedList.begin(), sortedList.end());

                                    // while there's no task that can be granted the request
                                    // abort a task until deadlock is solved
                                    if(nothingCanBeGranted(blockTaskList, addToAvail) == 1) {
                                        for(int a=0; a<sortedList.size(); a++) {
                                            int abortID = sortedList[a];
//                                            cout << "    Task " << abortID << " is aborted." << endl;
                                            Task* abortTask = taskList[abortID-1];
                                            abortTask->aborted = true;
                                            abortTask->delayLeft = 0;
                                            for(int r=0; r<(abortTask->resList).size(); r++) {
//                                                cout << "    Release resource " << r+1 << ": " << (abortTask->resList)[r]
//                                                     << " unit(s)." << endl;
                                                addToAvail[r] += (abortTask->resList)[r];
                                                (abortTask->resList)[r] = 0;
                                            }
                                            block--;
                                            terminated++;
                                            removeID(blockTaskList, abortID);
                                            rmReadyList.push_back(abortID);
                                            if(nothingCanBeGranted(blockTaskList, addToAvail) == 0) {
                                                break;
                                            }
                                        }
                                    }
                                    for (int r=0; r<rmReadyList.size(); r++) {
                                        removeID(readyList, rmReadyList[r]);
                                    }
                                }
                            }
                        }
                        // B. if the instruction is "release"
                        else if(nowWork.instruction == "release") {
                            int unitsToRelease = nowWork.unit;
                            int srcIDToRelease = nowWork.resource;
//                            cout << "    Task "<< nowTask->taskID << " releases "<< unitsToRelease
//                                 << " unit(s) of Resource " << srcIDToRelease << endl;
                            (nowTask->resList)[srcIDToRelease-1] = 0;       // updtae holding unit to zero
                            addToAvail[srcIDToRelease-1] += unitsToRelease;
                            nowTask->work += 1;                             // move on to the next work id
                            assignDelay(nowTask, nowTask->work);

                            // check if the next work is terminate
                            Works nextWork = nowTask->workList[nowTask->work];
                            if((nextWork.instruction == "terminate")&&(nowTask->delayLeft == 0)) {
//                                cout << "    Task " << nowTask->taskID << " terminates at " << cycle+1 << endl;
                                nowTask->isTerminated = true;
                                nowTask->timeTakenFIFO = cycle+1;
                                terminated += 1;
                                removeFromReady.push_back(nowTask->taskID);
                            }
                        }
                    }
                }
            }
            // add released units to each resource's availUnit
            for(int a=0; a<numResources; a++) {
                resourceList[a]->availUnit += addToAvail[a];
            }

            for (int rm=0; rm<removeFromReady.size(); rm++) {
                removeID(readyList, removeFromReady[rm]);
            }
            for (int r=0; r<resourceList.size(); r++) {
                Resource* res = resourceList[r];
//                cout << "    (Resource " << res->resourceID << " available: " << res->availUnit << " unit(s))" << endl;
            }
            cycle++;
        }
    } // End of while loop

    // After all tasks are terminated, print out the result summary
    printResult();
    cout << endl;
} // End of fifo()

// Function for checking safe states
// A task T requests for some units of some resource R.
// We will check if granting this request leads to a safe state.
// Function returns true if it is safe
// Result : Unsafe(0), Safe(1), Error(2)
int isSafe(Task* T, Resource* R, int reqUnit) {
    // Create arrays
    // 1. available[]: currently available units of each resource type
    int available[numResources];
    for (int i=0; i<numResources; i++) {
        available[i] = resourceList[i]->availUnit;
    }

    // 2. allocated[][]: units of resources of each type that are currently allocated to each task
    int allocated[numTasks][numResources];
    for(int i=0; i<numTasks; i++){
        Task* tsk = taskList[i];
        for(int j=0; j<numResources; j++) {
            allocated[i][j] = tsk->resList[j];
        }
    }

    // 3. need[][]: units of each resource type that can be requested from each task
    //              need = max - allocated
    int need[numTasks][numResources];
    for(int i=0; i<numTasks; i++) {
        Task* tsk = taskList[i];
        vector<int> initClaimList = tsk->initClaim;
        for(int j=0; j<numResources; j++) {
            // Terminated or aborted tasks will not need any units.
            if(tsk->isTerminated || tsk->aborted) {
                need[i][j] = 0;
            }
            else {
                need[i][j] = initClaimList[j] - allocated[i][j];
            }
        }
    }

    // Check if requested units exceeds available units (error)
    if(reqUnit>need[T->taskID-1][R->resourceID-1]) {
        return 2;
    }

    // Assume that the request is granted
    available[R->resourceID-1] -= reqUnit;
    allocated[T->taskID-1][R->resourceID-1] += reqUnit;
    need[T->taskID-1][R->resourceID-1] -= reqUnit;

    // Create an array of flags to check if all task can lead to termination
    int finish[numTasks];
    // initialize to false
    for(int i=0; i<numTasks; i++) {
        finish[i] = 0;
    }

    // Store safe sequence
    int safeSeq[numTasks];

    // Make a copy of available resources
    int work[numResources];
    for(int i=0; i<numResources; i++){
        work[i] = available[i];
    }

    int count = 0;
    // While all tasks are not finished
    while(count < numTasks)
    {
        // Find a process which is not finished and whose needs can be satisfied with current work[] resources.
        bool found = false;
        for (int p=0; p<numTasks; p++)
        {
            // first check if a process is finished. if not, go to the next condition
            if (finish[p] == 0)
            {
                int j;
                for(j = 0; j<numResources; j++)
                    // if need cannot be satisfied, go on to the next
                    if(need[p][j] > work[j])
                        break;

                // If all needs of the task was satisfied
                if (j == numResources)
                {
                    for (int k = 0; k < numResources; k++)
                        work[k] += allocated[p][k];     // release the resources

                    safeSeq[count++] = p;       // add to safe sequence
                    finish[p] = 1;              // mark as finished
                    found = true;
                }
            }
        }
        // if safe sequence could not be found for the whole step, UNSAFE
        if(found == false)
        {
            return 0;
        }
    }

    // SAFE
    return 1;
}

// Banker's Algorithm
void banker() {
    algorithm = "banker";

    // initialize all the tasks
    for (int t=0; t<taskList.size(); t++) {
        Task* tk = taskList[t];
        tk->work = 0;           // start from 0(initiate) to workList.size()(terminate
        tk->delayLeft = 0;
        tk->reqUnit = 0;
        tk->reqSrcID = -1;
        tk->blockHandled = false;
        tk->onblock = false;
        tk->aborted = false;
        tk->isTerminated = false;
        tk->resList = {};
    }

    int cycle = 0;
    int terminated = 0;
    int block = 0;

    vector<int> blockTaskList = {};      // stores the taskID of tasks on block

    // initialize the resList of all tasks with zeros (size == numResources)
    for(int i=0; i<taskList.size(); i++) {
        for(int j=0; j<numResources; j++) {
            (taskList[i]->resList).push_back(0);
        }
    }

    vector<int> readyList;
    for(int x=0; x<taskList.size(); x++) {
        readyList.push_back(taskList[x]->taskID);
    }
    // initialize accumList
//    for(int x=0; x<taskList.size(); x++) {
//        Task* cTask = taskList[x];
//        for(int y=0; y<resourceList.size(); y++){
//            (cTask->accumList).push_back(0);
//        }
//    }

    // Until all tasks terminate do:
    while(terminated < taskList.size()) {
        bool found = false;
        vector<int> toRemoveFromblockList;
        vector<int> removeFromReady;
        vector<int> addToAvail;

        for(int a=0; a<numResources; a++) {
            addToAvail.push_back(0);
        }

        // ERROR CHECK: abort any task that claims more than available
        if(cycle == 0) {
            // go through every task
            for(int t=0; t<taskList.size(); t++) {
                Task* tsk = taskList[t];
                vector<int> claimList = tsk->initClaim;

                // go through all resources
                for(int r=0; r<resourceList.size(); r++) {
                    if(claimList[r] > resourceList[r]->totalUnit) {
                        cout << "Banker aborts task " << t+1 << " before run begins:\n"
                                "       claim for resource " << r+1 << " ("<<claimList[r]
                             <<") exceeds number of units present ("<<resourceList[r]->totalUnit<<")" << endl;
                        tsk->willBeAborted = true;
                    }
                }

                if(tsk->willBeAborted == true) {
                    tsk->aborted = true;
                    tsk->delayLeft = 0;
                    terminated++;
                    removeID(readyList, tsk->taskID);
                }
            }
        }

        // Initiate step
        if(cycle < numResources) {
            for (int i=0; i<numResources; i++) {
//                cout << "During " << cycle << "-" << cycle+1 << " each task completes its initiate " << i+1 << endl;
                // move on to the next work on workList
                for(int i=0; i<taskList.size(); i++){
                    taskList[i]->work += 1;
                    assignDelay(taskList[i], taskList[i]->work); // ****
                }
                cycle++;
            }
        }

        else {
//            cout << "During " << cycle << "-" << cycle+1 << endl;
            // initialize blockHandled to false
            for (int i=0; i<taskList.size(); i++) {
                taskList[i]->blockHandled = false;
            }

            // check if tasks on block can be granted
            if(blockTaskList.size()>0){
//                cout << "    First check blocked tasks." << endl;
                for(int h=0; h<blockTaskList.size(); h++){
                    int tskID = blockTaskList[h];
                    Task* curTsk = taskList[tskID-1];
                    int srcID = curTsk->reqSrcID;

                    // [Unblock] Check if request can be granted
                    // 1) requested units are less than equal to available units
                    // 2) state is safe after granting the resources
                    if((curTsk->reqUnit <= resourceList[srcID-1]->availUnit)
                       && isSafe(curTsk, resourceList[srcID-1], curTsk->reqUnit) == 1)
                    {
//                        cout << "Requested ResourceID: " << srcID << ", Units:  " << curTsk->reqUnit << endl;
//                        cout << "        Task " << curTsk->taskID << "'s pending request is granted." << endl;
                        // grant the resources
                        // deduct from availUnit of the resource
                        resourceList[srcID-1]->availUnit -= curTsk->reqUnit;
                        // add the requested units to task's resList
                        (curTsk->resList)[srcID-1] += curTsk->reqUnit;
                        // remove units from reqUnit
                        curTsk->reqUnit = 0;
                        curTsk->reqSrcID = -1;
                        curTsk->blockHandled = true;
                        curTsk->onblock = false;
                        curTsk->work++;

                        // increment the accumulated request for this resource
//                        (curTsk->accumList)[srcID-1] += curTsk->reqUnit;
                        assignDelay(curTsk, curTsk->work);
                        block--;
                        // remove the task ID from blockTaskList
                        toRemoveFromblockList.push_back(tskID);
                        curTsk->onblock = false;
                    }
                    else{
                        // if request cannot be granted block it
                        curTsk->timeWaitingBanker += 1;
//                        cout << "        Task " << curTsk->taskID << " is on block, but could not be granted." << endl;
                        // leave the task on the block list
                    }

                }
            }

            for(int t=0; t<toRemoveFromblockList.size(); t++) {
                removeID(blockTaskList,toRemoveFromblockList[t]);
            }
            // after the tasks on block are handled
            // go through all the tasks in ready
            for(int k=0; k<readyList.size(); k++) {
                // that has not been handled for block
                Task* nowTask = taskList[readyList[k]-1];

                // if the task is on delay, compute for delay
                if((nowTask->delayLeft > 0)&& (nowTask->blockHandled == false)) {
//                    cout << "Task " << nowTask->taskID << " delayed " << nowTask->delayLeft << endl;
                    nowTask->delayLeft -= 1;
                    if(nowTask->delayLeft == 0) {
                        if((nowTask->workList[nowTask->work]).instruction == "terminate") {
//                            cout << "    Task " << nowTask->taskID << " terminates at " << cycle+1 << endl;
                            nowTask->isTerminated = true;
                            nowTask->timeTakenBanker = cycle+1;
                            terminated += 1;
                            removeFromReady.push_back(nowTask->taskID);
                        }
                    }
                }
                // if the task is not on delay
                else {
                    if(((nowTask->blockHandled)==false)&&(nowTask->onblock == false)){
                        Works nowWork = nowTask->workList[nowTask->work];

                        // A. if the instruction is "request"
                        if(nowWork.instruction == "request") {
                            nowTask->reqUnit = nowWork.unit;
                            nowTask->reqSrcID = nowWork.resource;
                            int unitsRequested = nowWork.unit;
                            int srcIDRequested = nowWork.resource;
                            int srcAccum = unitsRequested + (nowTask->resList)[srcIDRequested-1];
                            int srcInitClaim = (nowTask->initClaim)[srcIDRequested-1];

                            // Check if units requested exceeds its claim
                            if(srcAccum > srcInitClaim)
                            {
                                cout << "During cycle "<<cycle<<"-"<<cycle+1<<" of Banker's algorithms\n"<<
                                        "   Task "<<nowTask->taskID<<"'s request exceeds its claim; aborted; ";
                                nowTask->aborted = true;
                                nowTask->delayLeft = 0;

                                vector<int> srcList = nowTask->resList;
                                for(int u=0; u<srcList.size(); u++) {
                                    addToAvail[u] += srcList[u];
                                    (nowTask->resList)[u] = 0;
                                    cout << srcList[u] << " unit(s) of Resource " << u+1 << " available next cycle" << endl;
                                }
                                terminated++;
                                removeID(readyList, nowTask->taskID);
                            }
                            else {
                                // [Request] Check if request can be granted
                                // 1) requested units are less than equal to available units
                                // 2) state is safe after granting the resources
                                if((unitsRequested <= resourceList[srcIDRequested-1]->availUnit)
                                   && (isSafe(nowTask, resourceList[srcIDRequested-1], unitsRequested)==1)) {
//                                    cout << "    Task " << nowTask->taskID << " completes its request (i.e., the request is granted)." << endl;
                                    (nowTask->resList)[srcIDRequested-1] += unitsRequested;
                                    resourceList[srcIDRequested-1]->availUnit -= unitsRequested;
                                    nowTask->work += 1;

                                    // increment the accumulated request for this resource
//                                    (nowTask->accumList)[srcIDRequested-1] += unitsRequested;
                                    assignDelay(nowTask, nowTask->work);
                                }
                                // if impossible, put on block
                                else {
                                    nowTask->timeWaitingBanker += 1;
                                    nowTask->onblock = true;

                                    if (std::find(blockTaskList.begin(), blockTaskList.end(),nowTask->taskID)
                                        == blockTaskList.end()) {
                                        blockTaskList.push_back(nowTask->taskID);
                                        nowTask->onblock = true;
                                        block++;
                                    }
//                                    cout << "    Task " << nowTask->taskID << "'s request cannot be granted" << endl;

                                    if(((block+terminated)==numTasks)&&(toRemoveFromblockList.size()==0)) {
//                                        cout << "   << DEADLOCK >>" << endl;
                                        // abort task
                                        // always choose the task with min taskID
                                        vector<int> sortedList;
                                        vector<int> rmBlockList;
                                        vector<int> rmReadyList;

                                        for(int h=0; h<blockTaskList.size(); h++){
                                            sortedList.push_back(blockTaskList[h]);
                                        }

                                        std::sort(sortedList.begin(), sortedList.end());

                                        // while there's no task that can be granted the request
                                        // abort a task until deadlock is solved
                                        if(nothingCanBeGranted(blockTaskList, addToAvail) == 1) {
                                            for(int a=0; a<sortedList.size(); a++) {
                                                int abortID = sortedList[a];
//                                                cout << "    Task " << abortID << " is aborted." << endl;
                                                Task* abortTask = taskList[abortID-1];
                                                abortTask->aborted = true;
                                                abortTask->delayLeft = 0;
                                                for(int r=0; r<(abortTask->resList).size(); r++) {
//                                                    cout << "    Release resource " << r+1 << ": " << (abortTask->resList)[r]
//                                                         << " unit(s)." << endl;
                                                    addToAvail[r] += (abortTask->resList)[r];
                                                    (abortTask->resList)[r] = 0;
                                                }
                                                block--;
                                                terminated++;
                                                removeID(blockTaskList, abortID);
                                                rmReadyList.push_back(abortID);
                                                if(nothingCanBeGranted(blockTaskList, addToAvail) == 0) {
                                                    break;
                                                }
                                            }
                                        }

                                        for (int r=0; r<rmReadyList.size(); r++) {
                                            removeID(readyList, rmReadyList[r]);
                                        }
                                    }
                                }
                            }

                        }
                        // B. if the instruction is "release"
                        else if(nowWork.instruction == "release") {
                            int unitsToRelease = nowWork.unit;
                            int srcIDToRelease = nowWork.resource;
//                            cout << "    Task "<< nowTask->taskID << " releases "<< unitsToRelease
//                                 << " unit(s) of Resource " << srcIDToRelease << endl;
                            (nowTask->resList)[srcIDToRelease-1] = 0;
                            addToAvail[srcIDToRelease-1] += unitsToRelease;
                            nowTask->work += 1;

                            assignDelay(nowTask, nowTask->work);

                            // check if the next work is to terminate
                            Works nextWork = nowTask->workList[nowTask->work];
                            if((nextWork.instruction == "terminate")&&(nowTask->delayLeft == 0)) {
//                                cout << "    Task " << nowTask->taskID << " terminates at " << cycle+1 << endl;
                                nowTask->isTerminated = true;
                                nowTask->timeTakenBanker = cycle+1;
                                terminated += 1;
                                removeFromReady.push_back(nowTask->taskID);
                            }
                        }
                    }
                }
            }
            // add released units to each resource's availUnit
            for(int a=0; a<numResources; a++) {
                resourceList[a]->availUnit += addToAvail[a];
            }

            for (int rm=0; rm<removeFromReady.size(); rm++) {
                removeID(readyList, removeFromReady[rm]);
            }
            for (int r=0; r<resourceList.size(); r++) {
                Resource* res = resourceList[r];
//                cout << "    (Resource " << res->resourceID << " available: " << res->availUnit << " unit(s))" << endl;
            }
            cycle++;
        }
    } // End of while loop

    // After all tasks are terminated, print out the result summary
    printResult();
} // End of banker()

void printResult() {
    int totalTaken = 0;
    int totalWait = 0;
    int taken = 0;
    int wait = 0;


    if(algorithm=="fifo") {
        cout << setw(20) << "FIFO" << endl;
    }
    else if(algorithm=="banker") {
        cout << endl;
        cout << setw(23) << "Banker's" << endl;
    }

    for(int i=0; i<taskList.size(); i++) {
        if(taskList[i]->aborted == true) {
            cout << "Task " << i+1 << setw(14) << "aborted" << endl;
        }
        else {
            if(algorithm=="fifo") {
                taken = taskList[i]->timeTakenFIFO;
                wait = taskList[i]->timeWaitingFIFO;
            }
            else if(algorithm=="banker") {
                taken = taskList[i]->timeTakenBanker;
                wait = taskList[i]->timeWaitingBanker;
            }

            totalTaken += taken;
            totalWait += wait;
            int delayPercent = round(wait / double(taken) * 100);
            cout << "Task " << i+1 << setw(8) << taken << setw(8)
                 << wait << setw(8) << delayPercent << "%" << endl;
        }
    }
    int totalDelay = round(totalWait / double(totalTaken) * 100);
    cout << "Total " << setw(8) << totalTaken << setw(8)
                     << totalWait << setw(8) << totalDelay << "%" << endl;
}

int main(int argc, char* argv[]){
    // get input file as the first argument
    inputFile = argv[1];
    ifstream infile;
    infile.open(inputFile.c_str(), ifstream::in);

    if (infile.is_open()) {
        infile >> numTasks;
        for (int i=0; i<numTasks; i++){
            // create a new task
            Task* newTask = new Task();

            // taskID starts from 1
            newTask->taskID = i+1;
            taskList.push_back(newTask);
        }

        struct less_than_id {
            bool operator()(Task *task1, Task *task2)
            {
                return (task1->taskID < task2->taskID);
            }
        } less_than_id;

        // sort taskList by increasing order of taskID
        if (taskList.size() > 1) {
            std::sort(taskList.begin(), taskList.end(), less_than_id);
        }

        infile >> numResources;

        int resourceCap[numResources];              // The capacity of each resource

        for (int i=0; i<numResources; i++){
            Resource* newResource = new Resource();
            newResource->resourceID = i+1;          // resource ID starts from 1
            resourceList.push_back(newResource);

            int units = 0;
            infile >> units;

            resourceCap[i] = units;
            newResource->totalUnit = units;
            newResource->availUnit = units;
        }

        string instr;
        int t, d, r, u;
        // while there is work to be read
        while(infile >> instr >> t >> d >> r >> u){
            Works tmpWork;

            tmpWork.instruction = instr;
            tmpWork.task = t;
            tmpWork.delay = d;
            tmpWork.resource = r;
            tmpWork.unit = u;

            if (instr == "initiate") {
                // assign units value to initClaim
                (taskList[t-1]->initClaim).push_back(u);
            }
            (taskList[t-1]->workList).push_back(tmpWork);
        }
        fifo();
        banker();
    }

    // if the input file is not open return an error message
    else {
        cout << "File cannot be opened." << endl;
    }

    return 0;
}