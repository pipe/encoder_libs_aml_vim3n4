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

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdint.h>
#include "./test_dma_api.h"
#include "../amvenc_lib/include/vp_multi_codec_1_0.h"

#define WIDTH 3840
#define HEIGHT 3840    // consider rotated frame
#define FRAMERATE 60

#define INIT_RETRY 100


#define FORCE_PICTURE_TYPE	0x1
#define CHANGE_TARGET_RATE	0x2
#define CHANGE_MIN_MAX_QP	0x4
#define CHANGE_GOP_PERIOD	0x8
#define LONGTERM_REF_SET	0x10
#define CUST_SIZE_SMOOTH	0x20
#define CUST_APP_H264_PLUS	0x40
#define CHANGE_MULTI_SLICE	0x80
#define FORCE_PICTURE_SKIP	0x100
#define CHANGE_STRICT_RC	0x200

// GOP mode strings
static const char *Gop_string[] = {
	"Default mode IP only",
	"All I frame mode",
	"IP only mode",
	"IBBBP mode",
	"IP only 1-frame SVC mode",
	"IP only 2-frame SVC mode",
	"IP only 3-frame SVC mode",
	"IP only 4-frame SVC mode",
	"IP only customer mode",
};

typedef struct {
	int FrameNum; //change frame number
	int enable_option;
	//FORCE_PICTURE_TYPE
	int picType; //vl_frame_type_t
	//CHANGE_TARGET_RATE
	int bitRate;
	//CHANGE_MIN_MAX_QP
	int minQpI;
	int maxQpI;
	int maxDeltaQp;
	int minQpP;
	int maxQpP;
	int minQpB;
	int maxQpB;
	//CHANGE_GOP_PERIOD
	int intraQP;
	int GOPPeriod;
	//LONGTERM_REF_SET
	int LTRFlags; //bit 0: UseCurSrcAsLongtermPic; bit 1: Use LTR frame
	// CUST_SIZE_SMOOTH
	int smooth_level; //0 normal, 50(middle), 100 (high)
	// CUST_APP_H264_PLUS
	int newIDRIntervel; // shall be multiply of gop length
	int QPdelta_IDR;     // QP delta apply to IDRs
	int QPdelta_LTR;     // QP delta apply to LTR P frames
	// Change multi-slice
	int Slice_Mode;  //multi slice mode
	int Slice_Para;  //para
	// Change strict bitrate control
	int bitrate_window;  //bitrate cal window
	int pskip_threshold;  //threshold for pskip
} CfgChangeParam;

typedef struct {
	int CurFrameNum; //current frame number
	int CurGOPSize;  // current
	int CurGOPCounter;
	int PrevFrameQP; // previous frame QP value

	int minQpI_base;
	int maxQpI_base;
	int maxDeltaQp_base;
	int minQpP_base;
	int maxQpP_base;
	int minQpB_base;
	int maxQpB_base;
	int temp_QP_change; // QP enforcing flag

	int cust_size_smooth_enable; // flag indentify size_smooth is on going
	int smooth_qpdelta; // QP delata apply to I frames.
	// CUST_APP_H264_PLUS
	int cust_ltr_enable; //CUST H264 feature enabled
	int GopLTRIntervel;  // LTR P frame interval.
	int GopLTRCounter;   // counter to decide LTR P frames.
	int QPdelta_IDR;     // QP delta apply to IDRs
	int QPdelta_LTR;     // QP delta apply to LTR P frames
	int qp_idr;  // real applied qp_idr
	int qp_ltr;  //real applied qp_ltr
} PlayStatInfo;

static int ParseCfgUpdateFile(FILE *fp, CfgChangeParam *cfg_update);

int main(int argc, const char *argv[])
{
	int width, height, gop, framerate, bitrate, num, my_qp;
	int ret = 0;
	int outfd = -1;
	FILE *fp = NULL;
	FILE *fp_roi = NULL;
	FILE *fp_cfg = NULL;
	int datalen = 0;
	int retry_cnt = 0;
	int roi_enabled = 0;
	int ltf_enabled = 0;
	int cfg_upd_enabled = 0, has_cfg_update = 0;
	int cfg_option = 0, frame_num = 0, gop_pattern = 0, profile = 0;
	int src_buf_stride = 0, conver_stride = 0, intra_refresh_comb = 0;
	int bitstream_buf_size = 0;
	int multi_slice = 0, multi_slice_para = 0;
	int cust_qp_delta = 0, cust_qp_delta_en = 0;
	int arg_count = 0, arg_roi = 0, arg_upd = 0;
	int strict_rate_ctrl_en = 0, bitrate_window = 0, skip_threshold = 0;
	vl_img_format_t fmt = IMG_FMT_NONE;
	CfgChangeParam cfgChange;
	vl_frame_type_t enc_frame_type;
	PlayStatInfo playStat;

	unsigned char *vaddr = NULL;
	vl_codec_handle_t handle_enc = 0;
	vl_encode_info_t encode_info;
	vl_buffer_info_t ret_buf;
	vl_codec_id_t codec_id = CODEC_ID_H264;
	uint32_t frame_rotation;
	uint32_t frame_mirroring;
	encoding_metadata_t encoding_metadata;


	unsigned int framesize;
	unsigned ysize;
	unsigned usize;
	unsigned vsize;
	unsigned uvsize;

	int tmp_idr;
	int fd = -1;
	qp_param_t qp_tbl;
	int qp_mode = 0;
	int buf_type = 0;
	int num_planes = 1;
	struct usr_ctx_s ctx;

	if (argc < 13)
	{
		printf("Amlogic Encoder API \n");
		printf(" usage: aml_enc_test "
		       "[srcfile][outfile][width][height][gop][framerate][bitrate][num][fmt]["
		       "buf type][num_planes][codec_id] [cfg_opt] [roifile] [cfg_file] \n");
		printf("  options  \t:\n");
		printf("  srcfile  \t: yuv data url in your root fs\n");
		printf("  outfile  \t: stream url in your root fs\n");
		printf("  width    \t: width\n");
		printf("  height   \t: height\n");
		printf("  gop      \t: I frame refresh interval\n");
		printf("  framerate\t: framerate \n");
		printf("  bitrate  \t: bit rate \n");
		printf("  num      \t: encode frame count \n");
		printf("  fmt      \t: encode input fmt 1 : nv12, 2 : nv21, 3 : yuv420p(yv12/yu12)\n");
		printf("  buf_type \t: 0:vmalloc, 3:dma buffer\n");
		printf("  num_planes \t: used for dma buffer case. 2 : nv12/nv21, 3 : yuv420p(yv12/yu12)\n");
		printf("  codec_id \t: 4 : h.264, 5 : h.265\n");
		printf("  cfg_opt  \t: optional, flags to control the encode feature settings\n");
		printf("            \t\t  bit 0:roi control. 0: disabled (default) 1: enabled\n");
		printf("            \t\t  bit 1: update control. 0: disabled (default) 1: enabled\n");
		printf("            \t\t  bit 2 ~ bit 6 encode GOP patttern options:\n");
		printf("            \t\t\t 0 default(IP) 1:I only    2:IP_only    3: IBBBP\n");
		printf("            \t\t\t 4:IP_SVC1     5:IP_SVC2   6: IP_SVC3   7: IP_SVC4  8: CUSTP \n");
		printf("            \t\t  bit 7:long term refereces. 0: disabled (default) 1: enabled\n");
		printf("            \t\t  bit 8:cust source buffer stride . 0: disabled (default) 1: enabled\n");
		printf("            \t\t  bit 9: convert normal source file stride to cust source stride. 0: disabled (default) 1: enabled\n");
		printf("            \t\t  bit 10:enable intraRefresh. 0: disabled (default) 1: enabled\n");
		printf("            \t\t  bit 11 ~ bit 13: profile 0: auto (default) others as following\n");
		printf("            \t\t                   1: BASELINE(AVC) MAIN (HEVC);      2 MAIN (AVC) main10 (HEVC) \n");
		printf("            \t\t                   3: HIGH (AVC) STILLPICTURE (HEVC); 4 HIGH10 (AVC) \n");
		printf("            \t\t  bit 14 ~ bit 15: frame rotation(counter-clockwise, in degree): 0:none, 1:90, 2:180, 3:270\n");
		printf("            \t\t  bit 16 ~ bit 17: frame mirroring: 0:none, 1:vertical, 2:horizontal, 3:both V and H\n");
		printf("            \t\t  bit 18: enable customer bitstream buffer size\n");
		printf("            \t\t  bit 19 ~ bit 20: enable multi slice mode: 0, one slice(no para), 1 by MB(16x16)/CTU(64x64), 2 by CTU + size(byte)\n");
		printf("            \t\t  bit 21 cust_qp_delta_en \n");
		printf("            \t\t  bit 22 strict_rate_ctrl_en (skipP allowed to limit bitrate) default 0 \n");
		printf("  roifile  \t: optional, roi info url in root fs, no present if roi is disabled\n");
		printf("  cfg_file \t: optional, cfg update info url. no present if update is disabled\n");
		printf("  src_buf_stride \t: optional, source buffer stride\n");
		printf("  intraRefresh \t: optional, lower 4 bits for intra-refresh mode; others for refresh parameter, controlled by intraRefresh bit\n");
		printf("  bitStreamBuf \t: optional, encoded bitstream buffer size in MB\n");
		printf("  MultiSlice parameter \t: optional, multi slice parameter in MB/CTU or CTU and size (HIGH_16bits size + LOW_16bit CTU) combined\n");
		printf("  cust_qp_delta  \t: optional, cust_qp_delta apply to P frames value >0 lower; <0 increase the quality \n");
		printf("  strict rate control parmeter  \t: optional, High 16 bit bitrate window (frames max 120). low 16 bit bitrate threshold (percent)\n");
		return -1;
	} else
	{
		printf("%s\n", argv[1]);
		printf("%s\n", argv[2]);
	}

	width = atoi(argv[3]);
	if ((width < 1) || (width > WIDTH))
	{
		printf("invalid width \n");
		return -1;
	}

	height = atoi(argv[4]);
	if ((height < 1) || (height > HEIGHT))
	{
		printf("invalid height \n");
		return -1;
	}

	gop = atoi(argv[5]);
	framerate = atoi(argv[6]);
	bitrate = atoi(argv[7]);
	num = atoi(argv[8]);
	fmt = (vl_img_format_t) atoi(argv[9]);
	buf_type = atoi(argv[10]);
	num_planes = atoi(argv[11]);

	switch (atoi(argv[12]))
	{
	case 0:
		codec_id = CODEC_ID_NONE;
		break;
	case 1:
		codec_id = CODEC_ID_H261;
		break;
	case 2:
		codec_id = CODEC_ID_H261;
		break;
	case 3:
		codec_id = CODEC_ID_H263;
		break;
	case 4:
		codec_id = CODEC_ID_H264;
		break;
	case 5:
		codec_id = CODEC_ID_H265;
		break;
	default:
		break;
	}

	if (codec_id != CODEC_ID_H264 && codec_id != CODEC_ID_H265) {
		printf("unsupported codec id %d \n", codec_id);
		return -1;
	}

	if ((framerate < 0) || (framerate > FRAMERATE))
	{
		printf("invalid framerate %d \n", framerate);
		return -1;
	}
	if (bitrate <= 0)
	{
		printf("invalid bitrate \n");
		return -1;
	}
	if (num < 0)
	{
		printf("invalid num \n");
		return -1;
	}
	if (buf_type == 3 && (num_planes < 1 || num_planes > 3))
	{
		printf("invalid num_planes \n");
		return -1;
	}
	printf("src_url is\t: %s ;\n", argv[1]);
	printf("out_url is\t: %s ;\n", argv[2]);
	printf("width   is\t: %d ;\n", width);
	printf("height  is\t: %d ;\n", height);
	printf("gop     is\t: %d ;\n", gop);
	printf("frmrate is\t: %d ;\n", framerate);
	printf("bitrate is\t: %d ;\n", bitrate);
	printf("frm_num is\t: %d ;\n", num);
	printf("fmt     is\t: %d ;\n", fmt);
	printf("buf_type is\t: %d ;\n", buf_type);
	printf("num_planes is\t: %d ;\n", num_planes);
	printf("codec is\t: %d ;\n", num_planes);
	if (codec_id == CODEC_ID_H265)
		printf("codec is H265\n");
	else
		printf("codec is H264\n");
	if (argc > 13) {
		cfg_option = atoi(argv[13]);
		gop_pattern = (cfg_option >>2) & 0x1f;
		frame_rotation = (cfg_option >> 14) & 0x03;
		frame_mirroring = (cfg_option >> 16) & 0x03;
		printf("frame_rotation=%u, frame_mirroring=%u\n",
				frame_rotation, frame_mirroring);
		if (gop_pattern) {
			if (gop_pattern > 8) {
				printf("gop_pattern_is: %d invalid;\n",
					gop_pattern);
				return -1;
			}
			printf("gop_pattern_is \t: %d --> %s;\n",
				gop_pattern, Gop_string[gop_pattern]);
		}
		profile = (cfg_option >>11) & 0x7;
		if (profile) {
			if ((profile > 3 && codec_id == CODEC_ID_H265) ||
			    profile > 4) {
				printf("profile: %d invalid;\n",
					profile);
				return -1;
			}
			printf("profile is \t: %d\n", profile);
		}
		if (cfg_option & 0x80) {
			printf("longterm reference is enabled\n");
			ltf_enabled = 1;
		}
		multi_slice = (cfg_option >>19) & 0x3;
		if (multi_slice) {
			printf("multi slice is enabled mode: %d\n",
				multi_slice);
		}
		cust_qp_delta_en = (cfg_option >>21) & 0x1;
		if (cust_qp_delta_en) {
			printf("cust qp delta enabled: %d\n",
				cust_qp_delta_en);
		}
		strict_rate_ctrl_en = (cfg_option >>22) & 0x1;
		if (strict_rate_ctrl_en) {
			printf("strict bitrate control enabled: %d\n",
				strict_rate_ctrl_en);
		}
		if (argc > 14)
		{
			arg_count = 14;
			if ((cfg_option & 0x1) == 0x1 && argc > arg_count)
			{
				printf("roi_url is\t: %s ;\n",
					argv[arg_count]);
				roi_enabled = 1;
				arg_roi = arg_count;
				arg_count ++;
			}
			if ((cfg_option & 0x2) == 0x2 && argc > arg_count) {
				printf("cfg_upd_url is\t: %s ;\n",
					argv[arg_count]);
				cfg_upd_enabled = 1;
				arg_upd = arg_count;
				arg_count ++;
			}
			if ((cfg_option & 0x100) == 0x100 && argc > arg_count) {
				src_buf_stride  = atoi(argv[arg_count]);
				conver_stride = (cfg_option & 0x200) >> 9;
				printf("cust stride is\t: %d  convert: %d;\n",
					src_buf_stride, conver_stride);
				arg_count ++;
			}
			if ((cfg_option & 0x400) == 0x400 && argc > arg_count) {
				intra_refresh_comb = atoi(argv[arg_count]);
				printf("IntraRefresh mode %d, arg %d;\n",
					(intra_refresh_comb & 0xf),
					(intra_refresh_comb >> 4));
				arg_count ++;
			}
			if ((cfg_option & 0x40000) == 0x40000 && argc > arg_count) {
				bitstream_buf_size = atoi(argv[arg_count]);
				printf("bitstream_buf_size: %d \n",
					bitstream_buf_size);
				arg_count ++;
			}
			if (multi_slice && argc > arg_count) {
				multi_slice_para = atoi(argv[arg_count]);
				printf("Multi-slices para %d \n",
					multi_slice_para);
				arg_count ++;
			}
			if (cust_qp_delta_en && argc > arg_count) {
				cust_qp_delta = atoi(argv[arg_count]);
				printf("cust_qp_delta %d \n",
					cust_qp_delta);
				arg_count ++;
			}
			if (strict_rate_ctrl_en && argc > arg_count) {
				bitrate_window = atoi(argv[arg_count]);
				skip_threshold = bitrate_window  & 0xffff;
				bitrate_window >>=16;
				printf("bit-rate window %d frames, skip_threshold %d\n",
					   bitrate_window, skip_threshold);
				arg_count ++;
			}
			if (arg_count != argc) {
				printf("config no match conf %d argc %d\n",
					cfg_option, argc);
				return -1;
			}
		}
	}

	if (src_buf_stride && src_buf_stride < width) {
		printf("incorrect cust source stride %d width %d",
			src_buf_stride, width);
		return -1;
	}

	unsigned int outbuffer_len = 8*1024 * 1024 * sizeof(char);
	if (bitstream_buf_size > 8)
		outbuffer_len = bitstream_buf_size*1024 * 1024 * sizeof(char);
	unsigned char *outbuffer = (unsigned char *) malloc(outbuffer_len);
	unsigned char *inputBuffer = NULL;
	unsigned char *input[3] = { NULL };
	vl_buffer_info_t inbuf_info;
	vl_dma_info_t *dma_info = NULL;
	unsigned char *roi_buffer = NULL;
	unsigned int roi_size;

	if (src_buf_stride) {
		framesize = src_buf_stride * height * 3 / 2;
		ysize = src_buf_stride * height;
		usize = src_buf_stride * height / 4;
		vsize = src_buf_stride * height / 4;
		uvsize = src_buf_stride * height / 2;
	} else {
		framesize = width * height * 3 / 2;
		ysize = width * height;
		usize = width * height / 4;
		vsize = width * height / 4;
		uvsize = width * height / 2;
	}

	memset(&inbuf_info, 0, sizeof(vl_buffer_info_t));
	inbuf_info.buf_type = (vl_buffer_type_t) buf_type;
	inbuf_info.buf_stride = src_buf_stride;

	memset(&qp_tbl, 0, sizeof(qp_param_t));
	memset(&playStat, 0, sizeof(PlayStatInfo));

	qp_tbl.qp_min = 0;
	qp_tbl.qp_max = 51;
	qp_tbl.qp_I_base = 30;
	qp_tbl.qp_I_min = 0;
	qp_tbl.qp_I_max = 51;
	qp_tbl.qp_P_base = 30;
	qp_tbl.qp_P_min = 0;
	qp_tbl.qp_P_max = 51;

	memset(&encode_info, 0, sizeof(vl_encode_info_t));
	encode_info.width = width;
	encode_info.height = height;
	encode_info.bit_rate = bitrate;
	encode_info.frame_rate = framerate;
	encode_info.gop = gop;
	encode_info.img_format = fmt;
	encode_info.qp_mode = qp_mode;
	encode_info.profile = profile;
	if (frame_rotation == 0)
		encode_info.frame_rotation = 0;
	else if (frame_rotation == 1)
		encode_info.frame_rotation = 90;
	else if (frame_rotation == 2)
		encode_info.frame_rotation = 180;
	else if (frame_rotation == 3)
		encode_info.frame_rotation = 270;
	else {
		printf("frame rotation angle is wrong\n");
		encode_info.frame_rotation = 0;
	}
	if (frame_mirroring >= 0 && frame_mirroring <=3)
		encode_info.frame_mirroring = frame_mirroring;
	else
		encode_info.frame_mirroring = 0;
	if (intra_refresh_comb) {
		encode_info.intra_refresh_mode = intra_refresh_comb & 0xf;
		encode_info.intra_refresh_arg = intra_refresh_comb >> 4;
	}
	if (roi_enabled) {
		encode_info.enc_feature_opts |= ENABLE_ROI_FEATURE;
		roi_size = ((width+15)/16)*((height+15)/16);
		roi_buffer = (unsigned char *) malloc(roi_size + 1);
		if (roi_buffer == NULL) {
			printf("Can not allocat roi_buffer\n");
			goto exit;
		}
	}
	if (cfg_upd_enabled) {
		encode_info.enc_feature_opts |= ENABLE_PARA_UPDATE;
		memset(&cfgChange, 0, sizeof(CfgChangeParam));
	}
	if (ltf_enabled) {
		encode_info.enc_feature_opts |= ENABLE_LONG_TERM_REF;
	}
	if (gop_pattern) {
		encode_info.enc_feature_opts |= ((gop_pattern<<2) & 0x7c);
	}
	if (bitstream_buf_size) {
		encode_info.bitstream_buf_sz = bitstream_buf_size;
	}
	if (multi_slice) {
		encode_info.multi_slice_mode = multi_slice;
		encode_info.multi_slice_arg = multi_slice_para;
	}
	if (cust_qp_delta) {
		encode_info.cust_gop_qp_delta = cust_qp_delta;
	}
	if (strict_rate_ctrl_en) {
		encode_info.strict_rc_window = bitrate_window;
		encode_info.strict_rc_skip_thresh = skip_threshold;
	}
	if (inbuf_info.buf_type == DMA_TYPE)
	{
		dma_info = &(inbuf_info.buf_info.dma_info);
		dma_info->num_planes = num_planes;
		dma_info->shared_fd[0] = -1;	// dma buf fd
		dma_info->shared_fd[1] = -1;
		dma_info->shared_fd[2] = -1;
		ret = create_ctx(&ctx);
		if (ret < 0)
		{
			printf("gdc create fail ret=%d\n", ret);
			goto exit;
		}

		if (dma_info->num_planes == 3)
		{
			if (fmt != IMG_FMT_YUV420P)
			{
				printf("error fmt %d\n", fmt);
				goto exit;
			}
			ret = alloc_dma_buffer(&ctx, INPUT_BUFF_TYPE, ysize);
			if (ret < 0)
			{
				printf("alloc fail ret=%d, len=%u\n", ret,
				       ysize);
				goto exit;
			}
			vaddr = (unsigned char *) ctx.i_buff[0];
			if (!vaddr)
			{
				printf("mmap failed,Not enough memory\n");
				goto exit;
			}
			dma_info->shared_fd[0] = ret;
			input[0] = vaddr;

			ret = alloc_dma_buffer(&ctx, INPUT_BUFF_TYPE, usize);
			if (ret < 0)
			{
				printf("alloc fail ret=%d, len=%u\n", ret,
				       usize);
				goto exit;
			}
			vaddr = (unsigned char *) ctx.i_buff[1];
			if (!vaddr)
			{
				printf("mmap failed,Not enough memory\n");
				goto exit;
			}
			dma_info->shared_fd[1] = ret;
			input[1] = vaddr;

			ret = alloc_dma_buffer(&ctx, INPUT_BUFF_TYPE, vsize);
			if (ret < 0)
			{
				printf("alloc fail ret=%d, len=%u\n", ret,
				       vsize);
				goto exit;
			}
			vaddr = (unsigned char *) ctx.i_buff[2];
			if (!vaddr)
			{
				printf("mmap failed,Not enough memory\n");
				goto exit;
			}
			dma_info->shared_fd[2] = ret;
			input[2] = vaddr;
		} else if (dma_info->num_planes == 2)
		{
			ret = alloc_dma_buffer(&ctx, INPUT_BUFF_TYPE, ysize);
			if (ret < 0)
			{
				printf("alloc fail ret=%d, len=%u\n", ret,
				       ysize);
				goto exit;
			}
			vaddr = (unsigned char *) ctx.i_buff[0];
			if (!vaddr)
			{
				printf("mmap failed,Not enough memory\n");
				goto exit;
			}
			dma_info->shared_fd[0] = ret;
			input[0] = vaddr;
			if (fmt != IMG_FMT_NV12 && fmt != IMG_FMT_NV21)
			{
				printf("error fmt %d\n", fmt);
				goto exit;
			}
			ret = alloc_dma_buffer(&ctx, INPUT_BUFF_TYPE, uvsize);
			if (ret < 0)
			{
				printf("alloc fail ret=%d, len=%u\n", ret,
				       uvsize);
				goto exit;
			}
			vaddr = (unsigned char *) ctx.i_buff[1];
			if (!vaddr)
			{
				printf("mmap failed,Not enough memory\n");
				goto exit;
			}
			dma_info->shared_fd[1] = ret;
			input[1] = vaddr;
		} else if (dma_info->num_planes == 1)
		{
			ret =
			    alloc_dma_buffer(&ctx, INPUT_BUFF_TYPE, framesize);
			if (ret < 0)
			{
				printf("alloc fail ret=%d, len=%u\n", ret,
				       framesize);
				goto exit;
			}
			vaddr = (unsigned char *) ctx.i_buff[0];
			if (!vaddr)
			{
				printf("mmap failed,Not enough memory\n");
				goto exit;
			}
			dma_info->shared_fd[0] = ret;
			input[0] = vaddr;
		}

		printf("in[0] %d, in[1] %d, in[2] %d\n", dma_info->shared_fd[0],
		       dma_info->shared_fd[1], dma_info->shared_fd[2]);
		printf("input[0] %p, input[1] %p,input[2] %p\n", input[0],
		       input[1], input[2]);
	} else {
		inputBuffer = (unsigned char *) malloc(framesize);
		inbuf_info.buf_info.in_ptr[0] = (ulong) (inputBuffer);
		inbuf_info.buf_info.in_ptr[1] =
		    (ulong) (inputBuffer + ysize);
		inbuf_info.buf_info.in_ptr[2] =
		    (ulong) (inputBuffer + ysize + usize);
	}

	fp = fopen((argv[1]), "rb");
	if (fp == NULL)
	{
		printf("open src file error!\n");
		goto exit;
	}

	if (roi_enabled) {
		fp_roi = fopen((argv[arg_roi]), "rb");
		if (fp_roi == NULL)
		{
			printf("open roi file error!\n");
			goto exit;
		}
	}
	if (cfg_upd_enabled) {
		fp_cfg = fopen((argv[arg_upd]), "r");
		if (fp_cfg == NULL)
		{
			printf("open cfg file error!\n");
			goto exit;
		}
		has_cfg_update = ParseCfgUpdateFile(fp_cfg, &cfgChange);
	}
	outfd = open((argv[2]), O_CREAT | O_RDWR | O_TRUNC, 0644);
	if (outfd < 0)
	{
		printf("open dest file error!\n");
		goto exit;
	}

	// init playState
	playStat.minQpI_base = 8;
	playStat.minQpP_base = 8;
	playStat.minQpB_base = 8;
	playStat.maxQpI_base = 51;
	playStat.maxQpP_base = 51;
	playStat.maxQpB_base = 51;
	playStat.maxDeltaQp_base = 10;
	playStat.PrevFrameQP = 30;
	if (encode_info.qp_mode) {
		playStat.minQpP_base = qp_tbl.qp_P_min;
		playStat.maxQpP_base = qp_tbl.qp_P_max;
		playStat.minQpI_base = qp_tbl.qp_I_min;
		playStat.maxQpI_base = qp_tbl.qp_I_max;
		playStat.maxDeltaQp_base = qp_tbl.qp_I_max - qp_tbl.qp_I_min;
		playStat.PrevFrameQP = qp_tbl.qp_I_base;
	}
	playStat.CurGOPSize = encode_info.gop;
	playStat.qp_ltr = -1;
	playStat.qp_idr = -1;

retry:
	handle_enc = vl_multi_encoder_init(codec_id, encode_info, &qp_tbl);
	if (handle_enc == 0) {
		if ( retry_cnt >= INIT_RETRY) {
			printf("Init encoder retry timeout\n");
			goto exit;
		} else
		{
			printf("Init encoder fail retrying \n");
			retry_cnt ++;
			usleep(100*1000);
			goto retry;
		}
	}

	while (num > 0)
	{
		if (conver_stride) { // exmaple of covert normal with stride to cust stride
			int line;
			unsigned char *dst;
			/* y */
			if (inbuf_info.buf_type == DMA_TYPE)
				dst = input[0];
			else
				dst = inputBuffer;
			memset(dst, 0, ysize);
			for (line = 0; line < height; line++) {
				if (fread(dst, 1, width, fp) != width) {
					printf("read input file error!\n");
					goto exit;
				}
				dst += src_buf_stride;
			}
			if (inbuf_info.buf_type == DMA_TYPE &&
			    dma_info->num_planes > 1)
				dst = input[1];
			if (fmt != IMG_FMT_YUV420P) { //nv12 or nv21
				memset(dst, 0, uvsize);
				for (line = 0; line < height / 2; line++) {
					if (fread(dst, 1, width, fp) != width) {
						printf("read input file error!\n");
						goto exit;
					}
					dst += src_buf_stride;
				}
			} else { // YUV420p seperated
				memset(dst, 0, usize);
				for (line = 0; line < height / 2; line++) {
					if (fread(dst, 1, width / 2, fp) != width / 2) {
						printf("read input file error!\n");
						goto exit;
					}
					dst += (src_buf_stride / 2);
				}
				if (inbuf_info.buf_type == DMA_TYPE &&
				    dma_info->num_planes ==3)
					dst = input[2];
				memset(dst, 0, vsize);
				for (line = 0; line < height / 2; line++) {
					if (fread(dst, 1, width / 2, fp) != width / 2) {
						printf("read input file error!\n");
						goto exit;
					}
					dst += (src_buf_stride / 2);
				}
			}
		} else if (inbuf_info.buf_type == DMA_TYPE) {	// read data to dma buf vaddr
			if (dma_info->num_planes == 1)
			{
				if (fread(input[0], 1, framesize, fp) !=
				    framesize)
				{
					printf("read input file error!\n");
					goto exit;
				}
			} else if (dma_info->num_planes == 2)
			{
				if (fread(input[0], 1, ysize, fp) != ysize)
				{
					printf("read input file error!\n");
					goto exit;
				}
				if (fread(input[1], 1, uvsize, fp) != uvsize)
				{
					printf("read input file error!\n");
					goto exit;
				}
			} else if (dma_info->num_planes == 3)
			{
				if (fread(input[0], 1, ysize, fp) != ysize)
				{
					printf("read input file error!\n");
					goto exit;
				}
				if (fread(input[1], 1, usize, fp) != usize)
				{
					printf("read input file error!\n");
					goto exit;
				}
				if (fread(input[2], 1, vsize, fp) != vsize)
				{
					printf("read input file error!\n");
					goto exit;
				}
			}
		} else
		{		// read data to vmalloc buf vaddr
			if (fread(inputBuffer, 1, framesize, fp) != framesize)
			{
				printf("read input file error!\n");
				goto exit;
			}
		}

		memset(outbuffer, 0, outbuffer_len);
		if (inbuf_info.buf_type == DMA_TYPE)
		{
			sync_cpu(&ctx);
		}
		if (roi_enabled) {
			if (fread(roi_buffer, 1, roi_size, fp_roi) != roi_size)
			{
				fseek(fp_roi, 0, SEEK_SET); //back to start
				if (fread(roi_buffer, 1, roi_size,
				    fp_roi) != roi_size) {
					printf("read roi file error!\n");
					goto exit;
				}
			}
			if (vl_video_encoder_update_qp_hint(handle_enc,
			    roi_buffer,
			    roi_size) != 0) {
				printf("update qp hint failed.\n");
			}
		}
		enc_frame_type = FRAME_TYPE_AUTO;
		if (cfg_upd_enabled && has_cfg_update) {
			if (cfgChange.FrameNum == frame_num) { // apply updates
				if (cfgChange.enable_option
				    & FORCE_PICTURE_TYPE)
				{
					if (cfgChange.picType == FRAME_TYPE_IDR
					    || cfgChange.picType
						== FRAME_TYPE_I)
					{
						enc_frame_type = FRAME_TYPE_I;
						printf("force I frame on %d \n",
						    frame_num);
					}
				}
				if (cfgChange.enable_option
				    & FORCE_PICTURE_SKIP)
				{
					vl_video_encoder_skip_frame(handle_enc);
					printf("force skip frame on %d \n",
						    frame_num);
				}
				if (cfgChange.enable_option
				    & CHANGE_TARGET_RATE)
				{
					vl_video_encoder_change_bitrate(
					    handle_enc,
					    cfgChange.bitRate);
					printf("Change bitrate to %d on %d \n",
					    cfgChange.bitRate, frame_num);
				}
				if (cfgChange.enable_option
				    & CHANGE_MIN_MAX_QP)
				{
					vl_video_encoder_change_qp(
					    handle_enc,
					    cfgChange.minQpI,cfgChange.maxQpI,
					    cfgChange.maxDeltaQp,
					    cfgChange.minQpP,cfgChange.maxQpP,
					    cfgChange.minQpB,cfgChange.maxQpB);
					printf("Change QP table on %d QPs: ",
					    frame_num);
					printf("%d %d %d %d %d %d %d\n",
					    cfgChange.minQpI,cfgChange.maxQpI,
					    cfgChange.maxDeltaQp,
					    cfgChange.minQpP,cfgChange.maxQpP,
					    cfgChange.minQpB,cfgChange.maxQpB);
					playStat.minQpI_base = cfgChange.minQpI;
					playStat.maxQpI_base = cfgChange.maxQpI;
					playStat.maxDeltaQp_base =
					         cfgChange.maxDeltaQp;
					playStat.minQpP_base = cfgChange.minQpP;
					playStat.maxQpP_base = cfgChange.maxQpP;
					playStat.minQpB_base = cfgChange.minQpB;
					playStat.maxQpB_base = cfgChange.maxQpB;
				}
				if (cfgChange.enable_option
				    & CHANGE_GOP_PERIOD)
				{
					vl_video_encoder_change_gop(
					    handle_enc,
					    cfgChange.intraQP,
					    cfgChange.GOPPeriod);
					printf("Change gop on %d QP %d %d\n",
					    frame_num, cfgChange.intraQP,
					    cfgChange.GOPPeriod);
					playStat.CurGOPSize =  cfgChange.GOPPeriod;
					if (playStat.CurGOPCounter >= playStat.CurGOPSize)
					    playStat.CurGOPCounter = 0;
				}
				if ((cfgChange.enable_option
				    & LONGTERM_REF_SET) && ltf_enabled)
				{
					vl_video_encoder_longterm_ref(
					    handle_enc,
					    cfgChange.LTRFlags);
					printf("Longterm Ref on %d flag 0x%x\n",
					    frame_num, cfgChange.LTRFlags);
				}
				if (cfgChange.enable_option
				    & CUST_SIZE_SMOOTH)
				{
					if (cfgChange.smooth_level) {
					    playStat.cust_size_smooth_enable = 1;
					    if (cfgChange.smooth_level< 100)
						playStat.smooth_qpdelta = 4;
					    else
						playStat.smooth_qpdelta = 6;
					} else {
					    playStat.cust_size_smooth_enable = 0;
					    playStat.smooth_qpdelta = 0;
					    playStat.qp_ltr = -1;
					    playStat.qp_idr = -1;
					}
					printf("Set CustSizeSmooth on %d level %d \n",
					       frame_num, cfgChange.smooth_level);
				}
				if ((cfgChange.enable_option
				    & CUST_APP_H264_PLUS) && ltf_enabled)
				{
				    if (cfgChange.newIDRIntervel) { //enable
					if (playStat.cust_ltr_enable == 0) {
					    playStat.cust_ltr_enable = 1;
					    playStat.GopLTRIntervel = playStat.CurGOPSize;
					    playStat.GopLTRCounter = playStat.CurGOPCounter;
					}
					if (playStat.CurGOPSize != cfgChange.newIDRIntervel)
					{
					    vl_video_encoder_change_gop(
						handle_enc,
						playStat.PrevFrameQP,
						cfgChange.newIDRIntervel
						);
					    printf("Gop change on %d  to %d qp %d\n",
						frame_num, cfgChange.newIDRIntervel,
						playStat.PrevFrameQP);
					}
					playStat.CurGOPSize = cfgChange.newIDRIntervel;
					playStat.QPdelta_IDR = cfgChange.QPdelta_IDR;
					playStat.QPdelta_LTR = cfgChange.QPdelta_LTR;
					if ( playStat.CurGOPCounter >= playStat.CurGOPSize)
					    playStat.CurGOPCounter = 0;
					if (playStat.GopLTRCounter >= playStat.GopLTRIntervel)
					    playStat.GopLTRCounter = 0;
					} else {
					    if (playStat.cust_ltr_enable == 1) {
						playStat.cust_ltr_enable = 0;
						playStat.CurGOPSize = playStat.GopLTRIntervel;
						vl_video_encoder_change_gop(
						    handle_enc,
						    playStat.PrevFrameQP,
						    playStat.CurGOPSize);
						printf("Gop change on %d  to %d qp %d\n",
						    frame_num,
						    playStat.CurGOPSize,
						    playStat.PrevFrameQP);
						if (playStat.CurGOPCounter >= playStat.CurGOPSize)
						    playStat.CurGOPCounter = 0;
						playStat.GopLTRIntervel = 0;
						playStat.GopLTRCounter = 0;
						playStat.QPdelta_IDR = 0;
						playStat.QPdelta_LTR = 0;
						playStat.qp_ltr = -1;
						playStat.qp_idr = -1;
					    }
					}
					printf("Set Cust APP H264+ on %d interval %d qp delta idr %d, ltr %d \n",
					    frame_num, cfgChange.newIDRIntervel,
					    cfgChange.QPdelta_IDR,
					    cfgChange.QPdelta_LTR);
				}
				if (cfgChange.enable_option
				    & CHANGE_MULTI_SLICE)
				{
					vl_video_encoder_change_multi_slice(
					    handle_enc,
					    cfgChange.Slice_Mode,
					    cfgChange.Slice_Para);
					printf("Change slice on %d mode %d %d\n",
					       frame_num,
					       cfgChange.Slice_Mode,
					       cfgChange.Slice_Para);
				}
				if (cfgChange.enable_option
				    & CHANGE_STRICT_RC)
				{
					vl_video_encoder_change_strict_rc(
					    handle_enc,
					    cfgChange.bitrate_window,
					    cfgChange.pskip_threshold);
					printf("Change strict rate control on %d window %d threshold %d\n",
					       frame_num,
					       cfgChange.bitrate_window,
					       cfgChange.pskip_threshold);
				}
			}
			if (cfgChange.FrameNum <= frame_num) {
				has_cfg_update =
				    ParseCfgUpdateFile(fp_cfg, &cfgChange);
			}
		}
		if (playStat.CurGOPCounter >= playStat.CurGOPSize)
			playStat.CurGOPCounter = 0;
		if (playStat.cust_ltr_enable) {
			if (playStat.GopLTRCounter >= playStat.GopLTRIntervel)
				playStat.GopLTRCounter = 0;
		}

		if ( playStat.CurGOPCounter == 0)
		{ // I frame
		    if (playStat.cust_size_smooth_enable) {
			my_qp = playStat.PrevFrameQP + playStat.smooth_qpdelta;
			if (my_qp > playStat.maxQpI_base)
			    my_qp = playStat.maxQpI_base;
			if (playStat.qp_idr < 0) // first-time
			     playStat.qp_idr = my_qp;
			if ((playStat.qp_idr > my_qp + 5) ||
			     (playStat.qp_idr < my_qp - 5))
			     playStat.qp_idr = my_qp;

			vl_video_encoder_change_qp(
					    handle_enc,
					    playStat.qp_idr, playStat.maxQpI_base,
					    playStat.maxDeltaQp_base,
					    playStat.qp_idr, playStat.maxQpI_base,
					    playStat.qp_idr, playStat.maxQpI_base);
			playStat.temp_QP_change = 1;
			printf("apply QPminI at frame %d min qp %d, my_qp %d \n", frame_num, playStat.qp_idr, my_qp);
		    } else if (playStat.cust_ltr_enable) {
			my_qp = playStat.PrevFrameQP - playStat.QPdelta_IDR;
			if (my_qp < playStat.minQpI_base)
			    my_qp =playStat.minQpI_base;
			if (playStat.qp_idr < 0) // first-time
			     playStat.qp_idr = my_qp;
			if ((playStat.qp_idr > my_qp + 5) ||
			     (playStat.qp_idr < my_qp - 5))
			     playStat.qp_idr = my_qp;
			vl_video_encoder_change_qp(
					    handle_enc,
					    playStat.minQpI_base, playStat.qp_idr,
					    playStat.maxDeltaQp_base,
					    playStat.minQpI_base, playStat.qp_idr,
					    playStat.minQpI_base, playStat.qp_idr);
			printf("apply QPMaxI at frame %d qp %d , my_qp %d \n", frame_num, playStat.qp_idr, my_qp);
			playStat.temp_QP_change = 1;
		    }
		    if (playStat.cust_ltr_enable) {
			vl_video_encoder_longterm_ref(
					    handle_enc,
					    1); // set LTR
			printf("set ltr at frame %d \n", frame_num);
		    }
		} else if (playStat.GopLTRCounter == 0 && playStat.cust_ltr_enable) { // LTR P
		    my_qp = playStat.PrevFrameQP - playStat.QPdelta_LTR;
		    if (my_qp < playStat.minQpP_base)
			my_qp = playStat.minQpP_base;
			if (playStat.qp_ltr < 0) // first-time
			     playStat.qp_ltr = my_qp;
			if ((playStat.qp_ltr > my_qp + 5) ||
			     (playStat.qp_ltr < my_qp - 5))
			    playStat.qp_ltr = my_qp;
		    vl_video_encoder_change_qp(
					    handle_enc,
					    playStat.minQpI_base,playStat.maxQpI_base,
					    playStat.maxDeltaQp_base,
					    playStat.minQpP_base, playStat.qp_ltr,
				            playStat.minQpB_base,  playStat.maxQpB_base);

		    vl_video_encoder_longterm_ref(
					    handle_enc,
					    2); //  USE LTR
		    printf("apply QPMaxP LTR at frame %d qp %d, my_qp %d \n", frame_num, playStat.qp_ltr, my_qp);
		    playStat.temp_QP_change = 1;
		} else if(playStat.temp_QP_change == 1) {
		    vl_video_encoder_change_qp(
					    handle_enc,
					    playStat.minQpI_base,playStat.maxQpI_base,
					    playStat.maxDeltaQp_base,
					    playStat.minQpP_base,playStat.maxQpP_base,
				            playStat.minQpB_base,  playStat.maxQpB_base);
		    printf("recover QP frame %d to %d, %d, %d P %d, %d, B %d, %d\n", frame_num,
					    playStat.minQpI_base,playStat.maxQpI_base,
					    playStat.maxDeltaQp_base,
					    playStat.minQpP_base,playStat.maxQpP_base,
				            playStat.minQpB_base,  playStat.maxQpB_base);
		    playStat.temp_QP_change = 0;
		}

		encoding_metadata =
		    vl_multi_encoder_encode(handle_enc, enc_frame_type,
					    outbuffer, &inbuf_info, &ret_buf);

		if (encoding_metadata.is_valid)
		{
			write(outfd, (unsigned char *) outbuffer,
			      encoding_metadata.encoded_data_length_in_bytes);
		} else
		{
			printf("encode error %d!\n",
			       encoding_metadata.encoded_data_length_in_bytes);
		}
		num--;
		frame_num++;
		playStat.PrevFrameQP = encoding_metadata.extra.average_qp_value;
		playStat.CurFrameNum++;
		playStat.CurGOPCounter++;
		if (playStat.cust_ltr_enable) {
			playStat.GopLTRCounter++;
		}
	}

exit:

	if (handle_enc)
		vl_multi_encoder_destroy(handle_enc);

	if (outfd >= 0)
		close(outfd);

	if (fp)
		fclose(fp);

	if (fp_roi)
		fclose(fp_roi);

	if (fp_cfg)
		fclose(fp_cfg);

	if (outbuffer != NULL)
		free(outbuffer);

	if (roi_buffer != NULL)
		free(roi_buffer);

	if (inbuf_info.buf_type == DMA_TYPE)
	{
		if (dma_info->shared_fd[0] >= 0)
			close(dma_info->shared_fd[0]);

		if (dma_info->shared_fd[1] >= 0)
			close(dma_info->shared_fd[1]);

		if (dma_info->shared_fd[2] >= 0)
			close(dma_info->shared_fd[2]);

		destroy_ctx(&ctx);
	} else
	{
		if (inputBuffer)
			free(inputBuffer);
	}
	printf("Encode End!\n");

	return 0;
}


static int ParseCfgUpdateFile(FILE *fp, CfgChangeParam *cfg_update)
{
	int parsed_num = 0, frameIdx = 0;
	char* token = NULL;
	static char lineStr[256] = {0, };
	char valueStr[256] = {0, };
	int has_update = 0;
	while (1) {
		if (lineStr[0] == 0x0) { //need a new line;
			if (fgets(lineStr, 256, fp) == NULL ) {
				if (cfg_update->enable_option && has_update)
					return 1;
				else {
					if (feof(fp)) return 0;
					continue;
				}
			}
		}
		if ((lineStr[0] == '#')
		    ||(lineStr[0] == ';')||(lineStr[0] == ':'))
		{ // skip comments
			lineStr[0] = 0x0;
			continue;
		}
		// keep the original
		memcpy(valueStr, lineStr, strlen(lineStr));
		//frame number is separated by ' ' or ':'
		token = strtok(valueStr, ": ");
		if (token == NULL) {
			lineStr[0] = 0x0;
			continue;
		}
		frameIdx = atoi(token);
		if (frameIdx <0 || frameIdx < cfg_update->FrameNum) {
			// invid number
			lineStr[0] = 0x0;
			continue;
		}
		else if (frameIdx > cfg_update->FrameNum && has_update)
		{// done the currrent frame;
			if (cfg_update->enable_option)
				return 1;
			has_update = 0;
			continue;

		} else if (has_update ==0) {// clear the options
			cfg_update->FrameNum = frameIdx;
			cfg_update->enable_option = 0;
			has_update = 1;
		}

		token = strtok(NULL, " :"); // check the operation code
		while (token && strlen(token) == 1
		    && strncmp(token, " ", 1) == 0)
			token = strtok(NULL, " :"); // skip spaces
		if (token == NULL) { // no operate code
			lineStr[0] = 0x0;
			continue;
		}
		if (strcasecmp("ForceType",token) == 0) {
			// force picture type
			token = strtok(NULL, ":\r\n");
			while (token && strlen(token) == 1
			    && strncmp(token, " ", 1) == 0)
			token = strtok(NULL, ":\r\n"); //check space
			if (token) {
				while ( *token == ' ' ) token++;//skip spaces;
				cfg_update->picType = atoi(token);
				cfg_update->enable_option |=
				    FORCE_PICTURE_TYPE;
			}
		} else if (strcasecmp("ForceSkip",token) == 0) {
			// force picture type
			if (token) {
				cfg_update->enable_option |=
				    FORCE_PICTURE_SKIP;
			}
		} else if (strcasecmp("ChangeBitRate",token) == 0) {
			token = strtok(NULL, ":\r\n");
			while ( token && strlen(token) == 1
			    && strncmp(token, " ", 1) == 0)
				token = strtok(NULL, ":\r\n"); //check space
			if (token) {
				while ( *token == ' ' ) token++;//skip spaces;
				cfg_update->bitRate = atoi(token);
				cfg_update->enable_option |=
				    CHANGE_TARGET_RATE;
			}
		} else if (strcasecmp("ChangeMinMaxQP",token) == 0) {
			token = strtok(NULL, ":\r\n");
			while (token && strlen(token) == 1
			    && strncmp(token, " ", 1) == 0)
				token = strtok(NULL, ":\r\n"); //check space
			if (token) {
				while ( *token == ' ' ) token++;//skip spaces;
				parsed_num = sscanf(token,
				    "%d %d %d %d %d %d %d",
				    &cfg_update->minQpI, &cfg_update->maxQpI,
				    &cfg_update->maxDeltaQp,
				    &cfg_update->minQpP, &cfg_update->maxQpP,
				    &cfg_update->minQpB, &cfg_update->maxQpB);
				if (parsed_num == 7)
					cfg_update->enable_option |=
					    CHANGE_MIN_MAX_QP;
			}
		} else if (strcasecmp("ChangePeriodGOP",token) == 0) {
			token = strtok(NULL, ":\r\n");
			while ( token && strlen(token) == 1
			    && strncmp(token, " ", 1) == 0)
				token = strtok(NULL, ":\r\n"); //check space
			if (token) {
				while ( *token == ' ' ) token++;//skip spaces;
				parsed_num = sscanf(token, "%d %d",
					&cfg_update->intraQP,
					&cfg_update->GOPPeriod);
				if (parsed_num == 2)
					cfg_update->enable_option |=
					    CHANGE_GOP_PERIOD;
			}
		} else if (strcasecmp("SetLongTermRef",token) == 0) {
			token = strtok(NULL, ":\r\n");
			while ( token && strlen(token) == 1
			    && strncmp(token, " ", 1) == 0)
				token = strtok(NULL, ":\r\n"); //check space
			if (token) {
				while ( *token == ' ' ) token++;//skip spaces;
				cfg_update->LTRFlags = atoi(token);
				cfg_update->enable_option |=
				    LONGTERM_REF_SET;
			}
		} else if (strcasecmp("SetCustSmooth",token) == 0) {
			token = strtok(NULL, ":\r\n");
			while ( token && strlen(token) == 1
			    && strncmp(token, " ", 1) == 0)
				token = strtok(NULL, ":\r\n"); //check space
			if (token) {
				while ( *token == ' ' ) token++;//skip spaces;
				cfg_update->smooth_level = atoi(token);
				cfg_update->enable_option |=
				    CUST_SIZE_SMOOTH;
			}
		} else if (strcasecmp("SetCustH264P",token) == 0) {
			token = strtok(NULL, ":\r\n");
			while ( token && strlen(token) == 1
			    && strncmp(token, " ", 1) == 0)
				token = strtok(NULL, ":\r\n"); //check space
			if (token) {
				while ( *token == ' ' ) token++;//skip spaces;
				parsed_num = sscanf(token,
				    "%d %d %d",
				    &cfg_update->newIDRIntervel,
				    &cfg_update->QPdelta_IDR,
				    &cfg_update->QPdelta_LTR);
				if (parsed_num == 3)
					cfg_update->enable_option |=
					    CUST_APP_H264_PLUS;
			}
		} else if (strcasecmp("ChangeMultiSlice",token) == 0) {
			token = strtok(NULL, ":\r\n");
			while ( token && strlen(token) == 1
			    && strncmp(token, " ", 1) == 0)
				token = strtok(NULL, ":\r\n"); //check space
			if (token) {
				while ( *token == ' ' ) token++;//skip spaces;
				parsed_num = sscanf(token, "%d %d",
					&cfg_update->Slice_Mode,
					&cfg_update->Slice_Para);
				if (parsed_num == 2)
					cfg_update->enable_option |=
					    CHANGE_MULTI_SLICE;
			}
		}
		else if (strcasecmp("ChangeStrictRC",token) == 0) {
			token = strtok(NULL, ":\r\n");
			while ( token && strlen(token) == 1
			    && strncmp(token, " ", 1) == 0)
				token = strtok(NULL, ":\r\n"); //check space
			if (token) {
				while ( *token == ' ' ) token++;//skip spaces;
				parsed_num = sscanf(token, "%d %d",
					&cfg_update->bitrate_window,
					&cfg_update->pskip_threshold);
				if (parsed_num == 2)
					cfg_update->enable_option |=
					    CHANGE_STRICT_RC;
			}
		}
		lineStr[0] = 0x0;
		continue;
	}
}