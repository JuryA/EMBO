/*
 * CTU/EMBO - EMBedded Oscilloscope <github.com/parezj/EMBO>
 * Author: Jakub Parez <parez.jakub@gmail.com>
 */


#include "cfg.h"
#include "comm_proto.h"

#include "app_data.h"
#include "main.h"
#include "utility.h"

#include "FreeRTOS.h"
#include "task.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/************************* [IEEE 488] *************************/

scpi_result_t EM_Reset(scpi_t* context)
{
    const char* p1;
    size_t p1l;

    SCPI_ParamCharacters(context, &p1, &p1l, FALSE);

    daq_enable(&daq, EM_FALSE);
    daq_mode_set(&daq, VM);
    daq_enable(&daq, EM_TRUE);

    if (p1l == 1 && p1[0] == 'S')
    {
        daq_settings_init(&daq, EM_TRUE, EM_FALSE);
        daq_mode_set(&daq, SCOPE);
    }

    else if (p1l == 1 && p1[0] == 'L')
    {
        daq_settings_init(&daq, EM_FALSE, EM_TRUE);
        daq_mode_set(&daq, LA);
    }
    else
    {
        daq_settings_init(&daq, EM_TRUE, EM_TRUE);

        cntr_enable(&cntr, EM_FALSE, EM_FALSE);
        pwm_disable(&pwm);
#ifdef EM_DAC
        sgen_disable(&sgen);
#endif

#ifdef EM_DEBUG
        //pwm_set(&pwm, 1000, 50, 50, 50, EM_TRUE, EM_TRUE);
#endif
    }

    SCPI_ResultText(context, SCPI_OK);
    return SCPI_RES_OK;
}

/************************* [System Actions] *************************/

scpi_result_t EM_SYS_Mode(scpi_t* context)
{
    const char* p1;
    size_t p1l;

    if (!SCPI_ParamCharacters(context, &p1, &p1l, TRUE))
        return SCPI_RES_ERR;

    if (((p1l == 5) && (strncmp(p1, "SCOPE", 5) == 0)) || ((p1l == 4) && (strncmp(p1, "SCOP", 4) == 0)))
        daq_mode_set(&daq, SCOPE);
    else if ((p1l == 2) && (strncmp(p1, "VM", 2) == 0))
        daq_mode_set(&daq, VM);
    else if ((p1l == 2) && (strncmp(p1, "LA", 2) == 0))
        daq_mode_set(&daq, LA);
    else
    {
        SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
        return SCPI_RES_ERR;
    }

    SCPI_ResultText(context, SCPI_OK);
    return SCPI_RES_OK;
}

scpi_result_t EM_SYS_ModeQ(scpi_t* context)
{
    if (daq.mode == SCOPE)
        SCPI_ResultText(context, "SCOPE");
    else if (daq.mode == VM)
        SCPI_ResultText(context, "VM");
    else
        SCPI_ResultText(context, "LA");

    return SCPI_RES_OK;
}

scpi_result_t EM_SYS_LimitsQ(scpi_t* context)
{
    char buff[120];
    char dual[2] = {'\0'};
    char inter[2] = {'\0'};
    uint8_t dac = 0;
    uint8_t bit8 = 0;
    uint8_t adcs = 0;
    uint8_t pwm2 = 0;
    uint8_t daqch = 0;

#ifdef EM_DAC
    dac = 1;
#endif
#ifdef EM_ADC_BIT8
    bit8 = 1;
#endif

#if defined(EM_DAQ_4CH)
    daqch = 4;
#else
    daqch = 2;
#endif

#if defined(EM_ADC_MODE_ADC1)
    adcs = 1;
#elif defined(EM_ADC_MODE_ADC12)
    adcs = 2;
#elif defined(EM_ADC_MODE_ADC1234)
    adcs = 4;
#endif

#if defined(EM_ADC_DUALMODE)
    dual[0] = 'D';
#endif
#if defined(EM_ADC_INTERLEAVED)
    inter[0] = 'I';
#endif

#ifdef EM_TIM_PWM2
    pwm2 = 1;
#endif

    int gpio1 = EM_GPIO_LA_CH1_NUM;
    int gpio2 = EM_GPIO_LA_CH2_NUM;
    int gpio3 = 0;
    int gpio4 = 0;
#ifdef EM_DAQ_4CH
    gpio3 = EM_GPIO_LA_CH3_NUM;
    gpio4 = EM_GPIO_LA_CH4_NUM;
#endif

    int len = sprintf(buff, "%d,%d,%d,%d,%d,%d,%d%d%s%s,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d%d%d%d", EM_DAQ_MAX_B12_FS, EM_DAQ_MAX_B8_FS, EM_DAQ_MAX_MEM,
                      EM_LA_MAX_FS, EM_PWM_MAX_F, pwm2, daqch, adcs, dual, inter, bit8, dac, EM_VM_FS, EM_VM_MEM, EM_CNTR_MEAS_MS,
                      EM_SGEN_MAX_F, EM_DAC_BUFF_LEN, EM_CNTR_MAX_F, EM_MEM_RESERVE,
                      gpio1, gpio2, gpio3, gpio4);

    SCPI_ResultCharacters(context, buff, len);
    return SCPI_RES_OK;
}

scpi_result_t EM_SYS_InfoQ(scpi_t* context)
{
    char buff[100];

    int len = sprintf(buff, "%s,%s,%s,%d,%d,%s,%s,%s,%s,%s", tskKERNEL_VERSION_NUMBER, EM_LL_VER, EM_DEV_COMM,
                      EM_FREQ_HCLK/1000000, (int)daq.vcc_mv, EM_PINS_SCOPE_VM, EM_PINS_LA, EM_PINS_CNTR, EM_PINS_PWM, EM_PINS_SGEN);

    SCPI_ResultCharacters(context, buff, len);
    return SCPI_RES_OK;
}

scpi_result_t EM_SYS_UptimeQ(scpi_t* context)
{
    char buff[20];
    int ms = daq.uwTick;

    int h = daq.uwTick / 3600000;
    ms -= 3600000 * h;

    int m = ms / 60000;
    ms -= 60000 * m;

    int s = ms / 1000;
    ms -= 1000 * s;

    int len = sprintf(buff, "%02d:%02d:%02d.%01d", h, (uint8_t)m, (uint8_t)s, (uint8_t)(ms / 100));

    SCPI_ResultCharacters(context, buff, len);
    return SCPI_RES_OK;
}

/************************* [VM Actions] *************************/

scpi_result_t EM_VM_ReadQ(scpi_t* context)
{
    if (daq.mode == VM)
    {
        //daq_enable(&daq, EM_FALSE);

        uint32_t p1 = 0;
        if (context != NULL)
            SCPI_ParamUInt32(context, &p1, FALSE);
        else
            p1 = 0;

        double vref_raw = 0;
        double ch1_raw = 0;
        double ch2_raw = 0;
        double ch3_raw = 0;
        double ch4_raw = 0;

        uint8_t seq_mode = EM_FALSE;

        if (p1 == 1)
            seq_mode = EM_TRUE;
        else if (p1 == 0)
            daq.vm_seq = -1;
        else
        {
            SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
            return SCPI_RES_ERR;
        }

        int last_idx = EM_DMA_LAST_IDX(daq.buff1.len, EM_DMA_CH_ADC1, EM_DMA_ADC1);

#if defined(EM_ADC_MODE_ADC1)
#ifdef EM_DAQ_4CH
        int buff1_size = 5;
#else
        int buff1_size = 3;
#endif
#elif defined(EM_ADC_MODE_ADC12)
        int buff1_size = 3;
#elif defined(EM_ADC_MODE_ADC1234)
        int buff1_size = 2;
#endif

        for (int x = 0; x < buff1_size; x++) // normalize index to mem size
        {
            if (last_idx % buff1_size == buff1_size - 1)
                break;

            last_idx--;
            if (last_idx < 0)
                last_idx = daq.buff1.len - 1;
        }

        int last_mem = last_idx / buff1_size; // normalized index can be truncated to mem size
        //ASSERT(last_idx % buff1_size == buff1_size - 1);

        if (seq_mode == EM_TRUE)
        {
            if (daq.vm_seq == -1) // start seq. transfer
            {
                daq.vm_seq = last_mem;
            }
            else // continue seq. transfer
            {
                int diff = last_mem - daq.vm_seq;
                if (diff < 0)
                    diff += daq.set.mem + EM_MEM_RESERVE;

                if (diff == 0) // no new data
                {
                    //daq_enable(&daq, EM_TRUE);

                    SCPI_ResultText(context, "Empty");
                    return SCPI_RES_OK;
                }
                else if (diff > 5) // too old, disable seq. mode
                {
                    //ASSERT(0);
                    daq.vm_seq = last_mem;
                }
                else // good
                {
                    daq.vm_seq++;
                    if (daq.vm_seq >= daq.set.mem + EM_MEM_RESERVE)
                        daq.vm_seq = 0;
                    last_idx = (daq.vm_seq * buff1_size) + (buff1_size - 1);
                    last_mem = last_idx / buff1_size;
                }
            }
        }

#if defined(EM_ADC_MODE_ADC1)

#ifdef EM_DAQ_4CH
        get_1val_from_circ(last_idx, 5, daq.buff1.len, daq.buff1.data, daq.set.bits, &vref_raw, &ch1_raw, &ch2_raw, &ch3_raw, &ch4_raw);
#else
        get_1val_from_circ(last_idx, 3, daq.buff1.len, daq.buff1.data, daq.set.bits, &vref_raw, &ch1_raw, &ch2_raw, NULL, NULL);
#endif

#elif defined(EM_ADC_MODE_ADC12)

        get_1val_from_circ(last_idx, 3, daq.buff1.len, daq.buff1.data, daq.set.bits, &vref_raw, &ch1_raw, &ch2_raw, NULL, NULL);
        get_1val_from_circ((last_mem * 2) + 1, 2, daq.buff2.len, daq.buff2.data, daq.set.bits, &ch3_raw, &ch4_raw, NULL, NULL, NULL);

#elif defined(EM_ADC_MODE_ADC1234)

        get_1val_from_circ(last_idx, 2, daq.buff1.len, daq.buff1.data, daq.set.bits, &vref_raw, &ch1_raw, NULL, NULL, NULL);
        get_1val_from_circ(last_mem, 1, daq.buff2.len, daq.buff2.data, daq.set.bits, &ch2_raw, NULL, NULL, NULL, NULL);
        get_1val_from_circ(last_mem, 1, daq.buff3.len, daq.buff3.data, daq.set.bits, &ch3_raw, NULL, NULL, NULL, NULL);
        get_1val_from_circ(last_mem, 1, daq.buff4.len, daq.buff4.data, daq.set.bits, &ch4_raw, NULL, NULL, NULL, NULL);
#endif

        char vcc_s[10];
        char ch1_s[10];
        char ch2_s[10];
        char ch3_s[10];
        char ch4_s[10];

        double vcc = EM_ADC_VREF_CALVAL * (double)EM_ADC_VREF_CAL / vref_raw;
        double ch1 = vcc * ch1_raw / (double)daq.adc_max_val;
        double ch2 = vcc * ch2_raw / (double)daq.adc_max_val;
        double ch3 = vcc * ch3_raw / (double)daq.adc_max_val;
        double ch4 = vcc * ch4_raw / (double)daq.adc_max_val;

        daq.vref = vref_raw;
        daq.vcc_mv = vcc * 1000;

        if (context == NULL)
        {
            if (vref_raw > 0 && daq.vcc_mv > 2000)
                return SCPI_RES_OK;
            else
                return SCPI_RES_ERR;
        }

        sprint_fast(vcc_s, "%s", vcc, 4);
        sprint_fast(ch1_s, "%s", ch1, 4);
        sprint_fast(ch2_s, "%s", ch2, 4);
        sprint_fast(ch3_s, "%s", ch3, 4);
        sprint_fast(ch4_s, "%s", ch4, 4);

        char buff[100];
        int len = sprintf(buff, "%s,%s,%s,%s,%s", ch1_s, ch2_s, ch3_s, ch4_s, vcc_s);

        //daq_enable(&daq, EM_TRUE);

        SCPI_ResultCharacters(context, buff, len);
        return SCPI_RES_OK;
    }
    else
    {
        SCPI_ErrorPush(context, SCPI_ERROR_INVALID_MODE);
        return SCPI_RES_ERR;
    }
}

/************************* [SCOPE Actions] *************************/

scpi_result_t EM_SCOPE_ReadQ(scpi_t* context)
{
    if (daq.mode == SCOPE)
    {
        if (daq.trig.ready == EM_FALSE)
        {
            SCPI_ResultText(context, EM_RESP_NRDY);
            return SCPI_RES_OK;
        }

        ///*
#ifdef VREFINT_CAL_ADDR
        float cal = (float)EM_ADC_VREF_CAL / (float)daq.adc_max_val * EM_VREF;
#else
        float cal = EM_ADC_VREF_CAL;
#endif

        if (daq.set.bits == B8) // compressing
            cal /= 10.0;
        else
            cal *= 10.0;
        //*/

        size_t buff_len = daq.set.mem + EM_MEM_RESERVE;
        if (daq.set.bits == B12)
            buff_len *= 2;

#if defined(EM_ADC_MODE_ADC1)

        buff_len *= daq.set.ch1_en + daq.set.ch2_en + daq.set.ch3_en + daq.set.ch4_en;

        SCPI_ResultArbitraryBlocks(context, buff_len, 0, 0, 0, (uint8_t*)daq.buff1.data, NULL, NULL, NULL);

#elif defined(EM_ADC_MODE_ADC12)

        size_t buff_len1 = buff_len * (daq.set.ch1_en + daq.set.ch2_en);
        size_t buff_len2 = buff_len * (daq.set.ch3_en + daq.set.ch4_en);

        SCPI_ResultArbitraryBlocks(context, buff_len1, buff_len2, 0, 0,
                                            (daq.set.ch1_en == EM_TRUE || daq.set.ch2_en == EM_TRUE ? (uint8_t*)daq.buff1.data : NULL),
                                            (daq.set.ch3_en == EM_TRUE || daq.set.ch4_en == EM_TRUE ? (uint8_t*)daq.buff2.data : NULL),
                                            NULL, NULL);

#elif defined(EM_ADC_MODE_ADC1234)

        SCPI_ResultArbitraryBlocks(context, buff_len, buff_len, buff_len, buff_len,
                                            (daq.set.ch1_en == EM_TRUE ? (uint8_t*)daq.buff1.data : NULL),
                                            (daq.set.ch2_en == EM_TRUE ? (uint8_t*)daq.buff2.data : NULL),
                                            (daq.set.ch3_en == EM_TRUE ? (uint8_t*)daq.buff3.data : NULL),
                                            (daq.set.ch4_en == EM_TRUE ? (uint8_t*)daq.buff4.data : NULL));
#endif

        daq.trig.pretrig_cntr = 0;
        daq.trig.ready = EM_FALSE;
        daq.trig.ready_last = 0;

        if (daq.trig.set.mode != SINGLE)
            daq_enable(&daq, EM_TRUE);

        return SCPI_RES_OK;
    }
    else
    {
        SCPI_ErrorPush(context, SCPI_ERROR_INVALID_MODE);
        return SCPI_RES_ERR;
    }
}

scpi_result_t EM_SCOPE_Set(scpi_t* context)
{
    if (daq.mode == SCOPE)
    {
        uint32_t p1, p2, p3, p5, p6, p9;
        const char* p4;
        const char* p7;
        const char* p8;
        size_t p4l, p7l, p8l;

        if (!SCPI_ParamUInt32(context, &p1, TRUE) ||
            !SCPI_ParamUInt32(context, &p2, TRUE) ||
            !SCPI_ParamUInt32(context, &p3, TRUE) ||
            !SCPI_ParamCharacters(context, &p4, &p4l, TRUE) ||
            !SCPI_ParamUInt32(context, &p5, TRUE) ||
            !SCPI_ParamUInt32(context, &p6, TRUE) ||
            !SCPI_ParamCharacters(context, &p7, &p7l, TRUE) ||
            !SCPI_ParamCharacters(context, &p8, &p8l, TRUE) ||
            !SCPI_ParamUInt32(context, &p9, TRUE))
        {
            return SCPI_RES_ERR;
        }

        if (p5 < 1 || p5 > 4 ||
            p4l != 4 || p7l != 1 || p8l != 1 ||
            (p4[0] != '1' && p4[0] != '0') || (p4[1] != '1' && p4[1] != '0') ||
            (p4[2] != '1' && p4[2] != '0') || (p4[3] != '1' && p4[3] != '0') ||
            (p7[0] != 'R' && p7[0] != 'F') || //&& p7[0] != 'B') ||
            (p8[0] != 'A' && p8[0] != 'N' && p8[0] != 'S' && p8[0] != 'D'))
        {
            SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
            return SCPI_RES_ERR;
        }

#ifndef EM_DAQ_4CH
        if (p4[2] == '1' || p4[3] == '1' || p5 == 3 || p5 == 4)
        {
            SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
            return SCPI_RES_ERR;
        }
#endif

        uint8_t ch1_en = p4[0] - '0' ? EM_TRUE : EM_FALSE;
        uint8_t ch2_en = p4[1] - '0' ? EM_TRUE : EM_FALSE;
#ifdef EM_DAQ_4CH
        uint8_t ch3_en = p4[2] - '0' ? EM_TRUE : EM_FALSE;
        uint8_t ch4_en = p4[3] - '0' ? EM_TRUE : EM_FALSE;
#endif

        daq_settings_save(&daq.set, &daq.trig.set, &daq.save_s, &daq.trig.save_s);
        daq_enable(&daq, EM_FALSE);
        daq_reset(&daq);
        daq.dis_hold = EM_TRUE;

        daq_mem_set(&daq, 3); // safety guard
        int ret2 = daq_bit_set(&daq, (int)p1);
#ifdef EM_DAQ_4CH
        int ret4 = daq_ch_set(&daq, ch1_en, ch2_en, ch3_en, ch4_en, (int)p3);
#else
        int ret4 = daq_ch_set(&daq, ch1_en, ch2_en, EM_FALSE, EM_FALSE, (int)p3);
#endif
        int ret3 = daq_fs_set(&daq, p3);
        int ret1 = daq_mem_set(&daq, (int)p2);
        int ret5 = daq_trig_set(&daq, p5, p6, (p7[0] == 'R' ? RISING : (p7[0] == 'F' ? FALLING : BOTH)),
            (p8[0] == 'A' ? AUTO : (p8[0] == 'N' ? NORMAL : (p8[0] == 'S' ? SINGLE : DISABLED))), (int)p9);

        if (ret1 + ret2 + ret3 + ret4 + ret5 == 0)
        {
            daq.dis_hold = EM_FALSE;
            daq_settings_save(&daq.set, &daq.trig.set, &daq.save_s, &daq.trig.save_s); // added later
            daq_enable(&daq, EM_TRUE);

            char buff[60];
            char maxZ_s[15];
            double max_Z = EM_ADC_MAXZ(daq.smpl_time, daq.set.bits == B12 ? EM_LN2POW14 : EM_LN2POW10);
            sprint_fast(maxZ_s, "%s", max_Z, 1);

            char freq_real_s[20];
            sprint_fast(freq_real_s, "%s", daq.set.fs_real, 6);

            char T_s[15];
            sprint_fast(T_s, "%s", (1 / (double)EM_FREQ_ADCCLK) * daq.smpl_time * 1000000000.0, 2);

            int len = sprintf(buff, "\"OK\",%s,%s,%s", maxZ_s, T_s, freq_real_s);

            SCPI_ResultCharacters(context, buff, len);
            return SCPI_RES_OK;
        }
        else
        {
            daq.mode = VM;
            daq_mode_set(&daq, SCOPE);  // reload saved settings
            daq.dis_hold = EM_FALSE;
            daq_enable(&daq, EM_TRUE);

            SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
            return SCPI_RES_ERR;
        }
    }
    else
    {
        SCPI_ErrorPush(context, SCPI_ERROR_INVALID_MODE);
        return SCPI_RES_ERR;
    }
}

scpi_result_t EM_SCOPE_SetQ(scpi_t* context)
{
    if (daq.mode == SCOPE)
    {
        char buff[100];
        char chans_en[5];
        char edge_s[2];
        char mode_s[2];

        chans_en[0] = daq.set.ch1_en ? '1' : '0';
        chans_en[1] = daq.set.ch2_en ? '1' : '0';
        chans_en[2] = daq.set.ch3_en ? '1' : '0';
        chans_en[3] = daq.set.ch4_en ? '1' : '0';
        edge_s[0] = daq.trig.set.edge == RISING ? 'R' : (daq.trig.set.edge == FALLING ? 'F' : 'B');
        mode_s[0] = daq.trig.set.mode == AUTO ? 'A' : (daq.trig.set.mode == NORMAL ? 'N' :
            (daq.trig.set.mode == SINGLE ? 'S' : 'D'));
        chans_en[4] = '\0';
        edge_s[1] = '\0';
        mode_s[1] = '\0';

        char maxZ_s[15];
        double max_Z = EM_ADC_MAXZ(daq.smpl_time, daq.set.bits == B12 ? EM_LN2POW14 : EM_LN2POW10);
        sprint_fast(maxZ_s, "%s", max_Z, 3);

        char freq_real_s[20];
        sprint_fast(freq_real_s, "%s", daq.set.fs_real, 6);

        char T_s[15];
        sprint_fast(T_s, "%s", (1 / (double)EM_FREQ_ADCCLK) * daq.smpl_time * 1000000000.0, 2);


        int len = sprintf(buff, "%d,%d,%d,%s,%d,%d,%s,%s,%d,%s,%s,%s", daq.set.bits, daq.set.mem, daq.set.fs, chans_en,
                          daq.trig.set.ch, daq.trig.set.val_percent, edge_s, mode_s, daq.trig.set.pretrigger, maxZ_s, T_s, freq_real_s);

        SCPI_ResultCharacters(context, buff, len);
        return SCPI_RES_OK;
    }
    else
    {
        SCPI_ErrorPush(context, SCPI_ERROR_INVALID_MODE);
        return SCPI_RES_ERR;
    }
}

scpi_result_t EM_SCOPE_ForceTrig(scpi_t* context)
{
    if (daq.mode != SCOPE)
    {
        SCPI_ErrorPush(context, SCPI_ERROR_INVALID_MODE);
        return SCPI_RES_ERR;
    }

    if (daq.trig.post_start == EM_TRUE || daq.trig.ready == EM_TRUE || daq.trig.set.mode == DISABLED || daq.trig.set.mode == AUTO)
    {
        SCPI_ErrorPush(context, SCPI_ERROR_FUNCTION_NOT_AVAILABLE);
        return SCPI_RES_ERR;
    }

    uint8_t en = daq.enabled;

    if (daq.enabled == EM_FALSE)
        daq_enable(&daq, EM_TRUE);

    if (daq.trig.pretrig_cntr < daq.trig.pretrig_val || en == EM_FALSE)
    {
        daq.trig.force_single = EM_TRUE;

        SCPI_ErrorPush(context, SCPI_ERROR_FUNCTION_NOT_AVAILABLE2);
        return SCPI_RES_ERR;
    }

    daq.trig.ready = EM_TRUE;

    daq_enable(&daq, EM_FALSE);
    daq.trig.pos_frst = EM_DMA_LAST_IDX(daq.trig.buff_trig->len, daq.trig.dma_ch_trig, daq.trig.dma_trig);

    comm_daq_ready(comm_ptr, EM_RESP_RDY_F, daq.trig.pos_frst);

    SCPI_ResultText(context, SCPI_OK);
    return SCPI_RES_OK;
}

/************************* [LA Actions] *************************/

scpi_result_t EM_LA_ReadQ(scpi_t* context)
{
    if (daq.mode == LA)
    {
        if (daq.trig.ready == EM_FALSE)
        {
            SCPI_ResultText(context, EM_RESP_NRDY);
            return SCPI_RES_OK;
        }

        daq.trig.pretrig_cntr = 0;
        daq.trig.ready = EM_FALSE;
        daq.trig.ready_last = 0;

        SCPI_ResultArbitraryBlock(context, daq.buff1.data, daq.buff1.len);

        if (daq.trig.set.mode != SINGLE)
            daq_enable(&daq, EM_TRUE);

        return SCPI_RES_OK;
    }
    else
    {
        SCPI_ErrorPush(context, SCPI_ERROR_INVALID_MODE);
        return SCPI_RES_ERR;
    }
}

scpi_result_t EM_LA_Set(scpi_t* context)
{
    if (daq.mode == LA)
    {
        uint32_t p2, p3, p5, p9;
        const char* p7;
        const char* p8;
        size_t p7l, p8l;

        if (!SCPI_ParamUInt32(context, &p2, TRUE) ||
            !SCPI_ParamUInt32(context, &p3, TRUE) ||
            !SCPI_ParamUInt32(context, &p5, TRUE) ||
            !SCPI_ParamCharacters(context, &p7, &p7l, TRUE) ||
            !SCPI_ParamCharacters(context, &p8,&p8l, TRUE) ||
            !SCPI_ParamUInt32(context, &p9, TRUE))
        {
            return SCPI_RES_ERR;
        }

        if (p5 < 1 || p5 > 4 ||
            p7l != 1 || p8l != 1 ||
            (p7[0] != 'R' && p7[0] != 'F' && p7[0] != 'B') ||
            (p8[0] != 'A' && p8[0] != 'N' && p8[0] != 'S' && p8[0] != 'D'))
        {
            SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
            return SCPI_RES_ERR;
        }

#ifndef EM_DAQ_4CH
        if (p5 == 3 || p5 == 4)
        {
            SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
            return SCPI_RES_ERR;
        }
#endif

        daq_settings_save(&daq.set, &daq.trig.set, &daq.save_l, &daq.trig.save_l);
        daq_enable(&daq, EM_FALSE);
        daq_reset(&daq);
        daq.dis_hold = EM_TRUE;

        daq_mem_set(&daq, 3); // safety guard
        int ret2 = daq_bit_set(&daq, B1);
#ifdef EM_DAQ_4CH
        int ret4 = daq_ch_set(&daq, EM_TRUE, EM_TRUE, EM_TRUE, EM_TRUE, (int)p3);
#else
        int ret4 = daq_ch_set(&daq, EM_TRUE, EM_TRUE, EM_FALSE, EM_FALSE, (int)p3);
#endif
        int ret3 = daq_fs_set(&daq, p3);
        int ret1 = daq_mem_set(&daq, (int)p2);
        int ret5 = daq_trig_set(&daq, p5, 0, (p7[0] == 'R' ? RISING : (p7[0] == 'F' ? FALLING : BOTH)),
            (p8[0] == 'A' ? AUTO : (p8[0] == 'N' ? NORMAL : (p8[0] == 'S' ? SINGLE : DISABLED))), (int)p9);

        if (ret1 + ret2 + ret3 + ret4 + ret5 == 0)
        {
            daq.dis_hold = EM_FALSE;
            daq_settings_save(&daq.set, &daq.trig.set, &daq.save_l, &daq.trig.save_l); // added later
            daq_enable(&daq, EM_TRUE);

            char buff[30];
            char freq_real_s[20];
            sprint_fast(freq_real_s, "%s", daq.set.fs_real, 6);

            int len = sprintf(buff, "\"OK\",%s", freq_real_s);

            SCPI_ResultCharacters(context, buff, len);
            return SCPI_RES_OK;
        }
        else
        {
            daq.mode = VM;
            daq_mode_set(&daq, LA); // reload saved settings
            daq.dis_hold = EM_FALSE;
            daq_enable(&daq, EM_TRUE);

            SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
            return SCPI_RES_ERR;
        }
    }
    else
    {
        SCPI_ErrorPush(context, SCPI_ERROR_INVALID_MODE);
        return SCPI_RES_ERR;
    }
}

scpi_result_t EM_LA_SetQ(scpi_t* context)
{
    if (daq.mode == LA)
    {
        char buff[100];
        char edge_s[2];
        char mode_s[2];

        edge_s[0] = daq.trig.set.edge == RISING ? 'R' : (daq.trig.set.edge == FALLING ? 'F' : 'B');
        mode_s[0] = daq.trig.set.mode == AUTO ? 'A' : (daq.trig.set.mode == NORMAL ? 'N' :
            (daq.trig.set.mode == SINGLE ? 'S' : 'D'));
        edge_s[1] = '\0';
        mode_s[1] = '\0';

        char freq_real_s[20];
        sprint_fast(freq_real_s, "%s", daq.set.fs_real, 6);

        int len = sprintf(buff, "%d,%d,%d,%s,%s,%d,%s", daq.set.mem, daq.set.fs,
                          daq.trig.set.ch, edge_s, mode_s, daq.trig.set.pretrigger, freq_real_s);

        SCPI_ResultCharacters(context, buff, len);
        return SCPI_RES_OK;
    }
    else
    {
        SCPI_ErrorPush(context, SCPI_ERROR_INVALID_MODE);
        return SCPI_RES_ERR;
    }
}

scpi_result_t EM_LA_ForceTrig(scpi_t* context)
{
    if (daq.mode != LA)
    {
        SCPI_ErrorPush(context, SCPI_ERROR_INVALID_MODE);
        return SCPI_RES_ERR;
    }

    if (daq.trig.post_start == EM_TRUE || daq.trig.ready == EM_TRUE || daq.trig.set.mode == DISABLED || daq.trig.set.mode == AUTO)
    {
        SCPI_ErrorPush(context, SCPI_ERROR_FUNCTION_NOT_AVAILABLE);
        return SCPI_RES_ERR;
    }

    uint8_t en = daq.enabled;

    if (daq.enabled == EM_FALSE)
        daq_enable(&daq, EM_TRUE);

    if (daq.trig.pretrig_cntr < daq.trig.pretrig_val || en == EM_FALSE)
    {
        daq.trig.force_single = EM_TRUE;

        SCPI_ErrorPush(context, SCPI_ERROR_FUNCTION_NOT_AVAILABLE2);
        return SCPI_RES_ERR;
    }

    daq.trig.ready = EM_TRUE;
    daq_enable(&daq, EM_FALSE);

    daq.trig.pos_frst = EM_DMA_LAST_IDX(daq.trig.buff_trig->len, daq.trig.dma_ch_trig, daq.trig.dma_trig);

    comm_daq_ready(comm_ptr, EM_RESP_RDY_F, daq.trig.pos_frst);

    SCPI_ResultText(context, SCPI_OK);
    return SCPI_RES_OK;
}

/************************* [CNTR Actions] *************************/

scpi_result_t EM_CNTR_SetQ(scpi_t* context)
{
    char buff[3];
    buff[0] = cntr.enabled ? '1' : '0';
    buff[1] = ',';
    buff[2] = cntr.fast_mode ? '1' : '0';

    SCPI_ResultCharacters(context, buff, 3);
    return SCPI_RES_OK;
}

scpi_result_t EM_CNTR_Set(scpi_t* context)
{
    uint32_t p1, p2;

    if (!SCPI_ParamUInt32(context, &p1, TRUE) ||
        !SCPI_ParamUInt32(context, &p2, TRUE))
    {
        return SCPI_RES_ERR;
    }

    if (p1 < 0 || p1 > 1 || p2 < 0 || p2 > 1)
    {
        SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
        return SCPI_RES_ERR;
    }

    cntr_enable(&cntr, p1, p2);

    SCPI_ResultText(context, SCPI_OK);
    return SCPI_RES_OK;
}

scpi_result_t EM_CNTR_ReadQ(scpi_t* context)
{

    if (!cntr.enabled)
    {
        SCPI_ErrorPush(context, SCPI_ERROR_CNTR_NOT_ENABLED);
        return SCPI_RES_ERR;
    }

    double f = cntr.freq;

    if (f > -1)
    {
        char f_s[20];
        char T_s[20];

        double T = 1.0 / f;

        if (f < 1000)
            sprint_fast(f_s, "%s Hz", f, 3);
        else if (f < 1000000)
            sprint_fast(f_s, "%s kHz", f / 1000.0, 3);
        else //if (f < 1000000000)
            sprint_fast(f_s, "%s MHz", f / 1000000.0, 3);

        if (T >= 1)
            sprint_fast(T_s, "%s s", T, 3);
        else if (T >= 0.001)
            sprint_fast(T_s, "%s ms", T * 1000.0, 3);
        else if (T >= 0.000001)
            sprint_fast(T_s, "%s us", T * 1000000.0, 3);
        else //if (T >= 0.000000001)
            sprint_fast(T_s, "%s ns", T * 1000000000.0, 3);

        char buff[100];
        int len = sprintf(buff, "%s,%s", f_s, T_s);

        SCPI_ResultCharacters(context, buff, len);
        return SCPI_RES_OK;
    }
    else // if (f == -1)
    {
        SCPI_ErrorPush(context, SCPI_ERROR_TIME_OUT);
        return SCPI_RES_ERR;
    }
}

/************************* [SGEN Actions] *************************/

scpi_result_t EM_SGEN_SetQ(scpi_t* context)
{
#ifdef EM_DAC
    char buff[60];
    char freq_real_s[20];

    sprint_fast(freq_real_s, "%s", sgen.tim_f_real / (double)sgen.samples, 3);

    int len = sprintf(buff, "%d,%d,%d,%d,%d,%s,%d", sgen.freq, (int)(sgen.ampl * 10.0), sgen.offset,
                      sgen.mode, sgen.enabled, freq_real_s, sgen.samples);

    SCPI_ResultCharacters(context, buff, len);
    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_DAC_NA);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t EM_SGEN_Set(scpi_t* context)
{
#ifdef EM_DAC
    uint32_t param1, param2, param3, param4, param5;

    if (!SCPI_ParamUInt32(context, &param1, TRUE) ||
        !SCPI_ParamUInt32(context, &param2, TRUE) ||
        !SCPI_ParamUInt32(context, &param3, TRUE) ||
        !SCPI_ParamUInt32(context, &param4, TRUE) ||
        !SCPI_ParamUInt32(context, &param5, TRUE))
    {
        SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
        return SCPI_RES_ERR;
    }

    if (param1 < 0 || param1 > EM_SGEN_MAX_F || // freq
        param2 < 0 || param2 > 1000 ||          // ampl
        param3 < 0 || param3 > 100 ||           // offset
        param4 < 0 || param4 > 5 ||             // mode
        param5 < 0 || param5 > 1)               // enable
    {
        SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
        return SCPI_RES_ERR;
    }

    sgen_disable(&sgen);

    if (param5 == EM_TRUE)
        sgen_enable(&sgen, param4, (float)param2 / 10.0, param1, param3);

    char buff[45];
    char freq_real_s[20];

    sprint_fast(freq_real_s, "%s", sgen.tim_f_real / (double)sgen.samples, 3);

    int len = sprintf(buff, "\"OK\",%s,%d", freq_real_s, sgen.samples);

    SCPI_ResultCharacters(context, buff, len);
    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_DAC_NA);
    return SCPI_RES_ERR;
#endif
}

/************************* [PWM Actions] *************************/

scpi_result_t EM_PWM_SetQ(scpi_t* context)
{
    char buff[60];
    char buff_freq_real[15];

    int offset2 = pwm.ch2.offset;
    uint8_t en1 = pwm.ch1.enabled;
    uint8_t en2 = pwm.ch2.enabled;

    sprint_fast(buff_freq_real, "%s", pwm.ch1.freq_real, 3);

    int len = sprintf(buff, "%d,%d,%d,%d,%d,%d,%s", pwm.ch1.freq, (int)pwm.ch1.duty, (int)pwm.ch2.duty, offset2, en1, en2, buff_freq_real);

    SCPI_ResultCharacters(context, buff, len);

    return SCPI_RES_OK;
}

scpi_result_t EM_PWM_Set(scpi_t* context)
{
    uint32_t param1, param2, param3, param4, param5, param6;

    if (!SCPI_ParamUInt32(context, &param1, TRUE) ||
        !SCPI_ParamUInt32(context, &param2, TRUE) ||
        !SCPI_ParamUInt32(context, &param3, TRUE) ||
        !SCPI_ParamUInt32(context, &param4, TRUE) ||
        !SCPI_ParamUInt32(context, &param5, TRUE) ||
        !SCPI_ParamUInt32(context, &param6, TRUE))
    {
        SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
        return SCPI_RES_ERR;
    }

    int ret = pwm_set(&pwm, param1, param2, param3, param4, param5, param6);

    if (ret != 0)
    {
        SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
        return SCPI_RES_ERR;
    }

    char buff[25];
    char freq_real_s[15];
    sprint_fast(freq_real_s, "%s", pwm.ch1.freq_real, 3);

    int len = sprintf(buff, "\"OK\",%s", freq_real_s);

    SCPI_ResultCharacters(context, buff, len);
    return SCPI_RES_OK;
}

