#include <Windows.h>
#include <stdio.h>
#include <sys\stat.h>

#include "wu_msg.h"
#include "wu_log.h"

/*
 * Function description:
 * Build log path. Log path is with the form of "year\month\day\log_19700101.txt". So
 * first we create a year directory next a month directory in year directory next a
 * day directory in month directory and eventualy the log file ending with ".txt".
 * Arguments:
 * - logpath: The output buffer for full log path.
 * - log_filename: Log filename.
 */
void
create_log_directory(char logpath[512], char log_filename[sizeof("log_19700101.txt")]) {
	char userprofile[255 + sizeof(LOG_DIRECTORY)];
	struct _stat statbuff;
	SYSTEMTIME systime;
	DWORD read;
	int ret;

	ZeroMemory(logpath, 512);
	ZeroMemory(userprofile, 255 + sizeof(LOG_DIRECTORY));
	getenv_s((size_t*)&ret, userprofile, (size_t)(255 + sizeof(LOG_DIRECTORY)), "USERPROFILE");
	sprintf_s(logpath, 255 + 1 + sizeof(LOG_DIRECTORY), "%s\\%s", userprofile, LOG_DIRECTORY);
	ret = _stat(logpath, &statbuff);
	if (ret) {
		if (_mkdir(logpath)) {
			INPUT_RECORD inRec;
			write_info_in_console(ERR_MSG_CANNOT_CREATE_LOG_DIRECTORY, "logs");
			while (ReadConsoleInput(GetStdHandle(STD_INPUT_HANDLE), &inRec, sizeof(INPUT_RECORD), &read));
		}
		else {
logyear:	char wYearStr[5];

			GetSystemTime(&systime);
			ZeroMemory(log_filename, sizeof("log_19700101.txt"));
			ZeroMemory(wYearStr, 5);
			sprintf_s(wYearStr, 5, "%i", systime.wYear);
			strcat_s(logpath, 512, "\\");
			strcat_s(logpath, 512, wYearStr);

			if (_stat(logpath, &statbuff) && _mkdir(logpath)) {
				INPUT_RECORD inRec;
				write_info_in_console(ERR_MSG_CANNOT_CREATE_LOG_DIRECTORY, logpath);
				while (ReadConsoleInput(GetStdHandle(STD_INPUT_HANDLE), &inRec, sizeof(INPUT_RECORD), &read));
			}
			else {
				char wMonthStr[3];

				if (systime.wMonth < 10) {
					ZeroMemory(wMonthStr, 3);
					sprintf_s(wMonthStr, 3, "0%i", systime.wMonth);
					strcat_s(logpath, 512, "\\");
					strcat_s(logpath, 512, wMonthStr);
				}
				else {
					ZeroMemory(wMonthStr, 3);
					sprintf_s(wMonthStr, 3, "%i", systime.wMonth);
					strcat_s(logpath, 512, "\\");
					strcat_s(logpath, 512, wMonthStr);
				}
				if (_stat(logpath, &statbuff) && _mkdir(logpath)) {
					INPUT_RECORD inRec;
					write_info_in_console(ERR_MSG_CANNOT_CREATE_LOG_DIRECTORY, logpath);
					while (ReadConsoleInput(GetStdHandle(STD_INPUT_HANDLE), &inRec, sizeof(INPUT_RECORD), &read));
				}
				else {
					char wDayStr[3];

					if (systime.wMonth < 10) {
						if (systime.wDay < 10) {
							sprintf_s(wDayStr, 3, "0%i", systime.wDay);
							strcat_s(logpath, 512, "\\");
							strcat_s(logpath, 512, wDayStr);
							ZeroMemory(log_filename, sizeof("log_19700101.txt"));
							sprintf_s(log_filename, sizeof("log_19700101.txt"), "log_%i0%i0%i.txt", systime.wYear, systime.wMonth, systime.wDay);
						}
						else {
							sprintf_s(wDayStr, 3, "%i", systime.wDay);
							strcat_s(logpath, 512, "\\");
							strcat_s(logpath, 512, wDayStr);
							ZeroMemory(log_filename, sizeof("log_19700101.txt"));
							sprintf_s(log_filename, sizeof("log_19700101.txt"), "log_%i0%i%i.txt", systime.wYear, systime.wMonth, systime.wDay);
						}
					}
					else if (systime.wDay < 10) {
						sprintf_s(wDayStr, 3, "0%i", systime.wDay);
						strcat_s(logpath, 512, "\\");
						strcat_s(logpath, 512, wDayStr);
						ZeroMemory(log_filename, sizeof("log_19700101.txt"));
						sprintf_s(log_filename, sizeof("log_19700101.txt"), "log_%i%i0%i.txt", systime.wYear, systime.wMonth, systime.wDay);
					}
					else {
						sprintf_s(wDayStr, 3, "%i", systime.wDay);
						strcat_s(logpath, 512, "\\");
						strcat_s(logpath, 512, wDayStr);
						ZeroMemory(log_filename, sizeof("log_19700101.txt"));
						sprintf_s(log_filename, sizeof("log_19700101.txt"), "log_%i%i%i.txt", systime.wYear, systime.wMonth, systime.wDay);
					}

					if (_stat(logpath, &statbuff) && _mkdir(logpath)) {
						INPUT_RECORD inRec;
						write_info_in_console(ERR_MSG_CANNOT_CREATE_LOG_DIRECTORY, logpath);
						while (ReadConsoleInput(GetStdHandle(STD_INPUT_HANDLE), &inRec, sizeof(INPUT_RECORD), &read));
					}
				}
			}
		}
	}
	else {
		goto logyear;
	}

	return;
}
