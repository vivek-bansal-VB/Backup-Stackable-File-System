enum op_type
{
	LIST = 0, 		/* to list backup files */
	DELETE = 1, 		/* to delete backup files */
	VIEW = 2, 		/* to view all version of backup files */
	RESTORE = 3 		/* to restore my file to some backup version */
};

struct mystruct
{
	int oldest; 		/* arg for oldest */
	int newest; 		/* arg for newest */
	int all;    		/* arg for all */
	int version_no; 	/* If user specifies version_no */
	int min_version; 	/* min_version available of the backup files */ 
	int max_version;	/* max_version available of the backup files */
	char* file_content;     /* used in view option when user wants to view the content of the backup file */
	loff_t offset;		/* offset from where do we need to read the file */
};

typedef struct mystruct version;
typedef enum op_type operation;

/* ioctl commands */
/* reference: https://embetronicx.com/tutorials/linux/device-drivers/ioctl-tutorial-in-linux/ */
#define BKPFS_DEL _IOW('b','1',version*)
#define BKPFS_LIST _IOW('b','2',version*)
#define BKPFS_VIEW _IOW('b','3',version*)
#define BKPFS_RESTORE _IOW('b','4', version*)



