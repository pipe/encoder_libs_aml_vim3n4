
#include <stdio.h>
#include <stdlib.h>

#ifdef MAKEANDROID
#include <utils/Log.h>
#define LOGCAT
#endif

#include "include/vp_multi_codec_1_0.h"
#include "include/AML_MultiEncoder.h"
#include "include/enc_define.h"
#include "vdi_osal.h"

const char version[] = "Amlogic libvp_multi_codec version 1.0";

const char* vl_get_version() {
  return version;
}

typedef struct vp_multi_s {
  AMVEncInitParams mEncParams;

  AMVEncBufferType bufType;
  bool mPrependSPSPPSToIDRFrames;
  bool mSpsPpsHeaderReceived;
  bool mKeyFrameRequested;
  int mNumInputFrames;
  AMVEncFrameFmt fmt;
  int mSPSPPSDataSize;
  char *mSPSPPSData;
  amv_enc_handle_t am_enc_handle;
  int shared_fd[3];
  uint32 mNumPlanes;
} VPMultiEncHandle;


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
    handle->mEncParams.MBsIntraRefresh = 0;
    handle->mEncParams.MBsIntraOverlap = 0;
    handle->mEncParams.encode_once = 1;

  if (encode_info.img_format == IMG_FMT_NV12) {
    VLOG(INFO, "[%s] img_format is IMG_FMT_NV12 \n", __func__);
    handle->fmt = AMVENC_NV12;
  } else if (encode_info.img_format == IMG_FMT_NV21) {
    VLOG(INFO, "[%s] img_format is IMG_FMT_NV21 \n", __func__);
    handle->fmt = AMVENC_NV21;
  } else if (encode_info.img_format == IMG_FMT_YV12) {
    VLOG(INFO, "[%s] img_format is IMG_FMT_YUV420 \n", __func__);
    handle->fmt = AMVENC_YUV420;
  } else {
    VLOG(ERR, "[%s] img_format %d not supprot\n", __func__,
         encode_info.img_format);
    return AMVENC_FAIL;
  }
     handle->mEncParams.fmt = handle->fmt;
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
        handle->mEncParams.profile = HEVC_MAIN;
        handle->mEncParams.level = HEVC_LEVEL_NONE; // firmware determines a level.
        handle->mEncParams.hevc_tier = HEVC_TIER_MAIN;
        handle->mEncParams.initQP = 30;
        handle->mEncParams.BitrateScale = ENC_SETTING_OFF;
        handle->mEncParams.refresh_type = HEVC_CRA;
    } else if(codec_id == CODEC_ID_H264) {
        handle->mEncParams.stream_type = AMV_AVC;
        handle->mEncParams.profile = AVC_MAIN;
        handle->mEncParams.level = AVC_LEVEL4;
        handle->mEncParams.initQP = 30;
        handle->mEncParams.BitrateScale = ENC_SETTING_OFF;
    }
    return AMVENC_SUCCESS;
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
  ret = initEncParams(mHandle, codec_id, encode_info, qp_tbl);
  if (ret < 0)
    goto exit;

  mHandle->am_enc_handle = AML_MultiEncInitialize(&(mHandle->mEncParams));
  if (mHandle->am_enc_handle == NULL)
    goto exit;
  mHandle->mPrependSPSPPSToIDRFrames =
                encode_info.prepend_spspps_to_idr_frames;
  mHandle->mSpsPpsHeaderReceived = false;
  mHandle->mNumInputFrames = -1;  // 1st two buffers contain SPS and PPS

  return (vl_codec_handle_t)mHandle;

exit:
  if (mHandle != NULL)
    free(mHandle);
  return (vl_codec_handle_t)NULL;
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
  VPMultiEncHandle* handle = (VPMultiEncHandle *)codec_handle;

  encoding_metadata_t result;
  result.timestamp_us = 0;
  result.is_key_frame = false;
  result.encoded_data_length_in_bytes = 0;
  result.is_valid = false;

  if (in_buffer_info == NULL) {
    VLOG(ERR, "invalid input buffer_info\n");
    result.is_valid = false;
    return result;
  }
  handle->bufType = (AMVEncBufferType)(in_buffer_info->buf_type);

  if (handle->bufType == DMA_BUFF) {
    if (handle->mEncParams.width % 16) {
       VLOG(ERR, "dma buffer width must be multiple of 16!");
       result.is_valid = false;
       return result;
    }
  }
  if (!handle->mSpsPpsHeaderReceived) {
    ret = AML_MultiEncHeader(handle->am_enc_handle, (unsigned char*)out,
                            (unsigned int *)&dataLength);
    VLOG(DEBUG, "ret = %d", ret);
    if (ret == AMVENC_SUCCESS) {
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
      return result;
    }
  }

  if (handle->mNumInputFrames >= 0) {
    AMVMultiEncFrameIO videoInput, videoRet;
    memset(&videoInput, 0, sizeof(videoInput));
    memset(&videoInput, 0, sizeof(videoRet));
    videoInput.height = handle->mEncParams.height;
    videoInput.pitch = handle->mEncParams.width; //((handle->mEncParams.width + 15) >> 4) << 4;
    /* TODO*/
    videoInput.bitrate = handle->mEncParams.bitrate;
    videoInput.frame_rate = handle->mEncParams.frame_rate / 1000.0f;
    videoInput.coding_timestamp =
        (unsigned long long)handle->mNumInputFrames * 1000 / videoInput.frame_rate;  // in us

    VLOG(DEBUG, "videoInput.frame_rate %f videoInput.coding_timestamp %d, mNumInputFrames %d",
        videoInput.frame_rate, videoInput.coding_timestamp, handle->mNumInputFrames);
    result.timestamp_us = videoInput.coding_timestamp;
    handle->shared_fd[0] = -1;
    handle->shared_fd[1] = -1;
    handle->shared_fd[2] = -1;

    if (handle->bufType == DMA_BUFF) {
      vl_dma_info_t *dma_info;
      dma_info = &(in_buffer_info->buf_info.dma_info);
      if (handle->fmt == AMVENC_NV21 || handle->fmt == AMVENC_NV12) {
        if (dma_info->num_planes != 1
            && dma_info->num_planes != 2) {
          VLOG(ERR, "invalid num_planes %d\n", dma_info->num_planes);
          result.is_valid = false;
          return result;
        }
      } else if (handle->fmt == AMVENC_YUV420) {
        if (dma_info->num_planes != 1
            && dma_info->num_planes != 3) {
          VLOG(ERR, "YV12 invalid num_planes %d\n", dma_info->num_planes);
          result.is_valid = false;
          return result;
        }
      }
      handle->mNumPlanes = dma_info->num_planes;
      for (i = 0; i < dma_info->num_planes; i++) {
        if (dma_info->shared_fd[i] < 0) {
          VLOG(ERR, "invalid dma_fd %d\n", dma_info->shared_fd[i]);
          result.is_valid = false;
          return result;
        }
        handle->shared_fd[i] = dma_info->shared_fd[i];
        VLOG(NONE, "shared_fd %d\n", handle->shared_fd[i]);
        videoInput.shared_fd[i] = dma_info->shared_fd[i];
      }
      videoInput.num_planes = handle->mNumPlanes;
    } else {
      unsigned long* in = in_buffer_info->buf_info.in_ptr;
      videoInput.YCbCr[0] = (unsigned long)in[0];
      videoInput.YCbCr[1] = (unsigned long)(videoInput.YCbCr[0] +
      videoInput.height * videoInput.pitch);
      if (handle->fmt == AMVENC_NV21 || handle->fmt == AMVENC_NV12) {
        videoInput.YCbCr[2] = 0;
      } else if (handle->fmt == AMVENC_YUV420) {
        videoInput.YCbCr[2] = (unsigned long)(videoInput.YCbCr[1] + videoInput.height * videoInput.pitch / 4);
      }
    }
    videoInput.fmt = handle->fmt;
    videoInput.canvas = 0xffffffff;
    videoInput.type = handle->bufType;
    videoInput.disp_order = handle->mNumInputFrames;
    videoInput.op_flag = 0;

    if (handle->mKeyFrameRequested == true) {
      videoInput.op_flag = AMVEncFrameIO_FORCE_IDR_FLAG;
      handle->mKeyFrameRequested = false;
      VLOG(INFO, "Force encode a IDR frame at %d frame",
           handle->mNumInputFrames);
    }
    // if (handle->idrPeriod == 0) {
    // videoInput.op_flag |= AMVEncFrameIO_FORCE_IDR_FLAG;
    //}
    ret = AML_MultiEncSetInput(handle->am_enc_handle, &videoInput);
    ++(handle->mNumInputFrames);

    VLOG(NONE, "AML_MultiEncSetInput ret %d\n", ret);
    if (ret < AMVENC_SUCCESS) {
      VLOG(ERR, "encoderStatus = %d at line %d, handle: %p", ret, __LINE__,
           (void*)handle);
      result.is_valid = false;
      return result;
    }

    ret = AML_MultiEncNAL(handle->am_enc_handle, out,
                                (unsigned int*)&dataLength,&videoRet);
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
          VLOG(DEBUG, "copy mSPSPPSData to buffer size= %d at line %d \n",
               handle->mSPSPPSDataSize, __LINE__);
        }
        result.is_key_frame = true;
      }
    } else if ((ret == AMVENC_SKIPPED_PICTURE) || (ret == AMVENC_TIMEOUT)) {
      dataLength = 0;
      if (ret == AMVENC_TIMEOUT) {
        handle->mKeyFrameRequested = true;
        ret = AMVENC_SKIPPED_PICTURE;
      }
      VLOG(INFO, "ret = %d at line %d, handle: %p", ret, __LINE__,
           (void*)handle);
    } else if (ret != AMVENC_SUCCESS) {
      dataLength = 0;
    }

    if (ret < AMVENC_SUCCESS) {
      VLOG(ERR, "encoderStatus = %d at line %d, handle: %p", ret, __LINE__,
           (void*)handle);
      result.is_valid = false;
      return result;
    }
    /* check the returned frame if it has */
    if(videoRet.type == DMA_BUFF) { //have buffer return?
        if(videoRet.num_planes) {
            ret_buf ->buf_type = (vl_buffer_type_t)(videoRet.type);
            ret_buf ->buf_info.dma_info.num_planes = videoRet.num_planes;
            for (i = 0; i < videoRet.num_planes; i++)
                ret_buf ->buf_info.dma_info.shared_fd[i] = videoRet.shared_fd[i];
        }
    } else if (videoRet.YCbCr[0] != NULL) {
        ret_buf ->buf_type = (vl_buffer_type_t)(videoRet.type);
        ret_buf ->buf_info.in_ptr[0] = videoRet.YCbCr[0];
    }
  }
  result.is_valid = true;
  result.encoded_data_length_in_bytes = dataLength;
  return result;
}

int vl_multi_encoder_destroy(vl_codec_handle_t codec_handle) {
    VPMultiEncHandle *handle = (VPMultiEncHandle *)codec_handle;
    AML_MultiEncRelease(handle->am_enc_handle);
    if (handle->mSPSPPSData)
        free(handle->mSPSPPSData);
    if (handle)
        free(handle);
    return 1;
}
