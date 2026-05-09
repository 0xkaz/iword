/*
iWord Controler
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "iword.h"

static int json_mode = 0;

static void check_json_flag(int *argc, char **argv) {
	int i, j;
	for (i = 1; i < *argc; i++) {
		if (strcmp(argv[i], "--json") == 0) {
			json_mode = 1;
			for (j = i; j < *argc - 1; j++) argv[j] = argv[j + 1];
			(*argc)--;
			return;
		}
	}
}

int main(int argc, char **argv) {
	FILE *fp; int size, i; unsigned int *ol;

	if (argc < 2) goto usage;

	check_json_flag(&argc, argv);

	if (strcmp(argv[1], "dict") == 0) {
		if (argc < 4) goto usage;
		iword_set_strkey(argv[2], strlen(argv[2]));
		argc -= 2; argv += 2;
	}
	if (strcmp(argv[1], "stop") == 0) {
		if (argc != 2) goto usage;
		if (iword_unload()) {
			if (json_mode) printf("{\"ok\":false,\"error\":\"already unloaded\"}\n");
			else printf("Error: Already unloaded.\n");
		} else {
			if (json_mode) printf("{\"ok\":true}\n");
		}
		return 0;
	}
	if (strcmp(argv[1], "size") == 0) {
		if (argc != 2) goto usage;
		ol = (unsigned int *)iword_data();
		if (ol == NULL) {
			if (json_mode) printf("{\"loaded\":false}\n");
			else printf("No data to view.\n");
			return 0;
		}
		size = (int)((ol[*ol] >> 8) + (ol[*ol] & 255)) * 8;
		if (json_mode) printf("{\"loaded\":true,\"memory_kb\":%d}\n", (size + 4095) / 4096 * 4);
		else printf("iWord reserved %d KB ipc memory.\n", (size + 4095) / 4096 * 4);
		return 0;
	}
	if (strcmp(argv[1], "status") == 0) {
		if (argc != 2) goto usage;
		ol = (unsigned int *)iword_data();
		if (ol == NULL) {
			if (json_mode) printf("{\"loaded\":false,\"version\":\"%s\"}\n", IWORD_VERSION);
			else {
				printf("iWord version " IWORD_VERSION "\n");
				printf("iWord is not running.\n");
			}
			return 0;
		}
		size = (int)((ol[*ol] >> 8) + (ol[*ol] & 255)) * 8;
		if (json_mode) printf("{\"loaded\":true,\"version\":\"%s\",\"memory_kb\":%d}\n",
			IWORD_VERSION, (size + 4095) / 4096 * 4);
		else {
			printf("iWord version " IWORD_VERSION "\n");
			printf("iWord reserved %d KB ipc memory.\n", (size + 4095) / 4096 * 4);
		}
		return 0;
	}
	if (strcmp(argv[1], "mask") == 0) {
		if (json_mode) {
			int mask = iword_mask();
			printf("{\"mask\":%d,\"keys\":[", mask);
			int first = 1;
			for (i = 0; i < 15; i++) {
				if (mask & (1 << i)) {
					if (!first) printf(",");
					printf("%d", i);
					first = 0;
				}
			}
			printf("]}\n");
		} else {
			int mask = iword_mask();
			for (i = 0; i < 15; i++)
				printf("%d: %s\n", i, (mask & (1 << i)) ? "Yes" : "No");
		}
		return 0;
	}
	if (strcmp(argv[1], "view") == 0) {
		if (argc != 2) goto usage;
		ol = (unsigned int *)iword_data();
		if (ol == NULL) {
			if (json_mode) printf("{\"loaded\":false}\n");
			else printf("No data to view.\n");
			return 0;
		}
		size = (int)((ol[*ol] >> 8) + (ol[*ol] & 255) + 1) * 8;
		if (json_mode) {
			printf("[");
			for (i = 0; i < size / 4; i += 2) {
				if (i > 0) printf(",");
				printf("[%u,%u]", ol[i], ol[i + 1]);
			}
			printf("]\n");
		} else {
			for (i = 0; i < size / 4; i += 2) {
				printf("%08x %08x\n", ol[i], ol[i + 1]);
			}
		}
		return 0;
	}
	if (strcmp(argv[1], "seek") == 0) {
		if (argc != 3) goto usage;
		i = iword_seek(argv[2]);
		if (json_mode) {
			if (i == -1) printf("{\"word\":\"%s\",\"found\":false,\"key\":null}\n", argv[2]);
			else printf("{\"word\":\"%s\",\"found\":true,\"key\":%d}\n", argv[2], i);
		} else {
			if (i == -1) printf("\"%s\" is not found.\n", argv[2]);
			else printf("The key of the word \"%s\" is %d.\n", argv[2], i);
		}
		return 0;
	}
	if (strcmp(argv[1], "load") == 0) {
		char tmppath[32] = ""; char *path = argv[2];
		if (argc < 3) goto usage;
		if (argc != 3) {
			int q = 9, p = 2; FILE *fw;
			strncpy(tmppath, "/tmp/iword.XXXXXX", sizeof(tmppath) - 1);
			int tmpfd = mkstemp(tmppath);
			if (tmpfd == -1 || !(fw = fdopen(tmpfd, "w"))) {
				if (json_mode) printf("{\"ok\":false,\"error\":\"failed to create temporary file\"}\n");
				else printf("Error: failed to create temporary file.\n");
				return 1;
			}
			for (; p < argc; p++) if (argv[p][0] == '-') {
				if (isdigit(argv[p][1])) q = atoi(argv[p] + 1);
				else if (strcmp(argv[p] + 1, "spam") == 0) q = 2;
				else if (strcmp(argv[p] + 1, "adult") == 0) q = 1;
				else if (strcmp(argv[p] + 1, "word") == 0) q = 9;
			} else {
				FILE *fp = fopen(argv[p], "r");
				char str[1032]; int len, j;
				if (!fp) {
					if (json_mode) printf("{\"ok\":false,\"error\":\"no such file: %s\"}\n", argv[p]);
					else printf("Error: No such file: %s\n", argv[p]);
					return 1;
				}
				while (!feof(fp)) {
					len = fread(str, 1, 1024, fp);
					for (j = 0; j < len; fputc(str[j], fw), j++)
					 if (str[j] == '\n') fputc(9, fw), fputc(q + '0', fw);
				}
				fclose(fp);
				fputc('\n', fw);
			}
			fclose(fw);
			path = tmppath;
		}
		fp = fopen(path, "r");
		if (!fp) {
			if (json_mode) printf("{\"ok\":false,\"error\":\"no such file: %s\"}\n", path);
			else printf("Error: No such file: %s\n", path);
			return 1;
		}
		fclose(fp);
		if (iword_load(path)) {
			if (json_mode) printf("{\"ok\":false,\"error\":\"load failed\",\"required_kb\":%d}\n",
				(iword_needed_size() + 4095) / 4096 * 4);
			else printf("Error: Load failed. "
				"%d KB of the ipc memory should be free.\n",
				(iword_needed_size() + 4095) / 4096 * 4);
		} else {
			if (json_mode) printf("{\"ok\":true}\n");
		}
		return 0;
	}
	if (strcmp(argv[1], "version") == 0) {
		if (argc != 2) goto usage;
		if (json_mode) printf("{\"version\":\"%s\"}\n", IWORD_VERSION);
		else printf("iWord version " IWORD_VERSION "\n"
			"Copyright (C) 2009 @freaks, imos.\n");
		return 0;
	}
usage:
	printf(
		"Usage: iwordctl [dict id] [--json] command\n"
		"Options:\n"
		"  --json                   Output results as JSON\n"
		"Arguments:\n"
		"  dict id                  Control ID's dictionary (default id is \"\")\n"
		"Command List:\n"
		"  load filename            Load the dictionary file\n"
		"  stop                     Clean the ipc memory\n"
		"  size                     Show the size of which iword uses\n"
		"  status                   Show status and memory usage\n"
		"  view                     View the ipc data segment which iword reserves\n"
		"  seek word                Seek the word and show its key\n"
		"  mask                     Show which category keys are loaded\n"
		"  version                  Show information of this software\n"
	);
	return 0;
}
