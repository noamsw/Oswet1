#include <unistd.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <sys/wait.h>
#include <sys/types.h>
#include <iomanip>
#include "Commands.h"
#include <time.h>
using namespace std;

#if 0
#define FUNC_ENTRY()  \
  cout << __PRETTY_FUNCTION__ << " --> " << endl;

#define FUNC_EXIT()  \
  cout << __PRETTY_FUNCTION__ << " <-- " << endl;
#else
#define FUNC_ENTRY()
#define FUNC_EXIT()
#endif

void cleanUp(int num_args, char** arguments)
{
    for (int i = 0 ; i < num_args ; i++)
    {
        free(arguments[i]);
        arguments[i] = nullptr;
    }
}

const string WHITESPACE = " \n\r\t\f\v";

string _ltrim(const std::string& s)
{
  size_t start = s.find_first_not_of(WHITESPACE);
  return (start == std::string::npos) ? "" : s.substr(start);
}

string _rtrim(const std::string& s)
{
  size_t end = s.find_last_not_of(WHITESPACE);
  return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

string _trim(const std::string& s)
{
  return _rtrim(_ltrim(s));
}

int _parseCommandLine(const char* cmd_line, char** args) {
  FUNC_ENTRY()
  int i = 0;
  std::istringstream iss(_trim(string(cmd_line)).c_str());
  for(std::string s; iss >> s; ) {
    args[i] = (char*)malloc(s.length()+1);
    memset(args[i], 0, s.length()+1);
    strcpy(args[i], s.c_str());
    args[++i] = NULL;
  }
  return i;

  FUNC_EXIT()
}

bool _isBackgroundComamnd(const char* cmd_line) {
  const string str(cmd_line);
  return str[str.find_last_not_of(WHITESPACE)] == '&';
}

void _removeBackgroundSign(char* cmd_line) {
  const string str(cmd_line);
  // find last character other than spaces
  unsigned int idx = str.find_last_not_of(WHITESPACE);
  // if all characters are spaces then return
  if (idx == string::npos) {
    return;
  }
  // if the command line does not end with & then return
  if (cmd_line[idx] != '&') {
    return;
  }
  // replace the & (background sign) with space and then remove all tailing spaces.
  cmd_line[idx] = ' ';
  // truncate the command line string up to the last non-space character
  cmd_line[str.find_last_not_of(WHITESPACE, idx) + 1] = 0;
}

// TODO: Add your implementation for classes in Commands.h 

int checkSpecial(char** arguments, int num_args)
{
    for (int i = 0; i < num_args ; i++)
    {
        if(strcmp(arguments[i], ">") == 0)
            return 1;

        if(strcmp(arguments[i], ">>") == 0)
            return 2;

        if(strcmp(arguments[i], "|") == 0)
            return 3;

        if(strcmp(arguments[i], "|&") == 0)
            return 4;
    }
    return 0; // cmd isnt special
}

void splitSpecialCommand(char* cmd_line, char* first_part, char* special_sign, char* sec_part)
{
    char* token = strtok(cmd_line, special_sign);
    strcpy(first_part, token);
    while(token)
    {
        strcpy(sec_part, token);
        token = strtok(NULL, special_sign);
    }
}

Command::Command(const char* cmd_line) : p_id(-1), job_id(-1), cmd_line(cmd_line)
{
    num_args = _parseCommandLine(cmd_line, arguments);
}

Command::~Command()
{
    cleanUp(num_args, arguments);
}

BuiltInCommand::BuiltInCommand(const char* cmd_line) : Command(cmd_line){}

ExternalCommand::ExternalCommand(const char* cmd_line) : Command(cmd_line)
{
    char new_line[COMMAND_ARGS_MAX_LENGTH];
    strcpy(new_line, cmd_line);
    if (_isBackgroundComamnd(cmd_line))
    {
        _removeBackgroundSign(new_line);
    }
    bash_args[0] = "/bin/bash";
    bash_args[1] = "-c";
    bash_args[2] = new_line;
    bash_args[3] = NULL;
}

void ExternalCommand::execute()
{
    pid_t returned_pid = fork();
    this->p_id = returned_pid;
    if (returned_pid == 0) // son
    {
        setpgrp();
        this->p_id = getpid();
        execv("/bin/bash", bash_args);
    }
    else // father
    {
        if(_isBackgroundComamnd(cmd_line))
        {
            SmallShell::getInstance().jobslist.addJob(this , false);
        }
        else // its fg
        {
            SmallShell::getInstance().cur_cmd = this;
            SmallShell::getInstance().p_running = true;
            int wstaus;
            waitpid(returned_pid, &wstaus, WSTOPPED); // check if options should be 0
            SmallShell::getInstance().p_running = false;
            SmallShell::getInstance().cur_cmd = nullptr;
        }
    }
}

void RedirectionCommand::execute()
{
    close(1); // closing STDOUT in the FDT. should we also close STDERR(?)
    int fd; // 1. check if 0666 is needed. 2. should we edit the file_name with _parse?
    if (append)
        fd = open(file_name, O_WRONLY|O_CREAT|O_APPEND, 0666);
    else
        fd = open(file_name, O_WRONLY|O_CREAT, 0666);

    dup(fd); // setting FDT[1] to file_name
    Command* new_cmd = SmallShell::getInstance().CreateCommand(cmd_line_no_rd);
    if(new_cmd != nullptr)
    {
        new_cmd->execute();
        delete new_cmd;
    }
    // reset file descriptor
    close(1);
}

void PipeCommand::execute()
{
}

int SmallShell::get_a_job_id() {
    // will returnt the current id open for a job and increment
    int  id = SmallShell::getInstance().max_job_id;
    SmallShell::getInstance().max_job_id++;
    return id;
}
void JobsList::addJob(Command* cmd, bool isStopped) {
    if(cmd->job_id == -1)
    {
        cmd->job_id = SmallShell::getInstance().get_a_job_id();
    }
    time_t time_entered;
    time(&time_entered);
    JobEntry newjob(cmd->job_id, cmd->p_id, time_entered, cmd->cmd_line, isStopped);
    removeFinishedJobs();
    bool inserted = false;
    for(std::vector<JobEntry>::iterator it = jobslist.begin(); it != jobslist.end(); ++it){
        if(it->job_id > newjob.job_id){
            jobslist.insert(it, newjob);
            inserted = true;
            break;
        }
    }
    if(!inserted){
        jobslist.push_back(newjob);
    }
}

void JobsList::removeFinishedJobs() {
    int status, return_val;
    for(auto  it = jobslist.begin(); it != jobslist.end(); ++it) { // a little confused with variable type
        return_val = waitpid(it->cmd_pid, &status, WNOHANG);
        if ( it->cmd_pid ==  jobslist[jobslist.size()-1].cmd_pid)
        {
            if (return_val != 0)  //im ignoring status -1 which means there was an error and assuming that it was dealt with
            {
                jobslist.erase(it);
            }
            break;
        }
        if (return_val != 0) { //im ignoring status -1 which means there was an error and assuming that it was dealt with
            it = jobslist.erase(it);
        }
    }
}

void JobsList::printJobsList() {
    removeFinishedJobs();
    time_t time_now;
    time(&time_now);
    for(auto & it : jobslist){
        time(&time_now); // im not sure how much of a diff itll make but i updated the time in the loop
        double seconds_since = difftime(time_now, it.t_entered);
        if(it.isStopped)
            cout << "[" << it.job_id << "]" << it.cmd_line << ":" << it.cmd_pid << " " << seconds_since << " secs (stopped)" << endl;
        else
            cout << "[" << it.job_id << "]" << it.cmd_line << ":" << it.cmd_pid << " " << seconds_since << " secs" << endl;
    }
}

JobsList::JobEntry* JobsList::getJobById(int jobId) { //returns nullptr if no job exists
    JobEntry* job = nullptr;
    for (auto & it  : jobslist) {
        if(it.job_id == jobId)
            job = &it; // i think that this will give me the address of the job stored in the vector
    }
    return job;
}

void JobsList::removeJobById(int jobId)
{
    for(auto  it = jobslist.begin(); it != jobslist.end(); ++it)
    {
        if(it->job_id == jobId)
        {
            jobslist.erase(it);
            return;
        }
    }
}

JobsList::JobEntry* JobsList::getLastStoppedJob(int *jobId)
{
    JobEntry* job = nullptr;
    for (auto & it  : jobslist) {
        if(it.isStopped)
        {
            job = &it;
            *jobId = it.job_id;
        }
    }
    return job;
}

void JobsList::killAllJobs()
{
    for (auto & it  : jobslist)
    {
        cout << it.cmd_pid << ": " << it.cmd_line << endl;
        kill(it.cmd_pid, SIGKILL);
    }
}

SmallShell::~SmallShell()
{
    if(prev_dir){
        free(prev_dir);
        prev_dir = nullptr;
    }
}

/**
* Creates and returns a pointer to Command class which matches the given command line (cmd_line)
*/
Command * SmallShell::CreateCommand(const char* cmd_line) {
  jobslist.removeFinishedJobs();
  string cmd_s = _trim(string(cmd_line));
  string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));

  char new_line[COMMAND_ARGS_MAX_LENGTH];
  strcpy(new_line, cmd_line);

  // if the command is builtin it cant be BG
  // remove "&' if exists
  if (firstWord == "chprompt" || firstWord == "showpid" || firstWord == "pwd" || firstWord == "cd" ||
          firstWord == "jobs" || firstWord == "kill" || firstWord == "fg" || firstWord == "bg" || firstWord == "quit")
  {
      if (_isBackgroundComamnd(cmd_line))
      {
          _removeBackgroundSign(new_line);
      }
  }

  char* arguments[COMMAND_MAX_ARGS];
  int num_args = _parseCommandLine(cmd_line, arguments);
  int special_type = checkSpecial(arguments, num_args);
  cleanUp(num_args, arguments); // check if argumens[] is needed later
  if (special_type == 1 or special_type == 2) // [redirection]: (1 for >) (2 for >>)
      {
      bool append = false;
      char special_sign[] = ">";
      if (special_type == 2)
      {
          append = true;
          strcpy(special_sign, ">>");
      }
      char cmd_no_rd[COMMAND_ARGS_MAX_LENGTH];
      char file_name[COMMAND_ARGS_MAX_LENGTH];
      char non_const_cmd_line[COMMAND_ARGS_MAX_LENGTH];
      strcpy(non_const_cmd_line, cmd_line);
      splitSpecialCommand(non_const_cmd_line, cmd_no_rd, special_sign, file_name);
      return new RedirectionCommand(cmd_line, cmd_no_rd, file_name, append);
  }

  if ((special_type == 3 or special_type == 4)) // [pipes]: (3 for |) (4 for |&)
  {

  }
  // if HEAD

  if (firstWord.compare("chprompt") == 0)
  {
      char* arguments[COMMAND_MAX_ARGS];
      int num_args = _parseCommandLine(new_line, arguments);
      if (num_args == 1)
      {
          prompt = "smash> ";
      } else
      {
          prompt = arguments[1];
          prompt.append("> ");
      }
      cleanUp(num_args, arguments);
  }

  else if (firstWord.compare("showpid") == 0)
  {
      cout << "smash pid is " << getpid() << endl;
  }

  else if (firstWord.compare("pwd") == 0) {
        char cwd[PATH_MAX];
        getcwd(cwd, PATH_MAX);
        std::string pwd(cwd);
        // printf("%s", cwd);
        std::cout << pwd << endl;
    }

  else if (firstWord.compare("cd") == 0){
        char* arguments[COMMAND_MAX_ARGS];
        int num_args = _parseCommandLine(new_line, arguments);
        string new_path = arguments[1];

        if (num_args > 2)
        {
            cout << "smash error: cd: too many arguments" << endl;
            cleanUp(num_args, arguments);
            return nullptr;
        }

        if (new_path.compare("-") == 0)
        {
            if (prev_dir == nullptr)
            {
                cout << "smash error: cd: OLDPWD not set" << endl;
                cleanUp(num_args, arguments);
                return nullptr;
            }
            char tmp_path[PATH_MAX];
            strcpy(tmp_path, prev_dir);
            free(prev_dir);
            prev_dir = nullptr;
            char cwd[PATH_MAX];
            getcwd(cwd, PATH_MAX);
            prev_dir = (char*)(malloc(PATH_MAX));
            strcpy(prev_dir, cwd);
            chdir(tmp_path);
        }
        else
        {
            if(prev_dir) {
                free(prev_dir);
                prev_dir = NULL;
            }
            char cwd[PATH_MAX];
            getcwd(cwd, PATH_MAX);
            prev_dir = (char*)(malloc(PATH_MAX));
            strcpy(prev_dir, cwd);
            chdir(arguments[1]);
        }
        cleanUp(num_args, arguments);
        return nullptr;
  }

  else if(firstWord.compare("jobs") == 0){
      jobslist.printJobsList();
  }

  else if(firstWord.compare("kill") == 0){
        char* arguments[COMMAND_MAX_ARGS];
        int num_args = _parseCommandLine(new_line, arguments);
        string sig = arguments[1];
        string id = arguments[2];
        string sig_num = arguments[1];
        sig_num = sig_num.substr(1);
        bool sig_not_digits = true;
        bool id_not_digits = true;
        if(sig_num.find_first_not_of("1234567890") == std::string::npos)// check to see that a number was entered
            sig_not_digits = false; //im not 100% sure that this is what i need to check, perhaps by val of signals
        if(id.find_first_not_of("1234567890") == std::string::npos)// check to see that a number was entered
            id_not_digits = false;
        if(num_args != 3 || sig[0] != '-' || sig_not_digits || id_not_digits){
            cout << "smash error: kill: invalid arguments" << endl;
            cleanUp(num_args, arguments);
            return nullptr;
        }
        int id_int = atoi(arguments[2]);
        JobsList::JobEntry* job = jobslist.getJobById(id_int);
        if(!job){
            cout << "smash error: kill: job-id" << id << "does not exist" << endl;
            cleanUp(num_args, arguments);
            return nullptr;
        }
        stringstream st_sig(sig_num);
        int sig_int = 0;
        st_sig >> sig_int;
        int return_val =  kill(job->cmd_pid, sig_int);// do we need to check that this is not sig_cont
        if (return_val){
            perror("smash error: kill failed");
        }
        else
            cout << "signal number " << sig_num << " was sent to pid " << job->cmd_pid << endl;
        cleanUp(num_args, arguments);
  }

  else if (firstWord.compare("fg") == 0)
  {
      char* arguments[COMMAND_MAX_ARGS];
      int num_args = _parseCommandLine(new_line, arguments);

      if (num_args == 2)
      {
          bool bad_format = true;
          string job_id_s = arguments[1];
          if(job_id_s.find_first_not_of("1234567890") == std::string::npos)
              bad_format= false;
          if(bad_format)
          {
              cout << "smash error: fg: invalid arguments" << endl;
              cleanUp(num_args, arguments);
              return nullptr;
          }
      }
      if (num_args > 2)
      {
          cout << "smash error: fg: invalid arguments" << endl;
          cleanUp(num_args, arguments);
          return nullptr;
      }

      // find the command in the jobs list:
      int job_id;
      if (num_args == 1) // job_id isnt given
      {
          int vector_size = jobslist.jobslist.size();
          job_id = jobslist.jobslist[vector_size-1].job_id;
      }
      else // job_id is given
        job_id = atoi(arguments[1]);

      JobsList::JobEntry* cur_job = jobslist.getJobById(job_id);
      if (num_args >= 2 && cur_job == nullptr)
      {
          cout << "smash error: fg: job-id " << job_id << " does not exist" << endl;
          cleanUp(num_args, arguments);
          return nullptr;
      }
      if (num_args == 1 && cur_job == nullptr)
      {
          cout << "smash error: fg: jobs list is empty" << endl;
          cleanUp(num_args, arguments);
          return nullptr;
      }

      cout << cur_job->cmd_line << " : " << cur_job->cmd_pid << endl;

      if (cur_job->isStopped)
        kill(cur_job->cmd_pid ,SIGCONT);

      int wstaus;
      pid_t cur_job_pid = cur_job->cmd_pid;
      char cur_job_cmd_line[COMMAND_ARGS_MAX_LENGTH];
      strcpy(cur_job_cmd_line, cur_job->cmd_line);
      jobslist.removeJobById(job_id);
      SmallShell::getInstance().cur_cmd = SmallShell::CreateCommand(cur_job_cmd_line);
      SmallShell::getInstance().cur_cmd->p_id = cur_job_pid; //so that we get the correct process to kill
      SmallShell::getInstance().cur_cmd->job_id = job_id;// so that we dont update a new job_id
      SmallShell::getInstance().p_running = true;
      waitpid(cur_job_pid, &wstaus, WSTOPPED); // is 0 the right value for the "options" arg?
      SmallShell::getInstance().p_running = false;
      delete(SmallShell::getInstance().cur_cmd);
      SmallShell::getInstance().cur_cmd  = nullptr;

      cleanUp(num_args, arguments);
      return nullptr;
  }

  else if (firstWord.compare("bg") == 0)
  {
      char* arguments[COMMAND_MAX_ARGS];
      int num_args = _parseCommandLine(new_line, arguments);

      if (num_args == 2)
      {
          bool bad_format = true;
          string job_id_s = arguments[1];
          if(job_id_s.find_first_not_of("1234567890") == std::string::npos)
              bad_format= false;
          if(bad_format)
          {
              cout << "smash error: bg: invalid arguments" << endl;
              cleanUp(num_args, arguments);
              return nullptr;
          }
      }
      if (num_args > 2)
      {
          cout << "smash error: bg: invalid arguments" << endl;
          cleanUp(num_args, arguments);
          return nullptr;
      }

      // find the command in the jobs list:
      JobsList::JobEntry* cur_job;
      if (num_args == 1) // job_id isnt given
      {
          int job_id;
          cur_job = jobslist.getLastStoppedJob(&job_id);
          if (cur_job == nullptr)
          {
              cout << "smash error: bg: there is no stopped jobs to resume" << endl;
              cleanUp(num_args, arguments);
              return nullptr;
          }
      }
      else // job_id is given
      {
          int job_id = atoi(arguments[1]);
          cur_job = jobslist.getJobById(job_id);
          if (cur_job == nullptr)
          {
              cout << "smash error: bg: job-id " << job_id << " does not exist" << endl;
              cleanUp(num_args, arguments);
              return nullptr;
          }
          if (!cur_job->isStopped)
          {
              cout << "smash error: bg: job-id " << job_id << " is already running in the background" << endl;
              cleanUp(num_args, arguments);
              return nullptr;
          }
      }

      cout << cur_job->cmd_line << " : " << cur_job->cmd_pid << endl;
      cur_job->isStopped = false;
      kill(cur_job->cmd_pid, SIGCONT);
      cleanUp(num_args, arguments);
      return nullptr;
  }

  else if (firstWord.compare("quit") == 0)
  {
      char* arguments[COMMAND_MAX_ARGS];
      int num_args = _parseCommandLine(new_line, arguments);

      if (num_args >= 2)
      {
          if (strcmp(arguments[1], "kill") == 0)
          {
              cout << "smash: sending SIGKILL signal to " << jobslist.jobslist.size() << " jobs:" << endl;
              jobslist.killAllJobs();
          }
      }
      cleanUp(num_args, arguments);
      exit (0);
  }

  // else if special commands?
  else
  {
        return new ExternalCommand(cmd_line); // check if we need to free it later or not (AND HOW?)
  }
  return nullptr;
}

void SmallShell::executeCommand(const char *cmd_line) {
    // TODO: Add your implementation here
    Command* cmd = CreateCommand(cmd_line);
    if (cmd != nullptr)
  {
      cmd->execute();
      delete(cmd);
  }
  // Please note that you must fork smash process for some commands (e.g., external commands....)
}