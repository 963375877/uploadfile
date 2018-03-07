//好的习惯是成功的基础

#include "UploadFile.h"

// char* getcgidata(FILE* fp, char* requestmethod)   
//{   
//    char* input;   
//    int len;   
//    int size = 1024;   
//    int i = 0;   
//      
//    if (!strcmp(requestmethod, "GET"))   
//    {   
//                 input = getenv("QUERY_STRING");   
//                 return input;   
//    }   
//    else if (!strcmp(requestmethod, "POST"))   
//    {   
//         len = atoi(getenv("CONTENT_LENGTH"));   
//         input = (char*)malloc(sizeof(char)*(size + 1));   
//           
//         if (len == 0)   
//         {   
//            input[0] = '\0';   
//            return input;   
//         }   
//           
//         while(1)   
//         {   
//            input[i] = (char)fgetc(fp);   
//            if (i == size)   
//            {   
//                 input[i+1] = '\0';   
//                 return input;   
//            }   
//              
//            --len;   
//            if (feof(fp) || (!(len)))   
//            {   
//                 i++;   
//                 input[i] = '\0';   
//                 return input;   
//            }   
//            i++;                 
//         }   
//    }   
//    return NULL;  
//
//}
//

//
//int main(int argc, char* argv[])
//{
//	char *input;   
//    char *req_method;   
//    char name[64];   
//    char pass[64];   
//    int i = 0;   
//    int j = 0;   
//      
//	//printf("Content-type: text/plain; charset=iso-8859-1\n\n");   
//    FCIG_printf("Content-type: text/html\n\n");   
//    FCGI_printf("The following is query reuslt:<br><br>");   
//
//    req_method = getenv("REQUEST_METHOD");   
//    input = getcgidata(stdin, req_method);   
//
//    // 我们获取的input字符串可能像如下的形式   
//    // Username="admin"&Password="aaaaa"   
//    // 其中"Username="和"&Password="都是固定的   
//    // 而"admin"和"aaaaa"都是变化的，也是我们要获取的   
//      
//    // 前面9个字符是UserName=   
//    // 在"UserName="和"&"之间的是我们要取出来的用户名   
//    for ( i = 9; i < (int)strlen(input); i++ )   
//    {   
//         if ( input[i] == '&' )   
//         {   
//                name[j] = '\0';   
//                break;   
//         }                                       
//         name[j++] = input[i];   
//    }   
//
//    // 前面9个字符 + "&Password="10个字符 + Username的字符数   
//    // 是我们不要的，故省略掉，不拷贝   
//    for ( i = 19 + strlen(name), j = 0; i < (int)strlen(input); i++ )   
//    {   
//    	pass[j++] = input[i];   
//    } 
//    
//      
//    pass[j] = '\0';   
//    FCGI_printf("Your Username is %s<br>Your Password is %s<br> \n", name, pass);   
//      
//    return 0;   
//	
	
///
///int len = atoi(getenv("CONTENT_LENGTH"));

///char InputBuffer[4096] = {0};
///int i = 0; 
///int x;

///if(len < 0 || len >= MAX_CONTENT_LENGTH)
///	return;




///while( i < len )   
///{	 /*从stdin中得到Form数据*/    
///
///	x = FCGI_fgetc(stdin);  
///	if( x == EOF )	 
///		break; 	
///	InputBuffer[i++] = x;	 
	///	return 0;
//}	 

//InputBuffer[i] = '/0';	 
//
//len = i; 
//
//
//m_content = InputBuffer;
//	
//
//	return 0;
//}



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


int write_log(char * logPath, char *logName, char * log, int len)
{
		//char logPath[] = "/home/tbs/TBSNative/TBSUploadFile/src/log";
		FCGI_FILE *fp = NULL;
		if(access(logPath, W_OK) != -1)
		{
			FCGI_fopen(logPath, "ab+");
			FCGI_fwrite(log, 1, len, fp);
			FCGI_fclose(fp);
		}
		else
		{			
			fp = FCGI_fopen(logPath, "wb+");
			FCGI_fwrite(log, 1, len, fp);
			FCGI_fclose(fp);				
		}
		return 0;
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
 
int recv_save_file(long len, char *user, char *filename, long *p_size)
{
	
	char log[526] = {0};
	memset(log,0x00, 526);
    int ret = 0;
    char *file_buf = NULL;
    char *begin = NULL;
    char *p, *q, *k;

    char content_text[TEMP_BUF_MAX_LEN] = {0}; //文件头部信息
    char boundary[TEMP_BUF_MAX_LEN] = {0};     //分界线信息

    //==========> 开辟存放文件的 内存 <===========
    file_buf = (char *)malloc(len);
    
    if (file_buf == NULL)
    {
        //LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC, "malloc error! file size is to big!!!!\n");
        return -1;
    }

    int ret2 = fread(file_buf, 1, len, stdin); //从标准输入(web服务器)读取内容
    if(ret2 == 0)
    {    	
        //LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC, "fread(file_buf, 1, len, stdin) err\n");
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
    strncpy(content_text, begin, p-begin);
    content_text[p-begin] = '\0';
    //LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC,"content_text: [%s]\n", content_text);

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

    //begin---> file_len = (p-begin)

    //=====> 此时begin-->p两个指针的区间就是post的文件二进制数据
    //======>将数据写入文件中,其中文件名也是从post数据解析得来  <===========

    int fd = 0;
    fd = open(filename, O_CREAT|O_WRONLY, 0644);
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
    return ret;
}


int main()
{
    char filename[FILE_NAME_LEN] = {0}; //文件名
    char user[USER_NAME_LEN] = {0};   //文件上传者
    long size;  //文件大小

    while (FCGI_Accept() >= 0)
    {
        char *contentLength = getenv("CONTENT_LENGTH");
        long len;
        int ret = 0;		
 		
        FCGI_printf("Content-type: text/html\r\n\r\n");
   		
        if (contentLength != NULL)
        {
            len = strtol(contentLength, NULL, 10); //字符串转long， 或者atol
        }
        else
        {
            len = 0;
        }

        if (len <= 0)
        {
            FCGI_printf("No data from standard input\n");
            //LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC, "len = 0, No data from standard input\n");
            ret = -1;
        }
        else
        {
            //===============> 得到上传文件  <============
            if (recv_save_file(len, user, filename, &size) < 0)
            {
                ret = -1;
                goto END;
            }
			//LOG(UPLOAD_LOG_MODULE, UPLOAD_LOG_PROC, "%s成功上传[%s, 大小：%ld, md5码：%s]到本地\n", user, filename, size, md5);
       
END:
            memset(filename, 0, FILE_NAME_LEN);
            memset(user, 0, USER_NAME_LEN);

            char *out = NULL;
            //给前端返回，上传情况
            /*
               上传文件：
               成功：{"code":"008"}
               失败：{"code":"009"}
            */
            if(ret == 0) //成功上传
            {
                //out = return_status("008");//common.h
            }
            else//上传失败
            {
               // out = return_status("009");//common.h
            }

            if(out != NULL)
            {
                FCGI_printf(out); //给前端反馈信息
                free(out);   //记得释放
            }
        }
    } /* while */

    return 0;
}


