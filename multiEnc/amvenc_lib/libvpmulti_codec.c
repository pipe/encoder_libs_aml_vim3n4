/*
* Copyright (c) 2019 Amlogic, Inc. All rights reserved.
*
* This source code is subject to the terms and conditions defined in below
* which is part of this source code package.
*
* Description:
*/

// Copyright (C) 2019 Amlogic, Inc. All rights reserved.
//
// All information contained herein is Amlogic confidential.
//
// This software is provided to you pursuant to Software License
// Agreement (SLA) with Amlogic Inc ("Amlogic"). This software may be
// used only in accordance with the terms of this agreement.
//
// Redistribution and use in source and binary forms, with or without
// modification is strictly prohibited without prior written permission
// from Amlogic.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

#if defined(__ANDROID__)
#include <utils/Log.h>
#define LOGCAT
#endif

#include <sys/time.h>
#include <sys/mman.h>
#include "include/vp_multi_codec_1_0.h"
#include "include/AML_MultiEncoder.h"
#include "include/enc_define.h"
#include "vdi_osal.h"
#include "include/h264bitstream.h"
#include "shmem_ctl.h"



#define MAX_FRAME_HIST         (128)
#define MAX_FRAME_WINDOW       (120)

const char version[] = "Amlogic libvp_multi_codec version 1.0";

#define ENCODE_TIME_OUTER 0
#if ENCODE_TIME_OUTER
static unsigned long encode_time_per_frame;
static unsigned long long total_encode_time;
static unsigned long long total_encode_frames;
static struct timeval start_test;
static struct timeval end_test;
#endif

#define GET_DROPPABLE_P 1
#if defined(__ANDROID__)
#define LOG_LINE() ALOGD("[%s:%d]\n", __FUNCTION__, __LINE__)
#endif

const char* vl_get_version() {
  return version;
}

typedef struct vp_multi_s {
  AMVEncInitParams mEncParams;

  AMVEncBufferType bufType;
  bool mPrependSPSPPSToIDRFrames;
  bool mSpsPpsHeaderReceived;
  bool mKeyFrameRequested;
  bool mSkipFrameRequested;
  int mLongTermRefRequestFlags;
  int mNumInputFrames;
  AMVEncFrameFmt fmt;
  int mSPSPPSDataSize;
  char *mSPSPPSData;
  amv_enc_handle_t am_enc_handle;
  int shared_fd[3];
  uint32 mNumPlanes;
  int frame_sz_hist[MAX_FRAME_HIST];
  int hist_in_idx;
  int hist_out_idx;
  int total_win_size;
  int hist_win_len;
  int hist_skip_thresh;
  void *userData;
} VPMultiEncHandle;

static int64_t GetNowUs() {
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (int64_t) tv.tv_sec * 1000000 + (int64_t) tv.tv_usec;
}

AMVEnc_Status initEncParams(VPMultiEncHandle *handle,
                        vl_codec_id_t codec_id,
                        vl_encode_info_t encode_info,
                        qp_param_t* qp_tbl) {
    int width = encode_info.width;
    int height = encode_info.height;
    VLOG(DEBUG, "bit_rate:%d", encode_info.bit_rate);
    if ((width % 16 != 0 || height % 2 != 0)) {
        VLOG(DEBUG, "Video frame size %dx%d must be a multiple of 16", width, height);
        //return -1;
    } else if (height % 16 != 0) {
        VLOG(DEBUG, "Video frame height is not standard:%d", height);
    } else {
        VLOG(DEBUG, "Video frame size is %d x %d", width, height);
    }
    handle->mEncParams.rate_control = ENC_SETTING_ON;
    handle->mEncParams.initQP = qp_tbl->qp_I_base;
    handle->mEncParams.initQP_P = qp_tbl->qp_P_base;
    handle->mEncParams.initQP_B = qp_tbl->qp_B_base;
    handle->mEncParams.maxDeltaQP = qp_tbl->qp_I_max - qp_tbl->qp_I_min;
    handle->mEncParams.maxQP = qp_tbl->qp_I_max;
    handle->mEncParams.minQP = qp_tbl->qp_I_min;
    handle->mEncParams.maxQP_P = qp_tbl->qp_P_max;
    handle->mEncParams.minQP_P = qp_tbl->qp_P_min;
    handle->mEncParams.maxQP_B = qp_tbl->qp_B_max;
    handle->mEncParams.minQP_B = qp_tbl->qp_B_min;
    handle->mEncParams.qp_mode = encode_info.qp_mode;

    handle->mEncParams.forcePicQpEnable = encode_info.forcePicQpEnable;
    //printf("handle->mEncParams.forcePicQpEnable %d\n",handle->mEncParams.forcePicQpEnable);
    handle->mEncParams.forcePicQpI = encode_info.forcePicQpI;
    handle->mEncParams.forcePicQpP = encode_info.forcePicQpP;
    handle->mEncParams.forcePicQpB = encode_info.forcePicQpB;
    if (handle->mEncParams.forcePicQpEnable) {
        handle->mEncParams.rate_control = ENC_SETTING_OFF;
    }
    //handle->mEncParams.rotate_angle = encode_info.frame_rotation;
    handle->mEncParams.rotate_angle = getRotation(encode_info.frame_rotation);
    handle->mEncParams.mirror = getMirror(encode_info.frame_mirroring);
    handle->mEncParams.init_CBP_removal_delay = 1600;
    handle->mEncParams.auto_scd = ENC_SETTING_OFF;
    handle->mEncParams.out_of_band_param_set = ENC_SETTING_OFF;
    handle->mEncParams.num_ref_frame = 1;
    handle->mEncParams.num_slice_group = 1;
    //handle->mEncParams.nSliceHeaderSpacing = 0;
    handle->mEncParams.fullsearch = ENC_SETTING_OFF;
    handle->mEncParams.search_range = 16;
    //handle->mEncParams.sub_pel = ENC_SETTING_OFF;
    //handle->mEncParams.submb_pred = ENC_SETTING_OFF;
    handle->mEncParams.width = width;
    handle->mEncParams.height = height;
    handle->mEncParams.src_width = width;
    handle->mEncParams.src_height = height;
    handle->mEncParams.bitrate = encode_info.bit_rate;
    handle->mEncParams.frame_rate = encode_info.frame_rate;
    handle->mEncParams.CPB_size = (uint32)(encode_info.bit_rate >> 1);
    handle->mEncParams.FreeRun = ENC_SETTING_OFF;

    if (encode_info.bitstream_buf_sz_kb > 0 && encode_info.bitstream_buf_sz_kb <= (32 * 1024)) {
        handle->mEncParams.es_buf_sz =  encode_info.bitstream_buf_sz_kb*1024;
    } else {
        if (encode_info.bitstream_buf_sz > 0 && encode_info.bitstream_buf_sz <= 32)
            handle->mEncParams.es_buf_sz =  encode_info.bitstream_buf_sz*1024*1024;
        else  //default value
            handle->mEncParams.es_buf_sz = 0; //out of range, use default
    }

    if (encode_info.intra_refresh_mode && encode_info.intra_refresh_mode <= 4) {
      handle->mEncParams.IntraRefreshMode = encode_info.intra_refresh_mode;
      handle->mEncParams.IntraRefreshArg = encode_info.intra_refresh_arg;
    } else {
      handle->mEncParams.IntraRefreshMode = 0;
      handle->mEncParams.IntraRefreshArg = 0;
    }
    handle->mEncParams.encode_once = 1;

    if (encode_info.enc_feature_opts & ENABLE_ROI_FEATURE)
      handle->mEncParams.roi_enable = 1;
    if (encode_info.enc_feature_opts & ENABLE_PARA_UPDATE)
      handle->mEncParams.param_change_enable = 1;
    if (encode_info.enc_feature_opts & ENABLE_LONG_TERM_REF)
      handle->mEncParams.longterm_ref_enable = 1;
    if (encode_info.enc_feature_opts & 0x7c) {
        handle->mEncParams.GopPreset = (AMVGOPModeOPT)
                ((encode_info.enc_feature_opts >>2) & 0x1f);
    }

   VLOG(TRACE, "enc_feature_opts is 0x%x , GopPresetis 0x%x \n",encode_info.enc_feature_opts,handle->mEncParams.GopPreset);

  if (encode_info.img_format == IMG_FMT_NV12) {
    VLOG(INFO, "img_format is IMG_FMT_NV12 \n");
    handle->fmt = AMVENC_NV12;
  } else if (encode_info.img_format == IMG_FMT_NV21) {
    VLOG(INFO, "img_format is IMG_FMT_NV21 \n");
    handle->fmt = AMVENC_NV21;
  } else if (encode_info.img_format == IMG_FMT_YUV420P) {
    VLOG(INFO, "img_format is IMG_FMT_YUV420P \n");
    handle->fmt = AMVENC_YUV420P;
  } else if (encode_info.img_format == IMG_FMT_RGB888) {
    VLOG(INFO, "img_format is IMG_FMT_RGB888 \n");
    handle->fmt = AMVENC_RGB888;
  } else if (encode_info.img_format == IMG_FMT_RGBA8888) {
    VLOG(INFO, "img_format is IMG_FMT_RGBA8888 \n");
    handle->fmt = AMVENC_RGBA8888;
  } else {
    VLOG(ERR, "img_format %d not supprot\n",encode_info.img_format);
    return AMVENC_FAIL;
  }
     handle->mEncParams.fmt = handle->fmt;
    if ((encode_info.img_format == IMG_FMT_RGB888) || (encode_info.img_format == IMG_FMT_RGBA8888)) {
      handle->mEncParams.fmt = AMVENC_NV21; //ge2d dst format PIXEL_FORMAT_YCrCb_420_SP
    }
    // Set IDR frame refresh interval
    if (encode_info.gop <= 0) {
        handle->mEncParams.idr_period = 0;   //an infinite period, only one I frame
    } else {
        handle->mEncParams.idr_period = encode_info.gop; //period of I frame, 1 means all frames are I type.
    }
    VLOG(DEBUG, "mEncParams.idrPeriod: %d, gop %d\n", handle->mEncParams.idr_period, encode_info.gop);
    // Set profile and level
    if (codec_id == CODEC_ID_H265) {
        handle->mEncParams.stream_type = AMV_HEVC;
        handle->mEncParams.profile = encode_info.profile;
        handle->mEncParams.level = HEVC_LEVEL_NONE; // firmware determines a level.
        handle->mEncParams.hevc_tier = HEVC_TIER_MAIN;
        handle->mEncParams.initQP = 30;
        handle->mEncParams.BitrateScale = ENC_SETTING_OFF;
        handle->mEncParams.refresh_type = HEVC_CRA;
    } else if (codec_id == CODEC_ID_H264) {
        handle->mEncParams.stream_type = AMV_AVC;
        handle->mEncParams.profile = encode_info.profile;
        handle->mEncParams.level = AVC_LEVEL4;
        handle->mEncParams.initQP = 30;
        handle->mEncParams.BitrateScale = ENC_SETTING_OFF;
    } else {
        VLOG(ERR, "No surpported codec_id %d\n", codec_id);
        return AMVENC_FAIL;
    }
    /* multi-slice */
    if (encode_info.multi_slice_mode && encode_info.multi_slice_arg)
    {
        if (encode_info.multi_slice_mode > 2 ||
            encode_info.multi_slice_mode < 0) {
            VLOG(ERR, "Invalid muti_slice mode setting %d\n",
                 encode_info.multi_slice_mode);
        } else if (encode_info.multi_slice_mode > 1 &&
                   codec_id == CODEC_ID_H264) {
            VLOG(ERR, "H264 Invalid muti_slice mode setting %d\n",
                 encode_info.multi_slice_mode);
        } else {
            VLOG(DEBUG, "Multi slice set mode %d par %d\n",
                 encode_info.multi_slice_mode,
                 encode_info.multi_slice_arg);
            handle->mEncParams.slice_mode = encode_info.multi_slice_mode;
            handle->mEncParams.slice_arg = encode_info.multi_slice_arg;
        }
    }
    handle->mEncParams.cust_qp_delta = encode_info.cust_gop_qp_delta;
    return AMVENC_SUCCESS;
}

static void hist_reset(VPMultiEncHandle *handle)
{
    if (handle)
    {
        handle->hist_in_idx = 0;
        handle->hist_out_idx = 0;
        handle->total_win_size = 0;
    }
}

static void strict_rc_check(VPMultiEncHandle *handle) {
    int fullness, bitate, bitrate_target;
    fullness = handle->hist_in_idx - handle->hist_out_idx;
    if (fullness < 0) fullness += MAX_FRAME_HIST;
    while (fullness > handle->hist_win_len) { // have extra, moveout no need
        handle->total_win_size -= handle->frame_sz_hist[handle->hist_out_idx];
        if (handle->total_win_size <=0) {
            VLOG(ERR, "Size zero already size %d, fullness %d\n",
                 handle->total_win_size, fullness);
            break;
        }
        handle->hist_out_idx++;
        if (handle->hist_out_idx >= MAX_FRAME_HIST)
           handle->hist_out_idx = 0;
        fullness --;
    }
    if (fullness == handle->hist_win_len) {// get the enough check bitrate
        bitate = (handle->total_win_size * 8 * handle->mEncParams.frame_rate)/
                  handle->hist_win_len;
        bitrate_target = handle->mEncParams.bitrate*handle->hist_skip_thresh/100;
        VLOG(INFO, "real bitrate %d target %d\n", bitate, bitrate_target);
        if (bitate > bitrate_target)
        {
            VLOG(DEBUG, " frame %d real bitrate %d over target %d skip\n",
                     handle->mNumInputFrames, bitate, bitrate_target);
            handle->mSkipFrameRequested = true;
        }
    }
}

static void strict_rc_filled(VPMultiEncHandle *handle, int dataLength)
{
    int fullness, space;
    fullness = handle->hist_in_idx - handle->hist_out_idx;
    if (fullness < 0) fullness += MAX_FRAME_HIST;
    space = MAX_FRAME_HIST - fullness;
    if (space) { // we shall have space
       handle->frame_sz_hist[handle->hist_in_idx] = dataLength;
       handle->hist_in_idx ++;
       handle->total_win_size += dataLength;
       if (handle->hist_in_idx >= MAX_FRAME_HIST)
         handle->hist_in_idx = 0;
    } else {
            VLOG(ERR, "no space for frame %d, size %d out %d total %d\n",
                 handle->hist_in_idx, dataLength,
                 handle->hist_out_idx,
                 handle->total_win_size);
    }
}

bool check_qp_tbl(const qp_param_t* qp_tbl) {
  if (qp_tbl == NULL) {
    return false;
  }
  if (qp_tbl->qp_min < 0 || qp_tbl->qp_min > 51 || qp_tbl->qp_max < 0 ||
      qp_tbl->qp_max > 51) {
    VLOG(ERR,"qp_min or qp_max out of range [0, 51]\n");
    return false;
  }
  if (qp_tbl->qp_I_base < 0 || qp_tbl->qp_I_base > 51 ||
      qp_tbl->qp_P_base < 0 || qp_tbl->qp_P_base > 51) {
    VLOG(ERR,"qp_I_base or qp_P_base out of range [0, 51]\n");
    return false;
  }
  if (qp_tbl->qp_min > qp_tbl->qp_max || qp_tbl->qp_I_min > qp_tbl->qp_I_max ||
      qp_tbl->qp_P_min > qp_tbl->qp_P_max) {
    VLOG(ERR,"min qp larger than max qp\n");
    return false;
  }
  if (qp_tbl->qp_I_min < qp_tbl->qp_min || qp_tbl->qp_I_min > qp_tbl->qp_max ||
      qp_tbl->qp_I_max < qp_tbl->qp_min || qp_tbl->qp_I_max > qp_tbl->qp_max) {
    VLOG(ERR,"qp_min_I or qp_max_I out of range [qp_min, qp_max]\n");
    return false;
  }
  if (qp_tbl->qp_P_min < qp_tbl->qp_min || qp_tbl->qp_P_min > qp_tbl->qp_max ||
      qp_tbl->qp_P_max < qp_tbl->qp_min || qp_tbl->qp_P_max > qp_tbl->qp_max) {
    VLOG(ERR,"qp_min_P or qp_max_P out of range [qp_min, qp_max]\n");
    return false;
  }
  return true;
}

vl_codec_handle_t vl_multi_encoder_init(vl_codec_id_t codec_id,
                                       vl_encode_info_t encode_info,
                                        qp_param_t* qp_tbl) {
  int ret;
  VPMultiEncHandle* mHandle = (VPMultiEncHandle *) malloc(sizeof(VPMultiEncHandle));

  if (mHandle == NULL)
    goto exit;
  memset(mHandle, 0, sizeof(VPMultiEncHandle));
  if (!check_qp_tbl(qp_tbl)) {
    goto exit;
  }

  vp5_set_log_level(ERR);

  ret = initEncParams(mHandle, codec_id, encode_info, qp_tbl);
  if (ret < 0)
    goto exit;

  mHandle->userData = mapCtl();
  if (mHandle->userData != MAP_FAILED){
     fprintf(stderr,"Encoder inited userdata is %p\n",mHandle->userData);
     mHandle->mEncParams.param_change_enable = 1;
  } else {
	  fprintf(stderr,"failed to open/allocate shared memory\n");
  }

  mHandle->am_enc_handle = AML_MultiEncInitialize(&(mHandle->mEncParams));
  if (mHandle->am_enc_handle == 0)
    goto exit;
  mHandle->mPrependSPSPPSToIDRFrames =
                encode_info.prepend_spspps_to_idr_frames;
  mHandle->mSpsPpsHeaderReceived = false;
  mHandle->mNumInputFrames = -1;  // 1st two buffers contain SPS and PPS
  if (encode_info.strict_rc_window > MAX_FRAME_WINDOW)
     encode_info.strict_rc_window = MAX_FRAME_WINDOW;
  mHandle->hist_win_len = encode_info.strict_rc_window;
  mHandle->hist_skip_thresh = encode_info.strict_rc_skip_thresh;

  return (vl_codec_handle_t)mHandle;

exit:
  if (mHandle != NULL)
    free(mHandle);
  return (vl_codec_handle_t)NULL;
}

void vl_multi_encoder_adjust_header(AMVEncStreamType streamType,char *header,int *dataLength)
{
    //adjust pps baseline profile
    bs_t bs;
    pps_t pps;
    uint32_t i = 0;
    int sps_nalu_size;
    int pps_nalu_size;
    int new_pps_size;
    int pps_start = -1;

    VLOG(INFO,"vl_multi_encoder_adjust_header,stream_type:%d",streamType);
    if (streamType != AMV_AVC)
        return;
    uint8_t *sps_nalu = (uint8_t *) malloc(sizeof(uint8_t) * (*dataLength));
    uint8_t *pps_nalu = (uint8_t *) malloc(sizeof(uint8_t) * (*dataLength));

    if (sps_nalu == NULL || pps_nalu == NULL) {
        VLOG(ERR,"malloc for sps or pps failed");
        return;
    }

    for (i=0;i<*dataLength-5;i++) {
        if ((uint8_t)header[i+0] == 0 && (uint8_t)header[i+1] == 0 && (uint8_t)header[i+2] == 0 && (uint8_t)header[i+3] == 1 &&
            (((uint8_t)header[i+4]) & 0x1f) == 8) {
            pps_start = i;
            VLOG(INFO,"pps_start=%d\n", pps_start);
            break;
        }
    }

    memcpy(sps_nalu, header, pps_start);
    memcpy(pps_nalu, header + pps_start, *dataLength - pps_start);

    sps_nalu_size = pps_start;
    VLOG(INFO,"old sps_nalu_size=%d", sps_nalu_size);
    pps_nalu_size = *dataLength - pps_start;

    bs_init(&bs, pps_nalu + 5, pps_nalu_size - 5);
    read_pic_parameter_set_rbsp(&pps, &bs);
    read_rbsp_trailing_bits(&bs);

    memset(pps_nalu + 5, 0, *dataLength - 5);

    bs_init(&bs, pps_nalu + 5, *dataLength - 5);

    write_pic_parameter_set_rbsp(&pps, &bs);
    write_rbsp_trailing_bits(&bs);

    new_pps_size = bs.p - bs.start + 5;
    VLOG(INFO,"new_pps_size=%d", new_pps_size);
    memset(header, 0, new_pps_size + sps_nalu_size);

    memcpy(header, sps_nalu, sps_nalu_size);
    memcpy(header + sps_nalu_size, pps_nalu, new_pps_size);

    *dataLength = sps_nalu_size + pps_nalu_size;

    if (sps_nalu)
        free(sps_nalu);
    if (pps_nalu)
        free(pps_nalu);
}

encoding_metadata_t vl_multi_encoder_generate_header(vl_codec_handle_t codec_handle,
                                                              unsigned char *pHeader,
                                                              unsigned int *pLength)
{
  int ret;
  //uint32_t dataLength = 0;
  VPMultiEncHandle* handle = (VPMultiEncHandle *)codec_handle;
  encoding_metadata_t result;
  memset(&result, 0, sizeof(encoding_metadata_t));

  if (!handle->mSpsPpsHeaderReceived) {
    ret = AML_MultiEncHeader(handle->am_enc_handle, (unsigned char*)pHeader,
                             pLength);
    VLOG(DEBUG, "ret = %d", ret);
    if (ret == AMVENC_SUCCESS) {
      handle->mSPSPPSDataSize = 0;
      handle->mSPSPPSData = (char *)malloc(*pLength);
      if (handle->mSPSPPSData) {
        handle->mSPSPPSDataSize = *pLength;
        //*pLength = dataLength;
        VLOG(DEBUG, "begin memcpy");
        memcpy(handle->mSPSPPSData, (unsigned char*)pHeader,
               handle->mSPSPPSDataSize);
        VLOG(DEBUG, "get mSPSPPSData size= %d at line %d \n",
               handle->mSPSPPSDataSize, __LINE__);
      }
      handle->mNumInputFrames = 0;
      handle->mSpsPpsHeaderReceived = true;
      result.is_valid = true;
      result.err_cod = AMVENC_SUCCESS;
    } else {
      *pLength = 0;
      VLOG(ERR, "Encode SPS and PPS error, encoderStatus = %d. handle: %p\n",
           ret, (void*)handle);
      result.is_valid = false;
      if (ret == AMVENC_FAIL)
         result.err_cod = AMVENC_INVALID_PARAM;
      else
         result.err_cod = ret;
      return result;
    }
  }
  return result;
}



encoding_metadata_t vl_multi_encoder_encode(vl_codec_handle_t codec_handle,
                                           vl_frame_type_t type,
                                           unsigned char* out,
                                           vl_buffer_info_t *in_buffer_info,
                                           vl_buffer_info_t *ret_buf)
{
  int ret;
  int i;
  uint32_t dataLength = 0;
  uint32_t nbr = 0;
  VPMultiEncHandle* handle = (VPMultiEncHandle *)codec_handle;

  encoding_metadata_t result;

  memset(&result, 0, sizeof(encoding_metadata_t));

#if ENCODE_TIME_OUTER
  gettimeofday(&start_test, NULL);
#endif

  if (in_buffer_info == NULL) {
    VLOG(ERR, "invalid input buffer_info\n");
    result.is_valid = false;
    result.err_cod = AMVENC_UNINITIALIZED;
    return result;
  }
  handle->bufType = (AMVEncBufferType)(in_buffer_info->buf_type);


  if (handle->bufType == DMA_BUFF) {
    if (in_buffer_info->buf_fmt == AMVENC_NV12 || in_buffer_info->buf_fmt == AMVENC_NV21 || in_buffer_info->buf_fmt == AMVENC_YUV420P) {
        if ((handle->mEncParams.width % 16 && in_buffer_info->buf_stride == 0) ||
            in_buffer_info->buf_stride % 16) {
           VLOG(ERR, "dma buffer stride must be multiple of 16!");
           result.is_valid = false;
           result.err_cod = AMVENC_ENCPARAM_MEM_FAIL;
           return result;
        }
    }
  }

  if (!handle->mSpsPpsHeaderReceived) {
    ret = AML_MultiEncHeader(handle->am_enc_handle, (unsigned char*)out,
                            (unsigned int *)&dataLength);
    VLOG(DEBUG, "ret = %d", ret);
    if (ret == AMVENC_SUCCESS) {
      vl_multi_encoder_adjust_header(handle->mEncParams.stream_type, out,&dataLength);
      handle->mSPSPPSDataSize = 0;
      handle->mSPSPPSData = (char *)malloc(dataLength);
      if (handle->mSPSPPSData) {
        handle->mSPSPPSDataSize = dataLength;
        VLOG(DEBUG, "begin memcpy");
        memcpy(handle->mSPSPPSData, (unsigned char*)out,
               handle->mSPSPPSDataSize);
        VLOG(DEBUG, "get mSPSPPSData size= %d at line %d \n",
             handle->mSPSPPSDataSize, __LINE__);
      }
      handle->mNumInputFrames = 0;
      handle->mSpsPpsHeaderReceived = true;
    } else {
      VLOG(ERR, "Encode SPS and PPS error, encoderStatus = %d. handle: %p\n",
           ret, (void*)handle);
      result.is_valid = false;
      if (ret == AMVENC_FAIL)
         result.err_cod = AMVENC_INVALID_PARAM;
      else
          result.err_cod = ret;
      return result;
    }
  }


  if (type == FRAME_TYPE_I || type == FRAME_TYPE_IDR)
    handle->mKeyFrameRequested = true;

  if (0 != force_key(handle->userData, &(nbr))){
      fprintf(stderr,"Encoder force keyframe \n");
      fprintf(stderr,"Encoder bitrate was  %u \n",handle->mEncParams.bitrate);
      handle->mKeyFrameRequested = true;
      fprintf(stderr,"Encoder change bitrate to  %u \n",nbr);
      if (nbr != handle->mEncParams.bitrate){
	ret = vl_video_encoder_change_bitrate(codec_handle,nbr);
        fprintf(stderr,"Encoder change bitrate result  %d \n",ret);
      }
      fprintf(stderr,"Encoder bitrate now %u \n",handle->mEncParams.bitrate);
  }


  if (handle->mNumInputFrames >= 0) {
    AMVMultiEncFrameIO videoInput, videoRet;
    memset(&videoInput, 0, sizeof(videoInput));
    memset(&videoRet, 0, sizeof(videoRet));
    videoInput.height = handle->mEncParams.height;
    if (!in_buffer_info-> buf_stride)
        videoInput.pitch = handle->mEncParams.width; //((handle->mEncParams.width + 15) >> 4) << 4;
    else
        videoInput.pitch = in_buffer_info-> buf_stride;
    /* TODO*/
    videoInput.bitrate = handle->mEncParams.bitrate;
    videoInput.frame_rate = handle->mEncParams.frame_rate / 1000.0f;
    videoInput.coding_timestamp =
        (unsigned long long)handle->mNumInputFrames * 1000 / videoInput.frame_rate;  // in us

    VLOG(INFO, "videoInput.frame_rate %f videoInput.coding_timestamp %lld, mNumInputFrames %d",
        videoInput.frame_rate * 1000, videoInput.coding_timestamp, handle->mNumInputFrames);
    result.timestamp_us = videoInput.coding_timestamp;
    handle->shared_fd[0] = -1;
    handle->shared_fd[1] = -1;
    handle->shared_fd[2] = -1;

    if (handle->hist_win_len) { // have  strict bitrate control
        strict_rc_check(handle);
    }
    videoInput.fmt = handle->fmt;

    VLOG(INFO, "in_buffer_info->buf_fmt=%d", in_buffer_info->buf_fmt);

    if (in_buffer_info->buf_fmt == IMG_FMT_NV12) {
      VLOG(INFO, "img_format is IMG_FMT_NV12 \n");
      videoInput.fmt = AMVENC_NV12;
    } else if (in_buffer_info->buf_fmt == IMG_FMT_NV21) {
      VLOG(INFO, "img_format is IMG_FMT_NV21 \n");
      videoInput.fmt = AMVENC_NV21;
    } else if (in_buffer_info->buf_fmt == IMG_FMT_YUV420P) {
      VLOG(INFO, "img_format is IMG_FMT_YUV420P \n");
      videoInput.fmt = AMVENC_YUV420P;
    } else if (in_buffer_info->buf_fmt == IMG_FMT_RGB888) {
      VLOG(INFO, "img_format is IMG_FMT_RGB888 \n");
      videoInput.fmt = AMVENC_RGB888;
    } else if (in_buffer_info->buf_fmt == IMG_FMT_RGBA8888) {
      VLOG(INFO, "img_format is IMG_FMT_RGBA8888 \n");
      videoInput.fmt = AMVENC_RGBA8888;
    }

    if (handle->bufType == CANVAS_BUFFER) {
      VLOG(INFO, "setting canvas to videoInput");
      videoInput.canvas = in_buffer_info->buf_info.canvas;
      videoInput.fmt = AMVENC_NV21;
    }
    else if (handle->bufType == DMA_BUFF) {
      vl_dma_info_t *dma_info;
      dma_info = &(in_buffer_info->buf_info.dma_info);

    if (videoInput.fmt == AMVENC_NV21 || videoInput.fmt == AMVENC_NV12) {
      if (dma_info->num_planes != 1 && dma_info->num_planes != 2) {
          VLOG(ERR, "invalid num_planes %d\n", dma_info->num_planes);
          result.is_valid = false;
          result.err_cod = AMVENC_ENCPARAM_MEM_FAIL;

          return result;
        }
      } else if (videoInput.fmt == AMVENC_YUV420P) {
        if (dma_info->num_planes != 1 && dma_info->num_planes != 3) {
          VLOG(ERR, "YUV420P invalid num_planes %d\n", dma_info->num_planes);
          result.is_valid = false;
          result.err_cod = AMVENC_ENCPARAM_MEM_FAIL;

          return result;
        }
      } else if (videoInput.fmt == AMVENC_RGBA8888) {
        VLOG(INFO, "process rgba dma input");

        if (dma_info->num_planes != 1) {
          VLOG(ERR, "RGBA invalid num_planes %d\n", dma_info->num_planes);
          result.is_valid = false;
          result.err_cod = AMVENC_ENCPARAM_MEM_FAIL;
          return result;
        }
      } else {
        VLOG(ERR, "unsupported color format");
        result.is_valid = false;
        result.err_cod = AMVENC_ENCPARAM_MEM_FAIL;
        return result;
      }

      handle->mNumPlanes = dma_info->num_planes;

      for (i = 0; i < dma_info->num_planes; i++) {
        if (dma_info->shared_fd[i] < 0) {
          VLOG(ERR, "invalid dma_fd %d\n", dma_info->shared_fd[i]);
          result.is_valid = false;
          result.err_cod = AMVENC_ENCPARAM_MEM_FAIL;
          return result;
        }

        handle->shared_fd[i] = dma_info->shared_fd[i];
        VLOG(INFO, "**shared_fd %d\n", handle->shared_fd[i]);
        videoInput.shared_fd[i] = dma_info->shared_fd[i];
      }

        videoInput.num_planes = handle->mNumPlanes;
#if defined(__ANDROID__)
        ALOGD("videoInput.num_planes=%d", videoInput.num_planes);
#endif
    } else {
      unsigned long* in = in_buffer_info->buf_info.in_ptr;
      videoInput.YCbCr[0] = (unsigned long)in[0];
      videoInput.YCbCr[1] = (unsigned long)(videoInput.YCbCr[0] +
      videoInput.height * videoInput.pitch);

      if (videoInput.fmt == AMVENC_NV21 || videoInput.fmt == AMVENC_NV12) {
        videoInput.YCbCr[2] = 0;
      } else if (videoInput.fmt == AMVENC_YUV420P) {
        videoInput.YCbCr[2] = (unsigned long)(videoInput.YCbCr[1] + videoInput.height * videoInput.pitch / 4);
      }
    }

    //videoInput.fmt = handle->fmt;
    //videoInput.canvas = 0x00030201;
    videoInput.type = handle->bufType;
    videoInput.disp_order = handle->mNumInputFrames;
    videoInput.op_flag = 0;

    if (handle->mKeyFrameRequested == true) {
        if (handle->mEncParams.GopPreset == GOP_IP_SVC1 && (handle->mNumInputFrames % 2) ||
            handle->mEncParams.GopPreset == GOP_IP_SVC2 && (handle->mNumInputFrames % 3) ||
            handle->mEncParams.GopPreset == GOP_IP_SVC3 && (handle->mNumInputFrames % 4) ||
            handle->mEncParams.GopPreset == GOP_IP_SVC4 && (handle->mNumInputFrames % 5) ||
            handle->mEncParams.GopPreset == GOP_IP_SVC5 && (handle->mNumInputFrames % 4)) {
          VLOG(INFO, "Force encode key frame at %d frame,but this frame can't insert,skip!!",handle->mNumInputFrames);
        }
        else {
          videoInput.op_flag |= AMVEncFrameIO_FORCE_IDR_FLAG;
          handle->mKeyFrameRequested = false;
          VLOG(INFO, "Force encode key frame at %d frame",handle->mNumInputFrames);
        }
    }
    if (handle->mSkipFrameRequested == true) {
      videoInput.op_flag |= AMVEncFrameIO_FORCE_SKIP_FLAG;
      handle->mSkipFrameRequested = false;
      VLOG(INFO, "Force encode a skip frame at %d frame",
           handle->mNumInputFrames);
    }
    if (handle->mEncParams.longterm_ref_enable) {
      if (handle->mLongTermRefRequestFlags) {
        videoInput.op_flag |= ((handle->mLongTermRefRequestFlags & 0x3)<<2);
        VLOG(INFO, "Use LTR flags 0x%x at %d frame",
             handle->mLongTermRefRequestFlags,
             handle->mNumInputFrames);
        handle->mLongTermRefRequestFlags = 0;
      }
    }
    // if (handle->idrPeriod == 0) {
    // videoInput.op_flag |= AMVEncFrameIO_FORCE_IDR_FLAG;
    //}
    ret = AML_MultiEncSetInput(handle->am_enc_handle, &videoInput);
    ++(handle->mNumInputFrames);

    VLOG(NONE, "AML_MultiEncSetInput ret %d\n", ret);
    if (ret < AMVENC_SUCCESS) {
      VLOG(ERR, "encoderStatus = %d, handle: %p", ret,(void*)handle);
      result.is_valid = false;
      if (ret == AMVENC_FAIL)
         result.err_cod = AMVENC_INVALID_PARAM;
      else
          result.err_cod = ret;
      return result;
    }

    //t1 = GetNowUs();
    ret = AML_MultiEncNAL(handle->am_enc_handle, out, (unsigned int*)&dataLength, &videoRet);
    //t2 = GetNowUs();

    //VLOG(ERR, "AML_MultiEncNAL cost: %3.1f ms", (t2-t1)/1000.0);
    VLOG(NONE, "AML_MultiEnc ret %d,  dataLength %d\n",
         ret,dataLength);
    if (ret == AMVENC_PICTURE_READY) {
      if ((videoRet.encoded_frame_type == 0) ||  handle->mNumInputFrames == 1){
        if ((handle->mPrependSPSPPSToIDRFrames ||
             handle->mNumInputFrames == 1) &&
            (handle->mSPSPPSData)) {
          memmove(out + handle->mSPSPPSDataSize, out, dataLength);
          memcpy(out, handle->mSPSPPSData, handle->mSPSPPSDataSize);
          dataLength += handle->mSPSPPSDataSize;
          VLOG(DEBUG, "copy mSPSPPSData to buffer size= %d\n",handle->mSPSPPSDataSize);
        }
        result.is_key_frame = true;
      }
      if (videoRet.encoded_frame_type == 5)
        result.extra.frame_type = FRAME_TYPE_IDR;
      else if (videoRet.encoded_frame_type == 0)
        result.extra.frame_type = FRAME_TYPE_I;
      else if (videoRet.encoded_frame_type == 1) {
#if GET_DROPPABLE_P
        uint8 nal_ref_idc = 0;
        if (handle->mEncParams.stream_type == AMV_AVC) {
            nal_ref_idc = (out[4] >> 5) & 3;
            VLOG(DEBUG, "nal_ref_idc  %d\n", nal_ref_idc);
            if (nal_ref_idc < 2)
                result.extra.frame_type = FRAME_TYPE_DROPPABLE_P;
            else
                result.extra.frame_type = FRAME_TYPE_P;
        }
        else
#endif
        result.extra.frame_type = FRAME_TYPE_P;
      }
      else if (videoRet.encoded_frame_type == 2)
        result.extra.frame_type = FRAME_TYPE_B;
      result.extra.average_qp_value = videoRet.enc_average_qp;
      result.extra.intra_blocks = videoRet.enc_intra_blocks;
      result.extra.merged_blocks = videoRet.enc_merged_blocks;
      result.extra.skipped_blocks = videoRet.enc_skipped_blocks;
    } else if ((ret == AMVENC_SKIPPED_PICTURE) || (ret == AMVENC_TIMEOUT)) {
      dataLength = 0;
      if (ret == AMVENC_TIMEOUT) {
        handle->mKeyFrameRequested = true;
        ret = AMVENC_SKIPPED_PICTURE;
      }
      VLOG(INFO, "ret = %d, handle: %p", ret,(void*)handle);
    } else if (ret != AMVENC_SUCCESS) {
      dataLength = 0;
    }

    if (ret < AMVENC_SUCCESS) {
      VLOG(ERR, "encoderStatus = %d, handle: %p", ret,(void*)handle);
      result.is_valid = false;
      if (ret == AMVENC_FAIL)
         result.err_cod = AMVENC_INVALID_PARAM;
      else
          result.err_cod = ret;
      return result;
    }

    if (handle->hist_win_len) { // one frame encoded fill hist
        strict_rc_filled(handle, dataLength);
    }
    /* check the returned frame if it has */
    if (videoRet.type == DMA_BUFF) { //have buffer return?
        if (videoRet.num_planes) {
            ret_buf ->buf_type = (vl_buffer_type_t)(videoRet.type);
            ret_buf ->buf_info.dma_info.num_planes = videoRet.num_planes;
            for (i = 0; i < videoRet.num_planes; i++)
                ret_buf ->buf_info.dma_info.shared_fd[i] = videoRet.shared_fd[i];
        }
    } else if (videoRet.YCbCr[0] != 0) {
        ret_buf ->buf_type = (vl_buffer_type_t)(videoRet.type);
        ret_buf ->buf_info.in_ptr[0] = videoRet.YCbCr[0];
    }
  }
  result.is_valid = true;
  result.encoded_data_length_in_bytes = dataLength;
#if ENCODE_TIME_OUTER
    gettimeofday(&end_test, NULL);
    encode_time_per_frame = end_test.tv_sec - start_test.tv_sec;
    encode_time_per_frame = encode_time_per_frame * 1000000 + end_test.tv_usec - start_test.tv_usec;
    total_encode_frames++;
    total_encode_time += encode_time_per_frame;
    printf("%p#Encode time : %lu us, frame_number : %llu\n",
      handle->am_enc_handle, encode_time_per_frame, total_encode_frames);
#endif

  //  VLOG(INFO, "frame extra info type %d, av_qp %d,intra %d,merged %d,skip %d\n",
  //        result.extra.frame_type, result.extra.average_qp_value,
  //        result.extra.intra_blocks, result.extra.merged_blocks,
  //        result.extra.skipped_blocks);

  return result;
}

int vl_video_encoder_update_qp_hint(vl_codec_handle_t codec_handle,
                            unsigned char *pq_hint_table,
                            int size)
{
    int ret;
    VPMultiEncHandle* handle = (VPMultiEncHandle *)codec_handle;

    if (handle->am_enc_handle == 0) //not init the encoder yet
        return -1;
    if (handle->mEncParams.roi_enable == 0) //no eoi enabled
        return -2;
    ret = AML_MultiEncUpdateRoi(handle->am_enc_handle, pq_hint_table, size);
    if (ret != AMVENC_SUCCESS)
        return -3;
    return 0;
}


int vl_video_encoder_change_bitrate(vl_codec_handle_t codec_handle,
                            int bitRate)
{
    int ret;
    VPMultiEncHandle* handle = (VPMultiEncHandle *)codec_handle;

    if (handle->am_enc_handle == 0) //not init the encoder yet
        return -1;
    if (handle->mEncParams.param_change_enable == 0) //no change enabled
        return -2;
    ret = AML_MultiEncChangeBitRate(handle->am_enc_handle, bitRate);
    if (ret != AMVENC_SUCCESS)
        return -3;
    hist_reset(handle); //bitrate target change reset the hist
    handle->mEncParams.bitrate = bitRate;
    return 0;
}

int vl_video_encoder_skip_frame(vl_codec_handle_t codec_handle)
{
    VPMultiEncHandle* handle = (VPMultiEncHandle *)codec_handle;

    if (handle->am_enc_handle == 0) //not init the encoder yet
        return -1;
    handle->mSkipFrameRequested = true;

    return 0;
}

int vl_video_encoder_change_qp(vl_codec_handle_t codec_handle,
                                int minQpI, int maxQpI, int maxDeltaQp,
                                int minQpP, int maxQpP,
                                int minQpB, int maxQpB)
{
    int ret;
    VPMultiEncHandle* handle = (VPMultiEncHandle *)codec_handle;

    if (handle->am_enc_handle == 0) //not init the encoder yet
        return -1;
    if (handle->mEncParams.param_change_enable == 0) //no change enabled
        return -2;

    if (minQpI < 0 || minQpI > 51 || maxQpI < 0 || maxQpI > 51 ||
        maxDeltaQp < 0 || maxDeltaQp > 51 || minQpP < 0 || minQpP > 51 ||
        maxQpP < 0 || maxQpP > 51 || minQpB < 0 || minQpB > 51 ||
        maxQpB < 0 || maxQpB > 51) {
        VLOG(ERR,"qp min or qp max out of range [0, 51]\n");
        return -3;
    }
    if (minQpI >= maxQpI || minQpP >= maxQpP || minQpB >= maxQpB) {
        VLOG(ERR,"qp min  or qp_max out of range [min, max]\n");
        return -4;
    }

    ret = AML_MultiEncChangeQPMinMax(handle->am_enc_handle,
             minQpI, maxQpI, maxDeltaQp, minQpP, maxQpP, minQpB, maxQpB);
    if (ret != AMVENC_SUCCESS)
        return -5;
    return 0;
}

int vl_video_encoder_change_gop(vl_codec_handle_t codec_handle,
                                int IntraQP, int IntraPeriod)
{
    int ret;
    VPMultiEncHandle* handle = (VPMultiEncHandle *)codec_handle;

    if (handle->am_enc_handle == 0) //not init the encoder yet
        return -1;
    if (handle->mEncParams.param_change_enable == 0) //no change enabled
        return -2;
    if (IntraQP < 0 || IntraQP > 51 ) {
           VLOG(ERR, "QP value out of range [0, 51]\n");
        return -3;
    }
    if (IntraPeriod <= 1 ) {
           VLOG(ERR, "Invalid Intra Period %d\n", IntraPeriod);
        return -4;
    }
    ret = AML_MultiEncChangeIntraPeriod(handle->am_enc_handle,
              IntraQP, IntraPeriod);
    if (ret != AMVENC_SUCCESS)
        return -5;
    return 0;
}

int vl_video_encoder_change_multi_slice(vl_codec_handle_t codec_handle,
                                  int multi_slice_mode, int multi_slice_para)
{
    int ret;
    VPMultiEncHandle* handle = (VPMultiEncHandle *)codec_handle;

    if (handle->am_enc_handle == 0) //not init the encoder yet
        return -1;
    if (handle->mEncParams.param_change_enable == 0) //no change enabled
        return -2;
    if (multi_slice_mode < 0 || multi_slice_mode > 2 ) {
           VLOG(ERR, "multi slice mode value out of range [0, 2]\n");
        return -3;
    }
    if (multi_slice_mode == 2 && handle->mEncParams.stream_type == AMV_AVC) {
           VLOG(ERR, "multi slice mode 2 no support for H.264\n");
        return -4;
    }
    if (multi_slice_mode && multi_slice_para == 0) {
           VLOG(ERR, "Invalid multi-slice para %d\n", multi_slice_para);
        return -5;
    }
    ret = AML_MultiEncChangeMutiSlice(handle->am_enc_handle,
              multi_slice_mode, multi_slice_para);
    if (ret != AMVENC_SUCCESS)
        return -6;
    return 0;
}

int vl_video_encoder_longterm_ref(vl_codec_handle_t codec_handle,
                                  int LongtermRefFlags)
{
    VPMultiEncHandle* handle = (VPMultiEncHandle *)codec_handle;

    if (handle->am_enc_handle == 0) //not init the encoder yet
        return -1;
    if (handle->mEncParams.longterm_ref_enable == 0) //no enabled
        return -2;
    if (LongtermRefFlags > 3 || LongtermRefFlags < 0) {
           VLOG(ERR, "Longterm ref flag invalid  0x%x\n", LongtermRefFlags);
        return -3;
    }
    handle->mLongTermRefRequestFlags = LongtermRefFlags;
    return 0;
}

int vl_video_encoder_change_strict_rc(vl_codec_handle_t codec_handle,
                                  int bitrate_window, int skip_threshold)
{
    int ret;
    VPMultiEncHandle* handle = (VPMultiEncHandle *)codec_handle;

    if (handle->am_enc_handle == 0) //not init the encoder yet
        return -1;
    if (bitrate_window > MAX_FRAME_WINDOW)
        bitrate_window = MAX_FRAME_WINDOW;
    else if (bitrate_window < 0)
        bitrate_window = 0;

    handle->hist_win_len = bitrate_window;
    handle->hist_skip_thresh = skip_threshold;

    if (bitrate_window == 0)
    {// disable
        hist_reset(handle);
    }
    return 0;
}

int vl_multi_encoder_destroy(vl_codec_handle_t codec_handle) {
    VPMultiEncHandle *handle = (VPMultiEncHandle *)codec_handle;
    AML_MultiEncRelease(handle->am_enc_handle);
#if ENCODE_TIME_OUTER
    printf("%p#Total encode time : %llu, Total encode frames : %llu\n",
        handle->am_enc_handle, total_encode_time, total_encode_frames);
#endif
    if (handle->mSPSPPSData)
        free(handle->mSPSPPSData);
    if (handle)
        free(handle);

    return 1;
}
