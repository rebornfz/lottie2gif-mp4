#include "VideoCapture.h"

#define VIDEO_TMP_FILE "tmp.h264"
#define FINAL_FILE_NAME "record.mp4"


using namespace std;

FILE *fp_write;
static int write_packet(void *opaque, uint8_t *buf, size_t buf_size)
{
    struct buffer_data *bd = (struct buffer_data *)opaque;
    //buf_size = FFMIN(buf_size, bd->size);
    //printf("ptr:%p size:%zu\n", bd->ptr, bd->size);
    //std::cout << buf << " " << buf_size << std::endl;
    printf("ptr  :%p size:%zu\n", bd->ptr, bd->size);
    /* copy internal buffer data to buf */
    memcpy(bd->ptr + bd->size, buf, buf_size);
    //bd->ptr += buf_size;
	//std::cout << "buf " << std::endl;
    bd->size = buf_size + bd->size;
    return 1;
}

/*
//write h264
static int write_buffer(void *opaque, uint8_t *buf, int buf_size){
	if(!feof(fp_write)){
		//std::cout << buf << " " << buf_size << std::endl;
		printf("ptr:%p size:%zu\n", buf, buf_size);
		int true_size=fwrite(buf,1,buf_size,fp_write);
		return true_size;
	}else{
		return -1;
    }
}
*/
void VideoCapture::Init(int argc, char **argv, int width, int height, int fpsrate, int bitrate) {

    fps = fpsrate;
    filename = argv[1];

    int err;
    //write h264
    //fp_write=fopen("cuc60anniversary_start.h264","wb+");
    uint8_t *outbuffer=NULL;
	outbuffer=(uint8_t*)av_malloc(32768);
    bd = (struct buffer_data*)malloc(sizeof(struct buffer_data));
    bd->ptr = (uint8_t*)av_malloc(100000000);
	bd->size = 0;
    avio_out =avio_alloc_context(outbuffer, 32768,1,bd,NULL,write_packet,NULL);
    if (!(oformat = av_guess_format("h264", NULL, NULL))) {
        cout << "Failed to define output format";
        return;
    }

    if ((err = avformat_alloc_output_context2(&ofctx, NULL, "h264", NULL) < 0)) {
        cout  <<"Failed to allocate output context"<< endl;
        //Free();
        return;
    }

    if (!(codec = avcodec_find_encoder(oformat->video_codec))) {
        cout <<"Failed to find encoder"<< endl;
        //Free();
        return;
    }

    if (!(videoStream = avformat_new_stream(ofctx, codec))) {
        cout <<"Failed to create new stream"<< endl;
        //Free();
        return;
    }

    if (!(cctx = avcodec_alloc_context3(codec))) {
        cout <<"Failed to allocate codec context"<< endl;
        //Free();
        return;
    }

    videoStream->codecpar->codec_id = oformat->video_codec;
    videoStream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
    videoStream->codecpar->width = width;
    videoStream->codecpar->height = height;
    videoStream->codecpar->format = AV_PIX_FMT_YUV420P;
    videoStream->codecpar->bit_rate = bitrate * 1000;
    videoStream->time_base = { 1, fps };

    avcodec_parameters_to_context(cctx, videoStream->codecpar);
    cctx->time_base = { 1, fps };
    cctx->max_b_frames = 2;
    cctx->gop_size = 12;
    if (videoStream->codecpar->codec_id == AV_CODEC_ID_H264) {
        av_opt_set(cctx, "preset", "ultrafast", 0);
    }
    if (ofctx->oformat->flags & AVFMT_GLOBALHEADER) {
        cctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }
    avcodec_parameters_from_context(videoStream->codecpar, cctx);

    if ((err = avcodec_open2(cctx, codec, NULL)) < 0) {
        cout <<"Failed to open codec"<< endl;
        Free();
        return;
    }
/*
    if (!(oformat->flags & AVFMT_NOFILE)) {
        //改成从内存
        if ((err = avio_open(&ofctx->pb, VIDEO_TMP_FILE, AVIO_FLAG_WRITE)) < 0) {
            cout <<"Failed to open file"<< endl;
            Free();
            return;
        }
        //ofctx = avformat_alloc_context();
        //unsigned char * iobuffer=(unsigned char *)av_malloc(32768);
        //AVIOContext *avio =avio_alloc_context(iobuffer, 32768,0,NULL,fill_iobuffer,NULL,NULL);
        //ofctx->pb=avio;
    }
*/
    
    ofctx->pb = avio_out;
    
    //不知道有什么用
	ofctx->flags=AVFMT_FLAG_CUSTOM_IO;
    if ((err = avformat_write_header(ofctx, NULL)) < 0) {
        cout <<"Failed to write header"<< endl;
        Free();
        return;
    }

    //av_dump_format(ofctx, 0, VIDEO_TMP_FILE, 1);
	cout << "init com" << endl;
}

void VideoCapture::AddFrame(uint8_t *data) {
    int err;
    if (!videoFrame) {

        videoFrame = av_frame_alloc();
        videoFrame->format = AV_PIX_FMT_YUV420P;
        videoFrame->width = cctx->width;
        videoFrame->height = cctx->height;

        if ((err = av_frame_get_buffer(videoFrame, 32)) < 0) {
            cout <<"Failed to allocate picture"<< endl;
            return;
        }
    }

    if (!swsCtx) {
        swsCtx = sws_getContext(cctx->width, cctx->height, AV_PIX_FMT_RGBA, cctx->width, cctx->height, AV_PIX_FMT_YUV420P, SWS_BICUBIC, 0, 0, 0);
    }

    int inLinesize[1] = { 4 * cctx->width };

    
    sws_scale(swsCtx, (const uint8_t * const *)&data, inLinesize, 0, cctx->height, videoFrame->data, videoFrame->linesize);

    videoFrame->pts = frameCounter++;

    if ((err = avcodec_send_frame(cctx, videoFrame)) < 0) {
        cout <<"Failed to send frame"<< endl;
        return;
    }

    AVPacket pkt;
    av_init_packet(&pkt);
    pkt.data = NULL;
    pkt.size = 0;

    if (avcodec_receive_packet(cctx, &pkt) == 0) {
        pkt.flags |= AV_PKT_FLAG_KEY;
        av_interleaved_write_frame(ofctx, &pkt);
        av_packet_unref(&pkt);
    }
}

void VideoCapture::Finish() {
    
    AVPacket pkt;
    av_init_packet(&pkt);
    pkt.data = NULL;
    pkt.size = 0;

    for (;;) {
        avcodec_send_frame(cctx, NULL);
        if (avcodec_receive_packet(cctx, &pkt) == 0) {
            av_interleaved_write_frame(ofctx, &pkt);
            av_packet_unref(&pkt);
        }
        else {
            break;
        }
    }
    
    
    av_write_trailer(ofctx);
	/*
    if (!(oformat->flags & AVFMT_NOFILE)) {
        int err = avio_close(ofctx->pb);
        if (err < 0) {
            cout <<"Failed to close file"<< endl;
        }
    }
    */
	/*
    //write h264
    if(!feof(fp_write)){
		int true_size=fwrite(bd->ptr,1,bd->size,fp_write);
        std::cout << true_size << std::endl;
    }
	
	
	
	fcloseall();
    */
    Free();

    Remux();
}

void VideoCapture::Free() {
    if (videoFrame) {
        //av_frame_free(&videoFrame);
    }
    if (cctx) {
        //avcodec_free_context(&cctx);
    }
    if (ofctx) {
        //avformat_free_context(ofctx);
    }
    if (swsCtx) {
        //sws_freeContext(swsCtx);
    }
}

static int read_packet(void *opaque, uint8_t *buf, int buf_size)
{
    struct buffer_data *bd = (struct buffer_data *)opaque;
    buf_size = FFMIN(buf_size, bd->size);
    if(buf_size == 0) return -1;
    printf("ptr:%p size:%zu\n", bd->ptr, bd->size);
    /* copy internal buffer data to buf */
    memcpy(buf, bd->ptr, buf_size);
    bd->ptr  += buf_size;
    bd->size -= buf_size;
    return buf_size;
}

void VideoCapture::Remux() {
    AVFormatContext *ifmt_ctx = NULL, *ofmt_ctx = NULL;
    int err;

	unsigned char* inbuffer=NULL;
	inbuffer=(unsigned char*)av_malloc(32768);
	ifmt_ctx = avformat_alloc_context();
	AVIOContext *avio_in =avio_alloc_context(inbuffer, 32768 ,0,bd,read_packet,NULL,NULL);
	ifmt_ctx->pb=avio_in;
	
    
    //avio_in =avio_alloc_context(outbuffer, 32768,1,bd,NULL,write_packet,NULL);
    //ifmt_ctx->pb=avio_in;
    if ((err = avformat_open_input(&ifmt_ctx, "NULL", 0, 0)) < 0) {
        cout <<"Failed to open input file for remuxing"<< endl;
        goto end;
    }
    if ((err = avformat_find_stream_info(ifmt_ctx, 0)) < 0) {
        cout <<"Failed to retrieve input stream information"<< endl;
        goto end;
    }
    if ((err = avformat_alloc_output_context2(&ofmt_ctx, NULL, "mp4", NULL))) {
        cout <<"Failed to allocate output context"<< endl;
        goto end;
    }

    AVStream *inVideoStream = ifmt_ctx->streams[0];
    AVStream *outVideoStream = avformat_new_stream(ofmt_ctx, NULL);
    if (!outVideoStream) {
        cout <<"Failed to allocate output video stream" << endl;
        goto end;
    }
    outVideoStream->time_base = { 1, fps };
    avcodec_parameters_copy(outVideoStream->codecpar, inVideoStream->codecpar);
    outVideoStream->codecpar->codec_tag = 0;
    
	uint8_t *outbuffer=NULL;
	outbuffer=(uint8_t*)av_malloc(32768);
    bd = (struct buffer_data*)malloc(sizeof(struct buffer_data));
    bd->ptr = (uint8_t*)av_malloc(100000000);
	bd->size = 0;
    //avio_out =avio_alloc_context(outbuffer, 32768,1,bd,NULL,write_packet,NULL);
	//ofmt_ctx->pb = avio_out;

    if (!(ofmt_ctx->oformat->flags & AVFMT_NOFILE)) {
		avio_out =avio_alloc_context(outbuffer, 32768,1,bd,NULL,write_packet,NULL);
	    ofmt_ctx->pb = avio_out;
/*
        if ((err = avio_open(&ofmt_ctx->pb, FINAL_FILE_NAME, AVIO_FLAG_WRITE)) < 0) {
            cout <<"Failed to open output file"<< endl;
            goto end;
        }
*/
    }
	AVDictionary* opts = NULL;
	av_dict_set(&opts, "movflags", "frag_keyframe+empty_moov", 0);
    if ((err = avformat_write_header(ofmt_ctx, &opts)) < 0) {
        cout <<"Failed to write header to output file"<< endl;
        goto end;
    }

    AVPacket videoPkt;
    int ts = 0;
    while (true) {
        if ((err = av_read_frame(ifmt_ctx, &videoPkt)) < 0) {
            break;
        }
        videoPkt.stream_index = outVideoStream->index;
        videoPkt.pts = ts;
        videoPkt.dts = ts;
        videoPkt.duration = av_rescale_q(videoPkt.duration, inVideoStream->time_base, outVideoStream->time_base);
        ts += videoPkt.duration;
        videoPkt.pos = -1;
		//std::cout << "ss" << std::endl;
        if ((err = av_interleaved_write_frame(ofmt_ctx, &videoPkt)) < 0) {
            cout <<"Failed to mux packet"<< endl;
            av_packet_unref(&videoPkt);
            break;
        }
        av_packet_unref(&videoPkt);
    }

    av_write_trailer(ofmt_ctx);
	fp_write=fopen(filename,"wb+");
	if(!feof(fp_write)){
		int true_size=fwrite(bd->ptr,1,bd->size,fp_write);
        std::cout << true_size << std::endl;
    }
	fcloseall();

end:
    if (ifmt_ctx) {
        //avformat_close_input(&ifmt_ctx);
    }
    if (ofmt_ctx && !(ofmt_ctx->oformat->flags & AVFMT_NOFILE)) {
        //avio_closep(&ofmt_ctx->pb);
    }
    if (ofmt_ctx) {
        //avformat_free_context(ofmt_ctx);
    }
}
