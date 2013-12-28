/*
 * test2.c
 *
 *  Created on: Mar 9, 2012
 *      Author: dvi
 */
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <SDL2/SDL.h>
#include <stdio.h>
#define WIN_WIDTH  640
#define WIN_HEIGHT 480

void convert(AVFrame* frame, unsigned char* pixels, int pitch)
{
    if(pitch == frame->linesize[0]) {
        int size = pitch * frame->height;
        memcpy(pixels, frame->data[0], size);
        memcpy(pixels + size, frame->data[2], size / 4);
        memcpy(pixels + size * 5 / 4, frame->data[1], size / 4);
    } else {
        register unsigned char *y1,*y2,*y3,*i1,*i2,*i3;
        int i;
        y1 = pixels;
        y3 = pixels + pitch * frame->height;
        y2 = pixels + pitch * frame->height * 5 / 4;

        i1=frame->data[0];
        i2=frame->data[1];
        i3=frame->data[2];

        for (i = 0; i<(frame->height/2); i++) {
            memcpy(y1,i1,pitch);
            i1+=frame->linesize[0];
            y1+=pitch;
            memcpy(y1,i1,pitch);

            memcpy(y2,i2,pitch / 2);
            memcpy(y3,i3,pitch / 2);

            y1+=pitch;
            y2+=pitch / 2;
            y3+=pitch / 2;
            i1+=frame->linesize[0];
            i2+=frame->linesize[1];
            i3+=frame->linesize[2];
        }
    }
}

void convert_display_frame(SDL_Texture *texture, AVFrame* frame, SDL_Renderer *renderer)
{
    unsigned char* pixels;
    int pitch;

    SDL_LockTexture(texture, NULL, (void **)&pixels, &pitch);
    convert(frame, pixels, pitch);
    SDL_UnlockTexture(texture);
    /* Print out some timing information */
    //printf("TIMING %lld\n", avpacket.dts);

    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
    SDL_Delay(20);
}//frame

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

void sdl_init(AVFormatContext* format_context, AVCodecContext* codec_context, int videostream ) {
    SDL_Init(SDL_INIT_EVERYTHING);
    AVFrame* frame;
    AVPacket avpacket;

    SDL_RendererInfo info;
    SDL_Texture *texture;
    SDL_Window *window = SDL_CreateWindow("SDL",
                         SDL_WINDOWPOS_UNDEFINED, 
                         SDL_WINDOWPOS_UNDEFINED, 
                         WIN_WIDTH, WIN_HEIGHT, 0);

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, 0);

    SDL_GetRendererInfo(renderer, &info);
    printf("Using %s rendering\n", info.name);

    SDL_Log("+++++ INIT DONE +++++");
    frame = avcodec_alloc_frame();
    if (frame == NULL)
    {
        printf("Cannot allocate pFrame\n");
        exit(-1);
    }
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_YV12, SDL_TEXTUREACCESS_STREAMING, codec_context->width, codec_context->height);//3 plane texture

    if (!texture) {
        fprintf(stderr, "Couldn't set create texture: %s\n", 
                SDL_GetError());
        exit(-1);
    }

    while (av_read_frame(format_context, &avpacket) >= 0) {
        if (avpacket.stream_index == videostream) {
            // Video stream packet
            int frame_finished;

            avcodec_decode_video2(codec_context, frame, &frame_finished, &avpacket);

            if(frame_finished)
            {
                convert_display_frame(texture, frame, renderer);
            }//frame
            av_free_packet(&avpacket);
        }//avpacket

        if (event_handle()) break;
    }//while av_read_frame

    //free frame
    av_free(frame);

    SDL_Log("+++++ FINISHED +++++");
    SDL_Quit();
}

int main(int argc, char * argv[]) {

    AVCodecContext* codec_context;
    int videostream;

    if (argc < 2) {
        printf("Usage: %s filename\n", argv[0]);
        return 0;
    }

    // Register all available file formats and codecs
    av_register_all();

    int err;
    // Init SDL with video support
    err = SDL_Init(SDL_INIT_VIDEO);
    if (err < 0) {
        fprintf(stderr, "Unable to init SDL: %s\n", SDL_GetError());
        return -1;
    }

    // Open video file
    const char* filename = argv[1];
    AVFormatContext* format_context = NULL;
    err = avformat_open_input(&format_context, filename, NULL, NULL);
    if (err < 0) {
        fprintf(stderr, "ffmpeg: Unable to open input\n");
        return -1;
    }

    // Retrieve stream information
    err = avformat_find_stream_info(format_context, NULL);
    if (err < 0) {
        fprintf(stderr, "ffmpeg: Unable to find stream info\n");
        return -1;
    }

    // Dump information about file onto standard error
    av_dump_format(format_context, 0, argv[1], 0);

    // Find the first video stream

    for (videostream = 0; videostream < format_context->nb_streams; 
            ++videostream) {
        if (format_context->streams[videostream]->codec->codec_type == 
                AVMEDIA_TYPE_VIDEO) {
            break;
        }
    }
    if (videostream == format_context->nb_streams) {
        fprintf(stderr, "ffmpeg: Unable to find video stream\n");
        return -1;
    }

    codec_context = format_context->streams[videostream]->codec;
    AVCodec* codec = avcodec_find_decoder(codec_context->codec_id);

    avcodec_alloc_context3(codec);


    if (avcodec_open2(codec_context, codec, NULL) < 0)
    {
        fprintf(stderr, "ffmpeg: Unable to allocate codec context\n");
    }

    else {
        printf("Codec initialized\n");

    }

    /*
       Initializing display
       */
    printf("Width:%d\n",codec_context->width);
    printf("height:%d\n",codec_context->height);
    //exit(0);

    sdl_init(format_context, codec_context,videostream);

    return 0;
}
