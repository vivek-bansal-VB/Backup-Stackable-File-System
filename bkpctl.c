#include <asm/unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include "struct.h"
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#define NEW_VERSION "newest"
#define OLD_VERSION "oldest"
#define ALL_VERSION "all"
#define MY_PAGE_SIZE 4096

/* to check if passed string is a number or not.
 * Assuming str can't be equal to "0"  */
int isnumber(char* str)
{
	int num = atoi(str);
	return num;
}

/* https://stackoverflow.com/questions/6537436/how-do-you-get-file-size-by-fd */
int fileSize(int fd) {
   struct stat s;
   int saveErrno = 0;

   if (fstat(fd, &s) == -1) {
      saveErrno = errno;
      fprintf(stderr, "fstat(%d) returned errno=%d.", fd, saveErrno);
      return(-1);
   }
   return(s.st_size);
}

/* arguments:
 *  l : list all versions of the backup file
 *  d : delete versions of the given file, ARG can be "newest", "oldest", or "all"
 *  v : to view the version of the given file, ARG: can be "newest", "oldest", or N
 *  r : restore the version of the given file, ARG: "newest" or N
 *  FILE : filename on which the operations need to be performed
 */
int main(int argc, char *argv[]){
        int opt, fd = 0, err = 0, i, len1;
        char myoptarg[7] = {'l', 'd', ':', 'v', ':', 'r', ':'};
        operation flag = -1;
        extern char* optarg;
        extern int optind, optopt;
        char* arg = NULL;
        char *inputfile = NULL;
        char* filename = NULL;
	version obj;
	obj.newest = obj.oldest = obj.all = 0;
	obj.version_no = 0;
	obj.min_version = obj.max_version = 0;
	char temp[10];
	char* back_up_filename = NULL;
	obj.file_content = NULL;
	obj.offset = 0;
	int f_size = 0;

    /* parsing arguments of command line and validating input from the user */
    while ((opt = getopt(argc, argv, (char*)myoptarg)) != -1) {
        switch (opt) {

            case 'l': /* without any arg */
                flag = LIST;
                break;

            case 'd': /* with three args: newest, oldest, N */
                flag = DELETE;
                arg = malloc(strlen(optarg) + 1);
                memset(arg, 0, strlen(optarg) + 1);
                strcpy(arg, optarg);
		if(strcmp(arg, NEW_VERSION) && strcmp(arg, OLD_VERSION) && strcmp(arg, ALL_VERSION)){
			err = -EINVAL;
			printf("Invalid argument is passed in the delete option\n");
			goto exit_block;
		}
		if(!strcmp(arg, NEW_VERSION))
			obj.newest = 1;
		else if (!strcmp(arg, OLD_VERSION))
			obj.oldest = 1;
		else
			obj.all = 1;
                break;

            case 'v': /* with three args: newest, oldest, N */
                flag = VIEW;
                arg = malloc(strlen(optarg) + 1);
                memset(arg, 0, strlen(optarg) + 1);
                strcpy(arg, optarg);
		if(strcmp(arg, NEW_VERSION) && strcmp(arg, OLD_VERSION) && !isnumber(arg)){
			err = -EINVAL;
			printf("Invalid argument is passed in the view option\n");
			goto exit_block;
		}
                if(!strcmp(arg, NEW_VERSION)) /* to view newest backup version */
                        obj.newest = 1;
                else if (!strcmp(arg, OLD_VERSION)) /* to view oldest backup version */
                        obj.oldest = 1;
                else
                        obj.version_no = atoi(arg); /* to view Nth backup version */
                break;

            case 'r': /* with two args: newest, N */
                flag = RESTORE;
                arg = malloc(strlen(optarg) + 1);
                memset(arg, 0, strlen(optarg) + 1);
                strcpy(arg, optarg);
                if(strcmp(arg, NEW_VERSION) && !isnumber(arg)){
                        err = -EINVAL;
			printf("Invalid argument is passed in the restore option\n");
                        goto exit_block;
                }
                if(!strcmp(arg, NEW_VERSION)) /* to restore to newest backup version */
                        obj.newest = 1;
                else
                        obj.version_no = atoi(arg); /* to restore to Nth backup version */
                break;

            case ':': /* without operand */
                printf("Option -%c requires an operand for the valid command line\n", optopt);
                err = -EINVAL;
                goto exit_block;

            default:
                break;
        }
    }

    /* invalid options entered by the user */
    if(flag != LIST && flag != DELETE && flag != VIEW && flag != RESTORE){
        printf("Invalid commmand entered\n");
	printf("Please enter out of 4 flags: -d for delete, -v to view, -r for restore, -l to list\n");
        err = -EINVAL;
        goto exit_block;
    }

    /* when inputfile is missing from the command line */
    if(argv[optind] == NULL){
        printf("Input file missing from command line\n");
        err = -EINVAL;
        goto exit_block;
    }

    inputfile = malloc(strlen(argv[optind]) + 1);
    if(inputfile == NULL){
        err = -ENOMEM;
        goto exit_block;
    }
    memset(inputfile, 0, strlen(argv[optind]) + 1);
    strcpy(inputfile, argv[optind]);

    filename = malloc(strlen(inputfile) + 1);
    strcpy(filename, inputfile);

    if(flag == RESTORE)
    	fd = open(filename, 777);
    else
	fd = open(filename, O_RDWR, 777);

    if(fd < 0){ /* error while opening a file */
	printf("file opening got failed in the user space. File opening try of %s\n", filename);
        err = -errno;
        goto exit_block;
    }

    /* reference: https://embetronicx.com/tutorials/linux/device-drivers/ioctl-tutorial-in-linux/ */    
    switch(flag)
    {
	case LIST:
		err = ioctl(fd, BKPFS_LIST, &obj); /* list ioctl returns the minimum and maximum version of the backup file available */
		if(err){ /* list ioctl got failed */
			err = -errno;
			goto exit_block;
		}
		printf("All backup versions of the file: %s are listed below:\n", inputfile);
		for(i = obj.min_version; i <= obj.max_version; i++) /* producing all backup versions looping from min to max version */
		{
			sprintf(temp, "%d", i);
			len1 = strlen(inputfile) + strlen(temp) + strlen(".tmp");
			back_up_filename = (char*)malloc(len1);
                 	strcpy(back_up_filename, inputfile);
                 	strcat(back_up_filename, temp);
                 	strcat(back_up_filename, ".tmp");
                 	printf("%s\n", back_up_filename);
                 	if(back_up_filename)
                        	free(back_up_filename);
		}		
		break;

	case DELETE:
		printf("Deleting the file\n");
		err = ioctl(fd, BKPFS_DEL, &obj); /* call delete ioctl to delete a backup file */
		break;
	case VIEW:
		f_size = fileSize(fd); /* getting the file size */
		while(obj.offset < f_size) /* reading the contents from the file in chunks of size 4K by calling view ioctl and retrieving the content from the kernel */
		{
                	obj.file_content = malloc(MY_PAGE_SIZE);
                	err = ioctl(fd, BKPFS_VIEW, &obj);
                	printf("%s\n", obj.file_content);
                	free(obj.file_content);
			obj.offset += MY_PAGE_SIZE;
		}
		break;
	case RESTORE:
		err = ioctl(fd, BKPFS_RESTORE, &obj); /* restore the content of original file to the backup file(version is specified in the obj structure which is passed to the kernel */
		break;
	default:
		break;
    }	

    if(err < 0){
        err = -errno;
        goto exit_block;
    }

   exit_block:
        if(fd > -1)
                close(fd);
        if(inputfile)
                free(inputfile);
        if(filename)
                free(filename);
        if(arg)
                free(arg);
        return err;
}
