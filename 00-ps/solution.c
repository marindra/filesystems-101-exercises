#include <errno.h>
#include <solution.h>
#include <stdio.h>
#include <stdlib.h>
//#include <sys/types.h>
#include <dirent.h>
//#include <unistd.h>

void totalFree(char** argvAr, const size_t start, const size_t end) {
	for (size_t i = start; i < end; ++i) {
		free(argvAr[i]);
	}
}

void WorkWithDirectory(struct dirent* dirInProc) {
	pid_t pid = strtol(dirInProc->d_name, NULL, 10);
	if (pid == 0) {
		return;
	}
//	printf("%s\n", dirInProc->d_name);

	// Get path to exe

//	char symlinkToExe[PATH_MAX];
	char pathToExe[PATH_MAX] = "";
	{
		char symlinkToExe[PATH_MAX];
		if (sprintf(symlinkToExe, "/proc/%d/exe",pid) == -1) {
			//sprintf_s don't exist on my computer((
			// It isn't error of access, so I don't use report_error
			perror("Sprintf failed.\n");
			return;
		}

		if (readlink(symlinkToExe, pathToExe, PATH_MAX) == -1) {
			report_error(symlinkToExe, errno);
			return;
		}
//	printf("%s\n", pathToExe);
	}

	// Get array of comand line parameters

	const size_t MAX_ARGS_COUNT = 1 << 12;
	const size_t MAX_ARG_LENGTH = 1 << 12;

	char* possArgvAr[MAX_ARGS_COUNT];
	char* argvAr[MAX_ARGS_COUNT];

	size_t countOfArgs = 0;
	for (size_t i = 0; i < MAX_ARGS_COUNT; ++i) {
		possArgvAr[i] = (char*) malloc(MAX_ARG_LENGTH * sizeof(char));
		if (possArgvAr[i] == NULL) {
			perror("I have big troubles because I don't have enough memory\n");
			totalFree(possArgvAr, 0, i);
			// It isn't access error so I don't call report_error
			return;
		}
	}

	{
		char pathToCmdPar[PATH_MAX] = "";
		if (sprintf(pathToCmdPar, "/proc/%d/cmdline", pid) == -1) {
			totalFree(possArgvAr, 0, MAX_ARGS_COUNT);
			// It isn't access error so I don't call report_error
			return;
		}

		FILE* cmdlineParam = fopen(pathToCmdPar, "r");
		if (cmdlineParam == NULL) {
			report_error(pathToCmdPar, errno);
			totalFree(possArgvAr, 0, MAX_ARGS_COUNT);
			return;
		}

		for (size_t i = 0; i < MAX_ARGS_COUNT; ++i) {
			size_t buf_size = MAX_ARG_LENGTH;
			if ((getline(possArgvAr+i, &buf_size, cmdlineParam) == -1) || (possArgvAr[i][0] == '\0')) {
				argvAr[i] = NULL;
				countOfArgs = i;
				break;
			} else {
				argvAr[i] = possArgvAr[i];
			}
		}

		fclose(cmdlineParam);
	}

	// Get environ variables

	const size_t MAX_ENV_VARS_COUNT = 1 << 12;
	const size_t MAX_ENV_VAR_LENGTH = 1 << 12;

	char* possEnvVars[MAX_ENV_VARS_COUNT];
	char* envAr[MAX_ENV_VARS_COUNT];

	size_t countOfEnvVars = 0;
	for (size_t i = 0; i < MAX_ENV_VARS_COUNT; ++i) {
		possEnvVars[i] = (char*) malloc(MAX_ENV_VAR_LENGTH*sizeof(char));
		if (possEnvVars[i] == NULL) {
			perror("I have big troubles because I don't have enough memory\n");
			totalFree(possEnvVars, 0, i);
			totalFree(argvAr, 0, countOfArgs);
			totalFree(possArgvAr, countOfArgs, MAX_ARGS_COUNT)
;			// It isn't access error so I don't call report_error
			return;
		}
	}

	{
		char pathToEnvironVar[PATH_MAX] = "";
		if (sprintf(pathToEnvironVar, "/proc/%d/environ", pid) == -1) {
			totalFree(argvAr, 0, countOfArgs);
			totalFree(possArgvAr, countOfArgs, MAX_ARGS_COUNT);
			totalFree(possEnvVars, 0, MAX_ENV_VARS_COUNT);
			// It isn't access error so I don't call report_error
			return;
		}

		FILE* environ = fopen(pathToEnvironVar, "r");
		if (environ == NULL) {
			report_error(pathToEnvironVar, errno);
			totalFree(possEnvVars, 0, MAX_ENV_VARS_COUNT);
			totalFree(possArgvAr, 0, countOfArgs);
			totalFree(argvAr, countOfArgs, MAX_ARGS_COUNT);
			return;
		}

		for (size_t i = 0; i < MAX_ENV_VARS_COUNT; ++i) {
			size_t buf_size = MAX_ENV_VAR_LENGTH;
			if ((getline(possEnvVars+i, &buf_size, environ) == -1) || (possEnvVars[i][0] == '\0')) {
//				printf("done\n");
				envAr[i] = NULL;
				countOfEnvVars = i;
				break;
			} else {
				envAr[i] = possEnvVars[i];
			}
		}

		fclose(environ);
	}

	// report process! Ya-hoo!
	report_process(pid, pathToExe, argvAr, envAr);

	// Total freedom)
	totalFree(argvAr, 0, countOfArgs);
	totalFree(possArgvAr, countOfArgs, MAX_ARGS_COUNT);
	totalFree(possEnvVars, countOfEnvVars, MAX_ENV_VARS_COUNT);
	totalFree(envAr, 0, countOfEnvVars);

};

void ps(void)
{
	/* implement me */
	DIR* procDir = opendir("/proc/");
	if (procDir == NULL) {
		report_error("/proc", errno);
		return;
	};

	struct dirent* dirInProc = NULL;
	while ((dirInProc = readdir(procDir)) != NULL) {
		WorkWithDirectory(dirInProc);
	}

/*	for (int i = 0; i < 254; ++i) {
		if ((dirInProc = readdir(procDir)) != NULL) {
        	        WorkWithDirectory(dirInProc);
	        }

	}*/

	closedir(procDir);
}
