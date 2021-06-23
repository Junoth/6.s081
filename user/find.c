#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/param.h"

char* fmtname(char *path)
{
	char *p;

	// Find first character after last slash.
	for(p=path+strlen(path); p >= path && *p != '/'; p--)
	;
	p++;

	return p;
}

void find(char *path, char *filename) {
	int fd, plen = strlen(path);
	struct dirent de;
	struct stat st;

	if((fd = open(path, 0)) < 0){
		fprintf(2, "find: cannot open %s\n", path);
		return;
	}

	if(fstat(fd, &st) < 0){
		fprintf(2, "find: cannot stat %s\n", path);
		close(fd);
		return;
	}

	switch(st.type){
	case T_FILE:
		if (!strcmp(fmtname(path), filename)) {
			printf("%s\n", path);
		}
	break;

	case T_DIR:
		while(read(fd, &de, sizeof(de)) == sizeof(de)) {
			if(de.inum == 0)
				continue;
			// skip . and ..
			if (!strcmp(de.name, ".") || !strcmp(de.name, ".."))
				continue;
			int dlen = strlen(de.name);	
			if (plen + dlen > MAXPATH) {
				fprintf(2, "path is too long\n");
				break;
			}

			path[plen] = '/';
			memmove(path + plen + 1, de.name, dlen);
			path[plen + 1 + dlen] = 0;
			find(path, filename);
		}
		break;
	}
	close(fd);	
}

int main(int argc, char *argv[]) {
	char path[MAXPATH];
	if(argc <= 2) {
		fprintf(2, "usage: find dir filename\n");
		exit(1);
	}

	memcpy(path, argv[1], strlen(argv[1]));
	find(path, argv[2]);
	exit(0);
}