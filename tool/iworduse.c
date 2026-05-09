/*
iWord Test
*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "iword.h"

int main(int argc, char **argv) {
	FILE *fp; int size, i, j, k; char *data; long long *ret;
	
	if (argc != 2) {
		printf("Usage: iwordtest file\n");
		return 0;
	}
	if (!(fp = fopen(argv[1], "r"))) {
		printf("No such file: %s\n", argv[1]);
		return 1;
	}
	fseek(fp, 0L, SEEK_END);
	size = ftell(fp);
	fseek(fp, 0L, SEEK_SET);
	data = (char *)malloc(size + 1);
	fread(data, size, 1, fp);
	data[size] = 0;
	fclose(fp);
	ret = iword_map(data, size, 0);
	for (i = j = 0; i < size && ret[j]; j++) {
		for (; i < (ret[j] >> 16); i++)
		 printf("%c", data[i]);
		printf("[");
		for (k = ret[j] & 0xff; k; k--, i++)
		 printf("%c", data[i]);
		printf("]");
	}
	for (; i < size; i++) printf("%c", data[i]);
	free(ret); free(data);
	return 0;
}
