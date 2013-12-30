// tutorial03.c
// A pedagogical video player that will stream through every video frame as fast as it can
// and play audio (out of sync).
//
// This tutorial was written by Stephen Dranger (dranger@gmail.com).
//
// Code based on FFplay, Copyright (c) 2003 Fabrice Bellard, 
// and a tutorial by Martin Bohme (boehme@inb.uni-luebeckREMOVETHIS.de)
// Tested on Gentoo, CVS version 5/01/07 compiled with GCC 4.1.1
//
// Use the Makefile to build all examples.
//
// Run using
// tutorial03 myvideofile.mpg
//
// to play the stream on your screen.


#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>

// 这里是PortAudio的头文件
#include <portaudio.h>
#include <assert.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_thread.h>

#ifdef __MINGW32__
#undef main /* Prevents SDL from overriding main() */
#endif

#include <stdio.h>

#define SDL_AUDIO_BUFFER_SIZE 1024
#define MAX_AUDIO_FRAME_SIZE 192000

#define INBUF_SIZE 4096
#define AUDIO_INBUF_SIZE 20480
#define AUDIO_REFILL_THRESH 4096

#define WIN_WIDTH  640
#define WIN_HEIGHT 480

typedef struct PacketQueue {
    AVPacketList *first_pkt, *last_pkt;
    int nb_packets;
    int size;
    SDL_mutex *mutex;
    SDL_cond *cond;
} PacketQueue;

PacketQueue g_audioq;

int quit = 0;

void packet_queue_init(PacketQueue *q) {
    memset(q, 0, sizeof(PacketQueue));
    q->mutex = SDL_CreateMutex();
    q->cond  = SDL_CreateCond();
}

int packet_queue_put(PacketQueue *q, AVPacket *pkt) {

    AVPacketList *pkt1;
    if(av_dup_packet(pkt) < 0) {
        return -1;
    }
    pkt1 = av_malloc(sizeof(AVPacketList));
    if (!pkt1)
        return -1;
    pkt1->pkt = *pkt;
    pkt1->next = NULL;

    SDL_LockMutex(q->mutex);

    if (!q->last_pkt)
        q->first_pkt = pkt1;
    else
        q->last_pkt->next = pkt1;
    q->last_pkt = pkt1;
    q->nb_packets++;
    q->size += pkt1->pkt.size;
    SDL_CondSignal(q->cond);

    SDL_UnlockMutex(q->mutex);
    return 0;
}

static int packet_queue_get(PacketQueue *q, AVPacket *pkt, int block)
{
    AVPacketList *pkt1;
    int ret;

    SDL_LockMutex(q->mutex);

    for(;;) {

        if(quit) {
            ret = -1;
            break;
        }

        pkt1 = q->first_pkt;
        if (pkt1) {
            q->first_pkt = pkt1->next;
            if (!q->first_pkt)
                q->last_pkt = NULL;
            q->nb_packets--;
            q->size -= pkt1->pkt.size;
            *pkt = pkt1->pkt;
            av_free(pkt1);
            ret = 1;
            break;
        } else if (!block) {
            ret = 0;
            break;
        } else {
            SDL_CondWait(q->cond, q->mutex);
        }
    }
    SDL_UnlockMutex(q->mutex);
    return ret;
}

int audio_decode_frame(AVCodecContext *aCodecCtx, uint8_t *audio_buf)
{
    static AVPacket avpkt;
    static uint8_t inbuf[AUDIO_INBUF_SIZE + FF_INPUT_BUFFER_PADDING_SIZE];
    static int frame_inx = 0;

    int data_size = -1;
    static AVFrame *frame=NULL;

    if (!frame) {
        if (!(frame = av_frame_alloc())) {
            fprintf(stderr, "Could not allocate audio frame\n");
            exit(1);
        }
    }

    if (avpkt.size <= 0){
        if(packet_queue_get(&g_audioq, &avpkt, 1) < 0) {
            return -1;
        }
    }

    while (avpkt.size > 0) {
        int got_frame = 0, len;

        len = avcodec_decode_audio4(aCodecCtx, frame, &got_frame, &avpkt);
        if (len < 0) {
            fprintf(stderr, "Error while decoding\n");
            memset(&avpkt, 0, sizeof(avpkt));
            return -1;
        }
        if (got_frame) {
            /* if a frame has been decoded, output it */
            data_size = av_samples_get_buffer_size(NULL, aCodecCtx->channels,
                                                       frame->nb_samples,
                                                       aCodecCtx->sample_fmt, 1);
            memcpy(audio_buf, frame->data[0], data_size);
            printf("the num of frame %d\n",frame_inx++);
            /*fwrite(frame->data[0], 1, data_size, outfile);*/
        }
        avpkt.size -= len;
        avpkt.data += len;
        avpkt.dts = avpkt.pts = AV_NOPTS_VALUE;
        if (avpkt.size < AUDIO_REFILL_THRESH) {
            /* Refill the input buffer, to avoid trying to decode
             * incomplete frames. Instead of this, one could also use
             * a parser, or use a proper container format through
             * libavformat. */
            memmove(inbuf, avpkt.data, avpkt.size);
            AVPacket pkt;
            if(packet_queue_get(&g_audioq, &pkt, 1) < 0) {
                fprintf(stderr, "Error while get audio data from queue\n");
                return -1;
            }
            memmove(inbuf+avpkt.size, pkt.data, pkt.size);
            avpkt.data = inbuf;
            avpkt.size += pkt.size;
        }
        if (data_size>0){
            break;
        }
    }
    return data_size;
}

void audio_callback(void *userdata, Uint8 *stream, int len) {

    static int num=0;
    printf("  %d times in audio callback\n",num++);
    AVCodecContext *aCodecCtx = (AVCodecContext *)userdata;
    int len1, audio_size;

    static uint8_t audio_buf[(MAX_AUDIO_FRAME_SIZE * 3) / 2];
    static unsigned int audio_buf_size = 0;
    static unsigned int audio_buf_index = 0;

    while(len > 0) {
        if(audio_buf_index >= audio_buf_size) {
            /* We have already sent all our data; get more */
            audio_size = audio_decode_frame(aCodecCtx, audio_buf);
            if(audio_size < 0) {
                /* If error, output silence */
                audio_buf_size = 1024; // arbitrary?
                memset(audio_buf, 0, audio_buf_size);
                /*exit(-1);*/
            } else {
                audio_buf_size = audio_size;
            }
            audio_buf_index = 0;
        }
        len1 = audio_buf_size - audio_buf_index;
        printf("audio callback uffer len1= %d\n", len1);
        if(len1 > len)
            len1 = len;
        /*memcpy(stream, (uint8_t *)audio_buf+audio_buf_index, len1);*/
        SDL_memcpy(stream, (uint8_t *)audio_buf+audio_buf_index, len1);
        len -= len1;
        stream += len1;
        audio_buf_index += len1;
    }
}

int event_handle()
{
    int done = 0;
    SDL_Event event;
    SDL_PollEvent(&event);
    switch(event.type) {
        case SDL_KEYDOWN:
            { 
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    done = 1;
                }
                break;
            }
        case SDL_QUIT:
            {
                done=1;
                break;
            }
    }
    return done;
}

int sdl_init(AVFormatContext* format_context, 
        AVCodecContext* audio_codec, int audiostream )
{
    /*SDL_Init(SDL_INIT_EVERYTHING);*/
    if(SDL_Init(SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
        fprintf(stderr, "Could not initialize SDL - %s\n", SDL_GetError());
        return -1;
    }
    AVPacket avpacket;

    SDL_Log("+++++ INIT DONE +++++");

    SDL_AudioSpec want, have;
    SDL_AudioDeviceID dev;

    SDL_zero(want);
    want.freq     = audio_codec->sample_rate;
    want.format   = AUDIO_S16SYS;
    want.channels = audio_codec->channels;
    want.samples  = 4096;
    want.callback = audio_callback;  // you wrote this function elsewhere.
    want.userdata = audio_codec;

    dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, SDL_AUDIO_ALLOW_ANY_CHANGE);
    if (dev == 0) {
        printf("Failed to open audio: %s\n", SDL_GetError());
    } 
    printf("audio dev =%d\n", dev);

    packet_queue_init(&g_audioq);
    SDL_PauseAudioDevice(dev, 0);  // start audio playing.

    while (av_read_frame(format_context, &avpacket) >= 0) {
        if(avpacket.stream_index==audiostream) {
            packet_queue_put(&g_audioq, &avpacket);
        } else {
            av_free_packet(&avpacket);
        }

        if (event_handle()) break;
    }//while av_read_frame

    while (!quit && (SDL_GetAudioDeviceStatus(dev) == SDL_AUDIO_PLAYING))
        SDL_Delay(1000);

    // Close the codec
    avcodec_close(audio_codec);

    // Close the video file
    avformat_close_input(&format_context);

    SDL_CloseAudioDevice(dev);
    SDL_Log("+++++ FINISHED +++++");
    SDL_Quit();
}

typedef struct ffmpeg_ctx{
    AVFormatContext *format_context;
    AVCodecContext  *audio_codec;
    int audiostream;
} FFMPEG_CTX;

FFMPEG_CTX *ffmpeg_init(char *filename)
{
    int             i, videoStream, audioStream;
    FFMPEG_CTX      *pCtx=NULL;
    AVFormatContext *pFormatCtx = NULL;

    AVCodecContext  *aCodecCtx = NULL;
    AVCodec         *aCodec = NULL;

    AVDictionary    *audioOptionsDict   = NULL;

    // Register all formats and codecs
    av_register_all();

    // Open video file
    if(avformat_open_input(&pFormatCtx, filename, NULL, NULL)!=0)
        return pCtx; // Couldn't open file

    // Retrieve stream information
    if(avformat_find_stream_info(pFormatCtx, NULL)<0)
        return pCtx; // Couldn't find stream information

    // Dump information about file onto standard error
    av_dump_format(pFormatCtx, 0, filename, 0);

    // Find the first video stream
    audioStream=-1;
    for(i=0; i<pFormatCtx->nb_streams; i++) {
        if(pFormatCtx->streams[i]->codec->codec_type==AVMEDIA_TYPE_AUDIO &&
                audioStream < 0) {
            audioStream=i;
        }
    }
    if(audioStream==-1)
        return pCtx;

    aCodecCtx=pFormatCtx->streams[audioStream]->codec;
    aCodec = avcodec_find_decoder(aCodecCtx->codec_id);
    if(!aCodec) {
        fprintf(stderr, "Unsupported codec!\n");
        return pCtx;
    }
    avcodec_open2(aCodecCtx, aCodec, &audioOptionsDict);

    pCtx = (FFMPEG_CTX *)malloc(sizeof(FFMPEG_CTX));
    if(!pCtx){
        fprintf(stderr, "malloc error!\n");
        return pCtx; 
    }

    pCtx->format_context = pFormatCtx;
    pCtx->audio_codec = aCodecCtx;
    pCtx->audiostream = audioStream;

    return pCtx;
}

int main(int argc, char *argv[]) {
    FFMPEG_CTX *pCtx = NULL;
    if(argc < 2) {
        fprintf(stderr, "Usage: test <file>\n");
        return 1;
    }
    pCtx = ffmpeg_init(argv[1]);
    if (pCtx){
        sdl_init(pCtx->format_context,
                pCtx->audio_codec, pCtx->audiostream);
    }

    return 0;
}
