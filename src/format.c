/*

 *
 * usage:
 *	./format <diskfile>
 */

#include "ufs.h"

int main(int argc, char *argv[])
{
	int	fd, i;
	off_t	offset;
	size_t	size_mb;
	struct stat stat;
	struct ufs_dsuper_block sb;
	static char buf[UFS_BLK_SIZE];
	struct ufs_dinode root;
	struct ufs_dir_entry ent;

	if (argc != 2)
		err_quit("usage: ./format <diskfile>");
	if ((fd = open(argv[1], O_RDWR)) < 0)
		err_sys("open %s error.", argv[1]);
	if (fstat(fd, &stat) < 0)
		err_sys("stat %s error.", argv[1]);
	if (!S_ISREG(stat.st_mode))
		err_quit("%s is not a regular file", argv[1]);
	size_mb = stat.st_size >> 20;
	if (size_mb < UFS_DISK_MIN_SIZE || size_mb > UFS_DISK_MAX_SIZE)
		err_quit("the size of %s is inappropriate", argv[1]);

	sb.s_magic = UFS_MAGIC;
	if (size_mb <= 10) {
		sb.s_imap_blocks = 1;
		sb.s_zmap_blocks = 5;
		sb.s_inode_blocks = 256;
	} else if (size_mb >= 21) {
		sb.s_imap_blocks = 2;
		sb.s_zmap_blocks = 16;
		sb.s_inode_blocks = 1024;
	} else {
		sb.s_imap_blocks = 1;
		sb.s_zmap_blocks = 11;
		sb.s_inode_blocks = 512;
	}
	/* minus one for super block */
	sb.s_zone_blocks = (stat.st_size >> UFS_BLK_SIZE_SHIFT) - 1
		- sb.s_zmap_blocks - sb.s_imap_blocks - sb.s_inode_blocks;
	sb.s_max_size = MAX_FILE_SIZE;

	/* write super block */
	if (lseek(fd, 0, SEEK_SET) < 0)
		err_sys("lseek %s error", argv[1]);
	memcpy(buf, &sb, sizeof(sb));
	if (write(fd, buf, sizeof(buf)) != sizeof(buf))
		err_sys("write %s error", argv[1]);

	/* inode map */
	memset(buf, 0, sizeof(buf));
	buf[0] = 3;
	if (write(fd, buf, sizeof(buf)) != sizeof(buf))
		err_sys("write %s error", argv[1]);
	buf[0] = 0;
	for (i = 1; i < sb.s_imap_blocks; i++)
		if (write(fd, buf, sizeof(buf)) != sizeof(buf))
			err_sys("write %s error", argv[1]);

	/*
	 * zone map.
	 * the first zone block for . and .. of root directory
	 */
	buf[0] = 3;
	if (write(fd, buf, sizeof(buf)) != sizeof(buf))
		err_sys("write %s error", argv[1]);
	buf[0] = 0;
	for (i = 1; i < sb.s_zmap_blocks; i++)
		if (write(fd, buf, sizeof(buf)) != sizeof(buf))
			err_sys("write %s error", argv[1]);

	/* initialize root directory, add . and .. for it */
	memset(&root, 0, sizeof(root));
	root.i_nlink = 2;
	root.i_mode = 0x3ed; /* drwxr-xr-x */
	root.i_uid = getuid();
	root.i_gid = getgid();
	root.i_size = sizeof(struct ufs_dir_entry) * 2;
	root.i_blocks = 1;
	root.i_zones[0] = 1;
	root.i_mtime = root.i_ctime = time(NULL);
	if (write(fd, &root, sizeof(root)) != sizeof(root))
		err_sys("write error for root inode");

	/* write directory entry of root */
	memset(buf, 0, sizeof(buf));
	ent.de_inum = UFS_ROOT_INO;
	strcpy(ent.de_name, ".");
	memcpy(buf, &ent, sizeof(ent));
	strcpy(ent.de_name, "..");
	memcpy(buf + sizeof(ent), &ent, sizeof(ent));
	offset = (1 + sb.s_imap_blocks + sb.s_zmap_blocks +
			sb.s_inode_blocks) << UFS_BLK_SIZE_SHIFT;
	if (pwrite(fd, buf, sizeof(buf), offset) != sizeof(buf))
		err_sys("pwrite error");

	close(fd);
	return(0);
}
