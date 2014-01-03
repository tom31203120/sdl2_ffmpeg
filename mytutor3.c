#include "portaudio.h"
#include <stdio.h>

#include <libavcodec/avcodec.h>

typedef struct slist{
    int    end_time;
    double avg_val;
    struct slist *pnext;
}S_LIST;

typedef struct {
    double avg_max;//区间内所有帧的平均值的最大值
    double avg_mid;
    double avg_min;

    double total_score;     //总体得分
    double cur_score;       //当前得分
    int    idx_rec_num;     //分数记录点位置
    int    total_rec_num;     //记录点总数

    uint16_t max_frame;//单帧的最大值 
    S_LIST *map_list;
}ClacCtx;

S_LIST *get_head_node(S_LIST *plist, int start_time)
{
    S_LIST *pnode = plist->pnext;
    while(pnode->end_time < start_time)
    {
        plist->pnext = pnode->pnext;
        free(pnode);
        pnode = plist->pnext;
    }
    return pnode;
}
int calc_init(char *music_name, ClacCtx **pOutCtx);
void calc_uninit(ClacCtx *pInCtx);
int calc_score(uint16_t *frame_stream, int len, int start_time, ClacCtx *pCtx);

int calc_init(char *music_name, ClacCtx **pOutCtx)
{
    FILE *fp = fopen(music_name,"rb");
    if (!fp)
    {
        //get_map_file_from_server
        printf("could not find the Muisic Data File\n");
        return -1;
    }

    //construct the 
    ClacCtx *pCtx = (ClacCtx*)malloc(sizeof(ClacCtx));
    if (!pCtx)
    {
        printf("error malloc \n");
        return -1;
    }
    memset(pCtx, 0, sizeof(ClacCtx));

    //construct the list from fp data
    S_LIST *plist = (S_LIST *)malloc(sizeof(S_LIST));
    if (!plist)
    {
        printf("error malloc \n");
        return -1;
    }
    memset(plist, 0, sizeof(S_LIST));

    //.......

    pCtx->map_list = plist;
    *pOutCtx = pCtx;
    return 0;
}

void calc_uninit(ClacCtx *pInCtx)
{
    // free list
    S_LIST *pdel, *pnode = pInCtx->map_list;
    while(pnode->pnext)
    {
        pdel = pnode->pnext;
        free(pdel);
        pnode = pdel;
    }
    free(pnode);

    free(pInCtx);
    return;
}

#define BINARY_COUNT 10
int bin_zation(double val, ClacCtx* pCtx)
{
    int binarys= 0, num_gap;
    double maximal = pCtx->avg_max;
    double mean    = pCtx->avg_mid;
    double minimal = pCtx->avg_min;
    double top, bottom, top_gap, bottom_gap; 

    top    = maximal - mean;
    bottom = mean - minimal;

    top_gap = top * 2 / BINARY_COUNT;
    bottom_gap = bottom * 2 / BINARY_COUNT;

    if (val > mean){
        num_gap = (int)((val - mean) / top_gap);
        binarys = BINARY_COUNT / 2 + num_gap + 1;
    }
    else{
        num_gap = (int)((mean - val) / bottom_gap);
        binarys = BINARY_COUNT / 2 - num_gap + 1;
    }

    return binarys;
}

int calc_score(uint16_t *frame_stream, int len, int start_time, ClacCtx *pCtx)
{
    double sum=0, avg_val = 0;    
    double score = 0;
    S_LIST *pHead = get_head_node(pCtx->map_list, start_time);
    if (pHead && start_time>pHead->end_time)
    {
        for(int i;i<len;i++)
        {
            sum += frame_stream[i]/pCtx->max_frame;
            /*avg_val = (avg_val*(i-1)+frame_stream[i])/i;*/
            /*avg_val = avg_val*(i-1)/i+frame_stream[i]/i;*/
        }
        avg_val = sum / len;
        //二值化数据1~10
        score = bin_zation(avg_val, pCtx);
        //和服务器端的计算结果比较
        score = avg_val/pHead->avg_val;
        if (score > 1) score = 2 - score;
        //记录当前得分，并综合总得分
        pCtx->cur_score   = score;
        pCtx->total_score = \
            (pCtx->cur_score + pCtx->total_score*(pCtx->idx_rec_num-1))/pCtx->idx_rec_num;

        //维护上下文，方便下次计算
        pCtx->idx_rec_num ++;
        pCtx->map_list->pnext = pHead->pnext;
        free(pHead);
        return 1;
    }
    return 0;
}


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
    ClacCtx     *pCtx;
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
    const SAMPLE *rptr = (const SAMPLE*)inputBuffer;

    (void) outputBuffer; /* Prevent unused variable warnings. */
    (void) timeInfo;
    (void) statusFlags;
    (void) userData;

    int get_score = calc_score((uint16_t *)inputBuffer, framesPerBuffer, data->cur_time, data->pCtx);
    if (get_score){
        printf("total score = %lf, current score = %lf\n", data->pCtx->total_score, data->pCtx->cur_score);
    }

    data->cur_time += 1.0*framesPerBuffer/SAMPLE_RATE;
    /*printf("cur_time = %lf\n", data->cur_time);*/

    if (data->pCtx->idx_rec_num >= data->pCtx->total_rec_num){
        return paComplete;
    }else{
        return paContinue;
    }
}

int start_portaudio(ClacCtx *pInCtx)
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
    ClacCtx *pCtx = NULL;
    char music_name[]="大海.wav";
    int res = calc_init(music_name, &pCtx);
    if (res == -1 || NULL == pCtx)
    {
        return -1;
    }

    (void)start_portaudio(pCtx);

    calc_uninit(pCtx);

    return 0;
}

