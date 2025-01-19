#define _GNU_SOURCE         /* See feature_test_macros(7) */

#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include <dlfcn.h>
#include <sys/mman.h>

const char *SO_MEMFD_NAME = "so_anon_name";
const int MAX_VIRT_FN_SIZE = 256;
const char *SHARED_FUNC_NAME = "handle_remote_http_session_rs";

int load_shared_library(char *path, void **buf, long *buf_size) {
  int fd = -1;
  void *ptr = NULL;

  struct stat status;
  int ret = stat(path, &status);
  if (ret < 0) {
    return ret;
  }
  long filesize = status.st_size;
  ptr = malloc(filesize);
  if (ptr == NULL) {
    goto error;
  }

  fd = open(path, O_RDONLY);
  if (fd < 0) {
    goto error;
  }

  int read_size = 16384;
  int n_bytes = 0;
  do {
    int remaining = filesize - n_bytes;
    if (remaining < read_size) {
      read_size = remaining;
    }
    if (read_size == 0) {
      break;
    }
    ret = read(fd, ptr+n_bytes, read_size);
    if (ret < 0) {
      goto error;
    }

    n_bytes += ret;
  } while (ret != 0);

  close(fd);
  *buf = ptr;
  *buf_size = filesize;
  return 0;

error:
  if (fd != -1) {
    close(fd);
  }
  if (ptr != NULL) {
    free(ptr);
  }
  return -1;
}

int bind_shared_library(void *shared_lib, long buf_size, void **shared_lib_handle) {
  int fd = -1;
  // create the virtual path to the mem fd
  char vfname[MAX_VIRT_FN_SIZE];

  fd = memfd_create(SO_MEMFD_NAME, 0);
  if (fd < 0) {
    goto error;
  }

  if (write(fd, shared_lib, buf_size) < buf_size) {
    goto error;
  }

  if (snprintf(vfname, MAX_VIRT_FN_SIZE, "/proc/self/fd/%d", fd) < 0) {
    goto error;
  }

  void *handle = dlopen(vfname, RTLD_NOW);
  if (handle == NULL) {
    goto error;
  }

  close(fd);
  *shared_lib_handle = handle;

  return 0;
error:
  if (fd != -1) {
    close(fd);
  }

  return -1;
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    printf("argument must specify disk location of shared object\n");
    return 1;
  }
  char *disk_loc = argv[1];
  printf("loading from: %s\n", disk_loc);

  void *so_buf = NULL;
  void *shared_lib_handle = NULL;

  long so_buf_size = 0;
  int ret = load_shared_library(disk_loc, &so_buf, &so_buf_size);
  if (ret < 0) {
    perror("unable to load shared object");
    goto error;
  }
  printf("shared object is %ld bytes long\n", so_buf_size);

  ret = bind_shared_library(so_buf, so_buf_size, &shared_lib_handle);
  if (ret < 0) {
    perror("unable to bind shared library");
    goto error;
  }
  printf("shared library has been bound\n");

  int (*handle_remote_http_session_rs)(char *);
  *(void **) (&handle_remote_http_session_rs) = dlsym(shared_lib_handle, SHARED_FUNC_NAME);
  if (handle_remote_http_session_rs == NULL) {
    perror("failed to find function in shared library");
    goto error;
  }
  if (handle_remote_http_session_rs("http://www.test.com") < 0) {
    perror("call to handle_remote_http_session_rs failed");
    goto error;
  }
  printf("shared object call succeeded\n");

  dlclose(shared_lib_handle);
  shared_lib_handle = NULL;
  free(so_buf);
  so_buf = NULL;

error:
  if (so_buf != NULL) {
    free(so_buf);
  }

  if (shared_lib_handle != NULL) {
    dlclose(shared_lib_handle);
  }

  return 0;
}