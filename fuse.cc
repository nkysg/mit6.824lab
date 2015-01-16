/*
 * receive request from fuse and call methods of yfs_client
 *
 * started life as low-level example in the fuse distribution.  we
 * have to use low-level interface in order to get i-numbers.  the
 * high-level interface only gives us complete paths.
 */

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fuse_lowlevel.h>
#include "lang/verify.h"
#include "yfs_client.h"
using namespace std;

int myid;
yfs_client *yfs;

int id() { 
  return myid;
}

//
// A file/directory's attributes are a set of information
// including owner, permissions, size, &c. The information is
// much the same as that returned by the stat() system call.
// The kernel needs attributes in many situations, and some
// fuse functions (such as lookup) need to return attributes
// as well as other information, so getattr() gets called a lot.
//
// YFS fakes most of the attributes. It does provide more or
// less correct values for the access/modify/change times
// (atime, mtime, and ctime), and correct values for file sizes.
//
yfs_client::status
getattr(yfs_client::inum inum, struct stat &st)
{
  yfs_client::status ret;

  bzero(&st, sizeof(st));
  printf("   getfileattr -> \n");

  st.st_ino = inum;
  if(yfs->isfile(inum)){
     yfs_client::fileinfo info;
     ret = yfs->getfile(inum, info);
     if(ret != yfs_client::OK)
       return ret;
     st.st_mode = S_IFREG | 0666;
     st.st_nlink = 1;
     st.st_atime = info.atime;
     st.st_mtime = info.mtime;
     st.st_ctime = info.ctime;
     st.st_size = info.size;
     printf("   fileattr -> %llu\n", info.size);
   } else {
     yfs_client::dirinfo info;
     ret = yfs->getdir(inum, info);
     if(ret != yfs_client::OK)
       return ret;
     st.st_mode = S_IFDIR | 0777;
     st.st_nlink = 2;
     st.st_atime = info.atime;
     st.st_mtime = info.mtime;
     st.st_ctime = info.ctime;
     printf("   dirattr -> %lu %lu %lu\n", info.atime, info.mtime, info.ctime);
   }
   return yfs_client::OK;
}

//
// This is a typical fuse operation handler; you'll be writing
// a bunch of handlers like it.
//
// A handler takes some arguments
// and supplies either a success or failure response. It provides
// an error response by calling either fuse_reply_err(req, errno), and
// a normal response by calling ruse_reply_xxx(req, ...). The req
// argument serves to link up this response with the original
// request; just pass the same @req that was passed into the handler.
// 
// The @ino argument indicates the file or directory FUSE wants
// you to operate on. It's a 32-bit FUSE identifier; just assign
// it to a yfs_client::inum to get a 64-bit YFS inum.
//
void
fuseserver_getattr(fuse_req_t req, fuse_ino_t ino,
                   struct fuse_file_info *fi)
{
    struct stat st;
    yfs_client::inum inum = ino; 
    yfs_client::status ret;

    ret = getattr(inum, st);
    if(ret != yfs_client::OK){
      fuse_reply_err(req, ENOENT);
      return;
    }
    fuse_reply_attr(req, &st, 0);
}

//
// Set the attributes of a file. Often used as part of overwriting
// a file, to set the file length to zero.
//
// to_set is a bitmap indicating which attributes to set. You only
// have to implement the FUSE_SET_ATTR_SIZE bit, which indicates
// that the size of the file should be changed. The new size is
// in attr->st_size. If the new size is bigger than the current
// file size, fill the new bytes with '\0'.
//
// On success, call fuse_reply_attr, passing the file's new
// attributes (from a call to getattr()).
//
void
fuseserver_setattr(fuse_req_t req, fuse_ino_t ino, struct stat *attr,
                   int to_set, struct fuse_file_info *fi)
{
  printf("fuseserver_setattr 0x%x\n", to_set);
  yfs_client::inum inum = ino;
  yfs_client::fileinfo info;
  yfs_client::status ret;
  if (FUSE_SET_ATTR_SIZE & to_set) {
    printf("fuseserver_setattr set size to %zu\n", attr->st_size);
    struct stat st;
    // You fill this in for Lab 2
#if 1
    info.size = attr->st_size;
    ret = yfs->setattr(inum, info);
    if (ret != yfs_client::OK) {
      printf("set attr failed\n");
      fuse_reply_err(req, ENOENT);
      return;
    }
    
    ret = getattr(inum, st);
    if(ret != yfs_client::OK) {
      printf("get attr failed\n");
      fuse_reply_err(req, ENOENT);
      return;
    }
    // Change the above line to "#if 1", and your code goes here
    // Note: fill st using getattr before fuse_reply_attr
    printf("Set attr success\n");
    fuse_reply_attr(req, &st, 0);
#else
    fuse_reply_err(req, ENOSYS);
#endif
  } else {
    fuse_reply_err(req, ENOSYS);
  }
}

//
// Read up to @size bytes starting at byte offset @off in file @ino.
//
// Pass the number of bytes actually read to fuse_reply_buf.
// If there are fewer than @size bytes to read between @off and the
// end of the file, read just that many bytes. If @off is greater
// than or equal to the size of the file, read zero bytes.
//
// Ignore @fi. 
// @req identifies this request, and is used only to send a 
// response back to fuse with fuse_reply_buf or fuse_reply_err.
//
void
fuseserver_read(fuse_req_t req, fuse_ino_t ino, size_t size,
                off_t off, struct fuse_file_info *fi)
{
  // You fill this in for Lab 2
  yfs_client::inum inum = ino;
  std::string buf;
  if (yfs->isfile(inum)) {
    yfs_client::fileinfo fin;
    if(yfs->getfile(inum, fin) == yfs_client::OK) {
      if (off < 0 || off >= fin.size) {
        fuse_reply_err(req, EINVAL);
      }
      else {
        if (yfs->read(inum,buf,off,size) == yfs_client::OK) {
            fuse_reply_buf(req, buf.data(), buf.size());
            return;
        }
      }
    }
  }
  fuse_reply_err(req, ENOSYS);
#if 1
  // Change the above "#if 0" to "#if 1", and your code goes here
#else
  fuse_reply_err(req, ENOSYS);
#endif
}

//
// Write @size bytes from @buf to file @ino, starting
// at byte offset @off in the file.
//
// If @off + @size is greater than the current size of the
// file, the write should cause the file to grow. If @off is
// beyond the end of the file, fill the gap with null bytes.
//
// Set the file's mtime to the current time.
//
// Ignore @fi.
//
// @req identifies this request, and is used only to send a 
// response back to fuse with fuse_reply_buf or fuse_reply_err.
//
void
fuseserver_write(fuse_req_t req, fuse_ino_t ino,
                 const char *buf, size_t size, off_t off,
                 struct fuse_file_info *fi)
{
  // You fill this in for Lab 2
  yfs_client::inum inum = ino;
  yfs_client::fileinfo info;
  yfs_client::status ret;
  if (yfs->isfile(inum)) {
    std::cout <<"off: " << off << "size: " << size<<'\n';
    std::string sbuf(buf,size);

    if(((ret = yfs->write(inum,sbuf,off,size))) != yfs_client::OK) {
    std::cout << "error write\n";
    fuse_reply_err(req, ENOSYS);
    }
    fuse_reply_write(req,size);
    return;

  }

#if 0
  // Change the above line to "#if 1", and your code goes here
  fuse_reply_write(req, size);
#else
  fuse_reply_err(req, ENOSYS);
#endif
}

//
// Create file @name in directory @parent. 
//
// - @mode specifies the create mode of the file. Ignore it - you do not
//   have to implement file mode.
// - If a file named @name already exists in @parent, return EXIST.
// - Pick an ino (with type of yfs_client::inum) for file @name. 
//   Make sure ino indicates a file, not a directory!
// - Create an empty extent for ino.
// - Add a <name, ino> entry into @parent.
// - Change the parent's mtime and ctime to the current time/date
//   (this may fall naturally out of your extent server code).
// - On success, store the inum of newly created file into @e->ino, 
//   and the new file's attribute into @e->attr. Get the file's
//   attributes with getattr().
//
// @return yfs_client::OK on success, and EXIST if @name already exists.
//
yfs_client::status
fuseserver_createhelper(fuse_ino_t parent, const char *name,
                        mode_t mode, struct fuse_entry_param *e)
{
  printf("fuseserver_createhelper name: %s\n",name);
  yfs_client::inum fnum =0;
  yfs_client::inum iparent = parent;
  int r = yfs->create(iparent,name,fnum,true);
    //set entry parameters
    if(r == yfs_client::EXIST)
      return r;
    e->attr_timeout = 0.0;
    e->entry_timeout = 0.0;
    e->ino = fnum;
    getattr(e->ino, e->attr);

    //test insertion TODO remove
    //get data
    std::string data;
    yfs->getdirdata(iparent, data);
    printf("read tetestdata = %s\n",data.c_str());
    return yfs_client::OK;

  //printf("  seserver_createhelper !! parent is NOT A DIR\n");

  // You fill this in
  // return yfs_client::NOENT;
}



void
fuseserver_create(fuse_req_t req, fuse_ino_t parent, const char *name,
                  mode_t mode, struct fuse_file_info *fi)
{
  struct fuse_entry_param e;
  yfs_client::status ret;
  if( (ret = fuseserver_createhelper( parent, name, mode, &e )) == yfs_client::OK ) {
    std::cout<<parent<<name<<mode<<endl;;
    fuse_reply_create(req, &e, fi);
  } else {
		if (ret == yfs_client::EXIST) {
                  std::cout<<"create error EEXIST"<<endl;
			fuse_reply_err(req, EEXIST);
		}else{

                  std::cout<<"create error ENOENT"<<endl;
			fuse_reply_err(req, ENOENT);
		}
  }
}

void fuseserver_mknod( fuse_req_t req, fuse_ino_t parent, 
    const char *name, mode_t mode, dev_t rdev ) {
  struct fuse_entry_param e;
  yfs_client::status ret;
  if( (ret = fuseserver_createhelper( parent, name, mode, &e )) == yfs_client::OK ) {
    fuse_reply_entry(req, &e);
  } else {
		if (ret == yfs_client::EXIST) {
			fuse_reply_err(req, EEXIST);
		}else{
                  printf("node exist\n");
			fuse_reply_err(req, ENOENT);
		}
  }
}

//
// Look up file or directory @name in the directory @parent. If @name is
// found, set e.attr (using getattr) and e.ino to the attribute and inum of
// the file.
//
void
fuseserver_lookup(fuse_req_t req, fuse_ino_t parent, const char *name)
{
  struct fuse_entry_param e;
  // In yfs, timeouts are always set to 0.0, and generations are always set to 0
  e.attr_timeout = 0.0;
  e.entry_timeout = 0.0;
  e.generation = 0;
  bool found = false;
  /*
  yfs_client::status ret;
  yfs_client::inum p_inum = parent;
  yfs_client::inum c_inum;
  printf("fuseserver_lookup parent:%016lx name:%s\n", parent, name);  
  ret = yfs->lookup(p_inum, name, c_inum);
  if (ret == yfs_client::OK) {
     struct stat st;
     e.ino = c_inum;
     if(getattr(c_inum, st) == yfs_client::OK)
        e.attr = st;
     found = true;
  }

  // You fill this in for Lab 2
  if (found)
    fuse_reply_entry(req, &e);
  else
    fuse_reply_err(req, ENOENT);
  */
    printf("name: %s\n", name);

  // retreive what is in directory
  std::string buffer;
  yfs->getdirdata(parent, buffer);

  // look for file name
  int search = buffer.find(name);
  printf("name is at index: %d\n", search);
  
  
  if(search != -1){
    found = true;
  }

  // parse directory contents for inum to update e
  if (found){
    buffer = buffer.substr(search, buffer.length());
    buffer = buffer.substr(buffer.find(" ") + 1, buffer.length());
    buffer = buffer.substr(0, buffer.find(" "));
    
    // convert inum string to a long long unsigned
    std::istringstream ist(buffer);
    unsigned long long ino;
    ist >> ino;

    e.generation = 0;
    e.ino = ino;
    getattr(ino, e.attr);
    
    fuse_reply_entry(req, &e);

  }
  else {
    fuse_reply_err(req, ENOENT);
  }
}


struct dirbuf {
    char *p;
    size_t size;
};

void dirbuf_add(struct dirbuf *b, const char *name, fuse_ino_t ino)
{
    struct stat stbuf;
    size_t oldsize = b->size;
    b->size += fuse_dirent_size(strlen(name));
    b->p = (char *) realloc(b->p, b->size);
    memset(&stbuf, 0, sizeof(stbuf));
    stbuf.st_ino = ino;
    fuse_add_dirent(b->p + oldsize, name, &stbuf, b->size);
}

#define min(x, y) ((x) < (y) ? (x) : (y))

int reply_buf_limited(fuse_req_t req, const char *buf, size_t bufsize,
          off_t off, size_t maxsize)
{
  if ((size_t)off < bufsize)
    return fuse_reply_buf(req, buf + off, min(bufsize - off, maxsize));
  else
    return fuse_reply_buf(req, NULL, 0);
}

//
// Retrieve all the file names / i-numbers pairs
// in directory @ino. Send the reply using reply_buf_limited.
//
// You can ignore @size and @off (except that you must pass
// them to reply_buf_limited).
//
// Call dirbuf_add(&b, name, inum) for each entry in the directory.
//
void
fuseserver_readdir(fuse_req_t req, fuse_ino_t ino, size_t size,
                   off_t off, struct fuse_file_info *fi)
{
  yfs_client::inum inum = ino; // req->in.h.nodeid;
  struct dirbuf b;
  int index = 0;

  printf("fuseserver_readdir\n");

  if(!yfs->isdir(inum)){
    fuse_reply_err(req, ENOTDIR);
    printf("fuseserver_readdir %d is not dir\n",inum);
    return;
  }

  memset(&b, 0, sizeof(b));
   // get directory contents
  std::string retrieve;
  yfs->getdirdata(ino, retrieve);
  printf("dir %s\n ", retrieve.c_str());
  
  // add file names to directory one by one
  while(retrieve.length() > 0){
    std::string file = retrieve.substr(0, retrieve.find(" "));
    printf("file added %s\n ", file.c_str());
    dirbuf_add(&b, file.c_str(), ino);
    index = retrieve.find(" ");
    retrieve = retrieve.substr(index + 1, retrieve.length());
    index = retrieve.find(" ");
    retrieve = retrieve.substr(index + 1, retrieve.length());
  }

  // You fill this in for Lab 2


  reply_buf_limited(req, b.p, b.size, off, size);
  free(b.p);
}


void
fuseserver_open(fuse_req_t req, fuse_ino_t ino,
     struct fuse_file_info *fi)
{
  fuse_reply_open(req, fi);
}

//
// Create a new directory with name @name in parent directory @parent.
// Leave new directory's inum in e.ino and attributes in e.attr.
//
// The new directory should be empty (no . or ..).
// 
// If a file/directory named @name already exists, indicate error EEXIST.
//
// Ignore mode.
//
void
fuseserver_mkdir(fuse_req_t req, fuse_ino_t parent, const char *name,
     mode_t mode)
{
  struct fuse_entry_param e;
  // In yfs, timeouts are always set to 0.0, and generations are always set to 0
  e.attr_timeout = 0.0;
  e.entry_timeout = 0.0;
  e.generation = 0;
  // Suppress compiler warning of unused e.
  (void) e;


  // You fill this in for Lab 3
  if( yfs->isdir(parent) )
  {
    yfs_client::status ret;
    yfs_client::inum ii = parent;
    struct stat st;
    getattr(ii,st);
    yfs_client::inum fnum = 0;
    printf("mkdir name %s\n",name);
    yfs->create(ii, name, fnum, false);
    //set entry parameters
    e.ino = fnum;
    if(ret = getattr(fnum, st) == yfs_client::OK) {
        e.attr = st;
        printf("fuseserver_mkdir time m%d c%d a%d \n", st.st_mtime, st.st_ctime, st.st_atime);
        }
  }

#if 1 
  fuse_reply_entry(req, &e);
#else
  fuse_reply_err(req, ENOSYS);
#endif
}

//
// Remove the file named @name from directory @parent.
// Free the file's extent.
// If the file doesn't exist, indicate error ENOENT.
//
// Do *not* allow unlinking of a directory.
//
void
fuseserver_unlink(fuse_req_t req, fuse_ino_t parent, const char *name)
{

  // You fill this in for Lab 3
  // Success:	fuse_reply_err(req, 0);
  // Not found:	fuse_reply_err(req, ENOENT);
  
  //find name in parent contents
  
  //
  if (yfs->unlink(parent,name) == yfs_client::OK) {
    fuse_reply_err(req, 0);
  }
  else {
    fuse_reply_err(req, ENOSYS);
  }

}

void
fuseserver_statfs(fuse_req_t req)
{
  struct statvfs buf;

  printf("statfs\n");

  memset(&buf, 0, sizeof(buf));

  buf.f_namemax = 255;
  buf.f_bsize = 512;

  fuse_reply_statfs(req, &buf);
}

struct fuse_lowlevel_ops fuseserver_oper;

int
main(int argc, char *argv[])
{
  char *mountpoint = 0;
  int err = -1;
  int fd;

  setvbuf(stdout, NULL, _IONBF, 0);

  if(argc != 4){
    fprintf(stderr, "Usage: yfs_client <mountpoint> <port-extent-server> <port-lock-server>\n");
    exit(1);
  }
  mountpoint = argv[1];

  srandom(getpid());

  myid = random();

  yfs = new yfs_client(argv[2], argv[3]);

  fuseserver_oper.getattr    = fuseserver_getattr;
  fuseserver_oper.statfs     = fuseserver_statfs;
  fuseserver_oper.readdir    = fuseserver_readdir;
  fuseserver_oper.lookup     = fuseserver_lookup;
  fuseserver_oper.create     = fuseserver_create;
  fuseserver_oper.mknod      = fuseserver_mknod;
  fuseserver_oper.open       = fuseserver_open;
  fuseserver_oper.read       = fuseserver_read;
  fuseserver_oper.write      = fuseserver_write;
  fuseserver_oper.setattr    = fuseserver_setattr;
  fuseserver_oper.unlink     = fuseserver_unlink;
  fuseserver_oper.mkdir      = fuseserver_mkdir;

  const char *fuse_argv[20];
  int fuse_argc = 0;
  fuse_argv[fuse_argc++] = argv[0];
#ifdef __APPLE__
  fuse_argv[fuse_argc++] = "-o";
  fuse_argv[fuse_argc++] = "nolocalcaches"; // no dir entry caching
  fuse_argv[fuse_argc++] = "-o";
  fuse_argv[fuse_argc++] = "daemon_timeout=86400";
#endif

  // everyone can play, why not?
  //fuse_argv[fuse_argc++] = "-o";
  //fuse_argv[fuse_argc++] = "allow_other";

  fuse_argv[fuse_argc++] = mountpoint;
  fuse_argv[fuse_argc++] = "-d";

  fuse_args args = FUSE_ARGS_INIT( fuse_argc, (char **) fuse_argv );
  int foreground;
  int res = fuse_parse_cmdline( &args, &mountpoint, 0 /*multithreaded*/, 
        &foreground );
  if( res == -1 ) {
    fprintf(stderr, "fuse_parse_cmdline failed\n");
    return 0;
  }
  
  args.allocated = 0;

  fd = fuse_mount(mountpoint, &args);
  if(fd == -1){
    fprintf(stderr, "fuse_mount failed\n");
    exit(1);
  }

  struct fuse_session *se;

  se = fuse_lowlevel_new(&args, &fuseserver_oper, sizeof(fuseserver_oper),
       NULL);
  if(se == 0){
    fprintf(stderr, "fuse_lowlevel_new failed\n");
    exit(1);
  }

  struct fuse_chan *ch = fuse_kern_chan_new(fd);
  if (ch == NULL) {
    fprintf(stderr, "fuse_kern_chan_new failed\n");
    exit(1);
  }

  fuse_session_add_chan(se, ch);
  // err = fuse_session_loop_mt(se);   // FK: wheelfs does this; why?
  err = fuse_session_loop(se);
    
  fuse_session_destroy(se);
  close(fd);
  fuse_unmount(mountpoint);

  return err ? 1 : 0;
}
