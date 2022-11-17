#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_L 2048

// bool to keep track of fg mode
int flagSIGTSTP = 1;

// handler for SIGTSTP. Will switch foreground only mode
void handleSIGTSTP() {
  if (flagSIGTSTP == 1) {
    char *text = "\nEntering foreground-only mode (& is now ignored)\n";
    int text_len = strlen(text);
    fflush(stdin); // flush the output buffer
    write(1, text, text_len);

    flagSIGTSTP = 0;
  }

  else {
    char *text = "\nExiting foreground-only mode\n";
    int text_len = strlen(text);
    write(1, text, text_len);
    fflush(stdin); // flush the output buffer
    flagSIGTSTP = 1;
  }
}

int main() {
  // init variables
  int exitStat = 0, numPids = 0, signal = 0, wordCount, charCount, i, j,
      argC,                // amount of commands
      exitMethod,          // track exit method
      FD,                  // open files
      result,              // result of dup2()
      notExit = 1,         // while loop flag
      background = 0;      // switch for the background
  pid_t spawnpid = -5;     // child pid variable
  char inp[MAX_L],         // store user input
      inp_array[256][100], // process input with 2D array to store words
      chngdir[100],        // directory var
      *inp_args[256];      // capture arguments

  // init empty SIGINT struct
  struct sigaction SIGINT_action = {{0}};
  // register handler
  SIGINT_action.sa_handler = SIG_IGN;
  // set flags to 0
  SIGINT_action.sa_flags = 0;
  // block signals while handler function running
  sigfillset(&SIGINT_action.sa_mask);
  // install handler
  sigaction(SIGINT, &SIGINT_action, NULL);

  // init empty SIGTSTP struct
  struct sigaction SIGTSTP_action = {{0}};
  // register handler
  SIGTSTP_action.sa_handler = handleSIGTSTP;
  // no flags
  SIGTSTP_action.sa_flags = 0;
  // block signals while handler function running
  sigfillset(&SIGTSTP_action.sa_mask);
  // install handler
  sigaction(SIGTSTP, &SIGTSTP_action, NULL);

  // store pid as string
  char pid[10];
  sprintf(pid, "%d", getpid());

  while (notExit) {
    fflush(stdin);
    printf(": ");
    fflush(stdout);
    fgets(inp, MAX_L, stdin);
    inp[strcspn(inp, "\n")] = 0; // remove the newline from the string

    // do nothing with comments and blanks
    if (inp[0] == '#' || strlen(inp) == 0) {
      continue;
    }

    // exit, break while loop
    if (strcmp(inp, "exit") == 0) {
      notExit = 0;
      continue;
    }

    // process input and replace $$ with process id
    wordCount = 0, charCount = 0;
    for (i = 0; i <= (strlen(inp)); i++) {
      // replace "$$" with pid
      if (inp[i] == '$' && inp[i + 1] == '$') {
        for (j = 0; j < strlen(pid); j++) {
          inp_array[wordCount][charCount] = pid[j];
          charCount++;
        }
        i++;
        // move on to next word when there is space
      } else if (inp[i] == ' ' || inp[i] == '\0') {
        inp_array[wordCount][charCount] = '\0';
        charCount = 0;
        wordCount++;
      }

      // switch mode when there is ampersand
      else if (inp[i] == '&' && charCount == 0 && inp[i + 1] == '\0') {
        i = strlen(inp);
        background = 1;
      }
      // add char
      else {
        inp_array[wordCount][charCount] = inp[i];
        charCount++;
      }
      // printf(inp_array[wordCount]);
    }

    // process cd
    if (strcmp(inp_array[0], "cd") == 0) {
      fflush(stdout);
      // if no argument, chnage dir to HOME
      if (wordCount == 1) {
        chdir(getenv("HOME"));
        fflush(stdout);
        // if argument given, change dir to it if possible
      } else if (wordCount == 2) {
        if (chdir(inp_array[1]) != 0) {
          printf("Change to %s directory failed\n", chngdir);
        }
        // fflush(stdout);
      } else {
        printf("Unexpected arguments with cd command\n");
      }

    }

    // process status
    else if (strcmp(inp_array[0], "status") == 0) {
      // print terminating signal of last fg process
      if (signal) {
        printf("terminated by signal %d\n", signal);
        signal = 0;
        // print exit status
      } else {
        printf("exit value %d\n", exitStat);
      }
    }

    // executing other commands
    else {
      // check if the process forked without an error
      if ((spawnpid = fork()) == -1) {
        perror("fork() error");
        exit(1);
      }

      // child process
      else if (spawnpid == 0) {

        // add default SIGINT handler
        SIGINT_action.sa_handler = SIG_DFL;
        // install handler
        sigaction(SIGINT, &SIGINT_action, NULL);

        // add direction for redirections
        argC = 0;
        for (i = 0; i < wordCount; i++) {
          if (strcmp(inp_array[i], ">") == 0 || strcmp(inp_array[i], "<") == 0)
            break;
          else {
            inp_args[argC] = inp_array[i];
            argC++;
          }
        }

        // redirection handling
        while (i < wordCount) {
          if (strcmp(inp_array[i], ">") == 0) {
            // open file which is next argument
            FD = open(inp_array[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
            // output error
            if (FD == -1) {
              perror("targetopen()");
              exit(1);
            }
            // perform duplication
            result = dup2(FD, 1);
            if (result == -1) {
              perror("source dup2()");
              exit(1);
            }
            i = i + 2;
          } else if (strcmp(inp_array[i], "<") == 0) {
            FD = open(inp_array[i + 1], O_RDONLY);
            if (FD == -1) {
              perror("Cannot open file");
              exit(1);
            }
            result = dup2(FD, 0);
            if (result == -1) {
              perror("source dup2()");
              exit(1);
            }
            i = i + 2;
          }
        }

        inp_args[argC] = NULL;
        if (execvp(inp_args[0], inp_args)) {
          printf("No such file or directory\n");
          fflush(stdout);
          exit(2);
        }

        // close any opened files
        fcntl(FD, F_SETFD, FD_CLOEXEC);
      } else {
        // check foreground commands
        if (background && flagSIGTSTP) {
          waitpid(spawnpid, &exitMethod, WNOHANG);
          printf("Background pid is %d\n", spawnpid);

          // increment numPids to keep track of background pids
          numPids++;
        } else {
          waitpid(spawnpid, &exitMethod, 0);

          if (strcmp("kill", inp_array[0]) == 0) {
          } // ignore kill
          else if (WIFEXITED(exitMethod)) {
            signal = 0;
            exitStat = WEXITSTATUS(exitMethod);
          } else {
            signal = WTERMSIG(exitMethod);
            printf("terminated by signal %d\n", signal);
          }
        }
      }

      // check background processes
      while ((spawnpid = waitpid(-1, &exitMethod, WNOHANG)) > 0) {
        if (numPids == 0) {
          // no background pids
        } else if (WIFEXITED(exitMethod)) {
          // check if there was a problem
          signal = 0;
          exitStat = WEXITSTATUS(exitMethod);
          printf("Background pid %d is done: exit value %d\n", spawnpid,
                 exitStat);
        } else {
          signal = WTERMSIG(exitMethod);
          printf("Background pid %d is done: terminated by signal %d\n",
                 spawnpid, signal);
        }
        numPids--;
        fflush(stdout);
      }
    }
    background = 0;
  }
  return 0;
}