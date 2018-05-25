#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/ioctl.h>
#include <assert.h>

#include "inno_fan.h"
#include "pid_controller.h"

#define N_PWM_CHIPS 3

#ifdef CHIP_A6
static const int inno_tsadc_table[] = {
    /* val temp_f */
    652, //-40, 
    645, //-35, 
    638, //-30, 
    631, //-25, 
    623, //-20, 
    616, //-15, 
    609, //-10, 
    601, // -5, 
    594, //  0, 
    587, //  5, 
    579, // 10, 
    572, // 15, 
    564, // 20, 
    557, // 25, 
    550, // 30, 
    542, // 35, 
    535, // 40, 
    527, // 45, 
    520, // 50, 
    512, // 55, 
    505, // 60, 
    498, // 65, 
    490, // 70, 
    483, // 75, 
    475, // 80, 
    468, // 85, 
    460, // 90, 
    453, // 95, 
    445, //100, 
    438, //105, 
    430, //110, 
    423, //115, 
    415, //120, 
    408, //125, 
};
#else
static const int inno_tsadc_table[] = {
    /* val temp_f */
    647, //-40, 
    640, //-35, 
    632, //-30, 
    625, //-25, 
    617, //-20, 
    610, //-15, 
    602, //-10, 
    595, // -5, 
    588, //  0, 
    580, //  5, 
    572, // 10, 
    565, // 15, 
    557, // 20, 
    550, // 25, 
    542, // 30, 
    535, // 35, 
    527, // 40, 
    520, // 45, 
    512, // 50, 
    505, // 55, 
    497, // 60, 
    489, // 65, 
    482, // 70, 
    474, // 75, 
    467, // 80, 
    459, // 85, 
    452, // 90, 
    444, // 95, 
    437, //100, 
    429, //105, 
    421, //110, 
    414, //115, 
    406, //120, 
    399 //125, 
};
#endif

struct chain_temp {
	int initialized, enabled;
	double min, max, avg;
	cgtimer_t time;
};

struct fan_control {
	int direction;
	double high, low;
};

static struct chain_temp chain_temps[ASIC_CHAIN_NUM];
static pthread_mutex_t fancontrol_lock;
static pthread_t fan_tid;
static struct fan_control fan;
static void set_fanspeed(int id, int duty);
static PIDControl temp_pid;
static FILE *pid_log;
static int xxtick;

enum {
	RISING,
	FALLING,
};

static void fancontrol_update_chain_temp(int chain_id, double min, double max, double avg)
{
	struct chain_temp *temp;
	assert(chain_id >= 0);
	assert(chain_id < ASIC_CHAIN_NUM);

	printf("update_chain_temp: chain=%d min=%2.3lf max=%2.3lf avg=%2.3lf\n", chain_id, min, max, avg);

	mutex_lock(&fancontrol_lock);
	temp = &chain_temps[chain_id];
	temp->initialized = 1;
	temp->min = min;
	temp->max = max;
	temp->avg = avg;
	cgtimer_time(&temp->time);
	mutex_unlock(&fancontrol_lock);
}

#define plog(f,a...) ({ if (pid_log) fprintf(pid_log, f "\n", ##a); fflush(pid_log); })

static int calc_duty(void)
{
	struct chain_temp *temp;
	double max, min, avg;
	int n = 0;

	for (int i = 0; i < ASIC_CHAIN_NUM; i++) {
		temp = &chain_temps[i];
		if (temp->enabled) {
			if (!temp->initialized)
				return 0;
			if (n == 0 || temp->min < min)
				min = temp->min;
			if (n == 0 || temp->max > max)
				max = temp->max;
			if (n == 0 || temp->avg > avg)
				avg = temp->avg;
			n++;
		}
	}

	printf("calc_duty: min=%2.3lf max=%2.3lf avg=%2.3lf\n", min, max, avg);
	if (max > 90) {
		plog("# very hot!");
		printf("very hot!\n");
		return 0;
	}


#if 0
	if (fan.direction == RISING) {
		if (avg < fan.high)
			return 50;
		fan.direction = FALLING;
		return 0;
	} else if (fan.direction == FALLING) {
		if (avg > fan.low)
			return 0;
		fan.direction = RISING;
		return 50;
	}
	return 0;
#else
	PIDInputSet(&temp_pid, avg);
	PIDCompute(&temp_pid);
	xxtick++;
	{
		int out = 100 - PIDOutputGet(&temp_pid);
		plog("t=%f min=%f max=%f avg=%f out=%d", xxtick*5.0, min, max, avg, out);
		return out;
	}
#endif
}

static void *fancontrol_thread(void __maybe_unused *argv)
{
	for (;;sleep(5)) {
		int duty;
		mutex_lock(&fancontrol_lock);
		duty = calc_duty();
		mutex_unlock(&fancontrol_lock);
		printf("fancontrol_thread: duty=%d\n", duty);
		set_fanspeed(0, duty);
	}
	return NULL;
}

void fancontrol_start(unsigned enabled_chains)
{
	struct chain_temp *temp;
	pid_log = fopen("/tmp/PID.log", "w");
	mutex_init(&fancontrol_lock);
	set_fanspeed(0, 0);
	for (int i = 0; i < ASIC_CHAIN_NUM; i++) {
		temp = &chain_temps[i];
		temp->enabled = 0;
		temp->initialized = 0;
		if (enabled_chains & (1u << i))
			temp->enabled = 1;
	}
#if 0
	fan.high = 69;
	fan.low = 68;
	fan.direction = RISING;
#else
	{
		float kp = 20;
		float ki = 0.05;
		float kd = 0.1;
		float dt = 5;
		float set_point = 70;

		PIDInit(&temp_pid, kp, ki, kd, dt, 40, 100, AUTOMATIC, REVERSE);
		PIDSetpointSet(&temp_pid, set_point);
		plog("# kp=%f ki=%f kd=%f dt=%f target=%f", kp, ki, kd, dt, set_point);
	}
#endif
	pthread_create(&fan_tid, NULL, fancontrol_thread, NULL);
}


static int inno_fan_temp_compare(const void *a, const void *b);
static void inno_fan_speed_max(INNO_FAN_CTRL_T *fan_ctrl);
static void inno_fan_pwm_set(INNO_FAN_CTRL_T *fan_ctrl, int duty);

void inno_fan_init(INNO_FAN_CTRL_T *fan_ctrl)
{
    int chain_id = 0;

#if 0 /* 测试风扇 */
    int j = 0;

	for(j = 100; j > 0; j -= 10)
	{
        applog_hw(LOG_ERR, "set test duty:%d", j);
        inno_fan_pwm_set(fan_ctrl, j);
        sleep(5);
	}

	for(j = 0; j < 100; j += 10)
    {
        applog_hw(LOG_ERR, "down test duty.");
        inno_fan_speed_down(fan_ctrl);
        sleep(1);
    }

	for(j = 0; j < 100; j += 10)
    {
        applog_hw(LOG_ERR, "up test duty.");
        inno_fan_speed_up(fan_ctrl);
        sleep(1);
    }
#endif
    mutex_init(&fan_ctrl->lock);

    inno_fan_pwm_set(fan_ctrl, 10); /* 90% */
    sleep(1);
    inno_fan_pwm_set(fan_ctrl, 5); /* 95% */

    for(chain_id = 0; chain_id < ASIC_CHAIN_NUM; chain_id++)
    {
        inno_fan_temp_clear(fan_ctrl, chain_id);
    }

    fan_ctrl->temp_nums = sizeof(inno_tsadc_table) / sizeof(inno_tsadc_table[0]);
    fan_ctrl->temp_v_min = inno_tsadc_table[fan_ctrl->temp_nums - 1];
    fan_ctrl->temp_v_max = inno_tsadc_table[0];
    fan_ctrl->temp_f_step = 5.0f;
    fan_ctrl->temp_f_min = -40.0f;
    fan_ctrl->temp_f_max = fan_ctrl->temp_f_min + fan_ctrl->temp_f_step * (fan_ctrl->temp_nums - 1);

	applog_hw(LOG_ERR, "chip nums:%d.", ASIC_CHIP_A_BUCKET);
	applog_hw(LOG_ERR, "pwm  name:%s.", ASIC_INNO_FAN_PWM0_DEVICE_NAME);
	applog_hw(LOG_ERR, "pwm  step:%d.", ASIC_INNO_FAN_PWM_STEP);
	applog_hw(LOG_ERR, "duty max: %d.", ASIC_INNO_FAN_PWM_DUTY_MAX);
	applog_hw(LOG_ERR, "targ freq:%d.", ASIC_INNO_FAN_PWM_FREQ_TARGET);
	applog_hw(LOG_ERR, "freq rate:%d.", ASIC_INNO_FAN_PWM_FREQ);
	applog_hw(LOG_ERR, "max  thrd:%5.2f.", ASIC_INNO_FAN_TEMP_MAX_THRESHOLD);
	applog_hw(LOG_ERR, "up   thrd:%5.2f.", ASIC_INNO_FAN_TEMP_UP_THRESHOLD);
	applog_hw(LOG_ERR, "down thrd:%5.2f.", ASIC_INNO_FAN_TEMP_DOWN_THRESHOLD);
	applog_hw(LOG_ERR, "temp nums:%d.", fan_ctrl->temp_nums);
	applog_hw(LOG_ERR, "temp vmin:%d.", fan_ctrl->temp_v_min);
	applog_hw(LOG_ERR, "temp vmax:%d.", fan_ctrl->temp_v_max);
	applog_hw(LOG_ERR, "temp fstp:%5.2f.", fan_ctrl->temp_f_step);
	applog_hw(LOG_ERR, "temp fmin:%5.2f.", fan_ctrl->temp_f_min);
	applog_hw(LOG_ERR, "temp fmax:%5.2f.", fan_ctrl->temp_f_max);
}

void inno_fan_temp_add(INNO_FAN_CTRL_T *fan_ctrl, int chain_id, int temp, bool warn_on)
{
    float temp_f = 0.0f;
    int index = 0;

    index = fan_ctrl->index[chain_id];

    applog_hw(LOG_DEBUG, "inno_fan_temp_add:chain_%d,chip_%d,temp:%7.4f(%d)", chain_id, index, inno_fan_temp_to_float(fan_ctrl, temp), temp);
    fan_ctrl->temp[chain_id][index] = temp;
    index++;
    fan_ctrl->index[chain_id] = index; 

    temp_f = inno_fan_temp_to_float(fan_ctrl, temp);
    //printf("chain=%d index=%d temp=%f\n", chain_id, index, temp_f);
    /* 避免工作中输出 温度告警信息,影响算力 */
    if(!warn_on)
    {
        return;
    }

    /* 有芯片温度过高,输出告警打印 */
    /* applog_hw(LOG_ERR, "inno_fan_temp_add: temp warn_on(init):%d", warn_on); */
    temp_f = inno_fan_temp_to_float(fan_ctrl, temp);
    if(temp_f > ASIC_INNO_FAN_TEMP_MAX_THRESHOLD)
    { 
        applog_hw(LOG_DEBUG, "inno_fan_temp_add:chain_%d,chip_%d,temp:%7.4f(%d) is too high!", chain_id, index, temp_f, temp);
    }
}

static void inno_fan_temp_sort(INNO_FAN_CTRL_T *fan_ctrl, int chain_id)
{
    int i = 0;
    int temp_nums = 0;

    temp_nums = fan_ctrl->index[chain_id];

    applog_hw(LOG_DEBUG, "not sort:");
    for(i = 0; i < temp_nums; i++)
    {
        applog_hw(LOG_DEBUG, "chip_%d:%08x(%d)", i, fan_ctrl->temp[chain_id][i], fan_ctrl->temp[chain_id][i]);
    }
    applog_hw(LOG_DEBUG, "sorted:");
    qsort(fan_ctrl->temp[chain_id], temp_nums, sizeof(fan_ctrl->temp[chain_id][0]), inno_fan_temp_compare);
    for(i = 0; i < temp_nums; i++)
    {
        applog_hw(LOG_DEBUG, "chip_%d:%08x(%d)", i, fan_ctrl->temp[chain_id][i], fan_ctrl->temp[chain_id][i]);
    }
    applog_hw(LOG_DEBUG, "sort end.");
}

static int inno_fan_temp_get_arvarge(INNO_FAN_CTRL_T *fan_ctrl, int chain_id)
{
    int   i = 0;
    int   temp_nums = 0;
    int   head_index = 0;
    int   tail_index = 0;
    float arvarge_temp = 0.0f; 

    temp_nums = fan_ctrl->index[chain_id];
    /* step1: delete temp (0, ASIC_INNO_FAN_TEMP_MARGIN_NUM) & (max - ASIC_INNO_FAN_TEMP_MARGIN_NUM, max) */
    head_index = temp_nums * ASIC_INNO_FAN_TEMP_MARGIN_RATE;
    tail_index = temp_nums - head_index;
    /* 防止越界 */
    if(head_index < 0)
    {
        head_index = 0;
    }
    if(tail_index < 0)
    {
        tail_index = head_index;
    }

    /* step2: arvarge */
    for(i = head_index; i < tail_index; i++)
    {
        arvarge_temp += fan_ctrl->temp[chain_id][i];
    }
    arvarge_temp /= (tail_index - head_index);

    float temp_f = 0.0f;
    temp_f = inno_fan_temp_to_float(fan_ctrl, (int)arvarge_temp);
	applog_hw(LOG_DEBUG, "inno_fan_temp_get_arvarge, chain_id:%d, temp nums:%d, valid index[%d,%d], reseult:%7.4f(%d).",
            chain_id, temp_nums, head_index, tail_index, inno_fan_temp_to_float(fan_ctrl, (int)arvarge_temp), (int)arvarge_temp); 

    return (int)arvarge_temp;
}

int inno_fan_temp_get_highest(INNO_FAN_CTRL_T *fan_ctrl, int chain_id)
{
    return fan_ctrl->temp[chain_id][0];
}

static int inno_fan_temp_get_lowest(INNO_FAN_CTRL_T *fan_ctrl, int chain_id)
{
    int temp_nums = 0;
    int index = 0;

    temp_nums = fan_ctrl->index[chain_id];
    index = temp_nums - 1;

    /* 避免越界 */
    if(index < 0)
    {
        index = 0;
    }

    return fan_ctrl->temp[chain_id][index];
}

void inno_fan_temp_clear(INNO_FAN_CTRL_T *fan_ctrl, int chain_id)
{
    int i = 0;

    fan_ctrl->index[chain_id] = 0;
    for(i = 0; i < ASIC_CHIP_NUM; i++)
    {
        fan_ctrl->temp[chain_id][i] = 0;
    }
}

void inno_fan_temp_init(INNO_FAN_CTRL_T *fan_ctrl, int chain_id)
{
    int temp = 0;

    inno_fan_temp_sort(fan_ctrl, chain_id);

    temp = inno_fan_temp_get_arvarge(fan_ctrl, chain_id);
    fan_ctrl->temp_init[chain_id] = temp;
    fan_ctrl->temp_arvarge[chain_id] = temp;

    temp = inno_fan_temp_get_highest(fan_ctrl, chain_id);
    fan_ctrl->temp_highest[chain_id] = temp;

    temp = inno_fan_temp_get_lowest(fan_ctrl, chain_id);
    fan_ctrl->temp_lowest[chain_id] = temp;

    inno_fan_temp_clear(fan_ctrl, chain_id);
}

static int write_to_file(const char *path_fmt, int id, const char *data_fmt, ...)
{
	char path[256];
	char data[256];
	va_list ap;
	int fd;
	size_t len;

	snprintf(path, sizeof(path), path_fmt,id);

	va_start(ap, data_fmt);
	vsnprintf(data, sizeof(data), data_fmt, ap);
	va_end(ap);

	a5_debug("writefile: %s <- \"%s\"\n", path, data);

	fd = open(path, O_WRONLY);
	if (fd < 0) {
		applog_hw(LOG_ERR, "open of %s failed", path);
		return 0;
	}
	len = strlen(data);
	if (write(fd, data, len) != len) {
		applog_hw(LOG_ERR, "short write to %s", path);
		return 0;
	}
	close(fd);
	return 1;
}

#define PWMCHIP_SYSFS "/sys/class/pwm/pwmchip%d"
#define PWMCHIP_PERIOD 100000

static int pwm_chip_initialized[N_PWM_CHIPS];

static void set_fanspeed(int id, int duty)
{
	assert(id >= 0 && id < N_PWM_CHIPS);
	if (!pwm_chip_initialized[id]) {
		pwm_chip_initialized[id] = 1;
		write_to_file(PWMCHIP_SYSFS "/export", id, "0");
		write_to_file(PWMCHIP_SYSFS "/pwm0/period", id, "%d", PWMCHIP_PERIOD);
		write_to_file(PWMCHIP_SYSFS "/pwm0/duty_cycle", id, "10000");
		write_to_file(PWMCHIP_SYSFS "/pwm0/enable", id, "1");
	}
	/* do not set the extreme values (full-cycle and no-cycle) just in case */
	/* it shouldn't be buggy, but if it would, setting duty-cycle to 100
	   would effectively stop the fan and cause miner to overheat and
	   explode. */
	write_to_file(PWMCHIP_SYSFS "/pwm0/duty_cycle", id, "%d", duty*PWMCHIP_PERIOD/100);
}



void inno_fan_pwm_set(INNO_FAN_CTRL_T *fan_ctrl, int duty)
{
    mutex_lock(&fan_ctrl->lock);
#if 0
    /* control all three PWM generators in sync */
    set_fanspeed(0, duty);
    set_fanspeed(1, duty);
    set_fanspeed(2, duty);
#endif
    fan_ctrl->duty = duty;
    mutex_unlock(&fan_ctrl->lock);
}

void inno_fan_speed_up(INNO_FAN_CTRL_T *fan_ctrl)
{
    int duty = 0;
    
    /* 已经到达最大值,不调 */
    if(0 == fan_ctrl->duty)
    {
        return;
    }

    duty = fan_ctrl->duty;
    duty -= ASIC_INNO_FAN_PWM_STEP;
    if(duty < 0)
    {
        duty = 0;
    } 
    applog_hw(LOG_DEBUG, "speed+(%02d%% to %02d%%)" , 100 - fan_ctrl->duty, 100 - duty);

    inno_fan_pwm_set(fan_ctrl, duty);
}

void inno_fan_speed_down(INNO_FAN_CTRL_T *fan_ctrl)
{
    int duty = 0;

    /* 已经到达最小值,不调 */
    if(ASIC_INNO_FAN_PWM_DUTY_MAX == fan_ctrl->duty)
    {
        return;
    }

    duty = fan_ctrl->duty;
    duty += ASIC_INNO_FAN_PWM_STEP;
    if(duty > ASIC_INNO_FAN_PWM_DUTY_MAX)
    {
        duty = ASIC_INNO_FAN_PWM_DUTY_MAX;
    }
    applog_hw(LOG_DEBUG, "speed-(%02d%% to %02d%%)" , 100 - fan_ctrl->duty, 100 - duty);

    inno_fan_pwm_set(fan_ctrl, duty);
}

#ifndef CHIP_A6
extern uint8_t A1Pll1;
extern uint8_t A1Pll2;
extern uint8_t A1Pll3;
extern const struct PLL_Clock PLL_Clk_12Mhz[142];
extern struct A1_chain *chain[ASIC_CHAIN_NUM];
#endif

void inno_fan_speed_update(INNO_FAN_CTRL_T *fan_ctrl, int chain_id, struct cgpu_info *cgpu)
{

	struct A1_chain *a1 = cgpu->device_data;

    int arvarge = 0;        /* 平均温度 */
    int highest = 0;        /* 最高温度 */
    int lowest  = 0;        /* 最低温度 */

    float arvarge_f = 0.0f; /* 最高温度 */
    float highest_f = 0.0f; /* 最高温度 */
    float lowest_f  = 0.0f; /* 最低温度 */
    float avg2;

    /* 统计温度 */
    inno_fan_temp_sort(fan_ctrl, chain_id);
    arvarge = inno_fan_temp_get_arvarge(fan_ctrl, chain_id);
    highest = inno_fan_temp_get_highest(fan_ctrl, chain_id);
    lowest  = inno_fan_temp_get_lowest(fan_ctrl, chain_id);

#if 0
    /* 控制策略1(否定,temp_init温度很低,风扇最大依然不够)
     * temp_init为初始值
     * temp_now - temp_init;
     *
     * temp_now > temp_init 表示 temp 较低, speed down
     * else                 表示 temp 较高, speed up
     */
    if(fan_ctrl->temp_now[chain_id] > fan_ctrl->temp_init[chain_id])
    {
        //inno_fan_speed_down();
        applog_hw(LOG_ERR, "- to %d" , fan_ctrl->duty);
    }
    else
    {
        //inno_fan_speed_up();
        applog_hw(LOG_ERR, "+ to %d" , fan_ctrl->duty);
    }
#endif

#if 0
    /* 控制策略2 OK(效果不好)
     * temp_delta为与上次温度的差值
     *
     * temp_delta > 0 表示 temp 较低, speed down
     * else           表示 temp 较高, speed up
     */
#endif

#if 1
    /* 控制策略3
     * 
     * 最高温度 highest > 80度(<475) speed up
     * 最高温度 highest < 50度(>520) speed down
     *
     * 最高温度 highest > 90度(<460) speed max
     *
     */
    {
	    int max = fan_ctrl->index[chain_id];
	    avg2 = 0;
	    if (max > 0) {
		    for (int i = 0; i < max; i++) {
			    avg2 += inno_fan_temp_to_float(fan_ctrl, fan_ctrl->temp[chain_id][i]);
		    }
		    avg2 /= fan_ctrl->index[chain_id];
	    }
    }
    /* 清空,为下一轮做准备 */
    inno_fan_temp_clear(fan_ctrl, chain_id);

    arvarge_f = inno_fan_temp_to_float(fan_ctrl, (int)arvarge);
    lowest_f = inno_fan_temp_to_float(fan_ctrl, (int)lowest);
    highest_f = inno_fan_temp_to_float(fan_ctrl, (int)highest);

    /* 加入整条链Power Down */
    if(highest_f > ASIC_INNO_FAN_TEMP_MAX_THRESHOLD)
    {
        applog_hw(LOG_ERR, "%s z:arv:%5.2f, lest:%5.2f, hest:%5.2f, power down", __func__, arvarge_f, lowest_f, highest_f);
    }

    /* 温度过高 */
    if(highest_f > ASIC_INNO_FAN_TEMP_UP_THRESHOLD)
    {
        if(0 != fan_ctrl->duty)
        {
            inno_fan_pwm_set(fan_ctrl, 0);
            applog_hw(LOG_ERR, "%s +:arv:%5.2f, lest:%5.2f, hest:%5.2f, speed:%d%%", __func__, arvarge_f, lowest_f, highest_f, 100 - fan_ctrl->duty);
        } 
    }

    /* 温度已经恢复低温 */
    if(highest_f < ASIC_INNO_FAN_TEMP_DOWN_THRESHOLD)
    {
        if(40 != fan_ctrl->duty) 
        {
            inno_fan_pwm_set(fan_ctrl, 40);
            applog_hw(LOG_ERR, "%s -:arv:%5.2f, lest:%5.2f, hest:%5.2f, speed:%d%%", __func__, arvarge_f, lowest_f, highest_f, 100 - fan_ctrl->duty);
        }
    } 

    fancontrol_update_chain_temp(chain_id, lowest_f, highest_f, avg2);

    cgpu->temp = arvarge_f;
    cgpu->temp_max = highest_f;
    cgpu->temp_min = lowest_f;
    cgpu->fan_duty = 100 - fan_ctrl->duty;
            
    cgpu->chip_num = a1->num_active_chips;
    cgpu->core_num = a1->num_cores;
#ifndef CHIP_A6
	switch(a1->chain_id){
		case 0:cgpu->mhs_av = (double)PLL_Clk_12Mhz[A1Pll1].speedMHz * 2ull * (a1->num_cores);break;
		case 1:cgpu->mhs_av = (double)PLL_Clk_12Mhz[A1Pll2].speedMHz * 2ull * (a1->num_cores);break;
		case 2:cgpu->mhs_av = (double)PLL_Clk_12Mhz[A1Pll3].speedMHz * 2ull * (a1->num_cores);break;
		default:;
	}
#endif
    //static int times = 0;       /* 降低风扇控制的频率 */
    /* 降低风扇打印的频率 */
    //if(times++ <  ASIC_INNO_FAN_CTLR_FREQ_DIV)
    //{
    //    return;
    //}
    /* applog_hw(LOG_DEBUG, "inno_fan_speed_updat times:%d" , times); */
    //times = 0;

    //applog_hw(LOG_ERR, "%s n:arv:%5.2f, lest:%5.2f, hest:%5.2f", __func__, arvarge_f, lowest_f, highest_f);
#endif

}

static void inno_fan_speed_max(INNO_FAN_CTRL_T *fan_ctrl)
{
    inno_fan_pwm_set(fan_ctrl, 0);
}

static int inno_fan_temp_compare(const void *a, const void *b)
{
    return *(int *)a - *(int *)b;
}

/**
 * Converts raw ADC value via inno_tsadc_table to actual temperature (float)
 *
 * The conversion uses a lookup table and interpolates a precise value once start/end points are found
 *
 * @param fan_ctrl
 * @param temp - raw ADC value
 * @return temperature in degrees of Celcius
 */
float inno_fan_temp_to_float(INNO_FAN_CTRL_T *fan_ctrl, int temp)
{
    int i = 0;
    int i_max = 0;
    float temp_f_min = 0.0f;
    float temp_f_step = 0.0f;

    float temp_f_start = 0.0f;
    float temp_f_end = 0.0f;
    int temp_v_start = 0;
    int temp_v_end = 0;
    float temp_f = 0.0f;

    /* 避免越界 */
    if(temp < fan_ctrl->temp_v_min)
    {
        return 9999.0f;
    }
    if(temp > fan_ctrl->temp_v_max)
    {
        return -9999.0f;
    } 
    
    /* 缩小范围 头和尾已经处理  */
    i_max = fan_ctrl->temp_nums;
    for(i = 1; i < i_max - 1; i++)
    {
        if(temp > inno_tsadc_table[i])
        {
            break;
        }
    }

    temp_f_min = fan_ctrl->temp_f_min;
    temp_f_step = fan_ctrl->temp_f_step;

    /* 分段线性,按照线性比例计算:
     *
     * (x - temp_f_start) / temp_f_step = (temp - temp_v_start) / (temp_v_end - temp_v_start)
     *
     * x = temp_f_start + temp_f_step * (temp - temp_v_start) / (temp_v_end - temp_v_start)
     *
     * */
    temp_f_end = temp_f_min + i * temp_f_step;
    temp_f_start = temp_f_end - temp_f_step;
    temp_v_start = inno_tsadc_table[i - 1];
    temp_v_end = inno_tsadc_table[i]; 

    temp_f = temp_f_start + temp_f_step * (temp - temp_v_start) / (temp_v_end - temp_v_start);

    applog_hw(LOG_DEBUG, "inno_fan_temp_to_float: temp:%d,%d,%d, %7.4f,%7.4f" , temp,
            temp_v_start, temp_v_end, temp_f_start, temp_f_end);
    applog_hw(LOG_DEBUG, "inno_fan_temp_to_float: :%7.4f,%7.4f,%d,%d",
            temp_f_start, temp_f_step,
            temp - temp_v_start, temp_v_end - temp_v_start);

    return temp_f;
}

#ifndef CHIP_A6
void inno_temp_contrl(INNO_FAN_CTRL_T *fan_ctrl, struct A1_chain *a1, int chain_id)
{
	int i;
	int arvarge = 0;
    float arvarge_f = 0.0f; 
	uint8_t reg[REG_LENGTH];

	arvarge_f = inno_fan_temp_to_float(fan_ctrl, fan_ctrl->temp_arvarge[chain_id]);

	if(arvarge_f >= ASIC_INNO_TEMP_CONTRL_THRESHOLD)
	{
		return;
	}

	while(arvarge_f < ASIC_INNO_TEMP_CONTRL_THRESHOLD)
	{
		for (i = a1->num_active_chips; i > 0; i--) 
		{	
			if (!inno_cmd_read_reg(a1, i, reg)) 
			{
				applog_hw(LOG_ERR, "%d: Failed to read temperature sensor register for chip %d ", a1->chain_id, i);
				continue;
			}
			/* update temp database */
            uint32_t temp = 0;
            float    temp_f = 0.0f;

            temp = 0x000003ff & ((reg[7] << 8) | reg[8]);
            inno_fan_temp_add(fan_ctrl, a1->chain_id, temp, false);
		} 

		inno_fan_temp_init(fan_ctrl, a1->chain_id);
		arvarge_f = inno_fan_temp_to_float(fan_ctrl, fan_ctrl->temp_arvarge[a1->chain_id]);
		applog_hw(LOG_WARNING, "%s +:arv:%7.4f. \t", __func__, arvarge_f);
		inno_fan_pwm_set(fan_ctrl, 100);
		sleep(1);
	}
	
	if(!inno_cmd_resetbist(a1, ADDR_BROADCAST))
	{
		applog_hw(LOG_WARNING, "reset bist failed!");
		return;
	}
	sleep(1);

	uint8_t buffer[64];
	uint8_t temp_reg[REG_LENGTH];
	
	memset(buffer, 0, sizeof(buffer));
	if(!inno_cmd_bist_start(a1, 0, buffer))
	{
		applog_hw(LOG_WARNING, "Reset bist but bist start fail");
	}

	if(buffer[3] != 0)
	{
		a1->num_chips = buffer[3];
	}
	
	applog_hw(LOG_WARNING, "%d: detected %d chips", chain_id, a1->num_chips);
	
	usleep(10000);

	if (!inno_cmd_bist_fix(a1, ADDR_BROADCAST))
	{
		applog_hw(LOG_WARNING, "Reset bist but inno_cmd_bist_fix failed!");
	}

	usleep(200);

	a1->num_cores = 0;

	for (i = 0; i < a1->num_active_chips; i++)
    {
		check_chip(a1, i);
    }
}
#endif
