#ifndef KERNEL_SLICEFS_H
#define KERNEL_SLICEFS_H

#include "../kernel.h"

extern char current_directory[FS_NAME_LENGTH];
extern char current_user[USERNAME_LENGTH];

int build_absolute_path(const char *input, char *output);
int fs_directory_exists(const char *path);
int fs_format(void);
int fs_save_superblock(void);
int fs_save_table(void);
int fs_touch(const char *input);
int fs_mkdir(const char *input);
void fs_ls(void);
int fs_list_directory(char names[][FS_NAME_LENGTH], uint8_t *types, int max_entries);
void fs_pwd(void);
int fs_cd(const char *input);
int fs_write(const char *input, const char *content);
int fs_write_absolute(const char *path, const char *content);
int fs_read_absolute(const char *path, char *buffer, int capacity);
int fs_cat(const char *input);
int fs_rm(const char *input);
int find_user(const char *username);
int useradd(const char *username, const char *password);
int login_user(const char *username, const char *password);

#endif
