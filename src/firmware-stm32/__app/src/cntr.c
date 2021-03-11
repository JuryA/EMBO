/*
 * CTU/EMBO - EMBedded Oscilloscope <github.com/parezj/EMBO>
 * Author: Jakub Parez <parez.jakub@gmail.com>
 */

#include "cfg.h"
#include "cntr.h"

#include "app_sync.h"
#include "periph.h"
#include "main.h"

#include "FreeRTOS.h"

#include <string.h>


static void cntr_reset(cntr_data_t* self);


void cntr_init(cntr_data_t* self)
{
    self->freq = -1;
    self->enabled = EM_FALSE;
    self->fast_mode = EM_FALSE;
    self->fast_mode_now = 0;
    cntr_reset(self);
}

static void cntr_reset(cntr_data_t* self)
{
    self->ovf = 0;
    memset(self->data_ccr, 0, EM_CNTR_BUFF_SZ * sizeof(uint16_t));
    memset(self->data_ovf, 0, EM_CNTR_BUFF_SZ * sizeof(uint16_t));

    dma_set((uint32_t)&EM_TIM_CNTR->EM_TIM_CNTR_CCR, EM_DMA_CNTR, EM_DMA_CH_CNTR, (uint32_t)&self->data_ccr, EM_CNTR_BUFF_SZ,
            LL_DMA_PDATAALIGN_HALFWORD, LL_DMA_MDATAALIGN_HALFWORD, LL_DMA_DIRECTION_PERIPH_TO_MEMORY);

    dma_set((uint32_t)&EM_TIM_CNTR->EM_TIM_CNTR_CCR2, EM_DMA_CNTR2, EM_DMA_CH_CNTR2, (uint32_t)&self->data_ovf, EM_CNTR_BUFF_SZ,
            LL_DMA_PDATAALIGN_HALFWORD, LL_DMA_MDATAALIGN_HALFWORD, LL_DMA_DIRECTION_PERIPH_TO_MEMORY);

    NVIC_SetPriority(TIM1_UP_TIM16_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(),1, 0));
    LL_TIM_EnableIT_UPDATE(EM_TIM_CNTR);
    EM_TIM_CNTR_OVF(LL_TIM_OC_SetCompare)(EM_TIM_CNTR, 0);
    LL_TIM_SetCounter(EM_TIM_CNTR, 0);

    self->fast_mode_now = self->fast_mode == EM_TRUE;
    LL_TIM_IC_SetPrescaler(EM_TIM_CNTR, LL_TIM_CHANNEL_CH1, self->fast_mode_now ? LL_TIM_ICPSC_DIV8 : LL_TIM_ICPSC_DIV1);
    LL_TIM_IC_SetPrescaler(EM_TIM_CNTR, LL_TIM_CHANNEL_CH2, self->fast_mode_now ? LL_TIM_ICPSC_DIV8 : LL_TIM_ICPSC_DIV1);
}

void cntr_enable(cntr_data_t* self, uint8_t enable, uint8_t fast_mode)
{
    uint8_t en = self->enabled;
    self->enabled = enable;
    self->fast_mode = fast_mode;

    if (en == EM_FALSE && enable == EM_TRUE)
        self->freq = -1;

    if (enable == EM_TRUE && en == EM_FALSE)
        xSemaphoreGive(sem3_cntr);
    else if (enable == EM_FALSE && en == EM_TRUE)
        xSemaphoreTake(sem3_cntr, 0);
}

void cntr_start(cntr_data_t* self, uint8_t start)
{
    if (start)
    {
        cntr_reset(self);

        EM_TIM_CNTR_CC(LL_TIM_EnableDMAReq_)(EM_TIM_CNTR);
        EM_TIM_CNTR_CC2(LL_TIM_EnableDMAReq_)(EM_TIM_CNTR);
        NVIC_EnableIRQ(TIM1_UP_TIM16_IRQn);
        LL_TIM_CC_EnableChannel(EM_TIM_CNTR, EM_TIM_CNTR_CH);
        LL_TIM_CC_EnableChannel(EM_TIM_CNTR, EM_TIM_CNTR_CH2);
        LL_TIM_EnableCounter(EM_TIM_CNTR);
    }
    else
    {
        LL_TIM_DisableCounter(EM_TIM_CNTR);
        LL_TIM_CC_DisableChannel(EM_TIM_CNTR, EM_TIM_CNTR_CH);
        LL_TIM_CC_DisableChannel(EM_TIM_CNTR, EM_TIM_CNTR_CH2);
        NVIC_DisableIRQ(TIM1_UP_TIM16_IRQn);
        EM_TIM_CNTR_CC(LL_TIM_DisableDMAReq_)(EM_TIM_CNTR);
        EM_TIM_CNTR_CC2(LL_TIM_DisableDMAReq_)(EM_TIM_CNTR);
    }
}

void cntr_meas(cntr_data_t* self)
{
    int pre_timeout = 0;
    int cntr_timeout = 0;
    uint32_t sz = 0;

    cntr_start(self, 1); // start

    while (1)
    {
        sz = LL_DMA_GetDataLength(EM_DMA_CNTR, EM_DMA_CH_CNTR);

        if (pre_timeout > 1000)
        {
            vTaskDelay(EM_CNTR_INT_DELAY);
            cntr_timeout += EM_CNTR_INT_DELAY;
        }

        pre_timeout++;

        if (cntr_timeout > EM_CNTR_MEAS_MS || sz == 0)
            break;
    }

    cntr_start(self, 0); // stop

    sz = EM_CNTR_BUFF_SZ - sz;

    if (sz >= 2)
    {
        uint16_t ovf = 0;
        uint32_t ccr_sum = 0;

        ovf = self->data_ovf[sz - 1] - self->data_ovf[0];

        if (ovf > 0)
        {
            ccr_sum += (EM_TIM_CNTR_MAX - self->data_ccr[0]) + self->data_ccr[sz - 1];
            ovf -= 1;
        }
        else
        {
            if (self->data_ccr[sz - 1] > self->data_ccr[0])
                ccr_sum += self->data_ccr[sz - 1] - self->data_ccr[0];
            else
                return;
        }

        double total = (ovf * EM_TIM_CNTR_MAX) + ccr_sum;
        total /= (double)(sz - 1);
        self->freq = ((double)EM_TIM_CNTR_FREQ / total) * (double)(self->fast_mode_now ? 8 : 1);

        //float diff = f - 1.6;
        //if (diff < 1 && diff > -1)
        //    ASSERT(0);
    }
    else
    {
        self->freq = -1; // timeout
    }
}