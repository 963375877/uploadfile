//好的习惯是成功的基础



#include <fcgi_stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <dirent.h>

#include "qsi.h"
#include "cJSON.h"

#define UPLOAD_LOG_MODULE "cgi"
#define UPLOAD_LOG_PROC   "upload"

#define TEMP_BUF_MAX_LEN 51200
#define FILE_NAME_LEN 256
#define USER_NAME_LEN 256




/*
	功能：
	参数：
	返回值：

*/


char* memstr(char* full_data, int full_data_len, char* substr)
{
    if (full_data == NULL || full_data_len <= 0 || substr == NULL)
    {
        return NULL;
    }

    if (*substr == '\0')
    {
        return NULL;
    }

    int sublen = strlen(substr);
    char* cur = full_data;
    int last_possible = full_data_len - sublen + 1;
    int i = 0;
    for (i = 0; i < last_possible; i++)
    {
        if (*cur == *substr)
        {
            if (memcmp(cur, substr, sublen) == 0)
            {
                // found
                return cur;
            }
        }
        cur++;
    }
    return NULL;
}



/**
 * @brief  解析上传的post数据 保存到本地临时路径
 *         同时得到文件上传者、文件名称、文件大小
 *
 * @param len       (in)    post数据的长度
 * @param user      (out)   文件上传者
 * @param filepath  (out)   文件存放路径
 * @param file_name (out)   文件的文件名
 * @param p_size    (out)   文件大小
 *
 * @returns
 *          0 succ, -1 fail
 */
int wx_recv_save_file(long len, char *user, char * filepath, char *filename, long *p_size)
{	
	char log[526] = {0};
	memset(log,0x00, 526);
    int ret = 0;
    
    char fullpath[2000] = {0};
     memset(fullpath,0x00, 2000);
   
     
    char *file_buf = NULL;
    char *begin = NULL;
    char *p, *q, *k;
    
     int fd = 0;
	int ret2 = 0;
	
	char *heto= NULL;
	char *to = NULL;
		 
	int totallen = 0;
	char * toen = NULL;
	
	int num = 0;
	 
	int FragmentName = 0;	
	char FragmentNum[50];	
	char FPATH[256] = {0};
	
    char content_text[TEMP_BUF_MAX_LEN] = {0}; //文件头部信息
    char boundary[TEMP_BUF_MAX_LEN] = {0};     //分界线信息

    //==========> 开辟存放文件的 内存 <===========
    file_buf = (char *)malloc(len);
    memset(file_buf, 0x00, len);
    
    if (file_buf == NULL)
    {

        goto END;
    }

    ret2 = FCGI_fread(file_buf, 1, len, stdin); //从标准输入(web服务器)读取内容
    if(ret2 == 0)
    {    	
        ret = -1;
        goto END;
    }
    
    //===========> 开始处理前端发送过来的post数据格式 <============
    begin = file_buf;    //内存起点
    p = begin;

    /*
       ------WebKitFormBoundary88asdgewtgewx\r\n
       Content-Disposition: form-data; user="mike"; filename="xxx.jpg"; md5="xxxx"; size=10240\r\n
       Content-Type: application/octet-stream\r\n
       \r\n
       真正的文件内容\r\n
       ------WebKitFormBoundary88asdgewtgewx
    */

    //get boundary 得到分界线, ------WebKitFormBoundary88asdgewtgewx
    p = strstr(begin, "\r\n");
    if (p == NULL)
    {
        ret = -1;
        goto END;
    }

    //拷贝分界线
    strncpy(boundary, begin, p-begin);
    boundary[p-begin] = '\0';   //字符串结束符 拿到分界线 
	
	
    p += 2;//\r\n
    //已经处理了p-begin的长度
    len -= (p-begin);

    //get content text head
    begin = p;

    //Content-Disposition: form-data; user="mike"; filename="xxx.jpg"; md5="xxxx"; size=10240\r\n
    p = strstr(begin, "\r\n");
    if(p == NULL)
    {
       ret = -1;
       goto END;
    }
    
    strncpy(content_text, begin, p-begin); //拷贝Content-Disposition这一行内容
    content_text[p-begin] = '\0';

    p += 2;//\r\n
    len -= (p-begin);

    //========================================获取文件上传者
    //Content-Disposition: form-data; user="mike"; filename="xxx.jpg"; md5="xxxx"; size=10240\r\n
    //                                ↑
    q = begin;
    q = strstr(begin, "name=");
    

    //Content-Disposition: form-data; user="mike"; filename="xxx.jpg"; md5="xxxx"; size=10240\r\n                                     ↑
    q += strlen("name=");
    q++;    //跳过第一个"

    //Content-Disposition: form-data; user="mike"; filename="xxx.jpg"; md5="xxxx"; size=10240\r\n
    //                                          ↑
    k = strchr(q, '"');
    strncpy(user, q, k-q);  //拷贝用户名
    user[k-q] = '\0';
    
    FCGI_printf("user = %s\n", user);
    //去掉一个字符串两边的空白字符
    //trim_space(user);   //util_cgi.h

    //========================================获取文件名字
    //"; filename="xxx.jpg"; md5="xxxx"; size=10240\r\n
    //   ↑
    begin = k;
    q = begin;
    q = strstr(begin, "filename=");

    //"; filename="xxx.jpg"; md5="xxxx"; size=10240\r\n
    //             ↑
    q += strlen("filename=");
    q++;    //跳过第一个"

    //"; filename="xxx.jpg"; md5="xxxx"; size=10240\r\n                    ↑
    k = strchr(q, '"');
    strncpy(filename, q, k-q);  //拷贝文件名
    filename[k-q] = '\0';
 	
 	
 	p+=2;	
    begin = p;
    p = strstr(begin, "\r\n");
    p += 2;//\r\n
    len -= (p-begin);
  
    begin = p;  
    
       
    p = strstr(begin, "\r\n");
    p+=2;
    //Content-Disposition: form-data; name="file"; 
    len -= (p-begin);  
   	
   	//----------------------------455617322038579812696183
    begin = p;
    char *f, *e; 
    f = strstr(begin, "name=");
    f+= strlen("name=");
    f++;
    e = strchr(f, '"');
    
    char newfile[256];
    strncpy(newfile, f, e - f);
    FCGI_printf("newfile = %s\n", newfile);
    
    p = strstr(begin, "\r\n");      
    p+=2;  
    
    len -= (p-begin);
    
    begin = p;
    p = strstr(begin, "\r\n");
    p+=4;
    len -= (p-begin);
     
     //下面才是文件的真正内容
	
    /*
       ------WebKitFormBoundary88asdgewtgewx\r\n
       Content-Disposition: form-data; user="mike"; filename="xxx.jpg"; md5="xxxx"; size=10240\r\n
       Content-Type: application/octet-stream\r\n
       \r\n
       真正的文件内容\r\n
       ------WebKitFormBoundary88asdgewtgewx
    */
     
    begin = p; 	
    len -= (p - begin); 
 
    p = memstr(begin, len, boundary);
   
    if (p == NULL)
    {      
        ret = -1;        
       	goto END;
    }
    else
    {
        p = p - 2;//\r\n
    }
	/*	
		------WebKitFormBoundaryqeJJMxl4wwu9T8YL
		Content-Disposition: form-data; name="total"

		295
		------WebKitFormBoundaryqeJJMxl4wwu9T8YL
		Content-Disposition: form-data; name="index"

		0	
	*/
	
	
	 sprintf(fullpath, "%s/%s",filepath, filename );
    //begin---> file_len = (p-begin)

    //=====> 此时begin-->p两个指针的区间就是post的文件二进制数据
    //======>将数据写入文件中,其中文件名也是从post数据解析得来  <===========		 
	  fd = open(fullpath, O_CREAT|O_WRONLY, 0644);	  	  
	  
	  if (fd < 0)
	  {
	      ret = -1;
	      goto END;
	  }
	
	  //ftruncate会将参数fd指定的文件大小改为参数length指定的大小
	  ftruncate(fd, (p-begin));
	  FCGI_write(fd, begin, (p-begin));
	  FCGI_close(fd);
	
END:
    free(file_buf);
    return ret;
}


		

int main ()
{
	int count = 0;
	int flag = 0;
	
	long len;
    int ret = 0;	
    
    char filename[256] = {0}; //文件名
    char user[56] = {0};      //文件上传者
    long size = 0;
    
	
	char get_filePath[20] = "filepath";
	char get_fileName[20] = "filename";
	
	
		
	while (FCGI_Accept() >= 0) 
	{
		FCGI_printf("Content-type: text/html; charset=UTF-8\r\n\r\n");
			   	 
		char *contentLength = getenv("CONTENT_LENGTH");	 			//获取环境变量	
		char *querystring = getenv("QUERY_STRING"); 
		
		char filepath[256] = "/home/tbs/TBSNative/TBSUploadFile/file/image";
		
		if(contentLength != NULL)
		{
			len = strtol(contentLength, NULL, 10);	
		}
				
		char name[256] = "method";
		char *method = get_query_string(querystring, name);
		
		//char *filepath = get_query_string(querystring, get_filePath);		
		char *filename = get_query_string(querystring, get_fileName);
			
										
		if(strcasecmp(method, "wxupload") == 0)
		{
			FCGI_printf("filename = %s ", filename);
			//wx_recv_save_file(len, user, filepath, filename, &size);
		}						
						
	}
	return 0;
}