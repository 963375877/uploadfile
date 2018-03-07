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
#include <pthread.h>
#include <fcgiapp.h>
#include <sys/time.h>  
#include <time.h>
#include <mysql/mysql.h>

#include "qsi.h"
#include "cJSON.h"

#define UPLOAD_LOG_MODULE "cgi"
#define UPLOAD_LOG_PROC   "upload"

#define TEMP_BUF_MAX_LEN 51200
#define FILE_NAME_LEN 256
#define USER_NAME_LEN 256

#define THREAD_COUNT 5  
static int counts[THREAD_COUNT];

#define _HOST_ "168.160.111.26"		//数据库所在服务器ip
#define _USER_ "tbs"  				//数据库用户
#define _PASSWD_ "tbs"
#define _DBNAME_ "fileInfo"


//FCGX_Request request;
static pthread_mutex_t pthread_mutex = PTHREAD_MUTEX_INITIALIZER;


/*
	功能：获取已经上传过后的文件列表,（文件名，MD5值）
	参数：
		filePath 	文件路径
		folderPath  文件夹路径
	返回值：成功0 失败返回-1
*/
int getChunkList(char *filePath, char *folderPath, FCGX_Request request)
{
	/*
		将目录下的所用文件组成一个文件列表，返回到前端	
	*/
	char fullpath[256];
	memset(fullpath, 0x00, 256);
	sprintf(fullpath, "%s/%s", filePath, folderPath);
		
    //创建一个文件jSON对象
    cJSON * allfile = cJSON_CreateArray();    
    cJSON * fullnode= cJSON_CreateObject();

    char * out = NULL;
    if(NULL == fullpath)
    {       
        FCGX_FPrintF(request.out,"%s", "enter path null!\n");
        return -1;
    }

    DIR *dirp = opendir(fullpath);	//打开目录
    if(dirp == NULL)
	{
        FCGX_FPrintF(request.out,"%s", "open dir err");
        return -1;
    }
        
	struct dirent * dentp = NULL;
	
	//读取目录下文件
    while((dentp = readdir(dirp)))
    {	      	
    	if((strcmp(".", dentp->d_name)) == 0 || (strcmp("..", dentp->d_name)) == 0)
        {
        	continue;
        }
    	
        //不是目录文件
        if(dentp->d_type == DT_REG)
    	{           
            struct stat filestruct;           
            char newfilepath[256];
            sprintf(newfilepath, "%s/%s", fullpath, dentp->d_name);
            stat(newfilepath, &filestruct);
            
            //创建一个jSON对象
            cJSON *chunkList = NULL;            
            if(strcmp(folderPath,dentp->d_name ) == 0)
            {
            	chunkList = cJSON_CreateObject();
            	cJSON_AddStringToObject(chunkList, "filename", dentp->d_name);
            	FCGX_FPrintF(request.out,"%s", cJSON_Print(chunkList));
            	return 0;
            }
            else
            {
            	cJSON_AddItemToArray(allfile, chunkList = cJSON_CreateObject());
            	cJSON_AddStringToObject(chunkList, "filename", dentp->d_name); 
            	cJSON_AddNumberToObject(chunkList, "indexof", atoi(dentp->d_name));             	
            }                                                              	                      
       	}   
       	else
       	{
       		return 0;	    		
       	}      		       
    }	
    cJSON_AddItemToObject(fullnode, "chunkList", allfile);  	 	
    FCGX_FPrintF(request.out,"%s", cJSON_Print(fullnode));
	return 0;	
}

/*
	判断文件夹是否存在，文件夹不存在创建文件夹，
	文件夹存在判断里面的内容是否正确，
	如果文件存在就组织文件的json格式返回给前端，
	如果文件不存在就返回目录下的所用文件返回给前端
*/

/*
	功能：判断文件夹是否存在，不存在创建一个文件夹
	
	参数：
		filepath   文件夹路径
		filename   文件夹名称
	
	返回值：成功返回0 失败返回-1
*/

int folderIsExit(char *filepath, char *filename, FCGX_Request request)
{		
	char fullpath[256];
	memset(fullpath, 0x00, sizeof(fullpath));
	
	sprintf(fullpath, "%s/%s", filepath, filename);
	
	if(access(fullpath, W_OK) == 0)
	{		
		//目录存在且有写的权限	遍历目录下所有文件组织文件链表
		getChunkList(filepath, filename,request);	
			
	}
	else
	{
		//目录不存在或没有写的权限	
		 mkdir(fullpath, S_IRWXU|S_IRWXG|S_IRWXO); //0777
		 cJSON *fileinfo = cJSON_CreateObject();
         cJSON_AddStringToObject(fileinfo, "fileName", filename);       
		 FCGX_FPrintF(request.out,"%s", cJSON_Print(fileinfo));
	}	
	return 0;	
}


/*
	功能：在指定长度的字符串中查找字符串
	参数：
		(in)	full_data   		 查找字符串
		(in)	full_data_len        查找长度长度
		(in)	substr               要查找的字符串
	返回值：成功返回要查找的字符串的初始位置 失败返回NULL	
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
 * @param file_name (out)   文件的文件名
 * @param p_size    (out)   文件大小
 *
 * @returns
 *          0 succ, -1 fail
 */
int recv_save_file(long len, char *user, char *filename, long *p_size, FCGX_Request request, char *filepath)
{	
	char log[526] = {0};
	memset(log,0x00, 526);
    int ret = 0;
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
	
    char content_text[TEMP_BUF_MAX_LEN] = {0}; 		//文件头部信息
    char boundary[TEMP_BUF_MAX_LEN] = {0};     		//分界线信息

    //==========> 开辟存放文件的 内存 <===========
    file_buf = (char *)malloc(len);
    memset(file_buf,0x00, sizeof(file_buf));
    
    if (file_buf == NULL)
    {
        //LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC, "malloc error! file size is to big!!!!\n");
        goto END;
    }
	  	
	  for(int i=0; i<len; i++)   //读取标准输入
	  {
	  	file_buf[i] = FCGX_GetChar(request.in);	  		
	  }
	  	  	
	//ret2 = fread(file_buf, 1, len, stdin); //从标准输入(web服务器)读取内容
	//if(ret2 == 0)
	//{    	
	//	LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC, "fread(file_buf, 1, len, stdin) err\n");
	//	ret = -1;
	//	goto END;
	//}
  
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
        //LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC,"wrong no boundary!\n");
        ret = -1;
        goto END;
    }

 		
    //拷贝分界线
    strncpy(boundary, begin, p-begin);
    boundary[p-begin] = '\0';   //字符串结束符
    //LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC,"boundary: [%s]\n", boundary);
    //sprintf(boundary, "ret2 = %d\n", ret2);
       
    p += 2;//\r\n
    //已经处理了p-begin的长度
    len -= (p-begin);

    //get content text head
    begin = p;

    //Content-Disposition: form-data; user="mike"; filename="xxx.jpg"; md5="xxxx"; size=10240\r\n
    p = strstr(begin, "\r\n");
    if(p == NULL)
    {
        //LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC,"ERROR: get context text error, no filename?\n");
        ret = -1;
       goto END;
    }
    strncpy(content_text, begin, p-begin); 				//拷贝Content-Disposition这一行内容
    content_text[p-begin] = '\0';
    //LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC,"content_text: [%s]\n", content_text);

    p += 2;//\r\n
    len -= (p-begin);

    //========================================获取文件上传者
    //Content-Disposition: form-data; user="mike"; filename="xxx.jpg"; md5="xxxx"; size=10240\r\n                                   ↑
    q = begin;
    q = strstr(begin, "name=");
    

    //Content-Disposition: form-data; user="mike"; filename="xxx.jpg"; md5="xxxx"; size=10240\r\n                                     ↑
    q += strlen("name=");
    q++;    //跳过第一个"

    //Content-Disposition: form-data; user="mike"; filename="xxx.jpg"; md5="xxxx"; size=10240\r\n                                          ↑
    k = strchr(q, '"');
    strncpy(user, q, k-q);  //拷贝用户名
    user[k-q] = '\0';
    
	//FCGI_printf("user = %s\n" , user);
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
 	 	
    begin = p;
    p = strstr(begin, "\r\n");
    p += 4;//\r\n\r\n
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
    
    //find file's end
    p = memstr(begin, len, boundary);//util_cgi.h， 找文件结尾
    if (p == NULL)
    {
        //LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC, "memstr(begin, len, boundary) error\n");
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

	//解析获的文件编号 获取总片数
	 heto = p + 2;
	 to = NULL;
	 to = strstr(heto, "\r\n");
	 to += 4;
	 
	 totallen = 0;
	 toen = strstr(to, "\r\n");
	 	 
	 toen += 4;
	 to = toen;
	 toen = strstr(to, "\r\n");
	 
	 totallen = toen - to;
	 	 
	 memset(FragmentNum, 0x00, sizeof(FragmentNum));
	 strncpy(FragmentNum, to, toen - to);  				//获取片段总数
	
	 num = atoi(FragmentNum);
	 toen+= 2;
	 to = toen;
	 
	 toen = strstr(to, "\r\n");
	 toen += 2;
	 
	 to = toen;
	 toen = strstr(to, "\r\n");
	 
	 toen += 4;
	 to = toen;
	 toen = strstr(to, "\r\n");
	 	 
	 memset(FragmentNum, 0x00, sizeof(FragmentNum));
	 strncpy(FragmentNum, to, toen - to);  
	 FragmentName = atoi(FragmentNum);
	
	 
    //begin---> file_len = (p-begin)

    //=====> 此时begin-->p两个指针的区间就是post的文件二进制数据
    //======>将数据写入文件中,其中文件名也是从post数据解析得来  <===========
	
	 
	  //fd = open(filename, O_CREAT|O_WRONLY, 0644);
	  
	  sprintf(FPATH, "%s/%s", filepath, FragmentNum);
	   	  
	  fd = open(FPATH, O_CREAT|O_WRONLY, 0644);
	  
	  if (fd < 0)
	  {
	      //LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC,"open %s error\n", filename);
	      ret = -1;
	      goto END;
	  }
	
	  //ftruncate会将参数fd指定的文件大小改为参数length指定的大小
	  ftruncate(fd, (p-begin));
	  write(fd, begin, (p-begin));
	  close(fd);
	
END:
    free(file_buf);
    FCGX_FFlush(request.in);
    return ret;
}

/*
	功能：获取文件大小
	参数：
		path  文件路径
	返回值：成功返回文件大小 失败返回-1
*/
int getfilesize(char *path)
{
	FILE *pf = fopen(path, "rb");
	if (pf==NULL)
	{
		return -1;
	} 
	else
	{
		fseek(pf, 0, SEEK_END);
		int size = ftell(pf);
		fclose(pf);
		return size;
	}
}



/*
	功能：将多个文件合并为一个文件, 同时删除片段文件
	参数：
		part_f_path  片段文件路径
		filepath     合并后文件路径
		filename	 合并后文件名
	返回值： 成功返回 0  失败返回-1

*/
int  merge(char *part_f_path , int part_num, char *filepath, char *filename, FCGX_Request request)
{
	char newpath[part_num][600]; 				//代表路径	
	for(int i=0; i<part_num; i++)
	{
		memset(newpath[i], 0x00, sizeof(newpath[i]));			
	}
	
	char fullpath[500] = {0};
	memset(fullpath, 0x00, sizeof(fullpath));
	sprintf(fullpath, "%s/%s",filepath, filename);
	for (int i = 0; i < part_num; i++)
	{
		sprintf(newpath[i], "%s/%d", part_f_path, i);
	}	
		
	FILE *pfw = fopen(fullpath, "wb");
	for (int i = 0; i < part_num; i++)
	{
		int length = getfilesize(newpath[i]);
		if (length != -1)
		{
			FILE *pfr = fopen(newpath[i], "rb"); //读取
			for (int j = 0; j < length; j++)
            {
				char ch = fgetc(pfr);
				fputc(ch, pfw);   				//写入
            }
			fclose(pfr);
			remove(newpath[i]);
		}
	}
	fclose(pfw);
	return 0;
}


/*
	功能：写log文件
	参数：
		logPath		log文件所在路径
		logName		log文件名
		log			要写的内容
		len			要写的内容长度
		level       错误等级
			#define DBG_INFOR       0x01  // call information  
			#define DBG_WARNING     0x02  // paramters invalid,    
			#define DBG_ERROR       0x04  // process error, leading to one call fails  
			#define DBG_CRITICAL    0x08  // process error, leading to voip process can't run exactly or exit  
	返回值：成功返回0 失败返回-1

*/

int write_log(char *logName, char * log, int len)
{
		char logPath[500];
		memset(logPath,0x00, sizeof(logPath));
		sprintf(logPath, "/home/tbs/TBSNative/TBSUploadFile/log/%s", logName);
		        	
    	time_t timer;     //time_t就是long int 类型
		struct tm *tblock;
		timer = time(NULL);
		tblock = localtime(&timer);
		FCGI_FILE *fp = NULL;	
		char time[500];
		memset(time,0x00, sizeof(time));
		sprintf(time, "%s", asctime(tblock));
		if(access(logPath, W_OK) != -1)
		{
			char n[] = "//n";
			FCGI_fopen(logPath, "ab+");	
			FCGI_fwrite(time, 1, strlen(time), fp);	
			FCGI_fwrite(log, 1, len, fp);
			FCGI_fwrite(n, 1, strlen(n), fp);	
			FCGI_fclose(fp);
		}
		else
		{		
			char n[] = "//n";	
			fp = FCGI_fopen(logPath, "wb+");
			FCGI_fwrite(time, 1, strlen(time), fp);	
			FCGI_fwrite(log, 1, len, fp);
			FCGI_fwrite(n, 1, strlen(n), fp);	
			FCGI_fclose(fp);				
		}
		return 0;
}
	

/*
	线程处理函数
*/
static void *doit(void *a)
{
    int rc, i, thread_id;
    thread_id = (int)((long)a);
    pid_t pid = getpid();
    char *server_name;   
    
    int count = 0;
	int flag = 0;
	
	long len;
    int ret = 0;	
    
    char filename[256] = {0}; //文件名
    char user[56] = {0};      //文件上传者
    long size = 0;
    	
	char get_filePath[20] = "filepath";
	char get_fileName[20] = "filename";
	char get_fileMd5Value[56] = "fileMd5Value";
	char get_chunks[56] = "chunks";
		
	FCGX_Request request;		
    FCGX_InitRequest(&request, 0, 0); //初始化request
	
    for (;;)
    {
        static pthread_mutex_t accept_mutex = PTHREAD_MUTEX_INITIALIZER;
        static pthread_mutex_t counts_mutex = PTHREAD_MUTEX_INITIALIZER;

        /* Some platforms require accept() serialization, some don't.. */        
        pthread_mutex_lock(&accept_mutex);
        rc = FCGX_Accept_r(&request); 
       	pthread_mutex_unlock(&accept_mutex);
        if (rc < 0)
            break;
            		
        server_name = FCGX_GetParam("SERVER_NAME", request.envp); 						//从环境变量中获取SERVER_NAME名                              
        FCGX_FPrintF(request.out,"Content-type: text/html; charset=UTF-8\r\n\r\n");				   	 
		char *contentLength = FCGX_GetParam("CONTENT_LENGTH", request.envp);	 		//获取环境变量	
		char *querystring = FCGX_GetParam("QUERY_STRING", request.envp);							
		char name[256] = "method";
		
		char *method = get_query_string(querystring, name);								
		char *filename = get_query_string(querystring, get_fileName);
		char *fileMd5Value = get_query_string(querystring, get_fileMd5Value);	
		char *chunks = 	get_query_string(querystring, get_chunks);	
													
		if(strcasecmp(method, "check") == 0)
		{	
			char filepath[256] = "../file";																				 									
			folderIsExit(filepath, fileMd5Value,request);
		}				
		else if(strcasecmp(method, "merge") == 0)
		{		
			 char filepath[600] ={0};	
			 int int_chunks  = atoi(chunks);	
	         sprintf(filepath, "../file/%s", fileMd5Value);	         
	         merge(filepath , int_chunks, filepath, filename, request);		               	
		}					
		else if(strcasecmp(method, "upload") == 0)
		{									
			if (contentLength != NULL)
	        {	        	
	        	time_t timer;     							//time_t就是long int 类型
      			struct tm *tblock;
      			timer = time(NULL);
      			tblock = localtime(&timer);
	        	
	        	len = strtol(contentLength, NULL, 10); 		//字符串转long， 或者atol
	        	cJSON *fileinfo = cJSON_CreateObject();
	        	cJSON_AddStringToObject(fileinfo, "fileName", filename);
	        	cJSON_AddStringToObject(fileinfo, "fileMd5Value", fileMd5Value);
	        	cJSON_AddNumberToObject(fileinfo, "len", len);
	        	cJSON_AddStringToObject(fileinfo, "time", asctime(tblock));
	        	FCGX_FPrintF(request.out,"%s", cJSON_Print(fileinfo));	
	        	
				//1. init 
			    MYSQL*mysql = mysql_init(NULL);

			    if(mysql == NULL)
			    {
			    	char err[] = "init err";
			    	write_log(FCGX_GetParam("SERVER_NAME",request.envp),err,strlen(err));
			        exit(1);
			    }	
			    	
			    //2. real_connect
			    mysql = mysql_real_connect(mysql,_HOST_,_USER_,_PASSWD_,_DBNAME_,0,NULL,0);	
			    		    
			    if(mysql == NULL)
			    {
			        //printf("connect err\n");
			        exit(1);
			    }			    
			    const char * csname = "utf8";
			     char rSql[500]={0};
			     
			    mysql_set_character_set(mysql, csname);			   
			    sprintf(rSql,"insert into fileTable values('%s','%s', '%s', '%ld')", filename,fileMd5Value,asctime(tblock),len );

			    if(mysql_query(mysql,rSql) != 0)
			    {
			    	//printf("mysql_query err\n");
			    	exit(1);
			    }
			    
			    //3. close
			    mysql_close(mysql);	       	        	        		        		          
	        }
	        else
	        {
	            len = 0;
	        }						
			if (len <= 0)
	        {
	            FCGI_printf("No data from standard input\n");            
	            ret = -1;
	        }
	        else
	        {	    
	        	char filepath[600] ={0};	
	         	sprintf(filepath, "../file/%s", fileMd5Value);	         	        		        		    
	            if (recv_save_file(len, user, filename, &size, request, filepath) < 0)
	            {	            	
	                ret = -1;
	                return 0;
	            }		                      	                        	         	            
			}				
		}  				 				          
        pthread_mutex_lock(&counts_mutex);
       	++counts[thread_id];
        pthread_mutex_unlock(&counts_mutex);    
        FCGX_Finish_r(&request);
    }
    
    return NULL;
}

int main()
{
	long i;  
		
    pthread_t id[THREAD_COUNT];    
    FCGX_Init();  
  	
    for (i = 1; i < THREAD_COUNT; i++)  
    {
    	pthread_create(&id[i], NULL, doit, (void*)i);  
        pthread_detach(id[i]);  
    }    	  
  	doit(0); 	
  		  		  		
     	//FCGI_printf("%s", "GHGGGGHGHGHGH");   
    	//for (i = 1; i < THREAD_COUNT; i++)      	
        //		pthread_join(id[i], NULL); 
                             
	return 0;	
}