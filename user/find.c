#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fs.h"
#include "user.h"

//find the name from path
char* basename(char *path) {
  char *p;
  for (p = path + strlen(path); p >= path && *p != '/'; p--)
    ;
  p++;
  return p;
}
//like ls.c
void find(char *path, char *target) {
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;

  if((fd = open(path, 0)) < 0){
    printf("find: cannot open %s\n", path);
    return;
  }

  if(fstat(fd, &st) < 0){
    printf("find: cannot stat %s\n", path);
    close(fd);
    return;
  }

  switch(st.type){
  case T_FILE:
    if(strcmp(basename(path), target) == 0){
      printf("%s\n", path);
    }
    break;

  case T_DIR:
    if(strcmp(basename(path),target) == 0) {
        printf("%s\n",path);
        break;
    }
    if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
      printf("find: path too long\n");
      break;
    }
    strcpy(buf, path);
    p = buf+strlen(buf);
    *p++ = '/';
    while(read(fd, &de, sizeof(de)) == sizeof(de)){
      if(de.inum == 0) continue;
      if(strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0) continue;
      memmove(p, de.name, DIRSIZ);
      p[DIRSIZ] = 0;
      find(buf, target);
    }
    break;
  }
  close(fd);
}

int main(int argc, char *argv[]) {
  if(argc != 3){
    printf("usage: find <path> <name>\n");
    exit(-1);
  }
  find(argv[1], argv[2]);
  exit(0);
}
