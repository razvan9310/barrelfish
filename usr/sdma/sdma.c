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

#include <stdlib.h>
#include <string.h>

#include <aos/aos_rpc.h>
#include <aos/waitset.h>
#include <driverkit/driverkit.h>
#include <omap44xx_map.h>
#include <omap_timer/timer.h>

#include "sdma.h"

struct client_state* sdma_identify_client_cap(struct sdma_driver* sd,
        struct capref* cap)
{
    struct capability ret;
    errval_t err = debug_cap_identify(*cap, &ret);
    if (err_is_fail(err)) {
        DEBUG_ERR(err, "identifying client cap");
        return NULL;
    }

    struct client_state* client = sd->clients;
    while (client != NULL) {
        if (client->remote_ep.listener == ret.u.endpoint.listener
                && client->remote_ep.epoffset == ret.u.endpoint.epoffset) {
            break;
        }
        client = client->next;
    }
    
    return client;
}

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

    sd->clients = NULL;
    // Initialize UID generator. Do this here, before any client connections
    // are set up, to prevent clients for potentially seeding the PRNG during
    // the same clock second so as to predict the UID sequence.
    omap_timer_init();
    omap_timer_ctrl(true);
    srand((unsigned) omap_timer_read());
}

void sdma_interrupt_handler(void* arg)
{
    debug_printf("!!! Got SDMA interrupt!\n");
    struct sdma_driver* sd = (struct sdma_driver*) arg;
    uint8_t irq_line = 0;
    uint32_t irq_status = omap44xx_sdma_dma4_irqstatus_line_rd(&sd->sdma_dev,
            irq_line);
    sdma_update_channel_status(sd, irq_line, irq_status);
}

errval_t sdma_setup_config(struct sdma_driver* sd)
{
    // 1. Setup the interrupt handler.
    CHECK("setup SDMA interrupt handler",
            inthandler_setup_arm(sdma_interrupt_handler, sd, SDMA_IRQ_LINE_0));

    // 2. Configure GCR register.
    omap44xx_sdma_dma4_gcr_t gcr = omap44xx_sdma_dma4_gcr_rd(&sd->sdma_dev);
    // 2.1. max_channel_fifo_depth.
    gcr = omap44xx_sdma_dma4_gcr_max_channel_fifo_depth_insert(gcr, 255);  // Maximum.
    // 2.2. arbitration_rate.
    gcr = omap44xx_sdma_dma4_gcr_arbitration_rate_insert(gcr, 1);  // 1:1.
    // 2.3. Write back.
    omap44xx_sdma_dma4_gcr_wr(&sd->sdma_dev, gcr);

    // 3. Enable & clear IRQ lie 0.
    // 3.1. Enable IRQ line 0.
    uint32_t enable_value = SDMA_IRQ_ENABLE_ALL;
    omap44xx_sdma_dma4_irqenable_wr(&sd->sdma_dev, 0, enable_value);

    // 3. CICR for each channel.
    for (chanid_t chan = 0; chan < SDMA_CHANNELS; ++chan) {
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

errval_t sdma_setup_rpc_server(struct sdma_driver* sd)
{
    sd->lc = get_init_rpc()->lc;

    static struct aos_rpc new_aos_rpc;
    static struct waitset init_ws;
    waitset_init(&init_ws);
    CHECK("initializing new aos_rpc_init",
            aos_rpc_init(&new_aos_rpc, &init_ws));
    set_init_rpc(&new_aos_rpc);

    CHECK("creating SDMA channel slot", lmp_chan_alloc_recv_slot(&sd->lc));
    CHECK("copying local cap to cap_sdma_ep",
            cap_copy(cap_sdma_ep, sd->lc.local_cap));

    size_t recv_arg_size = ROUND_UP(sizeof(struct sdma_driver*), 4)
            + ROUND_UP(sizeof(struct lmp_chan), 4)
            + ROUND_UP(sizeof(struct client_state*), 4);
    void* recv_arg = malloc(recv_arg_size);
    void* recv_arg_cpy = recv_arg;

    *((struct sdma_driver**) recv_arg) = sd;
    recv_arg = (void*) ROUND_UP(
            (uintptr_t) recv_arg + sizeof(struct sdma_driver*), 4);
    *((struct lmp_chan*) recv_arg) = sd->lc;
    CHECK("registering initial SDMA receive",
            lmp_chan_register_recv(&sd->lc, get_default_waitset(),
                    MKCLOSURE(sdma_serve_rpc, recv_arg_cpy)));

    return SYS_ERR_OK;
}

void sdma_serve_rpc(void* arg)
{
    void* arg_cpy = arg;
    struct sdma_driver** sd = (struct sdma_driver**) arg;
    arg = (void*) ROUND_UP((uintptr_t) arg + sizeof(struct sdma_driver*), 4);
    struct lmp_chan* lc = (struct lmp_chan*) arg;
    arg = (void*) ROUND_UP((uintptr_t) arg + sizeof(struct lmp_chan), 4);
    struct client_state** client = (struct client_state**) arg;

    struct lmp_recv_msg msg = LMP_RECV_MSG_INIT;
    struct capref client_cap;

    errval_t err = lmp_chan_recv(lc, &msg, &client_cap);

    // Reregister.
    lmp_chan_alloc_recv_slot(lc);
    lmp_chan_register_recv(lc, get_default_waitset(),
            MKCLOSURE(sdma_serve_rpc, arg_cpy));

    if (err_is_fail(err) && !lmp_err_is_transient(err)) {
        USER_PANIC_ERR(err, "lmp_chan_recv failed");
    }

    void* response_fn = NULL;
    void* response_arg = NULL;

    switch (msg.words[0]) {
        case SDMA_RPC_HANDSHAKE:
            response_fn = (void*) sdma_send_handshake;
            response_arg = sdma_process_handshake(*sd, &client_cap);
            break;
        case SDMA_RPC_MEMCPY_SRC:
        case SDMA_RPC_MEMCPY_DST:
            response_fn = (void*) sdma_send_err;
            response_arg = sdma_process_memcpy(*sd, *client, &msg, &client_cap);
            break;
        default:
            debug_printf("WARNING: invalid SDMA RPC code\n");
    }

    if (response_fn != NULL && response_arg != NULL) {
        struct lmp_chan* out = (struct lmp_chan*) response_arg;
        err = lmp_chan_register_send(out, get_default_waitset(),
                MKCLOSURE(response_fn, response_arg));
        if (err_is_fail(err)) {
            DEBUG_ERR(err, "register send in sdma_serve_rpc");
        }
    }
}

void* sdma_process_handshake(struct sdma_driver* sd, struct capref* cap)
{
    struct client_state* client = sdma_identify_client_cap(sd, cap);
    if (client != NULL) {
        // Client already exists?
        debug_printf("Got second SDMA hanshake request from same client, "
                "ignoring it\n");
        return NULL;
    }

    client = (struct client_state*) malloc(sizeof(struct client_state));
    if (sd->clients == NULL) {
        client->next = NULL;
    } else {
        sd->clients->prev = client;
        client->next = sd->clients;
    }
    client->prev = NULL;
    client->have_caps = 0;
    client->src_offset = 0;
    client->dst_offset = 0;
    client->len = 0;
    sd->clients = client;

    // Endpoint for further reference.
    struct capability ret;
    debug_cap_identify(*cap, &ret);
    client->remote_ep = ret.u.endpoint;

    // New channel.
    lmp_chan_accept(&client->lc, DEFAULT_LMP_BUF_WORDS, *cap);
    errval_t err = lmp_chan_alloc_recv_slot(&client->lc);
    if (err_is_fail(err)) {
        USER_PANIC_ERR(err, "lmp_chan_alloc_recv_slot for new client");
    }

    debug_cap_identify(client->lc.local_cap, &ret);

    size_t recv_arg_size = ROUND_UP(sizeof(struct sdma_driver*), 4)
            + ROUND_UP(sizeof(struct lmp_chan), 4)
            + ROUND_UP(sizeof(struct client_state*), 4);
    void* recv_arg = malloc(recv_arg_size);
    void* recv_arg_cpy = recv_arg;

    *((struct sdma_driver**) recv_arg) = sd;
    recv_arg = (void*) ROUND_UP(
            (uintptr_t) recv_arg + sizeof(struct sdma_driver*), 4);
    *((struct lmp_chan*) recv_arg) = client->lc;
    recv_arg = (void*) ROUND_UP(
            (uintptr_t) recv_arg + sizeof(struct lmp_chan), 4);
    *((struct client_state**) recv_arg) = client;

    err = lmp_chan_register_recv(&client->lc, get_default_waitset(),
            MKCLOSURE(sdma_serve_rpc, recv_arg_cpy));
    if (err_is_fail(err)) {
        USER_PANIC_ERR(err, "lmp_chan_register_recv for new client");
    }

    // Response args.
    return (void*) &client->lc;
}

void sdma_send_handshake(void* arg)
{
    // 1. Channel to send down.
    struct lmp_chan* lc = (struct lmp_chan*) arg;

    // 2. Send response.
    errval_t err = lmp_chan_send1(lc, LMP_FLAG_SYNC, lc->local_cap,
            SDMA_RPC_OK);
    if (err_is_fail(err)) {
        USER_PANIC_ERR(err, "lmp_chan_send handshake");
    }
}

void* sdma_process_memcpy(struct sdma_driver* sd, struct client_state* client,
        struct lmp_recv_msg* msg, struct capref* cap)
{
    if (msg->words[0] == SDMA_RPC_MEMCPY_SRC) {
        // Got src cap + len.
        client->src = *cap;
        client->have_caps |= CAP_MASK_SRC;
        client->src_offset = (size_t) msg->words[1];
        client->len = (size_t) msg->words[2];
    } else {
        // Got dst cap.
        client->dst = *cap;
        client->dst_offset = (size_t) msg->words[1];
        client->have_caps |= CAP_MASK_DST;
    }

    errval_t err = SYS_ERR_OK;
    if (CAP_MASK_READY == client->have_caps) {
        struct frame_identity src_id;
        err = frame_identify(client->src, &src_id);
        if (err_is_fail(err)) {
            DEBUG_ERR(err, "identifying src frame for memcpy");
        } else {
            struct frame_identity dst_id;
            err = frame_identify(client->dst, &dst_id);
            if (err_is_fail(err)) {
                DEBUG_ERR(err, "identifying dst frame for memcpy");
            } else {
                bool cond1 = client->src_offset < src_id.bytes
                        && client->dst_offset < dst_id.bytes;
                size_t src_bytes = (size_t) src_id.bytes - client->src_offset;
                size_t dst_bytes = (size_t) dst_id.bytes - client->dst_offset;
                size_t src_addr = (size_t) src_id.base + client->src_offset;
                size_t dst_addr = (size_t) dst_id.base + client->dst_offset;

                // As per the string.c memcpy implementation.
                bool cond2 = src_addr < dst_addr
                        && src_addr + client->len <= dst_addr;
                bool cond3 = dst_addr < src_addr
                        && dst_addr + client->len <= src_addr;

                // Plus len needs to be lte min{src_bytes, dst_bytes}.
                bool cond4 = client->len <= src_bytes
                        && client->len <= dst_bytes;

                if (cond1 && (cond2 || cond3) && cond4) {
                    sdma_start_transfer(sd, client->lc, src_addr, dst_addr,
                            client->len);
                } else {
                    err = SDMA_ERR_MEMCPY;
                }
            }
        }
    
        // Clear cap bits and length field.
        client->have_caps = 0;
        client->src_offset = 0;
        client->dst_offset = 0;
        client->len = 0;
    }

    // Response args.
    size_t arg_size = ROUND_UP(sizeof(struct lmp_chan), 4)
            + ROUND_UP(sizeof(errval_t), 4);
    void* arg = malloc(arg_size);
    void* return_arg = arg;

    // 1. Channel to send down.
    *((struct lmp_chan*) arg) = client->lc;

    // 2. Error code.
    arg = (void*) ROUND_UP((uintptr_t) arg + sizeof(struct lmp_chan), 4);
    *((errval_t*) arg) = err;

    return return_arg;
}

void sdma_send_err(void* arg)
{
    // 1. Channel to send down.
    struct lmp_chan* lc = (struct lmp_chan*) arg;

    // 2. Error code.
    arg = (void*) ROUND_UP((uintptr_t) arg + sizeof(struct lmp_chan), 4);
    errval_t* err = (errval_t*) arg;

    // 3. Generate response code.
    size_t code = err_is_ok(*err) ? SDMA_RPC_OK : SDMA_RPC_FAILED;

    // 4. Send response.
    errval_t send_err = lmp_chan_send2(lc, LMP_FLAG_SYNC, NULL_CAP, code, *err);
    if (err_is_fail(send_err)) {
        USER_PANIC_ERR(send_err, "lmp_chan_send err");
    }

    // 5. Free args.
    free(arg);
}

chanid_t sdma_avail_channel(struct sdma_driver* sd)
{
    for (chanid_t chan = 0; chan < SDMA_CHANNELS; ++chan) {
        if (!sd->chan_state[chan].transfer_in_progress) {
            return chan;
        }
    }
    return SDMA_CHANNELS;
}

errval_t sdma_start_transfer(struct sdma_driver* sd, struct lmp_chan lc,
        size_t src_addr, size_t dst_addr, size_t len)
{
    chanid_t chan = sdma_avail_channel(sd);
    if (!sdma_valid_channel(chan)) {
        return SDMA_ERR_NO_AVAIL_CHANNEL;
    }
    sd->chan_state[chan].out_lc = lc;

    // Queue up sdma job from src to dst, size len, on channel chan.
    // 1. CSDP.
    omap44xx_sdma_dma4_csdp_t csdp = omap44xx_sdma_dma4_csdp_rd(&sd->sdma_dev,
            chan);
    // 1.1. Transfer ES.
    size_t es = 4;  // 4-byte == 32-bit.
    csdp = omap44xx_sdma_dma4_csdp_data_type_insert(csdp,
            omap44xx_sdma_DATA_TYPE_32BIT);
    // 1.2. R/W port access types (single vs burst).
    csdp = omap44xx_sdma_dma4_csdp_src_burst_en_insert(csdp,
            omap44xx_sdma_BURST_EN_SINGLE);  // Single?
    csdp = omap44xx_sdma_dma4_csdp_dst_burst_en_insert(csdp,
            omap44xx_sdma_BURST_EN_SINGLE);  // Single?
    // 1.3. Src/dst endianism.
    csdp = omap44xx_sdma_dma4_csdp_src_endian_insert(csdp,
            omap44xx_sdma_ENDIAN_LITTLE);  // Little-endian?
    csdp = omap44xx_sdma_dma4_csdp_dst_endian_insert(csdp,
            omap44xx_sdma_ENDIAN_LITTLE);  // Little-endian?
    // 1.4. Write mode: last non posted.
    csdp = omap44xx_sdma_dma4_csdp_write_mode_insert(csdp,
            omap44xx_sdma_WRITE_MODE_LAST_NON_POSTED);
    // 1.5 Src/dst (non-)packed.
    csdp = omap44xx_sdma_dma4_csdp_src_packed_insert(csdp,
            omap44xx_sdma_SRC_PACKED_DISABLE);  // Non-packed?
    csdp = omap44xx_sdma_dma4_csdp_dst_packed_insert(csdp,
            omap44xx_sdma_SRC_PACKED_DISABLE);  // Non-packed?
    // 1.6 Write back reg value.
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
    size_t fs = es * en;
    size_t fn =  len / fs;
    if (len % fs != 0) {
        ++fn;
    }
    cfn = omap44xx_sdma_dma4_cfn_channel_frame_nbr_insert(cfn, fn);
    omap44xx_sdma_dma4_cfn_wr(&sd->sdma_dev, chan, cfn);

    // 4. CSSA, CDSA: Src and dest start addr.
    // 4.2. Src address.
    omap44xx_sdma_dma4_cssa_wr(&sd->sdma_dev, chan, (uint32_t) src_addr);
    // 4.3 Dst address.
    omap44xx_sdma_dma4_cdsa_wr(&sd->sdma_dev, chan, (uint32_t) dst_addr);

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

    // 7. IRQ status.
    const int irq_line = 0;
    omap44xx_sdma_dma4_irqstatus_line_wr(&sd->sdma_dev, irq_line, -1);

    // 8. Clear CSR register.
    omap44xx_sdma_dma4_csr_t csr = omap44xx_sdma_dma4_csr_rd(
            &sd->sdma_dev, chan);
    csr = 0x0;
    omap44xx_sdma_dma4_csr_wr(&sd->sdma_dev, chan, csr);

    // 9. Start transfer!
    ccr = omap44xx_sdma_dma4_ccr_rd(&sd->sdma_dev, chan);
    ccr = omap44xx_sdma_dma4_ccr_enable_insert(ccr, 1);
    omap44xx_sdma_dma4_ccr_wr(&sd->sdma_dev, chan, ccr);

    sd->chan_state[chan].transfer_in_progress = true;

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
            }

            // 5. Create response args.
            size_t arg_size = ROUND_UP(sizeof(struct lmp_chan), 4)
                    + ROUND_UP(sizeof(errval_t), 4);
            void* arg = malloc(arg_size);
            void* response_arg = arg;

            // 5.1. Channel to send down.
            *((struct lmp_chan*) arg) = sd->chan_state[chan].out_lc;

            // 5.2. Error code from sdma_start_transfer.
            arg = (void*) ROUND_UP((uintptr_t) arg + sizeof(struct lmp_chan), 4);
            *((errval_t*) arg) = sd->chan_state[chan].err;

            // 6. Send response to client.
            errval_t send_err = lmp_chan_register_send(
                    &sd->chan_state[chan].out_lc,
                    get_default_waitset(),
                    MKCLOSURE(sdma_send_err, response_arg));
            if (err_is_fail(send_err)) {
                USER_PANIC_ERR(send_err,
                        "lmp_chan_register_send in interrupt handler");
            }

            // 7. Clear CSR.
            omap44xx_sdma_dma4_csr_wr(&sd->sdma_dev, chan, -1);
        }
    }
}
