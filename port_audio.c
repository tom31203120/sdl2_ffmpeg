#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>

// 这里是PortAudio的头文件
#include <portaudio.h>
#include <assert.h>

#define INBUF_SIZE 4096
#define AUDIO_INBUF_SIZE 20480
#define AUDIO_REFILL_THRESH 4096

typedef struct audiocontext {
    AVCodecContext* codecContext;
    SwrContext* swrContext;
    ReSampleContext* resamplerContext;
}AudioContext ;

static void audio_copy(AudioContext *context,  AVFrame *dst, AVFrame* src)
{
    int nb_sample;
    int dst_buf_size;
    int out_channels;
    int bytes_per_sample = 0;

    dst->linesize[0] = src->linesize[0];
    *dst = *src;
    dst->data[0] = NULL;
    /*dst->type = 0;*/

    /* 备注: FFMIN(codecContext->channels, 2); 会有问题, 因为swr_alloc_set_opts的out_channel_layout参数. */
    out_channels = context->codecContext->channels;

    /*bytes_per_sample = av_get_bytes_per_sample(context->codecContext->sample_fmt);*/
    /* 备注: 由于 src->linesize[0] 可能是错误的, 所以计算得到的nb_sample会不正确, 直接使用src->nb_samples即可. */
    nb_sample = src->nb_samples;/* src->linesize[0] / codecContext->channels / bytes_per_sample; */
    bytes_per_sample = av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);
    dst_buf_size = nb_sample * bytes_per_sample * out_channels;
    dst->data[0] = (uint8_t*) av_malloc(dst_buf_size);
    assert(dst->data[0]);
    avcodec_fill_audio_frame(dst, out_channels, AV_SAMPLE_FMT_S16, dst->data[0], dst_buf_size, 0);

    /* 重采样到AV_SAMPLE_FMT_S16格式. */
    if (context->codecContext->sample_fmt != AV_SAMPLE_FMT_S16)
    {
        if (!context->swrContext)
        {
            uint64_t in_channel_layout = av_get_default_channel_layout(context->codecContext->channels);
            uint64_t out_channel_layout = av_get_default_channel_layout(out_channels);
            context->swrContext = swr_alloc_set_opts(NULL,
                out_channel_layout, AV_SAMPLE_FMT_S16, context->codecContext->sample_rate,
                in_channel_layout, context->codecContext->sample_fmt, context->codecContext->sample_rate,
                0, NULL);
            swr_init(context->swrContext);
        }

        if (context->swrContext)
        {
            int ret, out_count;
            out_count = dst_buf_size / out_channels / av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);
            ret = swr_convert(context->swrContext, dst->data, out_count, (const uint8_t**)(src->data), nb_sample);
            if (ret < 0)
                assert(0);
            src->linesize[0] = dst->linesize[0] = ret * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16) * out_channels;
            memcpy(src->data[0], dst->data[0], src->linesize[0]);
        }
    }

    /**//* 重采样到双声道. */
    /*if (context->codecContext->channels > 2)*/
    /*{*/
    /*if (!context->resamplerContext)*/
    /*{*/
    /*context->resamplerContext = av_audio_resample_init(*/
    /*FFMIN(2, context->codecContext->channels),*/
    /*context->codecContext->channels, context->codecContext->sample_rate,*/
    /*context->codecContext->sample_rate, AV_SAMPLE_FMT_S16, AV_SAMPLE_FMT_S16,*/
    /*16, 10, 0, 0.f);*/
    /*}*/

    /*if (context->resamplerContext)*/
    /*{*/
    /*int samples = src->linesize[0] / (av_get_bytes_per_sample(AV_SAMPLE_FMT_S16) * context->codecContext->channels);*/
    /*dst->linesize[0] = audio_resample(context->resamplerContext, (short *) dst->data[0], (short *) src->data[0], samples) * 4;*/
    /*}*/
    /*}*/
    /*else*/
    {
        dst->linesize[0] = dst->linesize[0];
        memcpy(dst->data[0], src->data[0], dst->linesize[0]);
    }
}

int main(int argc, char* argv[]) 
{
    // 将要打开的音频文件(视频文件也可以支持).
    const char* filename = argc > 1 ? argv[1] : "1.mp3";

    // 初始化libavformat,并注册所有的模块
    av_register_all();

    // 这里一定要设置成NULL, 或者调用avformat_alloc_context分配内存, 否则可能崩溃.
    AVFormatContext *formatContext = NULL;

    // 打开输入文件.
    if( avformat_open_input(&formatContext, filename, NULL, NULL) < 0) {
        printf("cannot open file\n");
        return -1;
    }

    // 探测文件里面的音视频流信息.
    if( avformat_find_stream_info(formatContext, NULL) < 0) {
        printf( "cannot find stream info\n");
        return -1;
    }

    // 输出来看看.
    av_dump_format(formatContext, 0, 0, 0);

    // 找到音频流的索引(如果是视频的话,可能存在多个流).
    // Find the first video stream
    int i, audioIndex=-1;
    /*if((audioIndex = av_find_best_stream(formatContext, AVMEDIA_TYPE_AUDIO, 0, 0, NULL, 0)) < 0) {*/
    for(i=0; i<formatContext->nb_streams; i++) {
        if(formatContext->streams[i]->codec->codec_type==AVMEDIA_TYPE_AUDIO && audioIndex < 0) {
            audioIndex=i;
        }
    }
    if(audioIndex== -1) {
        printf( "cannot find audio stream for \n") ;
    }

    AVCodecContext *codecContext = formatContext->streams[audioIndex]->codec;
    // 找到解码器.
    AVCodec *codec = avcodec_find_decoder(codecContext->codec_id);
    if(codec == NULL) {
        printf( "cannot find decoder for %s\n",codecContext->codec_name) ;
    }

    // 打开解码器.
    if( avcodec_open2(codecContext, codec, NULL) < 0) {
        printf( "cannot open decoder\n");
        return -1;
    }

    // AVPacket是解码前的数据, AVFrame是解码后的数据.
    AVPacket packet;
    AVFrame *frame = av_frame_alloc();
    int got;
    AudioContext context;
    context.resamplerContext = NULL;
    context.swrContext = NULL;
    context.codecContext = codecContext;

    // 下面是初始化PortAudio, 用PortAudio的Blocking API比较简单.
    PaStream *stream;
    Pa_Initialize();
    Pa_OpenDefaultStream(&stream, 0, codecContext->channels, 
        paInt16, codecContext->sample_rate, 
        1024, NULL, NULL);
    Pa_StartStream(stream);
    codecContext->sample_fmt;
    int size = 4092;
    int channels = codecContext->channels;
    static uint8_t inbuf[AUDIO_INBUF_SIZE + FF_INPUT_BUFFER_PADDING_SIZE];
    while(1) {
        // 从文件中读取一帧.
        if(av_read_frame(formatContext, &packet) < 0) {
            // 文件读完了.
            break;
        }

        // 解码.
        int len = avcodec_decode_audio4(codecContext, frame, &got, &packet);
        if(len < 0) {
            continue;
            printf( "cannot decode\n");
        }

        // 解码出来了一帧
        if(got) {
            // 因为frame->data[0]表示的是左声道LLL....,frame->data[1]表示右声道RRR...
            AVFrame *dst = av_frame_alloc();
            audio_copy(&context, dst, frame);
            Pa_WriteStream(stream, (int16_t*)(dst->data[0]), dst->nb_samples);
        }
        packet.size -= len;
        packet.data += len;
        packet.dts = packet.pts = AV_NOPTS_VALUE;
        if (packet.size < AUDIO_REFILL_THRESH) {
            /* Refill the input buffer, to avoid trying to decode
             * incomplete frames. Instead of this, one could also use
             * a parser, or use a proper container format through
             * libavformat. */
            memmove(inbuf, packet.data, packet.size);
            AVPacket pkt;
            if(av_read_frame(formatContext, &pkt) < 0) {
                // 文件读完了.
                break;
            }
            memmove(inbuf+packet.size, pkt.data, pkt.size);
            packet.data = inbuf;
            packet.size += pkt.size;
        }

    }
    return 0; 
}
