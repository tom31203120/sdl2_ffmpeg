#include <stdio.h>

#include "calc_score.h"
#include "portaudio.h"

/* #define SAMPLE_RATE  (17932) // Test failure to open with this value. */
#define SAMPLE_RATE  (44100)
#define FRAMES_PER_BUFFER (512)
#define NUM_CHANNELS    (1)

/* Select sample format. */
/*#if 1*/
/*#define PA_SAMPLE_TYPE  paFloat32*/
/*typedef float SAMPLE;*/
/*#define SAMPLE_SILENCE  (0.0f)*/
/*#define PRINTF_S_FORMAT "%.8f"*/
/*#elif 1*/
#define PA_SAMPLE_TYPE  paInt16
typedef short SAMPLE;
#define SAMPLE_SILENCE  (0)
#define PRINTF_S_FORMAT "%d"

typedef struct
{
    double       cur_time;
    CalcCtx     *pCtx;
}
CbRecordData;
typedef struct
{
    int          nm_frames;
    SAMPLE      *record_frames;
}
RecordData;

static int record_callback( const void *inputBuffer, void *outputBuffer,
        unsigned long framesPerBuffer,
        const PaStreamCallbackTimeInfo* timeInfo,
        PaStreamCallbackFlags statusFlags,
        void *userData )
{
    CbRecordData *data = (CbRecordData*)userData;

    (void) outputBuffer; /* Prevent unused variable warnings. */
    (void) timeInfo;
    (void) statusFlags;
    (void) userData;

    /*printf("data->cur_time=%lf, data->pCtx\n", data->cur_time, data->pCtx);*/
    int get_score = calc_score((short *)inputBuffer, framesPerBuffer, data->cur_time, data->pCtx);
    if (get_score){
        printf("total score = %0.2lf, current score = %0.2lf\n", data->pCtx->total_score*100, data->pCtx->cur_score*100);

        printf("total num= %d, current num= %d\n", data->pCtx->total_rec_num, data->pCtx->idx_rec_num);
        printf("===============================================\n");
      
    }

    data->cur_time += 1.0*framesPerBuffer/SAMPLE_RATE;
    /*printf("cur_time = %lf\n", data->cur_time);*/

    if (finish_calc(data->pCtx)){
        return paComplete;
    }else{
        return paContinue;
    }
}

int start_portaudio(CalcCtx *pInCtx)
{
    PaStreamParameters  inputParameters;
    PaStream*           stream;
    CbRecordData        data;

    data.cur_time = 0;
    data.pCtx     = pInCtx;

    PaError err = Pa_Initialize();
    if( err != paNoError ) goto done;

    inputParameters.device = Pa_GetDefaultInputDevice(); /* default input device */
    if (inputParameters.device == paNoDevice) {
        fprintf(stderr,"Error: No default input device.\n");
        goto done;
    }
    inputParameters.channelCount = NUM_CHANNELS;                    /* stereo input */
    inputParameters.sampleFormat = PA_SAMPLE_TYPE;
    inputParameters.suggestedLatency = Pa_GetDeviceInfo( inputParameters.device )->defaultLowInputLatency;
    inputParameters.hostApiSpecificStreamInfo = NULL;

    /* Record some audio. -------------------------------------------- */
    err = Pa_OpenStream(
            &stream,
            &inputParameters,
            NULL,                  /* &outputParameters, */
            SAMPLE_RATE,
            FRAMES_PER_BUFFER,
            paClipOff,      /* we won't output out of range samples so don't bother clipping them */
            record_callback,
            &data );
    if( err != paNoError ) goto done;

    err = Pa_StartStream( stream );
    if( err != paNoError ) goto done;

    while( ( err = Pa_IsStreamActive( stream ) ) == 1 ){Pa_Sleep(500);}
    if( err < 0 ) goto done;

    err = Pa_CloseStream( stream );
    if( err != paNoError ) goto done;

done:
    Pa_Terminate();
    if( err != paNoError )
    {
        fprintf( stderr, "An error occured while using the portaudio stream\n" );
        fprintf( stderr, "Error number: %d\n", err );
        fprintf( stderr, "Error message: %s\n", Pa_GetErrorText( err ) );
        err = 1;          /* Always return 0 or 1, but no other return codes. */
    }
    return err;
}

int main(int argc, char *argv[]) {
    CalcCtx *pCtx = NULL;
    char music_name[]="test.json";
    int res = calc_init(music_name, &pCtx);
    if (res == -1 || NULL == pCtx)
    {
        return -1;
    }

    (void)start_portaudio(pCtx);

    calc_uninit(pCtx);

    return 0;
}

