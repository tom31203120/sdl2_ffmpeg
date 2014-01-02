// tutorial01.c
//
// This tutorial was written by Stephen Dranger (dranger@gmail.com).
//
// Code based on a tutorial by Martin Bohme (boehme@inb.uni-luebeckREMOVETHIS.de)
// Tested on Gentoo, CVS version 5/01/07 compiled with GCC 4.1.1

// A small sample program that shows how to use libavformat and libavcodec to
// read video from a file.
//
// Use the Makefile to build all examples.
//
// Run using
//
// tutorial01 myvideofile.mpg
//
// to write the first five frames from "myvideofile.mpg" to disk in PPM
// format.

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>

#include <stdio.h>

typedef unsigned char ID[4];

typedef struct
{
    ID             chunkID;  /* {'f', 'm', 't', ' '} */
    long           chunkSize;

    short          wFormatTag;
    unsigned short wChannels;
    unsigned long  dwSamplesPerSec;
    unsigned long  dwAvgBytesPerSec;
    unsigned short wBlockAlign;
    unsigned short wBitsPerSample;
    /* Note: there may be additional fields here, */
    /*depending upon wFormatTag. */
} FormatChunk;

typedef struct
{
    ID             chunkID;  /* {'d', 'a', 't', 'a'}  */
    long           chunkSize;
    unsigned char  waveformData[];
} DataChunk;

int write_wave_header(char *file_name, int nb_channels, 
                      int samplerate, int samplewidth, int pcmfile_size)
{
    FILE *pcmfile, *wavfile;
    FormatChunk formatchunk;
    DataChunk   datachunk;

    wavfile = fopen(file_name, "wb");
    if (wavfile == NULL) {
        printf("!Error: Can't create wavfile.\n");
        return 1;
    }

    fwrite("RIFF", 1, 4, wavfile);
    fwrite("xxxx", 1, 4, wavfile);  //reserved for the total chunk size
    fwrite("WAVE", 1, 4, wavfile);

    formatchunk.chunkID[0] = 'f';
    formatchunk.chunkID[1] = 'm';
    formatchunk.chunkID[2] = 't';
    formatchunk.chunkID[3] = ' ';
    formatchunk.chunkSize  = sizeof(FormatChunk) - sizeof(ID) - sizeof(long);
    formatchunk.wFormatTag = 1;   /* uncompressed */
    formatchunk.wChannels = nb_channels;
    formatchunk.dwSamplesPerSec = samplerate;
    formatchunk.wBitsPerSample = samplewidth;
    formatchunk.wBlockAlign = formatchunk.wChannels * (formatchunk.wBitsPerSample >> 3);
    formatchunk.dwAvgBytesPerSec =  formatchunk.wBlockAlign * formatchunk.dwSamplesPerSec;
    fwrite(&formatchunk, 1, sizeof(formatchunk), wavfile);

    datachunk.chunkID[0] = 'd';
    datachunk.chunkID[1] = 'a';
    datachunk.chunkID[2] = 't';
    datachunk.chunkID[3] = 'a';
    datachunk.chunkSize = pcmfile_size;
    fwrite(&datachunk, 1, sizeof(ID)+sizeof(long), wavfile);

    return 0;
}

void save_frame(AVFrame *pFrame) 
{
    FILE *pFile;
    char szFilename[32];
    int  y;

    // Open file
    sprintf(szFilename, "framed.ppm");
    pFile=fopen(szFilename, "wb");
    if(pFile==NULL)
        return;

    /*// Write header*/
    /*fprintf(pFile, "P6\n%d %d\n255\n", width, height);*/

    /*// Write pixel data*/
    /*for(y=0; y<height; y++)*/
    /*fwrite(pFrame->data[0]+y*pFrame->linesize[0], 1, width*3, pFile);*/

    // Close file
    fclose(pFile);
}

int split_audio_file(AVFormatContext *pFormatCtx)
{
    // Find the first audio stream
    int audioStream=-1;
    int i;
    for(i=0; i<pFormatCtx->nb_streams; i++){
        if(pFormatCtx->streams[i]->codec->codec_type==AVMEDIA_TYPE_AUDIO) {
            audioStream=i;
            break;
        }
    }

    if(audioStream==-1)
        return -1; // Didn't find a audio stream

    printf("audioStream id = %d\n", audioStream);

    AVCodecContext  *pCodecCtx = NULL;
    AVCodec         *pCodec = NULL;
    AVDictionary    *optionsDict = NULL;
    AVFrame         *pFrame = NULL; 
    AVPacket        packet;
    int             frameFinished = 0;
    // Get a pointer to the codec context for the audio stream
    pCodecCtx=pFormatCtx->streams[audioStream]->codec;

    // Find the decoder for the audio stream
    pCodec=avcodec_find_decoder(pCodecCtx->codec_id);
    if(pCodec==NULL) {
        fprintf(stderr, "Unsupported codec!\n");
        return -1; // Codec not found
    }
    // Open codec
    if(avcodec_open2(pCodecCtx, pCodec, &optionsDict)<0)
        return -1; // Could not open codec

    // Allocate audio frame
    pFrame=avcodec_alloc_frame();
    if(pFrame==NULL)
        return -1;

    /*write_wave_header(argv[2]);*/
    while(av_read_frame(pFormatCtx, &packet)>=0) {
        // Is this a packet from the video stream?
        if(packet.stream_index==audioStream) {
            // Decode video frame
            avcodec_decode_audio4(pCodecCtx, pFrame, &frameFinished, &packet);
            if (frameFinished){
                save_frame(pFrame);
            }
        }
        // Free the packet that was allocated by av_read_frame
        av_free_packet(&packet);
    }

    // Free the YUV frame
    av_free(pFrame);

    // Close the codec
    avcodec_close(pCodecCtx);

    return 0;
}

int main(int argc, char *argv[]) {
    AVFormatContext *pFormatCtx = NULL;
    if(argc < 2) {
        printf("Please provide a movie file\n");
        return -1;
    }
    // Register all formats and codecs
    av_register_all();

    // Open video file
    if(avformat_open_input(&pFormatCtx, argv[1], NULL, NULL)!=0)
        return -1; // Couldn't open file

    // Retrieve stream information
    if(avformat_find_stream_info(pFormatCtx, NULL)<0)
        return -1; // Couldn't find stream information

    // Dump information about file onto standard error
    av_dump_format(pFormatCtx, 0, argv[1], 0);

    split_audio_file(pFormatCtx);

    // Close the video file
    avformat_close_input(&pFormatCtx);

    return 0;

}
