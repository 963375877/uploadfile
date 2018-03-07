#include <fcgi_stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>


#define UPLOAD_LOG_MODULE "cgi"
#define UPLOAD_LOG_PROC   "upload"

#define TEMP_BUF_MAX_LEN 512
#define FILE_NAME_LEN 256
#define USER_NAME_LEN 256



/**
 * @brief  解析上传的post数据 保存到本地临时路径
 *         同时得到文件上传者、文件名称、文件大小
 *
 * @param len       (in)    post数据的长度
 * @param user      (out)   文件上传者
 * @param file_name (out)   文件的文件名
 * @param p_size    (out)   文件大小
 *
 * @returns
 *          0 succ, -1 fail
 */
int recv_save_file(long len, char *user, char *filename, long *p_size);


char* memstr(char* full_data, int full_data_len, char* substr);

/*
 *@brief  将字符串写入到指定的log文件
 *         
 *
 * @param log       (in)    要写的字符串
 * @param len      	(in)   	要写字符串的长度
 * @param logPath   (in)    log文件所在路径
 * @param logName   (in)    log文件名
 *
 * @returns
 *          0 succ, -1 fail
 */
int write_log(char * logPath, char *logName, char * log, int len);