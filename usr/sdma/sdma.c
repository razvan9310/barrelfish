/**
 * \file
 * \brief SDMA (System Direct Memory Access) driver.
 */

/*
 * Copyright (c) 2016, ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Universitaetsstrasse 6, CH-8092 Zurich. Attn: Systems Group.
 */

#include <driverkit/driverkit.h>
#include <omap44xx_map.h>

#include "sdma.h"

errval_t sdma_map_device(struct sdma_driver* sd)
{
    return map_device_register(
            OMAP44XX_MAP_L4_CFG_SDMA,
            OMAP44XX_MAP_L4_CFG_SDMA_SIZE,
            &sd->sdma_vaddr);
}

void sdma_initialize_driver(struct sdma_driver* sd)
{
    omap44xx_sdma_initialize(&sd->sdma_dev, (mackerel_addr_t) sd->sdma_vaddr);
    debug_printf("omap44xx_sdma_dma4_revision_rd = 0x%x\n",
            omap44xx_sdma_dma4_revision_rd(&sd->sdma_dev));
}

void sdma_interrupt_handler(void* arg)
{
    debug_printf("!!! Got SDMA interrupt!\n");
    struct sdma_driver* sd = (struct sdma_driver*) arg;
    uint8_t irq_line = 0;
    sdma_update_channel_status(sd, irq_line,
            omap44xx_sdma_dma4_irqstatus_line_rd(&sd->sdma_dev, irq_line));
}

errval_t sdma_setup_config(struct sdma_driver* sd)
{
    // 1. Setup the interrupt handler.
    CHECK("setup SDMA interrupt handler",
            inthandler_setup_arm(sdma_interrupt_handler, sd, SDMA_IRQ_LINE_0));

    // 2. Configure GCR register.
    omap44xx_sdma_dma4_gcr_t gcr = omap44xx_sdma_dma4_gcr_rd(&sd->sdma_dev);
    // 2.1. max_channel_fifo_depth.
    omap44xx_sdma_dma4_gcr_max_channel_fifo_depth_insert(gcr, 255);  // Maximum.
    // 2.2. arbitration_rate.
    omap44xx_sdma_dma4_gcr_arbitration_rate_insert(gcr, 1);  // 1:1.
    // 2.3. Write back.
    omap44xx_sdma_dma4_gcr_wr(&sd->sdma_dev, gcr);

    // 3. Enable & clear IRQ lie 0.
    // 3.1. Enable IRQ line 0.
    uint32_t enable_value = 4294967295u;
    omap44xx_sdma_dma4_irqenable_wr(&sd->sdma_dev, 0, enable_value);
    debug_printf("Enabled IRQ line 0 with value %u\n", enable_value);

    // 3.2. Clear IRQ line 0.
    omap44xx_sdma_dma4_irqstatus_line_wr(&sd->sdma_dev, 0, 0);

    // 4. CSR, IRQLINE and CICR for each channel.
    for (chanid_t chan = 0; chan < SDMA_CHANNELS; ++chan) {
        // 4.1. Clear CSR register.
        omap44xx_sdma_dma4_csr_t csr = omap44xx_sdma_dma4_csr_rd(&sd->sdma_dev,
                chan);
        csr = 0x0;
        omap44xx_sdma_dma4_csr_wr(&sd->sdma_dev, chan, csr);

        // 4.2. Enable CICR interrupts.
        omap44xx_sdma_dma4_cicr_t cicr = omap44xx_sdma_dma4_cicr_rd(
                &sd->sdma_dev, chan);
        cicr = omap44xx_sdma_dma4_cicr_misaligned_err_ie_insert(cicr, 1);
        cicr = omap44xx_sdma_dma4_cicr_supervisor_err_ie_insert(cicr, 1);
        cicr = omap44xx_sdma_dma4_cicr_trans_err_ie_insert(cicr, 1);
        cicr = omap44xx_sdma_dma4_cicr_block_ie_insert(cicr, 1);
        omap44xx_sdma_dma4_cicr_wr(&sd->sdma_dev, chan, cicr);
    }

    return SYS_ERR_OK;
}

void sdma_update_channel_status(struct sdma_driver* sd, uint8_t irq_line,
        uint32_t irq_status)
{
    for (chanid_t chan = 0; chan < SDMA_CHANNELS; ++chan) {
        if (irq_status & (1u << chan)) {
            // 1. Misalignment error?
            uint8_t err = omap44xx_sdma_dma4_csr_misaligned_adrs_err_rdf(
                    &sd->sdma_dev, chan);
            if (err) {
                sd->chan_state[chan].err = SDMA_ERR_MISALIGNED;
            }

            // 2. Supervisor error?
            err = omap44xx_sdma_dma4_csr_supervisor_err_rdf(&sd->sdma_dev, 
                    chan);
            if (err) {
                sd->chan_state[chan].err = SDMA_ERR_SUPERVISOR;
            }

            // 3. Transfer error?
            err = omap44xx_sdma_dma4_csr_trans_err_rdf(&sd->sdma_dev, chan);
            if (err) {
                sd->chan_state[chan].err = SDMA_ERR_TRANSFER;
            }

            // 4. Block complete?
            uint8_t block_status = omap44xx_sdma_dma4_csr_block_rdf(
                    &sd->sdma_dev, chan);
            if (block_status) {
                sd->chan_state[chan].err = SYS_ERR_OK;
                sd->chan_state[chan].transfer_in_progress = false;
                
                // TODO: RPC back to requesting client or something similar.
                char* cbuf = (char*) sd->buf;
                for (size_t i = 0; i < 512; ++i) {
                    if (i % 128 == 0) {
                        printf("\n");
                    }
                    printf("%c", cbuf[i]);
                }
                printf("\n");
            }

            // 5. Clear IRQ status.
            omap44xx_sdma_dma4_irqstatus_line_wr(&sd->sdma_dev, irq_line, 0);
        }
    }
}

void sdma_test_copy(struct sdma_driver* sd, struct capref* frame)
{
    // 1. Channel to send via.
    chanid_t chan = 10;

    // 2. CSDP.
    omap44xx_sdma_dma4_csdp_t csdp = omap44xx_sdma_dma4_csdp_rd(&sd->sdma_dev,
            chan);
    // 2.1. Transfer ES.
    size_t es = 4;  // 4-byte == 32-bit.
    csdp = omap44xx_sdma_dma4_csdp_data_type_insert(csdp,
            omap44xx_sdma_DATA_TYPE_32BIT);
    // 2.2. R/W port access types (single vs burst).
    csdp = omap44xx_sdma_dma4_csdp_src_burst_en_insert(csdp,
            omap44xx_sdma_BURST_EN_SINGLE);  // Single?
    csdp = omap44xx_sdma_dma4_csdp_dst_burst_en_insert(csdp,
            omap44xx_sdma_BURST_EN_SINGLE);  // Single?
    // 2.3. Src/dst endianism.
    csdp = omap44xx_sdma_dma4_csdp_src_endian_insert(csdp,
            omap44xx_sdma_ENDIAN_LITTLE);  // Little-endian?
    csdp = omap44xx_sdma_dma4_csdp_dst_endian_insert(csdp,
            omap44xx_sdma_ENDIAN_LITTLE);  // Little-endian?
    // 2.4. Write mode: last non posted.
    csdp = omap44xx_sdma_dma4_csdp_write_mode_insert(csdp,
            omap44xx_sdma_WRITE_MODE_LAST_NON_POSTED);
    // 2.5 Src/dst (non-)packed.
    csdp = omap44xx_sdma_dma4_csdp_src_packed_insert(csdp,
            omap44xx_sdma_SRC_PACKED_DISABLE);  // Non-packed?
    csdp = omap44xx_sdma_dma4_csdp_dst_packed_insert(csdp,
            omap44xx_sdma_SRC_PACKED_DISABLE);  // Non-packed?
    // 2.6 Write back reg value.
    omap44xx_sdma_dma4_csdp_wr(&sd->sdma_dev, chan, csdp);

    // 2. CEN: elements per frame.
    omap44xx_sdma_dma4_cen_t cen = omap44xx_sdma_dma4_cen_rd(&sd->sdma_dev,
            chan);
    size_t en = 32;  // 32 elements per frame.
    cen = omap44xx_sdma_dma4_cen_channel_elmnt_nbr_insert(cen, en);
    omap44xx_sdma_dma4_cen_wr(&sd->sdma_dev, chan, cen);

    // 3. CFN: frames per block.
    omap44xx_sdma_dma4_cfn_t cfn = omap44xx_sdma_dma4_cfn_rd(&sd->sdma_dev,
            chan);
    size_t fn = 4;  // 4 frames per block.
    cfn = omap44xx_sdma_dma4_cfn_channel_frame_nbr_insert(cfn, fn);
    omap44xx_sdma_dma4_cfn_wr(&sd->sdma_dev, chan, cfn);

    // 4. CSSA, CDSA: Src and dest start addr.
    // 4.1. Identify frame. Will copy from frame.base to frame.base + 512.
    struct frame_identity fid;
    errval_t err = frame_identify(*frame, &fid);
    if (err_is_fail(err)) {
        USER_PANIC_ERR(err, "frame identify for copying");
    }

    // 4.2. Src address.
    omap44xx_sdma_dma4_cssa_wr(&sd->sdma_dev, chan, (uint32_t) fid.base);
    // 4.3 Dst address.
    omap44xx_sdma_dma4_cdsa_wr(&sd->sdma_dev, chan,
            (uint32_t) fid.base + es * en * fn);
    debug_printf("Copy src = %x, copy dst = %x\n", (uint32_t) fid.base,
            (uint32_t) fid.base + es * en * fn);

    // 5. CCR.
    // 5.1. R/W port addressing modes.
    omap44xx_sdma_dma4_ccr_t ccr = omap44xx_sdma_dma4_ccr_rd(&sd->sdma_dev,
            chan);
    ccr = omap44xx_sdma_dma4_ccr_src_amode_insert(ccr,
            omap44xx_sdma_ADDR_MODE_POST_INCR);  // Post-increment.
    ccr = omap44xx_sdma_dma4_ccr_dst_amode_insert(ccr,
            omap44xx_sdma_ADDR_MODE_POST_INCR);  // Post-increment.
    // 5.2. R/W port priority bit.
    ccr = omap44xx_sdma_dma4_ccr_read_priority_insert(ccr,
            omap44xx_sdma_PORT_PRIORITY_LOW);
    ccr = omap44xx_sdma_dma4_ccr_write_priority_insert(ccr,
            omap44xx_sdma_PORT_PRIORITY_LOW);
    // 5.3. DMA request number 0: software-triggered transfer.
    ccr = omap44xx_sdma_dma4_ccr_synchro_control_insert(ccr, 0);
    ccr = omap44xx_sdma_dma4_ccr_synchro_control_upper_insert(ccr, 0);
    // 5.4. Write back reg value.
    omap44xx_sdma_dma4_ccr_wr(&sd->sdma_dev, chan, ccr);

    // 6. CSE, CSF, CDE, CDF: all 1 as per the manual example in 16.5.2.
    // 6.1. CSE.
    omap44xx_sdma_dma4_csei_t cse = omap44xx_sdma_dma4_csei_rd(&sd->sdma_dev,
            chan);
    cse = omap44xx_sdma_dma4_csei_channel_src_elmnt_index_insert(cse, 1);
    omap44xx_sdma_dma4_csei_wr(&sd->sdma_dev, chan, cse);
    // 6.2. CSF.
    omap44xx_sdma_dma4_csfi_wr(&sd->sdma_dev, chan, 1);
    // 6.3. CDE.
    omap44xx_sdma_dma4_cdei_t cde = omap44xx_sdma_dma4_cdei_rd(&sd->sdma_dev,
            chan);
    cde = omap44xx_sdma_dma4_cdei_channel_dst_elmnt_index_insert(cde, 1);
    omap44xx_sdma_dma4_cdei_wr(&sd->sdma_dev, chan, 1);
    // 6.4. CDF.
    omap44xx_sdma_dma4_cdfi_wr(&sd->sdma_dev, chan, 1);

    // 7. Start transfer!
    ccr = omap44xx_sdma_dma4_ccr_rd(&sd->sdma_dev, chan);
    ccr = omap44xx_sdma_dma4_ccr_enable_insert(ccr, 1);
    omap44xx_sdma_dma4_ccr_wr(&sd->sdma_dev, chan, ccr);
}