#include <solution.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

void WorkWithDirectory(struct dirent* dirInProc) {
	pid_t pid = strtol(dirInProc->d_name, NULL, 10);
	if (pid == 0) {
		return;
	}

	char pathToFdDir[PATH_MAX] = "";
	if (sprintf(pathToFdDir, "/proc/%d/fd/", pid) < 0) {
		perror("Something get wrong.\n");
		return;
	}

	DIR* dirFd = opendir(pathToFdDir);
	if (dirFd == NULL) {
		report_error(pathToFdDir, errno);
		return;
	}

	struct dirent* symlinkFile = NULL;
	while ((symlinkFile = readdir(dirFd)) != NULL) {

		char* ptrToEnd = NULL;
		long int fileD = strtol(symlinkFile->d_name, &ptrToEnd, 10);
		if (*ptrToEnd != '\0') {
			continue;
		}

		char pathToFile[PATH_MAX] = "";
		char symlinkToFile[PATH_MAX] ="";

		if (sprintf(symlinkToFile, "/proc/%d/fd/%ld", pid, fileD) < 0) {
			perror("Something get wrong.\n");
			closedir(dirFd);
			return;
		}

		if (readlink(symlinkToFile, pathToFile, PATH_MAX) == -1) {
			report_error(symlinkToFile, errno);
			closedir(dirFd);
			return;
		}

		report_file(pathToFile);
	}

	closedir(dirFd);

}

void lsof(void)
{
	/* implement me */
	DIR* procDir = opendir("/proc/");
	if (procDir == NULL) {
		report_error("/proc", errno);
		return;
	}

	struct dirent* dirInProc = NULL;
	while ((dirInProc = readdir(procDir)) != NULL) {
		WorkWithDirectory(dirInProc);
	}

	closedir(procDir);
}
