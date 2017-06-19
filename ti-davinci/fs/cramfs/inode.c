/*
 * Compressed rom filesystem for Linux.
 *
 * Copyright (C) 1999 Linus Torvalds.
 *
 * This file is released under the GPL.
 */

/*
 * These are the VFS interfaces to the compressed rom filesystem.
 * The actual compression is based on zlib, see the other files.
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/pagemap.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/blkdev.h>
#include <linux/cramfs_fs.h>
#include <linux/slab.h>
#include <linux/cramfs_fs_sb.h>
#include <linux/buffer_head.h>
#include <linux/vfs.h>
#include <linux/mutex.h>
#include <linux/mtd/mtd.h>

#include <asm/uaccess.h>

static struct super_operations cramfs_ops;
static struct inode_operations cramfs_dir_inode_operations;
static const struct file_operations cramfs_directory_operations;
static const struct address_space_operations cramfs_aops;

static DEFINE_MUTEX(read_mutex);


/* These two macros may change in future, to provide better st_ino
   semantics. */
#define CRAMINO(x)	(((x)->offset && (x)->size)?(x)->offset<<2:1)
#define OFFSET(x)	((x)->i_ino)


static int cramfs_iget5_test(struct inode *inode, void *opaque)
{
	struct cramfs_inode *cramfs_inode = opaque;

	if (inode->i_ino != CRAMINO(cramfs_inode))
		return 0; /* does not match */

	if (inode->i_ino != 1)
		return 1;

	/* all empty directories, char, block, pipe, and sock, share inode #1 */

	if ((inode->i_mode != cramfs_inode->mode) ||
	    (inode->i_gid != cramfs_inode->gid) ||
	    (inode->i_uid != cramfs_inode->uid))
		return 0; /* does not match */

	if ((S_ISCHR(inode->i_mode) || S_ISBLK(inode->i_mode)) &&
	    (inode->i_rdev != old_decode_dev(cramfs_inode->size)))
		return 0; /* does not match */

	return 1; /* matches */
}

static int cramfs_iget5_set(struct inode *inode, void *opaque)
{
	static struct timespec zerotime;
	struct cramfs_inode *cramfs_inode = opaque;
	inode->i_mode = cramfs_inode->mode;
	inode->i_uid = cramfs_inode->uid;
	inode->i_size = cramfs_inode->size;
	inode->i_blocks = (cramfs_inode->size - 1) / 512 + 1;
	inode->i_blksize = PAGE_CACHE_SIZE;
	inode->i_gid = cramfs_inode->gid;
	/* Struct copy intentional */
	inode->i_mtime = inode->i_atime = inode->i_ctime = zerotime;
	inode->i_ino = CRAMINO(cramfs_inode);
	/* inode->i_nlink is left 1 - arguably wrong for directories,
	   but it's the best we can do without reading the directory
           contents.  1 yields the right result in GNU find, even
	   without -noleaf option. */
	if (S_ISREG(inode->i_mode)) {
		inode->i_fop = &generic_ro_fops;
		inode->i_data.a_ops = &cramfs_aops;
	} else if (S_ISDIR(inode->i_mode)) {
		inode->i_op = &cramfs_dir_inode_operations;
		inode->i_fop = &cramfs_directory_operations;
	} else if (S_ISLNK(inode->i_mode)) {
		inode->i_op = &page_symlink_inode_operations;
		inode->i_data.a_ops = &cramfs_aops;
	} else {
		inode->i_size = 0;
		inode->i_blocks = 0;
		init_special_inode(inode, inode->i_mode,
			old_decode_dev(cramfs_inode->size));
	}
	return 0;
}

static struct inode *get_cramfs_inode(struct super_block *sb,
				struct cramfs_inode * cramfs_inode)
{
	struct inode *inode = iget5_locked(sb, CRAMINO(cramfs_inode),
					    cramfs_iget5_test, cramfs_iget5_set,
					    cramfs_inode);
	if (inode && (inode->i_state & I_NEW)) {
		unlock_new_inode(inode);
	}
	return inode;
}

/*
 * We have our own block cache: don't fill up the buffer cache
 * with the rom-image, because the way the filesystem is set
 * up the accesses should be fairly regular and cached in the
 * page cache and dentry tree anyway..
 *
 * This also acts as a way to guarantee contiguous areas of up to
 * BLKS_PER_BUF*PAGE_CACHE_SIZE, so that the caller doesn't need to
 * worry about end-of-buffer issues even when decompressing a full
 * page cache.
 */
#define READ_BUFFERS (2)
/* NEXT_BUFFER(): Loop over [0..(READ_BUFFERS-1)]. */
#define NEXT_BUFFER(_ix) ((_ix) ^ 1)

/*
 * BLKS_PER_BUF_SHIFT should be at least 2 to allow for "compressed"
 * data that takes up more space than the original and with unlucky
 * alignment.
 */
#define BLKS_PER_BUF_SHIFT	(2)
#define BLKS_PER_BUF		(1 << BLKS_PER_BUF_SHIFT)
#define BUFFER_SIZE		(BLKS_PER_BUF*PAGE_CACHE_SIZE)

static unsigned char read_buffers[READ_BUFFERS][BUFFER_SIZE];
static unsigned buffer_blocknr[READ_BUFFERS];
static struct super_block * buffer_dev[READ_BUFFERS];
static int next_buffer;

/*
 * Returns a pointer to a buffer containing at least LEN bytes of
 * filesystem starting at byte offset OFFSET into the filesystem.
 */
static void *cramfs_read_old(struct super_block *sb, unsigned int offset, unsigned int len)
{
	struct address_space *mapping = sb->s_bdev->bd_inode->i_mapping;
	struct page *pages[BLKS_PER_BUF];
	unsigned i, blocknr, buffer;
	unsigned long devsize;
	char *data;

	if (!len)
		return NULL;
	blocknr = offset >> PAGE_CACHE_SHIFT;
	offset &= PAGE_CACHE_SIZE - 1;

	/* Check if an existing buffer already has the data.. */
	for (i = 0; i < READ_BUFFERS; i++) {
		unsigned int blk_offset;

		if (buffer_dev[i] != sb)
			continue;
		if (blocknr < buffer_blocknr[i])
			continue;
		blk_offset = (blocknr - buffer_blocknr[i]) << PAGE_CACHE_SHIFT;
		blk_offset += offset;
		if (blk_offset + len > BUFFER_SIZE)
			continue;
		return read_buffers[i] + blk_offset;
	}

	devsize = mapping->host->i_size >> PAGE_CACHE_SHIFT;

	/* Ok, read in BLKS_PER_BUF pages completely first. */
	for (i = 0; i < BLKS_PER_BUF; i++) {
		struct page *page = NULL;

		if (blocknr + i < devsize) {
			page = read_mapping_page(mapping, blocknr + i, NULL);
			/* synchronous error? */
			if (IS_ERR(page))
				page = NULL;
		}
		pages[i] = page;
	}

	for (i = 0; i < BLKS_PER_BUF; i++) {
		struct page *page = pages[i];
		if (page) {
			wait_on_page_locked(page);
			if (!PageUptodate(page)) {
				/* asynchronous error */
				page_cache_release(page);
				pages[i] = NULL;
			}
		}
	}

	buffer = next_buffer;
	next_buffer = NEXT_BUFFER(buffer);
	buffer_blocknr[buffer] = blocknr;
	buffer_dev[buffer] = sb;

	data = read_buffers[buffer];
	for (i = 0; i < BLKS_PER_BUF; i++) {
		struct page *page = pages[i];
		if (page) {
			memcpy(data, kmap(page), PAGE_CACHE_SIZE);
			kunmap(page);
			page_cache_release(page);
		} else
			memset(data, 0, PAGE_CACHE_SIZE);
		data += PAGE_CACHE_SIZE;
	}
	return read_buffers[buffer] + offset;
}


/*

 * build block map table: logic block ==> physic block.

 * skip each bad block.

 */

static int get_block_map(struct super_block *sb, int block)

{
	struct cramfs_sb_info *sbi = sb->s_fs_info;
	struct mtd_info *mtd = sbi->mtd;
	int *block_map = sbi->block_map;
	int blocks = sbi->nblock;
	int pblock = -1;
	int i;

	if(block_map[block]==-1){
		/* find first unmap block */
		for(i=0; i<blocks; i++){
			if(block_map[i]==-1){
				break;
			}
			pblock = block_map[i];
		}
		pblock++;

		/* scan from i to mblock+4 */
		while(i<=block+4 && i<blocks){
			while(mtd->block_isbad(mtd, pblock*mtd->erasesize)){
				//printk("cramfs: skip bad block %d(page %d)\n", pblock, pblock*32+4096);
				pblock++;
			}
			block_map[i] = pblock;
			//printk("cramfs: logic block %4d --> %4d(page %d)\n", i, pblock,pblock*32+4096);
			i++;
			pblock++;
		}
	}

	return block_map[block];
}

static void *cramfs_read_nand(struct super_block *sb, unsigned int offset, unsigned
int len)

{
	struct cramfs_sb_info *sbi = sb->s_fs_info;
	struct mtd_info *mtd = sbi->mtd;
	unsigned i, blocknr, buffer;
	unsigned mblock, moffset, pblock, maddr;

	int retv;
	size_t dummy;
	char *data;

	if (!len)
		return NULL;

	blocknr = offset >> PAGE_CACHE_SHIFT;

	/* Check if an existing buffer already has the data.. */

	for (i = 0; i < READ_BUFFERS; i++) {
		unsigned int blk_offset;

		if (buffer_dev[i] != sb){
			continue;
		}
		if (blocknr < buffer_blocknr[i]){
			continue;
		}
		blk_offset = (blocknr - buffer_blocknr[i]) << PAGE_CACHE_SHIFT;
		blk_offset += offset&(PAGE_CACHE_SIZE-1);
		if (blk_offset + len > BUFFER_SIZE){
			continue;
		}
		return read_buffers[i] + blk_offset;	
	}



	/* Ok, copy them to the staging area without sleeping. */

	buffer = next_buffer;

	next_buffer = NEXT_BUFFER(buffer);

	buffer_blocknr[buffer] = blocknr;

	buffer_dev[buffer] = sb;



	mblock = offset/mtd->erasesize;

	moffset = (offset & (mtd->erasesize-1))&(~(PAGE_CACHE_SIZE-1));

	offset &= (PAGE_CACHE_SIZE-1);



	pblock = get_block_map(sb, mblock);

	maddr = pblock*mtd->erasesize + moffset;



	data = read_buffers[buffer];

	if( moffset+BUFFER_SIZE <= mtd->erasesize ){
		retv = mtd->read(mtd, maddr, BUFFER_SIZE, &dummy, data);
	}else{
		int read_size;

		read_size = mtd->erasesize-moffset;
		retv = mtd->read(mtd, maddr, read_size, &dummy, data);
		pblock = get_block_map(sb, mblock+1);
		maddr = pblock*mtd->erasesize;
		retv = mtd->read(mtd, maddr, (BUFFER_SIZE-read_size), &dummy, data+read_size);
	}
	return read_buffers[buffer] + offset;
}

static void *cramfs_read(struct super_block *sb, unsigned int offset, unsigned int
len)
{
	struct cramfs_sb_info *sbi = sb->s_fs_info;
	if(sbi->nand){
		return cramfs_read_nand(sb, offset, len);
	}else{
		return cramfs_read_old(sb, offset, len);
	}
}

static void cramfs_put_super(struct super_block *sb)
{
	kfree(sb->s_fs_info);
	sb->s_fs_info = NULL;
}

static int cramfs_remount(struct super_block *sb, int *flags, char *data)
{
	*flags |= MS_RDONLY;
	return 0;
}

static int cramfs_fill_super(struct super_block *sb, void *data, int silent)
{
	int i;
	struct cramfs_super super;
	unsigned long root_offset;
	struct cramfs_sb_info *sbi;
	struct inode *root;

	sb->s_flags |= MS_RDONLY;

	sbi = kmalloc(sizeof(struct cramfs_sb_info), GFP_KERNEL);
	if (!sbi)
		return -ENOMEM;
	sb->s_fs_info = sbi;
	memset(sbi, 0, sizeof(struct cramfs_sb_info));

	sbi->nand = 0;
	if(MAJOR(sb->s_dev) == MTD_BLOCK_MAJOR){
		struct mtd_info *mtd;
		int blocks, *block_map;

		mtd = get_mtd_device(NULL, MINOR(sb->s_dev));
		if(mtd && mtd->type == MTD_NANDFLASH){
		sbi->mtd = mtd;
		sbi->nand = 1;

		blocks = mtd->size/mtd->erasesize;
		block_map = kmalloc(blocks*sizeof(int), GFP_KERNEL);
		for(i=0; i<blocks; i++){
			block_map[i] = -1;
		}

		sbi->block_map = block_map;
		sbi->nblock = blocks;
		}
	}
	/* Invalidate the read buffers on mount: think disk change.. */
	mutex_lock(&read_mutex);
	for (i = 0; i < READ_BUFFERS; i++)
		buffer_blocknr[i] = -1;

	/* Read the first block and get the superblock from it */
	memcpy(&super, cramfs_read(sb, 0, sizeof(super)), sizeof(super));
	mutex_unlock(&read_mutex);

	/* Do sanity checks on the superblock */
	if (super.magic != CRAMFS_MAGIC) {
		/* check at 512 byte offset */
		mutex_lock(&read_mutex);
		memcpy(&super, cramfs_read(sb, 512, sizeof(super)), sizeof(super));
		mutex_unlock(&read_mutex);
		if (super.magic != CRAMFS_MAGIC) {
			if (!silent)
				printk(KERN_ERR "cramfs: wrong magic\n");
			goto out;
		}
	}

	/* get feature flags first */
	if (super.flags & ~CRAMFS_SUPPORTED_FLAGS) {
		printk(KERN_ERR "cramfs: unsupported filesystem features\n");
		goto out;
	}

	/* Check that the root inode is in a sane state */
	if (!S_ISDIR(super.root.mode)) {
		printk(KERN_ERR "cramfs: root is not a directory\n");
		goto out;
	}
	root_offset = super.root.offset << 2;
	if (super.flags & CRAMFS_FLAG_FSID_VERSION_2) {
		sbi->size=super.size;
		sbi->blocks=super.fsid.blocks;
		sbi->files=super.fsid.files;
	} else {
		sbi->size=1<<28;
		sbi->blocks=0;
		sbi->files=0;
	}
	sbi->magic=super.magic;
	sbi->flags=super.flags;
	if (root_offset == 0)
		printk(KERN_INFO "cramfs: empty filesystem");
	else if (!(super.flags & CRAMFS_FLAG_SHIFTED_ROOT_OFFSET) &&
		 ((root_offset != sizeof(struct cramfs_super)) &&
		  (root_offset != 512 + sizeof(struct cramfs_super))))
	{
		printk(KERN_ERR "cramfs: bad root offset %lu\n", root_offset);
		goto out;
	}

	/* Set it all up.. */
	sb->s_op = &cramfs_ops;
	root = get_cramfs_inode(sb, &super.root);
	if (!root)
		goto out;
	sb->s_root = d_alloc_root(root);
	if (!sb->s_root) {
		iput(root);
		goto out;
	}
	return 0;
out:
	kfree(sbi);
	sb->s_fs_info = NULL;
	return -EINVAL;
}

static int cramfs_statfs(struct dentry *dentry, struct kstatfs *buf)
{
	struct super_block *sb = dentry->d_sb;

	buf->f_type = CRAMFS_MAGIC;
	buf->f_bsize = PAGE_CACHE_SIZE;
	buf->f_blocks = CRAMFS_SB(sb)->blocks;
	buf->f_bfree = 0;
	buf->f_bavail = 0;
	buf->f_files = CRAMFS_SB(sb)->files;
	buf->f_ffree = 0;
	buf->f_namelen = CRAMFS_MAXPATHLEN;
	return 0;
}

/*
 * Read a cramfs directory entry.
 */
static int cramfs_readdir(struct file *filp, void *dirent, filldir_t filldir)
{
	struct inode *inode = filp->f_dentry->d_inode;
	struct super_block *sb = inode->i_sb;
	char *buf;
	unsigned int offset;
	int copied;

	/* Offset within the thing. */
	offset = filp->f_pos;
	if (offset >= inode->i_size)
		return 0;
	/* Directory entries are always 4-byte aligned */
	if (offset & 3)
		return -EINVAL;

	buf = kmalloc(256, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	copied = 0;
	while (offset < inode->i_size) {
		struct cramfs_inode *de;
		unsigned long nextoffset;
		char *name;
		ino_t ino;
		mode_t mode;
		int namelen, error;

		mutex_lock(&read_mutex);
		de = cramfs_read(sb, OFFSET(inode) + offset, sizeof(*de)+256);
		name = (char *)(de+1);

		/*
		 * Namelengths on disk are shifted by two
		 * and the name padded out to 4-byte boundaries
		 * with zeroes.
		 */
		namelen = de->namelen << 2;
		memcpy(buf, name, namelen);
		ino = CRAMINO(de);
		mode = de->mode;
		mutex_unlock(&read_mutex);
		nextoffset = offset + sizeof(*de) + namelen;
		for (;;) {
			if (!namelen) {
				kfree(buf);
				return -EIO;
			}
			if (buf[namelen-1])
				break;
			namelen--;
		}
		error = filldir(dirent, buf, namelen, offset, ino, mode >> 12);
		if (error)
			break;

		offset = nextoffset;
		filp->f_pos = offset;
		copied++;
	}
	kfree(buf);
	return 0;
}

/*
 * Lookup and fill in the inode data..
 */
static struct dentry * cramfs_lookup(struct inode *dir, struct dentry *dentry, struct nameidata *nd)
{
	unsigned int offset = 0;
	int sorted;

	mutex_lock(&read_mutex);
	sorted = CRAMFS_SB(dir->i_sb)->flags & CRAMFS_FLAG_SORTED_DIRS;
	while (offset < dir->i_size) {
		struct cramfs_inode *de;
		char *name;
		int namelen, retval;

		de = cramfs_read(dir->i_sb, OFFSET(dir) + offset, sizeof(*de)+256);
		name = (char *)(de+1);

		/* Try to take advantage of sorted directories */
		if (sorted && (dentry->d_name.name[0] < name[0]))
			break;

		namelen = de->namelen << 2;
		offset += sizeof(*de) + namelen;

		/* Quick check that the name is roughly the right length */
		if (((dentry->d_name.len + 3) & ~3) != namelen)
			continue;

		for (;;) {
			if (!namelen) {
				mutex_unlock(&read_mutex);
				return ERR_PTR(-EIO);
			}
			if (name[namelen-1])
				break;
			namelen--;
		}
		if (namelen != dentry->d_name.len)
			continue;
		retval = memcmp(dentry->d_name.name, name, namelen);
		if (retval > 0)
			continue;
		if (!retval) {
			struct cramfs_inode entry = *de;
			mutex_unlock(&read_mutex);
			d_add(dentry, get_cramfs_inode(dir->i_sb, &entry));
			return NULL;
		}
		/* else (retval < 0) */
		if (sorted)
			break;
	}
	mutex_unlock(&read_mutex);
	d_add(dentry, NULL);
	return NULL;
}

static int cramfs_readpage(struct file *file, struct page * page)
{
	struct inode *inode = page->mapping->host;
	u32 maxblock, bytes_filled;
	void *pgdata;

	maxblock = (inode->i_size + PAGE_CACHE_SIZE - 1) >> PAGE_CACHE_SHIFT;
	bytes_filled = 0;
	if (page->index < maxblock) {
		struct super_block *sb = inode->i_sb;
		u32 blkptr_offset = OFFSET(inode) + page->index*4;
		u32 start_offset, compr_len;

		start_offset = OFFSET(inode) + maxblock*4;
		mutex_lock(&read_mutex);
		if (page->index)
			start_offset = *(u32 *) cramfs_read(sb, blkptr_offset-4, 4);
		compr_len = (*(u32 *) cramfs_read(sb, blkptr_offset, 4) - start_offset);
		mutex_unlock(&read_mutex);
		pgdata = kmap(page);
		if (compr_len == 0)
			; /* hole */
		else {
			mutex_lock(&read_mutex);
			bytes_filled = cramfs_uncompress_block(pgdata,
				 PAGE_CACHE_SIZE,
				 cramfs_read(sb, start_offset, compr_len),
				 compr_len);
			mutex_unlock(&read_mutex);
		}
	} else
		pgdata = kmap(page);
	memset(pgdata + bytes_filled, 0, PAGE_CACHE_SIZE - bytes_filled);
	kunmap(page);
	flush_dcache_page(page);
	SetPageUptodate(page);
	unlock_page(page);
	return 0;
}

static const struct address_space_operations cramfs_aops = {
	.readpage = cramfs_readpage
};

/*
 * Our operations:
 */

/*
 * A directory can only readdir
 */
static const struct file_operations cramfs_directory_operations = {
	.llseek		= generic_file_llseek,
	.read		= generic_read_dir,
	.readdir	= cramfs_readdir,
};

static struct inode_operations cramfs_dir_inode_operations = {
	.lookup		= cramfs_lookup,
};

static struct super_operations cramfs_ops = {
	.put_super	= cramfs_put_super,
	.remount_fs	= cramfs_remount,
	.statfs		= cramfs_statfs,
};

static int cramfs_get_sb(struct file_system_type *fs_type,
	int flags, const char *dev_name, void *data, struct vfsmount *mnt)
{
	return get_sb_bdev(fs_type, flags, dev_name, data, cramfs_fill_super,
			   mnt);
}

static struct file_system_type cramfs_fs_type = {
	.owner		= THIS_MODULE,
	.name		= "cramfs",
	.get_sb		= cramfs_get_sb,
	.kill_sb	= kill_block_super,
	.fs_flags	= FS_REQUIRES_DEV,
};

static int __init init_cramfs_fs(void)
{
	cramfs_uncompress_init();
	return register_filesystem(&cramfs_fs_type);
}

static void __exit exit_cramfs_fs(void)
{
	cramfs_uncompress_exit();
	unregister_filesystem(&cramfs_fs_type);
}

module_init(init_cramfs_fs)
module_exit(exit_cramfs_fs)
MODULE_LICENSE("GPL");