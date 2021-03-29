/*
 * CTU/EMBO - EMBedded Oscilloscope <github.com/parezj/EMBO>
 * Author: Jakub Parez <parez.jakub@gmail.com>
 */

#ifndef CONTAINERS_H
#define CONTAINERS_H

#include <QString>

enum MsgBoxType
{
    INFO,
    QUESTION,
    WARNING,
    CRITICAL
};

enum State
{
    DISCONNECTED,
    OPENING,
    CONNECTING1,
    CONNECTING2,
    CONNECTED
};

enum Mode
{
    NO_MODE = 0,
    VM = 1,
    SCOPE = 2,
    LA = 3
};

enum Ready
{
    NOT_READY = 0,
    READY_AUTO = 1,
    READY_NORMAL = 2,
    READY_DISABLED = 3
};

enum SgenMode
{
    CONSTANT = 0,
    SINE = 1,
    TRIANGLE = 2,
    SAWTOOTH = 3,
    SQUARE = 4,
    NOISE = 5
};

enum DaqBits
{
    B1,
    B8,
    B12
};

enum DaqTrigEdge
{
    RISING,
    FALLING
};

enum DaqTrigMode
{
    AUTO,
    NORMAL,
    SINGLE,
    DISABLED
};

class DevInfo
{
public:
    /* IDN */
    QString name;
    QString fw;

    /* SYS:INFO */
    QString rtos;
    QString ll;
    QString comm;
    QString fcpu;
    int ref_mv;
    QString pins_scope_vm;
    QString pins_la;
    QString pins_cntr;
    QString pins_pwm;
    QString pins_sgen;

    /* SYS:LIM */
    int adc_fs_12b;
    int adc_fs_8b;
    int mem;
    int la_fs;
    int pwm_fs;
    int adc_num;
    bool adc_dualmode;
    bool adc_interleaved;
    bool adc_bit8;
    bool dac;
    int vm_mem;
    int vm_fs;
    int cntr_timeout;
    int sgen_maxf;
    int sgen_maxmem;
};

class DaqSettings
{
public:
    /* common */
    DaqBits bits;
    int mem;
    int fs;
    bool ch1_en;
    bool ch2_en;
    bool ch3_en;
    bool ch4_en;

    /* trigger */
    int trig_ch;
    int trig_val;
    DaqTrigEdge trig_edge;
    DaqTrigMode trig_mode;
    int trig_pre;

    /* read-only */
    double maxZ_kohm;
    double fs_real;
};

#endif // CONTAINERS_H