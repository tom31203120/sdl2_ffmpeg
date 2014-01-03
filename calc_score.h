
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

