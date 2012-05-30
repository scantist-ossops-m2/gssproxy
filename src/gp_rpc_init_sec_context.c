/*
   GSS-PROXY

   Copyright (C) 2011 Red Hat, Inc.
   Copyright (C) 2011 Simo Sorce <simo.sorce@redhat.com>

   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the "Software"),
   to deal in the Software without restriction, including without limitation
   the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
   THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
   DEALINGS IN THE SOFTWARE.
*/

#include "gp_rpc_process.h"

int gp_init_sec_context(struct gssproxy_ctx *gpctx,
                        struct gp_service *gpsvc,
                        union gp_rpc_arg *arg,
                        union gp_rpc_res *res)
{
    struct gssx_arg_init_sec_context *isca;
    struct gssx_res_init_sec_context *iscr;
    gss_ctx_id_t ctx = GSS_C_NO_CONTEXT;
    gss_cred_id_t ich = GSS_C_NO_CREDENTIAL;
    gss_name_t target_name = GSS_C_NO_NAME;
    gss_OID mech_type = GSS_C_NO_OID;
    uint32_t req_flags;
    uint32_t time_req;
    struct gss_channel_bindings_struct cbs;
    gss_channel_bindings_t pcbs;
    gss_buffer_desc ibuf;
    gss_buffer_t pibuf;
    gss_OID actual_mech_type = GSS_C_NO_OID;
    gss_buffer_desc obuf = GSS_C_EMPTY_BUFFER;
    uint32_t ret_maj;
    uint32_t ret_min;
    int exp_ctx_type;
    int ret;

    isca = &arg->init_sec_context;
    iscr = &res->init_sec_context;

    exp_ctx_type = gp_get_exported_context_type(&isca->call_ctx);
    if (exp_ctx_type == -1) {
        ret_maj = GSS_S_FAILURE;
        ret_min = EINVAL;
        goto done;
    }

    if (isca->context_handle) {
        ret_maj = gp_import_gssx_to_ctx_id(&ret_min, 0,
                                           isca->context_handle, &ctx);
        if (ret_maj) {
            goto done;
        }
    }

    if (isca->cred_handle) {
        ret = gp_find_cred(gpsvc, isca->cred_handle, &ich);
        if (ret) {
            ret_maj = GSS_S_NO_CRED;
            ret_min = ret;
            goto done;
        }
    }

    ret_maj = gp_conv_gssx_to_name(&ret_min, isca->target_name, &target_name);
    if (ret_maj) {
        goto done;
    }

    ret = gp_conv_gssx_to_oid_alloc(&isca->mech_type, &mech_type);
    if (ret) {
        ret_maj = GSS_S_FAILURE;
        ret_min = ret;
        goto done;
    }

    req_flags = isca->req_flags;
    time_req = isca->time_req;

    if (isca->input_cb) {
        pcbs = &cbs;
        gp_conv_gssx_to_cb(isca->input_cb, pcbs);
    } else {
        pcbs = GSS_C_NO_CHANNEL_BINDINGS;
    }

    if (isca->input_token) {
        gp_conv_gssx_to_buffer(isca->input_token, &ibuf);
        pibuf = &ibuf;
    }

    ret_maj = gss_init_sec_context(&ret_min,
                                   ich,
                                   &ctx,
                                   target_name,
                                   mech_type,
                                   req_flags,
                                   time_req,
                                   pcbs,
                                   pibuf,
                                   &actual_mech_type,
                                   &obuf,
                                   NULL,
                                   NULL);
    if (ret_maj != GSS_S_COMPLETE &&
        ret_maj != GSS_S_CONTINUE_NEEDED) {
        goto done;
    }

    iscr->context_handle = calloc(1, sizeof(gssx_ctx));
    if (!iscr->context_handle) {
        ret_maj = GSS_S_FAILURE;
        ret_min = ENOMEM;
        goto done;
    }
    ret_maj = gp_export_ctx_id_to_gssx(&ret_min, exp_ctx_type,
                                       &ctx, iscr->context_handle);
    if (ret_maj) {
        goto done;
    }

    if (obuf.length != 0) {
        iscr->output_token = calloc(1, sizeof(gssx_buffer));
        if (!iscr->output_token) {
            ret_maj = GSS_S_FAILURE;
            ret_min = ENOMEM;
            goto done;
        }
        ret = gp_conv_buffer_to_gssx(&obuf, iscr->output_token);
        if (ret) {
            ret_maj = GSS_S_FAILURE;
            ret_min = ret;
            goto done;
        }
    }

done:
    ret = gp_conv_status_to_gssx(&isca->call_ctx,
                                 ret_maj, ret_min, mech_type,
                                 &iscr->status);
    gss_release_name(&ret_min, &target_name);
    gss_release_oid(&ret_min, &mech_type);
    return ret;
}
