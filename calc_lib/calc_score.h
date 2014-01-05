
#define BINARY_COUNT 10

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

    short max_frame;//单帧的最大值 
    S_LIST *map_list;
}CalcCtx;

int calc_init(char *music_name, CalcCtx **pOutCtx);
void calc_uninit(CalcCtx *pInCtx);
int calc_score(short *frame_stream, int len, int start_time, CalcCtx *pCtx);
int finish_calc(CalcCtx *pCtx);

