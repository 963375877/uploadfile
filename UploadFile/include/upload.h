
/*
	功能：获取已经上传过后的文件列表,（文件名，MD5值）
	参数：
		filePath 	(in)	文件路径
		folderPath  (in)	文件夹路径
	返回值：成功0 失败返回-1
*/
int getChunkList(char *filePath, char *folderPath, FCGX_Request request);


/*
	功能：判断文件夹是否存在，不存在创建一个文件夹	
	参数：
		filepath   (in)		文件夹路径
		filename   (in)		文件夹名称	
	返回值：成功返回0 失败返回-1
*/
int folderIsExit(char *filepath, char *filename, FCGX_Request request);


/*
	功能：在指定长度的字符串中查找字符串
	参数：
		full_data   	(in)	 查找字符串
		full_data_len   (in)     查找长度长度
		substr          (in)     要查找的字符串
	返回值：成功返回要查找的字符串的初始位置 失败返回NULL	
*/
char* memstr(char* full_data, int full_data_len, char* substr);


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
int recv_save_file(long len, char *user, char *filename, long *p_size, FCGX_Request request, char *filepath);


/*
	功能：获取文件大小
	参数：
		path  (in)	文件全路径
	返回值：成功返回文件大小 失败返回-1
*/
int getfilesize(char *path);

/*
	功能：将多个文件合并为一个文件, 同时删除片段文件
	参数：
		part_f_path  (in)	片段文件路径
		filepath     (in)	合并后文件路径
		filename	 (in)	合并后文件名
	返回值： 成功返回 0  失败返回-1

*/
int  merge(char *part_f_path , int part_num, char *filepath, char *filename, FCGX_Request request);


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
int write_log(char *logName, char * log, int len);


/**
 * 微信上传logo接口
 * @brief  解析上传的post数据 保存到本地临时路径
 *         同时得到文件上传者、文件名称、文件大小
 *
 * @param len       	(in)    post数据的长度
 * @param user      	(out)   文件上传者
 * @param filepath  	(out)   文件存放路径
 * @param file_name 	(out)   文件的文件名
 * @param p_size    	(out)   文件大小
 *
 * @returns
 *          0 succ, -1 fail
 */
 
int wx_recv_save_file(long len, char *user, char * filepath, char *filename, long *p_size, FCGX_Request request);

/*
	功能：将数据插入数据库中
	参数：
		host      (in)	端口
		user	  (in)	用户名
		psword	  (in)	密码
		dbname	  (in)	数据库名
		sql		  (in)	sql语句
	返回值：成功返回 0 失败返回-1
*/
int mysql(char * host, char *user, char *psword, char *dbname, char *sql,int len, FCGX_Request request);