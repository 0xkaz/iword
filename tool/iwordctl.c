/*
iWord Controler
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "iword.h"

int main(int argc, char **argv) {
	FILE *fp; int size, i; unsigned int *ol;
	
	if (argc < 2) goto usage;
	if (strcmp(argv[1], "dict") == 0) {
		if (argc < 4) goto usage;
		iword_set_strkey(argv[2], strlen(argv[2]));
		argc -= 2; argv += 2;
	}
	if (strcmp(argv[1], "stop") == 0) {
		if (argc != 2) goto usage;
		if (iword_unload()) printf("Error: Already unloaded.\n");
		return 0;
	}
	if (strcmp(argv[1], "size") == 0) {
		if (argc != 2) goto usage;
		ol = (unsigned int *)iword_data();
		if (ol == NULL) {
			printf("No data to view.\n");
			return 0;
		}
		size = (int)((ol[*ol] >> 8) + (ol[*ol] & 255)) * 8;
		printf("iWord reserved %d KB ipc memroy.\n", (size + 4095) / 4096 * 4);
		return 0;
	}
	if (strcmp(argv[1], "status") == 0) {
		if (argc != 2) goto usage;
		printf("iWord version " IWORD_VERSION "\n");
		ol = (unsigned int *)iword_data();
		if (ol == NULL) {
			printf("iWord is not running.\n");
			return 0;
		}
		size = (int)((ol[*ol] >> 8) + (ol[*ol] & 255)) * 8;
		printf("iWord reserved %d KB ipc memroy.\n", (size + 4095) / 4096 * 4);
		return 0;
	}
	if (strcmp(argv[1], "mask") == 0) {
		int mask = iword_mask();
		for (i = 0; i < 15; i++) {
			printf("%d: %s\n", i, (mask & (1 << i)) ? "Yes" : "No");
		}
		return 0;
	}
	if (strcmp(argv[1], "view") == 0) {
		if (argc != 2) goto usage;
		ol = (unsigned int *)iword_data();
		if (ol == NULL) {
			printf("No data to view.\n");
			return 0;
		}
		size = (int)((ol[*ol] >> 8) + (ol[*ol] & 255) + 1) * 8;
		for (i = 0; i < size / 4; i += 2) {
			printf("%08x %08x\n", ol[i], ol[i + 1]);
		}
		return 0;
	}
	if (strcmp(argv[1], "seek") == 0) {
		if (argc != 3) goto usage;
		i = iword_seek(argv[2]);
		if (i == -1) {
			printf("\"%s\" is not found.\n", argv[2]);
		} else {
			printf("The key of the word \"%s\" is %d.\n", argv[2], i);
		}
		return 0;
	}
	if (strcmp(argv[1], "load") == 0) {
		if (argc < 3) goto usage;
		if (argc != 3) {
			int q = 9, p = 2; FILE *fw;
			char tmppath[] = "/tmp/iword.XXXXXX";
			int tmpfd = mkstemp(tmppath);
			if (tmpfd == -1 || !(fw = fdopen(tmpfd, "w"))) {
				printf("Error: failed to create temporary file.\n");
				return 1;
			}
			for (; p < argc; p++) if (argv[p][0] == '-') {
				if (isdigit(argv[p][1])) q = atoi(argv[p] + 1);
				else if (strcmp(argv[p] + 1, "spam") == 0) q = 2;
				else if (strcmp(argv[p] + 1, "adult") == 0) q = 1;
				else if (strcmp(argv[p] + 1, "word") == 0) q = 9;
			} else {
				FILE *fp = fopen(argv[p], "r");
				char str[1032]; int len, i;
				if (!fp) {
					printf("Error: No such file: %s\n", argv[p]);
					return 1;
				}
				while (!feof(fp)) {
					len = fread(str, 1, 1024, fp);
					for (i = 0; i < len; fputc(str[i], fw), i++)
					 if (str[i] == '\n') fputc(9, fw), fputc(q + '0', fw);
				}
				fclose(fp);
				fputc('\n', fw);
			}
			fclose(fw);
			argv[2] = tmppath;
		}
		fp = fopen(argv[2], "r");
		if (!fp) {
			printf("Error: No such file: %s\n", argv[2]);
			return 1;
		}
		fclose(fp);
		if (iword_load(argv[2])) {
			printf("Error: Load failed. "
			 "%d KB of the ipc memory should be free.\n",
			 (iword_needed_size() + 4095) / 4096 * 4);
		}
		return 0;
	}
	if (strcmp(argv[1], "version") == 0) {
		if (argc != 2) goto usage;
		printf("iWord version " IWORD_VERSION "\n"
		 "Copyright (C) 2009 @freaks, imos.\n");
		return 0;
	}
usage:
	printf(
		"Usage: iwordctl [dict num] command\n"
		"Arguments:\n"
		"  dict id                 Control ID's dictionary (default id is \"\")\n"
		"Command List:\n"
		"  load filename            Load the dictionary file\n"
		"  stop                     Clean the ipc memory\n"
		"  size                     Show the size of which iword uses\n"
		"  view                     View the ipc data segment which iword reserves\n"
		"  seek word                Seek the word and show its key\n"
		"  version                  Show information of this software\n"
	);
	return 0;
}
