
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/param.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>

#include "cmd_parse.h"

unsigned short is_verbose = 0;

pid_t current_child_pid = -1;

#define PROMPT_LEN 100
#define HOST_LEN 50

#define HIST 15
char *hist[HIST] = {0};

void sigint_handler(int sig);

int main(int argc, char *argv[])
{
	char str[MAX_STR_LEN] = {'\0'};
	char *ret_val = NULL;
	char *raw_cmd = NULL;
	cmd_list_t *cmd_list = NULL;
	int cmd_count = 0;
	char prompt[PROMPT_LEN] = {'\0'};
	char *cwd;
	char hostname[HOST_LEN] = {'\0'};

	simple_argv(argc, argv);
	signal(SIGINT, sigint_handler);

	for (;;)
	{
		// Set up a cool user prompt.
		// test to see of stdout is a terminal device (a tty)
		if (isatty(STDOUT_FILENO))
		{
			cwd = getcwd(NULL, 0);
			if (cwd == NULL)
			{
				perror("getcwd failed");
				exit(EXIT_FAILURE);
			}
			gethostname(hostname, sizeof(hostname));

			sprintf(prompt, " %s%s\n%s@%s # ", PROMPT_STR, cwd, getenv("LOGNAME"), hostname);

			fputs(prompt, stdout);
			free(cwd);
		}

		memset(str, 0, MAX_STR_LEN);
		ret_val = fgets(str, MAX_STR_LEN, stdin);

		if (NULL == ret_val)
		{
			// Bust out of the input loop and go home.
			break;
		}

		// STOMP on the pesky trailing newline returned from fgets().
		if (str[strlen(str) - 1] == '\n')
		{
			// replace the newline with a NULL
			str[strlen(str) - 1] = '\0';
		}
		if (strlen(str) == 0)
		{
			// An empty command line.
			// Just jump back to the promt and fgets().
			continue;
		}

		if (strcmp(str, BYE_CMD) == 0)
		{
			// Pickup your toys and go home
			break;
		}

		// update of history command
		if (strcmp(str, HISTORY_CMD) != 0)
		{
			free(hist[HIST - 1]);
			for (int i = HIST - 2; i >= 0; i--)
			{
				hist[i + 1] = hist[i];
			}
			hist[0] = strdup(str);
		}

		// Basic commands are pipe delimited.
		raw_cmd = strtok(str, PIPE_DELIM);

		cmd_list = (cmd_list_t *)calloc(1, sizeof(cmd_list_t));

		cmd_count = 0;
		while (raw_cmd != NULL)
		{
			cmd_t *cmd = (cmd_t *)calloc(1, sizeof(cmd_t));

			cmd->raw_cmd = strdup(raw_cmd);
			cmd->list_location = cmd_count++;

			if (cmd_list->head == NULL)
			{
				// An empty list.
				cmd_list->tail = cmd_list->head = cmd;
			}
			else
			{
				// Make this the last in the list of cmds
				cmd_list->tail->next = cmd;
				cmd_list->tail = cmd;
			}
			cmd_list->count++;

			// Get the next raw command.
			raw_cmd = strtok(NULL, PIPE_DELIM);
		}
		// Now that I have a linked list of the pipe delimited commands,
		// go through each individual command.
		parse_commands(cmd_list);

		exec_commands(cmd_list);

		free_list(cmd_list);
		cmd_list = NULL;
	}

	for (int i = 0; i < HIST; i++)
	{
		free(hist[i]);
	}

	return (EXIT_SUCCESS);
}

void sigint_handler(int sig)
{
	(void)sig;
	if (current_child_pid > 0)
	{
		kill(current_child_pid, SIGINT);
	}
}

void simple_argv(int argc, char *argv[])
{
	int opt;

	while ((opt = getopt(argc, argv, "hv")) != -1)
	{
		switch (opt)
		{
		case 'h':
			fprintf(stdout, "Please take a look at the README.md file\n"
							"I'm not going to put helpful things in here.\n\n");
			exit(EXIT_SUCCESS);
			break;
		case 'v':
			// verbose option to anything
			is_verbose++;
			if (is_verbose)
			{
				fprintf(stderr, "verbose: verbose option selected: %d\n", is_verbose);
			}
			break;
		case '?':
			fprintf(stderr, "*** Unknown option used, ignoring. ***\n");
			break;
		default:
			fprintf(stderr, "*** Oops, something strange happened <%c> ... ignoring ...***\n", opt);
			break;
		}
	}
}

void exec_commands(cmd_list_t *cmds)
{
	cmd_t *cmd = cmds->head;
	pid_t child;
	int stat_loc;
	param_t *param;
	param = cmd->param_list;

	if (1 == cmds->count)
	{
		if (!cmd->cmd)
		{
			// if it is an empty command, bail.
			return;
		}
		if (0 == strcmp(cmd->cmd, CD_CMD))
		{
			if (0 == cmd->param_count)
			{
				// Just a "cd" on the command line without a target directory
				// need to cd to the HOME directory.

				chdir(getenv("HOME"));
			}
			else
			{
				if (0 == chdir(cmd->param_list->param))
				{
					// a happy chdir!  ;-)
				}
				else
				{
					// a sad chdir.  :-(
					perror("cd");
				}
			}
		}
		else if (0 == strcmp(cmd->cmd, CWD_CMD))
		{
			char str[MAXPATHLEN];

			// Fetch the Current Working Wirectory (CWD).
			// aka - get country western dancing
			getcwd(str, MAXPATHLEN);
			printf(" " CWD_CMD ": %s\n", str);
		}
		else if (0 == strcmp(cmd->cmd, ECHO_CMD))
		{
			while (cmd->param_list)
			{
				printf("%s ", cmd->param_list->param);
				cmd->param_list = cmd->param_list->next;
			}
			printf("\n");
		}
		else if (0 == strcmp(cmd->cmd, HISTORY_CMD))
		{
			int count = 0;
			// display the history here
			for (int i = HIST - 1; i >= 0; i--)
			{
				printf("%d: %s\n", ++count, hist[i]);
			}
		}
		else
		{

			// fork - what is the return value (num of child)
			child = fork();
			if (child < 0)
			{
				perror("fork failed");
				return;
			}

			if (0 == child) // the child process
			{

				// Populate the argv array
				int argc = cmd->param_count + 2;
				char *argv[argc];
				argv[0] = cmd->cmd; // first element is the command
				for (int i = 1; i < argc - 1 && param != NULL; i++)
				{
					argv[i] = param->param;
					param = param->next;
				}

				argv[argc - 1] = NULL;

				if (cmd->input_file_name && (cmd->input_src == REDIRECT_FILE))
				{
					int ifd = open(cmd->input_file_name, O_RDONLY);
					if (ifd < 0)
					{
						perror("Error opening input file");
						_exit(EXIT_FAILURE);
					}
					dup2(ifd, STDIN_FILENO);
					close(ifd);
				}

				if (cmd->output_file_name && (cmd->output_dest == REDIRECT_FILE))
				{
					int ofd = open(cmd->output_file_name, O_WRONLY | O_CREAT | O_TRUNC, 0644);
					if (ofd < 0)
					{
						perror("Error opening output file");
						_exit(EXIT_FAILURE);
					}
					dup2(ofd, STDOUT_FILENO);
					close(ofd);
				}
				// execute external commands
				// execvp - this is how you execute the external commands
				execvp(argv[0], argv);

				perror("execvp failed");
				_exit(EXIT_FAILURE);
			}
			else
			{
				current_child_pid = child;

				// wait the child process
				if (wait(&stat_loc) < 0)
				{
					perror("wait failed");
				}

				current_child_pid = -1;

				if (WIFSIGNALED(stat_loc) && WTERMSIG(stat_loc) == SIGINT)
				{
					printf("child killed\n");
				}
			}
		}
	}
	else
	{
		// More than one command on the command line.

		int pipefd[2];
		int prev_pipe_read = -1; // ptrail:read end of prev pipe

		while (cmd != NULL)
		{
			// if not last create pipe
			if (cmd->next != NULL && pipe(pipefd) < 0)
			{
				perror("pipe failed");
				exit(EXIT_FAILURE);
			}

			child = fork();
			if (child < 0)
			{
				perror("fork failed");
				exit(EXIT_FAILURE);
			}

			if (child == 0) // child process
			{
				int argc = cmd->param_count + 2;
				char *argv[argc];
				argv[0] = cmd->cmd; // first element is the command
				for (int i = 1; i < argc - 1 && param != NULL; i++)
				{
					argv[i] = param->param;
					param = param->next;
				}
				argv[argc - 1] = NULL;

				// if not first
				if (prev_pipe_read != -1)
				{
					dup2(prev_pipe_read, STDIN_FILENO);
				}

				if (cmd->next != NULL) // if last`
				{
					dup2(pipefd[1], STDOUT_FILENO);
					close(pipefd[0]);
					close(pipefd[1]);
				}

				if (cmd->input_src == REDIRECT_FILE)
				{
					int ifd = open(cmd->input_file_name, O_RDONLY);
					if (ifd < 0)
					{
						perror("Error opening input file");
						_exit(EXIT_FAILURE);
					}
					dup2(ifd, STDIN_FILENO);
					close(ifd);
				}

				if (cmd->output_dest == REDIRECT_FILE)
				{
					int ofd = open(cmd->output_file_name, O_WRONLY | O_CREAT | O_TRUNC, 0644);
					if (ofd < 0)
					{
						perror("error opening output file");
						_exit(EXIT_FAILURE);
					}
					dup2(ofd, STDOUT_FILENO);
					close(ofd);
				}

				// exec with ragged array
				execvp(argv[0], argv);
				perror("execvp failed");
				_exit(EXIT_FAILURE);
			}
			else // parent process
			{
				current_child_pid = child;
				// if !first
				if (prev_pipe_read != -1)
				{
					close(prev_pipe_read);
				}

				// if !last
				if (cmd->next != NULL)
				{
					close(pipefd[1]);
					prev_pipe_read = pipefd[0];
				}

				current_child_pid = -1;

				if (WIFSIGNALED(stat_loc) && WTERMSIG(stat_loc) == SIGINT)
				{
					printf("child killed\n");
				}
			}
			cmd = cmd->next;
		}

		while (wait(&stat_loc) > 0)
			;
	}
}

void free_list(cmd_list_t *cmd_list)
{

	cmd_t *current = cmd_list->head;
	cmd_t *next;
	if (NULL == cmd_list)
	{
		return;
	}

	while (NULL != current)
	{
		next = current->next;
		free_cmd(current);
		current = next;
	}

	current = NULL;
	next = NULL;

	free(cmd_list);
}

void print_list(cmd_list_t *cmd_list)
{
	cmd_t *cmd = cmd_list->head;

	while (NULL != cmd)
	{
		print_cmd(cmd);
		cmd = cmd->next;
	}
}

void free_cmd(cmd_t *cmd)
{
	param_t *current = cmd->param_list;
	param_t *next_param;
	if (NULL == cmd)
	{
		return;
	}

	while (NULL != current)
	{
		next_param = current->next;
		free(current->param);
		free(current);
		current = next_param;
	}

	free(cmd->raw_cmd);
	free(cmd->cmd);
	free(cmd->input_file_name);
	free(cmd->output_file_name);

	current = NULL;
	next_param = NULL;

	free(cmd);
}

void print_cmd(cmd_t *cmd)
{
	param_t *param = NULL;
	int pcount = 1;

	fprintf(stderr, "raw text: +%s+\n", cmd->raw_cmd);
	fprintf(stderr, "\tbase command: +%s+\n", cmd->cmd);
	fprintf(stderr, "\tparam count: %d\n", cmd->param_count);
	param = cmd->param_list;

	while (NULL != param)
	{
		fprintf(stderr, "\t\tparam %d: %s\n", pcount, param->param);
		param = param->next;
		pcount++;
	}

	fprintf(stderr, "\tinput source: %s\n", (cmd->input_src == REDIRECT_FILE ? "redirect file" : (cmd->input_src == REDIRECT_PIPE ? "redirect pipe" : "redirect none")));
	fprintf(stderr, "\toutput dest:  %s\n", (cmd->output_dest == REDIRECT_FILE ? "redirect file" : (cmd->output_dest == REDIRECT_PIPE ? "redirect pipe" : "redirect none")));
	fprintf(stderr, "\tinput file name:  %s\n", (NULL == cmd->input_file_name ? "<na>" : cmd->input_file_name));
	fprintf(stderr, "\toutput file name: %s\n", (NULL == cmd->output_file_name ? "<na>" : cmd->output_file_name));
	fprintf(stderr, "\tlocation in list of commands: %d\n", cmd->list_location);
	fprintf(stderr, "\n");
}

#define stralloca(_R, _S)              \
	{                                  \
		(_R) = alloca(strlen(_S) + 1); \
		strcpy(_R, _S);                \
	}

void parse_commands(cmd_list_t *cmd_list)
{
	cmd_t *cmd = cmd_list->head;
	char *arg;
	char *raw;

	while (cmd)
	{

		stralloca(raw, cmd->raw_cmd);

		arg = strtok(raw, SPACE_DELIM);
		if (NULL == arg)
		{
			// free(raw);
			cmd = cmd->next;
			continue;
		}

		if (arg[0] == '\'')
		{
			arg++;
		}
		if (arg[strlen(arg) - 1] == '\'')
		{
			arg[strlen(arg) - 1] = '\0';
		}
		cmd->cmd = strdup(arg);

		cmd->input_src = REDIRECT_NONE;
		cmd->output_dest = REDIRECT_NONE;

		while ((arg = strtok(NULL, SPACE_DELIM)) != NULL)
		{
			if (strcmp(arg, REDIR_IN) == 0)
			{
				// redirect stdin

				//
				// If the input_src is something other than REDIRECT_NONE, then
				// this is an improper command.
				//

				// If this is anything other than the FIRST cmd in the list,
				// then this is an error.

				cmd->input_file_name = strdup(strtok(NULL, SPACE_DELIM));
				cmd->input_src = REDIRECT_FILE;
			}
			else if (strcmp(arg, REDIR_OUT) == 0)
			{
				// redirect stdout

				//
				// If the output_dest is something other than REDIRECT_NONE, then
				// this is an improper command.
				//

				// If this is anything other than the LAST cmd in the list,
				// then this is an error.

				cmd->output_file_name = strdup(strtok(NULL, SPACE_DELIM));
				cmd->output_dest = REDIRECT_FILE;
			}
			else
			{
				// add next param
				param_t *param = (param_t *)calloc(1, sizeof(param_t));
				param_t *cparam = cmd->param_list;

				cmd->param_count++;

				if (arg[0] == '\'')
				{
					arg++;
				}
				if (arg[strlen(arg) - 1] == '\'')
				{
					arg[strlen(arg) - 1] = '\0';
				}
				param->param = strdup(arg);
				if (NULL == cparam)
				{
					cmd->param_list = param;
				}
				else
				{

					while (cparam->next != NULL)
					{
						cparam = cparam->next;
					}
					cparam->next = param;
				}
			}
		}
		if (cmd->list_location > 0)
		{
			cmd->input_src = REDIRECT_PIPE;
		}
		if (cmd->list_location < (cmd_list->count - 1))
		{
			cmd->output_dest = REDIRECT_PIPE;
		}

		cmd = cmd->next;
	}

	if (is_verbose > 0)
	{
		print_list(cmd_list);
	}
}
