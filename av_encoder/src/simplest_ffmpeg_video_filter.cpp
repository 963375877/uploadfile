/** 
 * 最简单的基于FFmpeg的AVFilter例子（叠加水印）
 * Simplest FFmpeg AVfilter Example (Watermark)
 *
 * 雷霄骅 Lei Xiaohua
 * leixiaohua1020@126.com
 * 中国传媒大学/数字电视技术
 * Communication University of China / Digital TV Technology
 * http://blog.csdn.net/leixiaohua1020
 * 
 * 本程序使用FFmpeg的AVfilter实现了视频的水印叠加功能。
 * 可以将一张PNG图片作为水印叠加到视频上。
 * 是最简单的FFmpeg的AVFilter方面的教程。
 * 适合FFmpeg的初学者。
 *
 * This software uses FFmpeg's AVFilter to add watermark in a video file.
 * It can add a PNG format picture as watermark to a video file.
 * It's the simplest example based on FFmpeg's AVFilter. 
 * Suitable for beginner of FFmpeg 
 *
 */
#include <stdio.h>
#include <stdlib.h>

#define __STDC_CONSTANT_MACROS

#ifdef _WIN32
#define snprintf _snprintf
//Windows
extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavfilter/avfiltergraph.h"
#include "libavfilter/buffersink.h"
#include "libavfilter/buffersrc.h"
#include "libavutil/avutil.h"
#include "libswscale/swscale.h"
#include "libswresample/swresample.h"
#include "SDL/SDL.h"
};
#else
//Linux...
#ifdef __cplusplus
extern "C"
{
#endif
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavfilter/avfiltergraph.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/avutil.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>
#include <SDL/SDL.h>
#include <errno.h>
#ifdef __cplusplus
};
#endif
#endif

//Output YUV data?
#define ENABLE_YUVFILE 1

#define INBUF_SIZE 4096
#define AUDIO_INBUF_SIZE 20480
#define AUDIO_REFILL_THRESH 4096
#define MAX_AUDIO_FRAME_SIZE 192000 // 1 second of 48khz 32bit audio

const char *filter_descr = "movie=my_logo.png[wm];[in][wm]overlay=5:5[out]";

static AVFormatContext *pFormatCtx;
static AVCodecContext *pCodecCtx;
AVFilterContext *buffersink_ctx;
AVFilterContext *buffersrc_ctx;
AVFilterGraph *filter_graph;
static int video_stream_index = -1;

int  width = 0;
int  height = 0;


//定义一个结构体
typedef struct yuv_info{
		int in_w;       //视频宽
		int in_h; 		//视频高	
		int framenum;	//视频，码率
		int duration;   //是以微秒为单位,视频总时长	
   }YUV_INFO;



static int open_input_file(const char *filename)
{
    int ret;
    AVCodec *dec;

    if ((ret = avformat_open_input(&pFormatCtx, filename, NULL, NULL)) < 0) {
        printf( "Cannot open input file\n");
        return ret;
    }

    if ((ret = avformat_find_stream_info(pFormatCtx, NULL)) < 0) {
        printf( "Cannot find stream information\n");
        return ret;
    }

    /* select the video stream */
    ret = av_find_best_stream(pFormatCtx, AVMEDIA_TYPE_VIDEO, -1, -1, &dec, 0);
    if (ret < 0) {
        printf( "Cannot find a video stream in the input file\n");
        return ret;
    }
    video_stream_index = ret;
    pCodecCtx = pFormatCtx->streams[video_stream_index]->codec;

    /* init the video decoder */
    if ((ret = avcodec_open2(pCodecCtx, dec, NULL)) < 0) {
        printf( "Cannot open video decoder\n");
        return ret;
    }

    return 0;
}

static int init_filters(const char *filters_descr)
{
    char args[512];
    int ret;
    AVFilter *buffersrc  = avfilter_get_by_name("buffer");
    AVFilter *buffersink = avfilter_get_by_name("buffersink");
    AVFilterInOut *outputs = avfilter_inout_alloc();
    AVFilterInOut *inputs  = avfilter_inout_alloc();
    enum AVPixelFormat pix_fmts[] = { AV_PIX_FMT_YUV420P, AV_PIX_FMT_NONE };
    AVBufferSinkParams *buffersink_params;

    filter_graph = avfilter_graph_alloc();

    /* buffer video source: the decoded frames from the decoder will be inserted here. */
    snprintf(args, sizeof(args),
            "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
            pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt,
            pCodecCtx->time_base.num, pCodecCtx->time_base.den,
            pCodecCtx->sample_aspect_ratio.num, pCodecCtx->sample_aspect_ratio.den);

    ret = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in",
                                       args, NULL, filter_graph);
    if (ret < 0) {
        printf("Cannot create buffer source\n");
        return ret;
    }

    /* buffer video sink: to terminate the filter chain. */
    buffersink_params = av_buffersink_params_alloc();
    buffersink_params->pixel_fmts = pix_fmts;
    ret = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out",
                                       NULL, buffersink_params, filter_graph);
    av_free(buffersink_params);
    if (ret < 0) {
        printf("Cannot create buffer sink\n");
        return ret;
    }

    /* Endpoints for the filter graph. */
    outputs->name       = av_strdup("in");
    outputs->filter_ctx = buffersrc_ctx;
    outputs->pad_idx    = 0;
    outputs->next       = NULL;

    inputs->name       = av_strdup("out");
    inputs->filter_ctx = buffersink_ctx;
    inputs->pad_idx    = 0;
    inputs->next       = NULL;

    if ((ret = avfilter_graph_parse_ptr(filter_graph, filters_descr,
                                    &inputs, &outputs, NULL)) < 0)
        return ret;

    if ((ret = avfilter_graph_config(filter_graph, NULL)) < 0)
        return ret;
    return 0;
}
/**------------------------------------------------------------------------------------------------**/

/*
首先处理原来视频文件，将原视频分为两个yuv,然后处理要插入的视频文件，
然后将要插入的视频追加到原始视频yuv文件后，
原视频文件路径
要插入的视频文件路径
新的视频路径
插入的的时间点
获取视频的码率，获取视频的宽高，
插入时间点，结束时间点，单独函数，用户选择时间点转换为时间，从第几秒开始，到第几秒结束，后面追加文件
流程步骤
1、将原视频转换为yuv文件
2、将插入的视频转换为yuv文件
3、将插入的视频追加到原视频中
4、将新视频封装为MP4文件
*/
/*
	功能：将点分十进制时长转换为秒数
	参数：
		time 传入时长 (00:10:00 转换为600秒)
	返回值：返回秒数
*/
int time_to_ms(char * time)
{
	int len = 0;
	int total = 0;
	char hour[10];
	char minute[10];
	char second[10];
	char *h, *m;
	memset(hour, 0x00, strlen(hour));
	memset(minute, 0x00, strlen(minute));
	memset(second, 0x00, strlen(second));
		
	h = strchr(time, ':');
	h++;	
	
	m = strchr(h, ':');
	m++;
		
	len = abs(h - time);
	snprintf(hour, len, time);
	
	total += atoi(hour) * 3600;
	
	len = abs(h - m);
	snprintf(minute, len, h);
	
	total += atoi(minute) * 60;
	
	snprintf(second, sizeof(m), m);
	total += atoi(second);
			
	return total;	
}


/*
	功能：刷新编码器
	参数：
	返回值：成功返回0，失败返回-1

*/
int flush_encoder(AVFormatContext *fmt_ctx,unsigned int stream_index){
	int ret;
	int got_frame;
	AVPacket enc_pkt;
	if (!(fmt_ctx->streams[stream_index]->codec->codec->capabilities &
		CODEC_CAP_DELAY))
		return 0;
	while (1) {
		enc_pkt.data = NULL;
		enc_pkt.size = 0;
		av_init_packet(&enc_pkt);
		ret = avcodec_encode_video2 (fmt_ctx->streams[stream_index]->codec, &enc_pkt, NULL, &got_frame);
		av_frame_free(NULL);
		if (ret < 0)
			break;
		if (!got_frame){
			ret=0;
			break;
		}
		printf("Flush Encoder: Succeed to encode 1 frame!\tsize:%5d\n",enc_pkt.size);
		/* mux encoded frame */
		ret = av_write_frame(fmt_ctx, &enc_pkt);
		if (ret < 0)
			break;
	}
	return ret;
}


/*
	功能: 将yuv文件转换为h264文件
	参数：
		yuv_file         (in)            要转换的yuv文件，输入文件
		h264_file        (in)            转换后的h264文件，输出文件
		in_w             (in)            yuv文件的宽
		in_h             (in)            yuv文件的高
		framenum         (in)            要转的总帧数
		
	返回值：成功返回0，失败返回-1
*/
int yuv_to_h264(const char *yuv_file, char *h264_file, int in_w, int in_h, int framenum)
{
	AVFormatContext* pFormatCtx;
	AVOutputFormat* fmt;
	AVStream* video_st;
	AVCodecContext* pCodecCtx;
	AVCodec* pCodec;
	AVPacket pkt;
	uint8_t* picture_buf;
	AVFrame* pFrame;
	int picture_size;
	int y_size;
	int framecnt=0;
	
	FILE *in_file = fopen(yuv_file, "rb");     //Input raw YUV data
	//int in_w=480,in_h=272;                              //Input data's width and height	
	//int framenum=100;                                     //Frames to encode	
	//const char* out_file = "ds.h264";                	 //Output Filepath 

	av_register_all();
	
	//Method1.
	pFormatCtx = avformat_alloc_context();
	
	//Guess Format
	fmt = av_guess_format(NULL, h264_file, NULL);
	pFormatCtx->oformat = fmt;
	
	//Method 2.
	//avformat_alloc_output_context2(&pFormatCtx, NULL, NULL, h264_file);
	//fmt = pFormatCtx->oformat;


	//Open output URL
	if (avio_open(&pFormatCtx->pb,h264_file, AVIO_FLAG_READ_WRITE) < 0){
		printf("Failed to open output file! \n");
		return -1;
	}

	video_st = avformat_new_stream(pFormatCtx, 0);
	//video_st->time_base.num = 1; 
	//video_st->time_base.den = 25;  

	if (video_st==NULL){
		return -1;
	}
	//Param that must set
	pCodecCtx = video_st->codec;
	//pCodecCtx->codec_id =AV_CODEC_ID_HEVC;
	pCodecCtx->codec_id = fmt->video_codec;
	pCodecCtx->codec_type = AVMEDIA_TYPE_VIDEO;
	pCodecCtx->pix_fmt = AV_PIX_FMT_YUV420P;
	pCodecCtx->width = in_w;  
	pCodecCtx->height = in_h;
	pCodecCtx->bit_rate = 400000;  
	pCodecCtx->gop_size=250;

	pCodecCtx->time_base.num = 1;  
	pCodecCtx->time_base.den = 25;  

	//H264
	//pCodecCtx->me_range = 16;
	//pCodecCtx->max_qdiff = 4;
	//pCodecCtx->qcompress = 0.6;
	pCodecCtx->qmin = 10;
	pCodecCtx->qmax = 51;

	//Optional Param
	pCodecCtx->max_b_frames=3;

	// Set Option
	AVDictionary *param = 0;
	//H.264
	if(pCodecCtx->codec_id == AV_CODEC_ID_H264) {
		av_dict_set(&param, "preset", "slow", 0);
		av_dict_set(&param, "tune", "zerolatency", 0);
		//av_dict_set(&param, "profile", "main", 0);
	}
	//H.265
	if(pCodecCtx->codec_id == AV_CODEC_ID_H265){
		av_dict_set(&param, "preset", "ultrafast", 0);
		av_dict_set(&param, "tune", "zero-latency", 0);
	}

	//Show some Information
	av_dump_format(pFormatCtx, 0, h264_file, 1);

	pCodec = avcodec_find_encoder(pCodecCtx->codec_id);
	if (!pCodec){
		printf("Can not find encoder! \n");
		return -1;
	}
	if (avcodec_open2(pCodecCtx, pCodec,&param) < 0){
		printf("Failed to open encoder! \n");
		return -1;
	}


	pFrame = av_frame_alloc();
	picture_size = avpicture_get_size(pCodecCtx->pix_fmt, pCodecCtx->width, pCodecCtx->height);
	picture_buf = (uint8_t *)av_malloc(picture_size);
	avpicture_fill((AVPicture *)pFrame, picture_buf, pCodecCtx->pix_fmt, pCodecCtx->width, pCodecCtx->height);

	//Write File Header
	avformat_write_header(pFormatCtx,NULL);

	av_new_packet(&pkt,picture_size);

	y_size = pCodecCtx->width * pCodecCtx->height;

	for (int i=0; i<framenum; i++){
		//Read raw YUV data
		if (fread(picture_buf, 1, y_size*3/2, in_file) <= 0){
			printf("Failed to read raw data! \n");
			return -1;
		}else if(feof(in_file)){
			break;
		}
		pFrame->data[0] = picture_buf;              // Y
		pFrame->data[1] = picture_buf+ y_size;      // U 
		pFrame->data[2] = picture_buf+ y_size*5/4;  // V
		//PTS
		//pFrame->pts=i;
		pFrame->pts=i*(video_st->time_base.den)/((video_st->time_base.num)*25);
		int got_picture=0;
		//Encode
		int ret = avcodec_encode_video2(pCodecCtx, &pkt,pFrame, &got_picture);
		if(ret < 0){
			printf("Failed to encode! \n");
			return -1;
		}
		if (got_picture==1){
			//printf("Succeed to encode frame: %5d\tsize:%5d\n",framecnt,pkt.size);
			framecnt++;
			pkt.stream_index = video_st->index;
			ret = av_write_frame(pFormatCtx, &pkt);
			av_free_packet(&pkt);
		}
	}
	//Flush Encoder
	int ret = flush_encoder(pFormatCtx,0);
	if (ret < 0) {
		printf("Flushing encoder failed\n");
		return -1;
	}

	//Write file trailer
	av_write_trailer(pFormatCtx);

	//Clean
	if (video_st){
		avcodec_close(video_st->codec);
		av_free(pFrame);
		av_free(picture_buf);
	}
	avio_close(pFormatCtx->pb);
	avformat_free_context(pFormatCtx);

	fclose(in_file);

	return 0;
}


/*
	功能：获取视频信息
	参数：
		input_path       (in)      多媒体文件
		yuv_info         (out)     文件信息结构体
	返回值：成功返回0， 失败返回 -1
*/
int get_video_info(char *input_path, YUV_INFO * yuv_info)
{
	int bit_rate = 0;
    //1、注册所有组件
    av_register_all();	
	
    //2、打开视频文件
    AVFormatContext *pFormatCtx = avformat_alloc_context();
    if ((avformat_open_input(&pFormatCtx, input_path, NULL, NULL)) < 0) 
    {
    	av_log(pFormatCtx,AV_LOG_ERROR,"Cannot open input file.\n");       
        return -1;
    }
    
    //3、获取视频信息
    if (avformat_find_stream_info(pFormatCtx, NULL) < 0) 
    {
    	av_log(pFormatCtx,AV_LOG_ERROR,"Cannot find stream.\n");        
        return -1;
    }
    
    //printf("AVFormatContext -> duration = %d", pFormatCtx->duration/1000000);  //duration是以微秒为单位
    
    //4、找到视频流的位置
    int video_stream_index = -1, audio_stream_index = -1;
    
    int i = 0;
    for (; i < pFormatCtx->nb_streams; i++) 
    {
        if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) 
        {
            video_stream_index = i;
            av_log(pFormatCtx, AV_LOG_ERROR, "find the stream index %d.\n", video_stream_index);           
            //break;
        }
        if(pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO)
        {
        	audio_stream_index = i;      	
        }
    }
    
    //5、获取解码器
    AVCodecContext *pCodeCtx = pFormatCtx->streams[video_stream_index]->codec;
    yuv_info->in_w = pCodeCtx->width;
    yuv_info->in_h = pCodeCtx->height;
    yuv_info->duration = pFormatCtx->duration; //duration是以微秒为单位
   // printf("duration = %d\n",pFormatCtx->streams[audio_stream_index]->codec);
    
    
    //获取帧率,获取小数，进位
  	AVStream *stream = pFormatCtx->streams[video_stream_index];  
  	AVStream *audio_stream = pFormatCtx->streams[audio_stream_index];
  	int frame_rate_mold = (int)(((float)stream->avg_frame_rate.num/(float)stream->avg_frame_rate.den)*10.0)%10;
  	int frame_rate = stream->avg_frame_rate.num/stream->avg_frame_rate.den; 		//每秒多少帧 
  	if(frame_rate_mold > 0)
  	{
  		frame_rate += 1;
  	}
  	
  	//核心错误
  	//printf("audio_stream.avg_frame_rate.den = %d  audio_stream->avg_frame_rate.num = %d audio_stream->avg_frame_rate.nb_samples = %d\n", audio_stream->avg_frame_rate.den,audio_stream->avg_frame_rate.num,pCodeCtx->coded_frame->nb_samples);
  	//int frame_rate = stream->avg_frame_rate.num; //一秒的帧数
  	//stream->avg_frame_rate.den 时间 码率 = stream->avg_frame_rate.den/stream->avg_frame_rate.num;
  	
	yuv_info->framenum = frame_rate;	
	
	
	return 0;	
}


/*
	功能：将MP4文件转换为yuv文件
	参数：
		input_path    (in)		输入文件
		output_path   (in)		输出文件
		start_time    (in)      开始时间
		end_time      (in)      结束时间
	返回值：成功返回0 失败返回-1
*/
int mp4_to_yuv(char *input_path, char *output_path, int start_time, int end_time) 
{   
	int bit_rate = 0;
    //1、注册所有组件
    av_register_all();	
	
    //2、打开视频文件
    AVFormatContext *pFormatCtx = avformat_alloc_context();
    if ((avformat_open_input(&pFormatCtx, input_path, NULL, NULL)) < 0) 
    {
    	av_log(pFormatCtx,AV_LOG_ERROR,"Cannot open input file.\n");       
        return -1;
    }
    
    //3、获取视频信息
    if (avformat_find_stream_info(pFormatCtx, NULL) < 0) 
    {
    	av_log(pFormatCtx,AV_LOG_ERROR,"Cannot find stream.\n");        
        return -1;
    }
    
    //4、找到视频流的位置
    int video_stream_index = -1;
    int i = 0;
    for (; i < pFormatCtx->nb_streams; i++) 
    {
        if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) 
        {
            video_stream_index = i;
            av_log(pFormatCtx, AV_LOG_ERROR, "find the stream index %d.\n", video_stream_index);           
            break;
        }
    }
    
    //5、获取解码器
    AVCodecContext *pCodeCtx = pFormatCtx->streams[video_stream_index]->codec;
    width = pCodeCtx->width;
    height = pCodeCtx->height;
    
    //获取帧率,获取小数，进位
  	AVStream *stream=pFormatCtx->streams[video_stream_index];  
  	int frame_rate_mold = (int)(((float)stream->avg_frame_rate.num/(float)stream->avg_frame_rate.den)*10.0)%10;
  	int frame_rate=stream->avg_frame_rate.num/stream->avg_frame_rate.den; 		//每秒多少帧 
  	if(frame_rate_mold > 0)
  	{
  		frame_rate += 1;
  	}
  	start_time *= frame_rate;
  	end_time *= frame_rate;
  	if(end_time == 0)
  	{
  		end_time = 2147483647;
  	}
  	
  	
   
    AVCodec *pCodec = avcodec_find_decoder(pCodeCtx->codec_id);
    if (pCodec == NULL) 
    {
    	av_log(pFormatCtx, AV_LOG_ERROR, "Cannot find decoder.\n");     
        return -1;
    }
    
    //6、打开解码器
    if (avcodec_open2(pCodeCtx, pCodec, NULL) < 0) 
    {
    	av_log(pFormatCtx, AV_LOG_ERROR, "Cannot open codec.\n");     
        return -1;
    }
    
    //7、解析每一帧数据
    int got_picture_ptr, frame_count = 1;
    
    //压缩数据
    AVPacket *packet = (AVPacket *) av_malloc(sizeof(AVPacket));
    
    //解压缩数据
    AVFrame *frame = av_frame_alloc();
    AVFrame *yuvFrame = av_frame_alloc();
    
    //将视频转换成指定的420P的YUV格式
    //缓冲区分配内存
    uint8_t *out_buffer = (uint8_t *) av_malloc(avpicture_get_size(AV_PIX_FMT_YUV420P, pCodeCtx->width, pCodeCtx->height));
    
    //初始化缓冲区
    avpicture_fill((AVPicture *) yuvFrame, out_buffer, AV_PIX_FMT_YUV420P, pCodeCtx->width,
                   pCodeCtx->height);
    //用于像素格式转换或者缩放
    struct SwsContext *sws_ctx = sws_getContext(pCodeCtx->width, pCodeCtx->height, pCodeCtx->pix_fmt,pCodeCtx->width, pCodeCtx->height, AV_PIX_FMT_YUV420P, SWS_BILINEAR, NULL, NULL, NULL);
    
    //输出文件
    FILE *fp_yuv = fopen(output_path, "ab");
    
    //一帧一帧读取压缩的视频数据
    while (av_read_frame(pFormatCtx, packet) >= 0 ) 
    {
        //找到视频流
        if (packet->stream_index == video_stream_index) 
        {        	
            avcodec_decode_video2(pCodeCtx, frame, &got_picture_ptr, packet);
            if(frame_count >= start_time && frame_count <= end_time)  //在这里控制视频 多少秒
            {
	            //正在解码
	            if (got_picture_ptr) 
	            {
	                //frame->yuvFrame，转为指定的YUV420P像素帧
	                sws_scale(sws_ctx, (const uint8_t *const *) frame->data, frame->linesize, 0,
	                          frame->height, yuvFrame->data, yuvFrame->linesize);
	                //计算视频数据总大小
	                int y_size = pCodeCtx->width * pCodeCtx->height;
	                //AVFrame->YUV，由于YUV的比例是4:1:1
	                fwrite(yuvFrame->data[0], 1, y_size, fp_yuv);
	                fwrite(yuvFrame->data[1], 1, y_size / 4, fp_yuv);
	                fwrite(yuvFrame->data[2], 1, y_size / 4, fp_yuv);	                             
	            }
            }    
            else if((0 == start_time) && (0 == end_time))
            {
            	//正在解码
	            if (got_picture_ptr) 
	            {
	                //frame->yuvFrame，转为指定的YUV420P像素帧
	                sws_scale(sws_ctx, (const uint8_t *const *) frame->data, frame->linesize, 0,
	                          frame->height, yuvFrame->data, yuvFrame->linesize);
	                //计算视频数据总大小
	                int y_size = pCodeCtx->width * pCodeCtx->height;
	                //AVFrame->YUV，由于YUV的比例是4:1:1
	                fwrite(yuvFrame->data[0], 1, y_size, fp_yuv);
	                fwrite(yuvFrame->data[1], 1, y_size / 4, fp_yuv);
	                fwrite(yuvFrame->data[2], 1, y_size / 4, fp_yuv);	                             
	            }            	           	
            }
                    	
            av_log(pFormatCtx, AV_LOG_ERROR, "解析第%d帧,平均码率为%d, stream->avg_frame_rate.num = %d, stream->avg_frame_rate.den=%d\n", (frame_count++), frame_rate,stream->avg_frame_rate.num,stream->avg_frame_rate.den);
            //释放packet
            av_packet_unref(packet);
        }         
    }  
         
    //8、释放资源
    fclose(fp_yuv);
    av_frame_free(&frame);
    avcodec_close(pCodeCtx);
    avformat_free_context(pFormatCtx);
}


/*
	功能：将视频文件转换为yuv文件，拼接两yuv文件成一个新的yuv文件
	参数：
		original       (in)       原视频文件
		invideo        (in)       要插入的视频文件
		newvideo       (in)       生成的新的文件名
		start_time     (in)       插入的起始时间
		end_time       (in)       插入的结束时间		
	返回值：成功返回0，失败返回-1
*/
int Splice_video(char *original, char *invideo, char *newvideo, int start_time, int end_time)
{
	char input_path[200] = {0};
	char output_path[200] = {0};
	
	////1、将于原视频转换为yuv
	mp4_to_yuv(original, newvideo, start_time, end_time); 
	
	//2、将插入视频转换为yuv文件
	mp4_to_yuv(invideo, newvideo, 0, 0); 
	
	//3、将源文件后半段追加到新文件中
	mp4_to_yuv(original, newvideo, end_time, 0); 
	
	
	//2、
	return 0;	
}


/*
	功能：将h264视频流和aac音频封装为 MP4文件
	参数：
		h264_file        (in)        h264文件
		aac_file         (in)        aac文件
		mp4_file         (in)        输出的MP4文件
	返回值：成功返回0，失败返回 -1
*/
int h264_aac_to_mp4(char* h264_file, char* aac_file, char *mp4_file)
{
	AVOutputFormat *ofmt = NULL;
	//Input AVFormatContext and Output AVFormatContext
	AVFormatContext *ifmt_ctx_v = NULL, *ifmt_ctx_a = NULL,*ofmt_ctx = NULL;
	AVPacket pkt;
	int ret, i;
	int videoindex_v=-1,videoindex_out=-1;
	int audioindex_a=-1,audioindex_out=-1;
	int frame_index=0;
	int64_t cur_pts_v=0,cur_pts_a=0;
	
	av_register_all();
	
	//Input
	if ((ret = avformat_open_input(&ifmt_ctx_v, h264_file, 0, 0)) < 0) {
		printf( "Could not open input file.");
		//goto end
		avformat_close_input(&ifmt_ctx_v);
		avformat_close_input(&ifmt_ctx_a);
		/* close output */
		if (ofmt_ctx && !(ofmt->flags & AVFMT_NOFILE))
			avio_close(ofmt_ctx->pb);
		avformat_free_context(ofmt_ctx);
		if (ret < 0 && ret != AVERROR_EOF) {
			printf( "Error occurred.\n");
			return -1;
		}
	}
	if ((ret = avformat_find_stream_info(ifmt_ctx_v, 0)) < 0) {
		printf( "Failed to retrieve input stream information");
		//goto end
		avformat_close_input(&ifmt_ctx_v);
		avformat_close_input(&ifmt_ctx_a);
		/* close output */
		if (ofmt_ctx && !(ofmt->flags & AVFMT_NOFILE))
			avio_close(ofmt_ctx->pb);
		avformat_free_context(ofmt_ctx);
		if (ret < 0 && ret != AVERROR_EOF) {
			printf( "Error occurred.\n");
			return -1;
		}
	}

	if ((ret = avformat_open_input(&ifmt_ctx_a, aac_file, 0, 0)) < 0) {
		printf( "Could not open input file.");
		//goto end
		avformat_close_input(&ifmt_ctx_v);
		avformat_close_input(&ifmt_ctx_a);
		/* close output */
		if (ofmt_ctx && !(ofmt->flags & AVFMT_NOFILE))
			avio_close(ofmt_ctx->pb);
		avformat_free_context(ofmt_ctx);
		if (ret < 0 && ret != AVERROR_EOF) {
			printf( "Error occurred.\n");
			return -1;
		}
	}
	if ((ret = avformat_find_stream_info(ifmt_ctx_a, 0)) < 0) {
		printf( "Failed to retrieve input stream information");
		//goto end
		avformat_close_input(&ifmt_ctx_v);
		avformat_close_input(&ifmt_ctx_a);
		/* close output */
		if (ofmt_ctx && !(ofmt->flags & AVFMT_NOFILE))
			avio_close(ofmt_ctx->pb);
		avformat_free_context(ofmt_ctx);
		if (ret < 0 && ret != AVERROR_EOF) {
			printf( "Error occurred.\n");
			return -1;
		}
	}
	//printf("===========Input Information==========\n");
	av_dump_format(ifmt_ctx_v, 0, h264_file, 0);
	av_dump_format(ifmt_ctx_a, 0, aac_file, 0);
	//printf("======================================\n");
	//Output
	avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, mp4_file);
	if (!ofmt_ctx) {
		printf( "Could not create output context\n");
		ret = AVERROR_UNKNOWN;
		//goto end
		avformat_close_input(&ifmt_ctx_v);
		avformat_close_input(&ifmt_ctx_a);
		/* close output */
		if (ofmt_ctx && !(ofmt->flags & AVFMT_NOFILE))
			avio_close(ofmt_ctx->pb);
		avformat_free_context(ofmt_ctx);
		if (ret < 0 && ret != AVERROR_EOF) {
			printf( "Error occurred.\n");
			return -1;
		}
	}
	ofmt = ofmt_ctx->oformat;

	for (i = 0; i < ifmt_ctx_v->nb_streams; i++) {
		//Create output AVStream according to input AVStream
		if(ifmt_ctx_v->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO){
		AVStream *in_stream = ifmt_ctx_v->streams[i];
		AVStream *out_stream = avformat_new_stream(ofmt_ctx, in_stream->codec->codec);
		videoindex_v=i;
		if (!out_stream) {
			printf( "Failed allocating output stream\n");
			ret = AVERROR_UNKNOWN;
			//goto end
			avformat_close_input(&ifmt_ctx_v);
			avformat_close_input(&ifmt_ctx_a);
			/* close output */
			if (ofmt_ctx && !(ofmt->flags & AVFMT_NOFILE))
				avio_close(ofmt_ctx->pb);
			avformat_free_context(ofmt_ctx);
			if (ret < 0 && ret != AVERROR_EOF) {
				printf( "Error occurred.\n");
				return -1;
			}
		}
		videoindex_out=out_stream->index;
		//Copy the settings of AVCodecContext
		if (avcodec_copy_context(out_stream->codec, in_stream->codec) < 0) {
			printf( "Failed to copy context from input to output stream codec context\n");
			//goto end
			avformat_close_input(&ifmt_ctx_v);
			avformat_close_input(&ifmt_ctx_a);
			/* close output */
			if (ofmt_ctx && !(ofmt->flags & AVFMT_NOFILE))
				avio_close(ofmt_ctx->pb);
			avformat_free_context(ofmt_ctx);
			if (ret < 0 && ret != AVERROR_EOF) {
				printf( "Error occurred.\n");
				return -1;
			}
		}
		out_stream->codec->codec_tag = 0;
		if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
			out_stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
		break;
		}
	}

	for (i = 0; i < ifmt_ctx_a->nb_streams; i++) {
		//Create output AVStream according to input AVStream
		if(ifmt_ctx_a->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO){
			AVStream *in_stream = ifmt_ctx_a->streams[i];
			AVStream *out_stream = avformat_new_stream(ofmt_ctx, in_stream->codec->codec);
			audioindex_a=i;
			if (!out_stream) {
				printf( "Failed allocating output stream\n");
				ret = AVERROR_UNKNOWN;
				//goto end
				avformat_close_input(&ifmt_ctx_v);
				avformat_close_input(&ifmt_ctx_a);
				/* close output */
				if (ofmt_ctx && !(ofmt->flags & AVFMT_NOFILE))
					avio_close(ofmt_ctx->pb);
				avformat_free_context(ofmt_ctx);
				if (ret < 0 && ret != AVERROR_EOF) {
					printf( "Error occurred.\n");
					return -1;
				}
			}
			audioindex_out=out_stream->index;
			//Copy the settings of AVCodecContext
			if (avcodec_copy_context(out_stream->codec, in_stream->codec) < 0) {
				printf( "Failed to copy context from input to output stream codec context\n");
				//goto end
				avformat_close_input(&ifmt_ctx_v);
				avformat_close_input(&ifmt_ctx_a);
				/* close output */
				if (ofmt_ctx && !(ofmt->flags & AVFMT_NOFILE))
					avio_close(ofmt_ctx->pb);
				avformat_free_context(ofmt_ctx);
				if (ret < 0 && ret != AVERROR_EOF) {
					printf( "Error occurred.\n");
					return -1;
				}
			}
			out_stream->codec->codec_tag = 0;
			if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
				out_stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;

			break;
		}
	}

	//printf("==========Output Information==========\n");
	av_dump_format(ofmt_ctx, 0, mp4_file, 1);
	//printf("======================================\n");
	//Open output file
	if (!(ofmt->flags & AVFMT_NOFILE)) {
		if (avio_open(&ofmt_ctx->pb, mp4_file, AVIO_FLAG_WRITE) < 0) {
			printf( "Could not open output file '%s'", mp4_file);
			//goto end
			avformat_close_input(&ifmt_ctx_v);
			avformat_close_input(&ifmt_ctx_a);
			/* close output */
			if (ofmt_ctx && !(ofmt->flags & AVFMT_NOFILE))
				avio_close(ofmt_ctx->pb);
			avformat_free_context(ofmt_ctx);
			if (ret < 0 && ret != AVERROR_EOF) {
				printf( "Error occurred.\n");
				return -1;
			}
		}
	}
	//Write file header
	if (avformat_write_header(ofmt_ctx, NULL) < 0) {
		printf( "Error occurred when opening output file\n");
		
		avformat_close_input(&ifmt_ctx_v);
		avformat_close_input(&ifmt_ctx_a);
		/* close output */
		if (ofmt_ctx && !(ofmt->flags & AVFMT_NOFILE))
			avio_close(ofmt_ctx->pb);
		avformat_free_context(ofmt_ctx);
		if (ret < 0 && ret != AVERROR_EOF) {
			printf( "Error occurred.\n");
			return -1;
		}		
	}

	//FIX

	AVBitStreamFilterContext* h264bsfc =  av_bitstream_filter_init("h264_mp4toannexb"); 
	AVBitStreamFilterContext* aacbsfc =  av_bitstream_filter_init("aac_adtstoasc"); 

	while (1) {
		AVFormatContext *ifmt_ctx;
		int stream_index=0;
		AVStream *in_stream, *out_stream;

		//Get an AVPacket
		if(av_compare_ts(cur_pts_v,ifmt_ctx_v->streams[videoindex_v]->time_base,cur_pts_a,ifmt_ctx_a->streams[audioindex_a]->time_base) <= 0){
			ifmt_ctx=ifmt_ctx_v;
			stream_index=videoindex_out;

			if(av_read_frame(ifmt_ctx, &pkt) >= 0){
				do{
					in_stream  = ifmt_ctx->streams[pkt.stream_index];
					out_stream = ofmt_ctx->streams[stream_index];

					if(pkt.stream_index==videoindex_v){
						//FIX：No PTS (Example: Raw H.264)
						//Simple Write PTS
						if(pkt.pts==AV_NOPTS_VALUE){
							//Write PTS
							AVRational time_base1=in_stream->time_base;
							//Duration between 2 frames (us)
							int64_t calc_duration=(double)AV_TIME_BASE/av_q2d(in_stream->r_frame_rate);
							//Parameters
							pkt.pts=(double)(frame_index*calc_duration)/(double)(av_q2d(time_base1)*AV_TIME_BASE);
							pkt.dts=pkt.pts;
							pkt.duration=(double)calc_duration/(double)(av_q2d(time_base1)*AV_TIME_BASE);
							frame_index++;
						}

						cur_pts_v=pkt.pts;
						break;
					}
				}while(av_read_frame(ifmt_ctx, &pkt) >= 0);
			}else{
				break;
			}
		}else{
			ifmt_ctx=ifmt_ctx_a;
			stream_index=audioindex_out;
			if(av_read_frame(ifmt_ctx, &pkt) >= 0){
				do{
					in_stream  = ifmt_ctx->streams[pkt.stream_index];
					out_stream = ofmt_ctx->streams[stream_index];

					if(pkt.stream_index==audioindex_a){

						//FIX：No PTS
						//Simple Write PTS
						if(pkt.pts==AV_NOPTS_VALUE){
							//Write PTS
							AVRational time_base1=in_stream->time_base;
							//Duration between 2 frames (us)
							int64_t calc_duration=(double)AV_TIME_BASE/av_q2d(in_stream->r_frame_rate);
							//Parameters
							pkt.pts=(double)(frame_index*calc_duration)/(double)(av_q2d(time_base1)*AV_TIME_BASE);
							pkt.dts=pkt.pts;
							pkt.duration=(double)calc_duration/(double)(av_q2d(time_base1)*AV_TIME_BASE);
							frame_index++;
						}
						cur_pts_a=pkt.pts;
						break;
					}
				}while(av_read_frame(ifmt_ctx, &pkt) >= 0);
			}else{
				break;
			}
		}

		//FIX:Bitstream Filter
		av_bitstream_filter_filter(h264bsfc, in_stream->codec, NULL, &pkt.data, &pkt.size, pkt.data, pkt.size, 0);
		av_bitstream_filter_filter(aacbsfc, out_stream->codec, NULL, &pkt.data, &pkt.size, pkt.data, pkt.size, 0);

		//Convert PTS/DTS
		pkt.pts = av_rescale_q_rnd(pkt.pts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
		pkt.dts = av_rescale_q_rnd(pkt.dts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
		pkt.duration = av_rescale_q(pkt.duration, in_stream->time_base, out_stream->time_base);
		pkt.pos = -1;
		pkt.stream_index=stream_index;

		//printf("Write 1 Packet. size:%5d\tpts:%lld\n",pkt.size,pkt.pts);
		//Write
		if (av_interleaved_write_frame(ofmt_ctx, &pkt) < 0) {
			printf( "Error muxing packet\n");
			break;
		}
		av_free_packet(&pkt);

	}
	//Write file trailer
	av_write_trailer(ofmt_ctx);

    // USE_H264BSF
	av_bitstream_filter_close(h264bsfc);

    // USE_AACBSF
	av_bitstream_filter_close(aacbsfc);


	return 0;
}


/*
    功能：将aac文件转换为pcm文件 并截取指定定时间内的音频
    参数：
        inpath        (in)          aac文件全路径
        outpath       (in)          pcm文件全路径
        start_time    (in)          截取音频起始时间
        end_time      (in)          截取音频结束时间       
    返回值：成功返回0，失败返回-1
*/
int aac_to_pcm(char *inpath, char* outpath, char *start_time, char *end_time)
{
	AVFormatContext	*pFormatCtx;
	int				i, audioStream;
	AVCodecContext	*pCodecCtx;
	AVCodec			*pCodec;
	AVPacket		*packet;
	uint8_t			*out_buffer;
	AVFrame			*pFrame;	
    int ret;
	uint32_t len = 0;
	int got_picture;
	int index = 0;
	int64_t in_channel_layout;
	struct SwrContext *au_convert_ctx;

	FILE *pFile = NULL;

	av_register_all();
	avformat_network_init();
	pFormatCtx = avformat_alloc_context();
	
	//Open
	if(avformat_open_input(&pFormatCtx,inpath,NULL,NULL)!=0){
		printf("Couldn't open input stream.\n");
		return -1;
	}
	// Retrieve stream information
	if(avformat_find_stream_info(pFormatCtx,NULL)<0){
		printf("Couldn't find stream information.\n");
		return -1;
	}
	
	// Dump valid information onto standard error
	av_dump_format(pFormatCtx, 0, inpath, false);

	// Find the first audio stream
	audioStream=-1;
	for(i = 0; i < pFormatCtx->nb_streams; i++)
		if(pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO){
			audioStream=i;
			break;
		}

	if(audioStream == -1){
		printf("Didn't find a audio stream.\n");
		return -1;
	}

	// Get a pointer to the codec context for the audio stream
	pCodecCtx=pFormatCtx->streams[audioStream]->codec;

	// Find the decoder for the audio stream
	pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
	if(pCodec == NULL){
		printf("Codec not found.\n");
		return -1;
	}

	// Open codec
	if(avcodec_open2(pCodecCtx, pCodec,NULL)<0){
		printf("Could not open codec.\n");
		return -1;
	}

	pFile = fopen(outpath, "ab");

	packet=(AVPacket *)av_malloc(sizeof(AVPacket));
	av_init_packet(packet);

	//Out Audio Param
	uint64_t out_channel_layout=AV_CH_LAYOUT_STEREO;
	//nb_samples: AAC-1024 MP3-1152
	int out_nb_samples = pCodecCtx->frame_size;
	AVSampleFormat out_sample_fmt = AV_SAMPLE_FMT_S16;
	int out_sample_rate = 44100;
	int out_channels=av_get_channel_layout_nb_channels(out_channel_layout);
	//Out Buffer Size
	int out_buffer_size=av_samples_get_buffer_size(NULL,out_channels ,out_nb_samples,out_sample_fmt, 1);

	out_buffer=(uint8_t *)av_malloc(MAX_AUDIO_FRAME_SIZE * 2);
	pFrame=av_frame_alloc();

	//FIX:Some Codec's Context Information is missing
	in_channel_layout = av_get_default_channel_layout(pCodecCtx->channels);

	au_convert_ctx = swr_alloc();
	au_convert_ctx = swr_alloc_set_opts(au_convert_ctx,out_channel_layout, out_sample_fmt, out_sample_rate,
		in_channel_layout,pCodecCtx->sample_fmt , pCodecCtx->sample_rate,0, NULL);
	swr_init(au_convert_ctx);

	int audio_data = 0;
	int skip_data = 0;
	//将起始时间转换为秒，结束时间转换为秒，结束时间减去起始时间得到要转换的时间的总数据
	int start_time_int = time_to_ms(start_time);
	int end_time_int = time_to_ms(end_time);
	int all_time_int = end_time_int - start_time_int;
	printf("all_time_int = %d\n", all_time_int);

   //计算音频指定时间内的数据 采样率* (采样位数/2) * 声道数 * 时间s 44100*(16/2)*2*10 
	while(av_read_frame(pFormatCtx, packet)>=0 ){
		if(packet->stream_index == audioStream){
			ret = avcodec_decode_audio4( pCodecCtx, pFrame,&got_picture, packet);
			if ( ret < 0 ) {
                printf("Error in decoding audio frame.\n");
                return -1;
            }
            enum AVSampleFormat sample;
            int sample_bye = 1;
			sample = (enum AVSampleFormat)pFrame->format;   //获取音频采样位格式
			sample_bye = av_get_bytes_per_sample(sample);   //获取一个音频样本的字节数
			
			audio_data = pFrame->sample_rate * sample_bye * pFrame->channels * all_time_int;  //获取指定时间内总数据
            skip_data = pFrame->sample_rate * sample_bye * pFrame->channels * start_time_int;
            //printf("pFrame->sample_rate= %d sample_bye = %d pFrame->channels = %d audio_data = %d skip_data = %d\n",pFrame->sample_rate,sample_bye, pFrame->channels,audio_data, skip_data);
           
           //跳过起始数据，
           //起始时间是0 从头开始写数据，一直写够指定的数据，不是从头开始
			if ( got_picture > 0){
				swr_convert(au_convert_ctx,&out_buffer, MAX_AUDIO_FRAME_SIZE,(const uint8_t **)pFrame->data , pFrame->nb_samples);
				//printf("index:%5d\t pts:%lld\t packet size:%d\n",index,packet->pts,packet->size);
				if( index >= skip_data && index <= (audio_data + skip_data))
				{
					//Write PCM
					fwrite(out_buffer, 1, out_buffer_size, pFile);
				}				
				//index++;
				index += out_buffer_size;
			}
		}
		av_free_packet(packet);
	}
	swr_free(&au_convert_ctx);
	fclose(pFile);
	av_free(out_buffer);
	avcodec_close(pCodecCtx);
	avformat_close_input(&pFormatCtx);

	return 0;
}


/*
    功能：将pcm文件编码为aac文件
    参数：
          input_file         (in)       输入文件pcm
          output_file        (in)       输出文件aac
   返回值：成功返回0，失败返回-1
*/
int pcm_to_aac(char *input_file, char* output_file)
{
	AVCodec *pCodec;
    AVCodecContext *pCodecCtx= NULL;
    int i, ret, got_output;
    FILE *fp_in;
	FILE *fp_out;

    AVFrame *pFrame;
	uint8_t* frame_buf;
	int size=0;

	AVPacket pkt;
	int y_size;
	int framecnt = 0;

	AVCodecID codec_id = AV_CODEC_ID_AAC; 

	int framenum = 2000;	//需要编码的总帧数
	
	av_register_all();

	avcodec_register_all();

   // pCodec = avcodec_find_encoder(codec_id);
    pCodec = avcodec_find_encoder_by_name("libfdk_aac");  //指定使用文件编码类型
    if (!pCodec) {
        printf("Codec not found\n");
        return -1;
    }
    pCodecCtx = avcodec_alloc_context3(pCodec);
    if (!pCodecCtx) {
        printf("Could not allocate video codec context\n");
        return -1;
    }

	pCodecCtx->codec_id = codec_id;
	pCodecCtx->codec_type = AVMEDIA_TYPE_AUDIO;
	pCodecCtx->sample_fmt = AV_SAMPLE_FMT_S16;
	pCodecCtx->sample_rate = 44100;
	pCodecCtx->channel_layout = AV_CH_LAYOUT_STEREO;
	pCodecCtx->channels = av_get_channel_layout_nb_channels(pCodecCtx->channel_layout);
	pCodecCtx->bit_rate = 64000;  

    if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0) {
        printf("Could not open codec\n");
        return -1;
    }
    
	pFrame = av_frame_alloc();
	pFrame->nb_samples= pCodecCtx->frame_size;
	pFrame->format= pCodecCtx->sample_fmt;
	size = av_samples_get_buffer_size(NULL, pCodecCtx->channels,pCodecCtx->frame_size,pCodecCtx->sample_fmt, 1);
	frame_buf = (uint8_t *)av_malloc(size);
	avcodec_fill_audio_frame(pFrame, pCodecCtx->channels, pCodecCtx->sample_fmt,(const uint8_t*)frame_buf, size, 1);

	//Input raw data
	fp_in = fopen(input_file, "rb");
	if (!fp_in) {
		printf("Could not open %s\n", input_file);
		return -1;
	}
	//Output bitstream
	fp_out = fopen(output_file, "wb");
	if (!fp_out) {
		printf("Could not open %s\n", output_file);
		return -1;
	}

    //Encode
    for (i = 0; i < framenum; i++) {
        av_init_packet(&pkt);
        pkt.data = NULL;    // packet data will be allocated by the encoder
        pkt.size = 0;
		//Read raw data
		if (fread(frame_buf, 1, size, fp_in) <= 0){
			printf("Failed to read raw data! \n");
			return -1;
		}else if(feof(fp_in)){
			break;
		}

        pFrame->pts = i;
        ret = avcodec_encode_audio2(pCodecCtx, &pkt, pFrame, &got_output);
        if (ret < 0) {
            printf("Error encoding frame\n");
            return -1;
        }
        if (got_output) {
            //printf("Succeed to encode frame: %5d\tsize:%5d\n",framecnt,pkt.size);
			framecnt++;
            fwrite(pkt.data, 1, pkt.size, fp_out);
            av_free_packet(&pkt);
        }
    }
    //Flush Encoder
    for (got_output = 1; got_output; i++) {
        ret = avcodec_encode_audio2(pCodecCtx, &pkt, NULL, &got_output);
        if (ret < 0) {
            printf("Error encoding frame\n");
            return -1;
        }
        if (got_output) {
            printf("Flush Encoder: Succeed to encode 1 frame!\tsize:%5d\n",pkt.size);
            fwrite(pkt.data, 1, pkt.size, fp_out);
            av_free_packet(&pkt);
        }
    }

    fclose(fp_out);
    avcodec_close(pCodecCtx);
    av_free(pCodecCtx);
    av_freep(&pFrame->data[0]);
    av_frame_free(&pFrame);

	return 0;
}






int main()
{	
	Splice_video("../file/testvideo.mp4", "../file/testvideo.mp4", "../file/test.yuv", 0, 5);	
	YUV_INFO yuv_info;
	get_video_info("../file/testvideo.mp4", &yuv_info);
	//原视频时长 + 插入视频时长
	//获取视频帧数 时长/帧率	
	yuv_to_h264("../file/test.yuv", "../file/ds.h264", yuv_info.in_w, yuv_info.in_h, (yuv_info.duration/1000000)* yuv_info.framenum * 2 );
	//remove("../file/test.yuv");
	//剪接音频
			
	//aac_to_pcm("../file/gowest.aac", "../file/output.pcm","00:00:10","00:00:20");
	pcm_to_aac("../file/output.pcm", "../file/output.aac");
	h264_aac_to_mp4("../file/ds.h264", "../file/output.aac", "../file/mp4_file.mp4");
	//remove("../file/ds.h264");
	//remove("../file/output.aac");
	return 0;	
}




