/*
 * CTU/PillScope project
 * Author: Jakub Parez <parez.jakub@gmail.com>
 */

#include "cntr.h"

#include <string.h>

#include "FreeRTOS.h"

#include "cfg.h"
#include "main.h"
#include "app_sync.h"
#include "periph.h"


static void cntr_reset(cntr_data_t* self);


void cntr_init(cntr_data_t* self)
{
    self->freq = -1;
    self->enabled = 0;
    cntr_reset(self);
}

static void cntr_reset(cntr_data_t* self)
{
    self->ovf = 0;
    memset(self->data_ccr, 0, PS_CNTR_BUFF_SZ * sizeof(uint16_t));
    memset(self->data_ovf, 0, PS_CNTR_BUFF_SZ * sizeof(uint16_t));

    dma_set((uint32_t)&PS_TIM_CNTR->PS_TIM_CNTR_CCR, PS_DMA_CNTR, PS_DMA_CH_CNTR, (uint32_t)&self->data_ccr, PS_CNTR_BUFF_SZ,
            LL_DMA_PDATAALIGN_HALFWORD, LL_DMA_MDATAALIGN_HALFWORD, LL_DMA_DIRECTION_PERIPH_TO_MEMORY);

    dma_set((uint32_t)&PS_TIM_CNTR->PS_TIM_CNTR_CCR2, PS_DMA_CNTR2, PS_DMA_CH_CNTR2, (uint32_t)&self->data_ovf, PS_CNTR_BUFF_SZ,
            LL_DMA_PDATAALIGN_HALFWORD, LL_DMA_MDATAALIGN_HALFWORD, LL_DMA_DIRECTION_PERIPH_TO_MEMORY);

    NVIC_SetPriority(TIM1_UP_TIM16_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(),1, 0));
    LL_TIM_EnableIT_UPDATE(PS_TIM_CNTR);
    PS_TIM_CNTR_OVF(LL_TIM_OC_SetCompare)(PS_TIM_CNTR, 0);
    LL_TIM_SetCounter(PS_TIM_CNTR, 0);
}

void cntr_enable(cntr_data_t* self, uint8_t enable)
{
    uint8_t en = self->enabled;
    self->enabled = enable;

    if (enable && !en)
        ASSERT(xSemaphoreGive(sem3_cntr) == pdPASS);
}

void cntr_start(cntr_data_t* self, uint8_t start)
{
    if (start)
    {
        cntr_reset(self);

        PS_TIM_CNTR_CC(LL_TIM_EnableDMAReq_)(PS_TIM_CNTR);
        PS_TIM_CNTR_CC2(LL_TIM_EnableDMAReq_)(PS_TIM_CNTR);
        NVIC_EnableIRQ(TIM1_UP_TIM16_IRQn);
        LL_TIM_CC_EnableChannel(PS_TIM_CNTR, PS_TIM_CNTR_CH);
        LL_TIM_CC_EnableChannel(PS_TIM_CNTR, PS_TIM_CNTR_CH2);
        LL_TIM_EnableCounter(PS_TIM_CNTR);
    }
    else
    {
        LL_TIM_DisableCounter(PS_TIM_CNTR);
        LL_TIM_CC_DisableChannel(PS_TIM_CNTR, PS_TIM_CNTR_CH);
        LL_TIM_CC_DisableChannel(PS_TIM_CNTR, PS_TIM_CNTR_CH2);
        NVIC_DisableIRQ(TIM1_UP_TIM16_IRQn);
        PS_TIM_CNTR_CC(LL_TIM_DisableDMAReq_)(PS_TIM_CNTR);
        PS_TIM_CNTR_CC2(LL_TIM_DisableDMAReq_)(PS_TIM_CNTR);
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
        sz = LL_DMA_GetDataLength(PS_DMA_CNTR, PS_DMA_CH_CNTR);

        if (pre_timeout > 1000)
        {
            vTaskDelay(PS_CNTR_INT_DELAY);
            cntr_timeout += PS_CNTR_INT_DELAY;
        }

        pre_timeout++;

        if (cntr_timeout > PS_CNTR_MEAS_MS || sz == 0)
            break;
    }

    cntr_start(self, 0); // stop

    sz = PS_CNTR_BUFF_SZ - sz;

    if (sz >= 2)
    {
        uint16_t ovf = 0;
        uint32_t ccr_sum = 0;

        ovf = self->data_ovf[sz - 1] - self->data_ovf[0];

        if (ovf > 0)
        {
            ccr_sum += (PS_TIM_CNTR_MAX - self->data_ccr[0]) + self->data_ccr[sz - 1];
            ovf -= 1;
        }
        else
        {
            if (self->data_ccr[sz - 1] > self->data_ccr[0])
                ccr_sum += self->data_ccr[sz - 1] - self->data_ccr[0];
            else
                return;
        }

        float total = (ovf * PS_TIM_CNTR_MAX) + ccr_sum;
        total /= (float)(sz - 1);
        float f = (float)PS_TIM_CNTR_FREQ / total;
        self->freq = f;

        float diff = f - 1.6;
        if (diff < 1 && diff > -1)
            ASSERT(0);
    }
    else
    {
        self->freq = -1; // timeout
    }
}