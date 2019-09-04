
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
#ifndef VPUAPI_UTIL_H_INCLUDED
#define VPUAPI_UTIL_H_INCLUDED

#include "vpuapi.h"

// COD_STD
enum {
    AVC_DEC  = 0,
    VC1_DEC  = 1,
    MP2_DEC  = 2,
    MP4_DEC  = 3,
    DV3_DEC  = 3,
    RV_DEC   = 4,
    AVS_DEC  = 5,
    VPX_DEC  = 7,
    MAX_DEC  = 7,
    AVC_ENC  = 8,
    MP4_ENC  = 11,
    MAX_CODECS,
};

// new COD_STD since VPU series
enum {
    W_HEVC_DEC                 = 0x00,
    W_HEVC_ENC                 = 0x01,

    W_AVC_DEC                  = 0x02,
    W_AVC_ENC                  = 0x03,

    W_VP9_DEC                  = 0x16,
    W_AVS2_DEC                 = 0x18,

    W_SVAC_DEC                 = 0x20,
    W_SVAC_ENC                 = 0x21,

    STD_UNKNOWN                = 0xFF
};

// AUX_COD_STD
enum {
    MP4_AUX_MPEG4 = 0,
    MP4_AUX_DIVX3 = 1
};

enum {
    VPX_AUX_THO = 0,
    VPX_AUX_VP6 = 1,
    VPX_AUX_VP8 = 2,
    VPX_AUX_NUM
};

enum {
    AVC_AUX_AVC = 0,
    AVC_AUX_MVC = 1
};

// BIT_RUN command
enum {
    DEC_SEQ_INIT = 1,
    ENC_SEQ_INIT = 1,
    DEC_SEQ_END = 2,
    ENC_SEQ_END = 2,
    PIC_RUN = 3,
    SET_FRAME_BUF = 4,
    ENCODE_HEADER = 5,
    ENC_PARA_SET = 6,
    DEC_PARA_SET = 7,
    DEC_BUF_FLUSH = 8,
    RC_CHANGE_PARAMETER    = 9,
    VPU_SLEEP = 10,
    VPU_WAKE = 11,
    ENC_ROI_INIT = 12,
    FIRMWARE_GET = 0xf
};

enum {
    SRC_BUFFER_EMPTY        = 0,    //!< source buffer doesn't allocated.
    SRC_BUFFER_ALLOCATED    = 1,    //!< source buffer has been allocated.
    SRC_BUFFER_SRC_LOADED   = 2,    //!< source buffer has been allocated.
    SRC_BUFFER_USE_ENCODE   = 3     //!< source buffer was sent to VPU. but it was not used for encoding.
};

typedef enum {
    AUX_BUF_TYPE_MVCOL,
    AUX_BUF_TYPE_FBC_Y_OFFSET,
    AUX_BUF_TYPE_FBC_C_OFFSET
} AUX_BUF_TYPE;

#ifndef MAX
#define MAX(a, b)       (((a) > (b)) ? (a) : (b))
#endif

#ifndef MIN
#define MIN(a,b)            (((a) < (b)) ? (a) : (b))
#endif

#define HEVC_MAX_SUB_LAYER_ID   6
#define AVS2_MAX_SUB_LAYER_ID   7
#define SVAC_MAX_SUB_LAYER_ID   3

//#define API_DEBUG
#ifdef API_DEBUG
#ifdef _MSC_VER
#define APIDPRINT(_fmt, ...)            printf(_fmt, __VA_ARGS__)
#else
#define APIDPRINT(_fmt, ...)            printf(_fmt, ##__VA_ARGS__)
#endif
#else
#define APIDPRINT(_fmt, ...)
#endif

extern Uint32 __VPU_BUSY_TIMEOUT;
/**
 * PRODUCT: old
 */
typedef struct {
    union {
        struct {
            int             useBitEnable;
            int             useIpEnable;
            int             useDbkYEnable;
            int             useDbkCEnable;
            int             useOvlEnable;
            int             useBtpEnable;
            PhysicalAddress bufBitUse;
            PhysicalAddress bufIpAcDcUse;
            PhysicalAddress bufDbkYUse;
            PhysicalAddress bufDbkCUse;
            PhysicalAddress bufOvlUse;
            PhysicalAddress bufBtpUse;
        } old;
        struct {
            int             useIpEnable;
            int             useLfRowEnable;
            int             useBitEnable;
            PhysicalAddress bufIp;
            PhysicalAddress bufLfRow;
            PhysicalAddress bufBit;
            int             useEncImdEnable;
            int             useEncRdoEnable;
            int             useEncLfEnable;
            PhysicalAddress bufImd;
            PhysicalAddress bufRdo;
            PhysicalAddress bufLf;
        } vp;
    } u;
    int             bufSize;
    PhysicalAddress bufBase;
} SecAxiInfo;

typedef struct {
    DecOpenParam    openParam;
    DecInitialInfo  initialInfo;
    DecInitialInfo  newSeqInfo;     /* Temporal new sequence information */
    PhysicalAddress streamWrPtr;
    PhysicalAddress streamRdPtr;
    Int32           streamBufFull;
    int             streamEndflag;
    int             frameDisplayFlag;
    int             clearDisplayIndexes;
    int             setDisplayIndexes;
    PhysicalAddress streamRdPtrRegAddr;
    PhysicalAddress streamWrPtrRegAddr;
    PhysicalAddress streamBufStartAddr;
    PhysicalAddress streamBufEndAddr;
    PhysicalAddress frameDisplayFlagRegAddr;
    PhysicalAddress currentPC;
    PhysicalAddress busyFlagAddr;
    int             streamBufSize;
    FrameBuffer     frameBufPool[MAX_REG_FRAME];
    vpu_buffer_t    vbFrame;
    vpu_buffer_t    vbWTL;
    vpu_buffer_t    vbPPU;
    vpu_buffer_t    vbMV[MAX_REG_FRAME];
    vpu_buffer_t    vbFbcYTbl[MAX_REG_FRAME];
    vpu_buffer_t    vbFbcCTbl[MAX_REG_FRAME];
    int             chBwbFrameIdx;
    int             chFbcFrameIdx;
    BOOL            interResChange;
    int             frameAllocExt;
    int             ppuAllocExt;
    int             numFrameBuffers;
    int             numFbsForDecoding;                  /*!<< number of framebuffers used in decoding */
    int             numFbsForWTL;                       /*!<< number of linear framebuffer for displaying when DecInfo::wtlEnable is set to 1 */
    int             stride;
    int             frameBufferHeight;
    int             rotationEnable;
    int             mirrorEnable;
    int             deringEnable;
    MirrorDirection mirrorDirection;
    int             rotationAngle;
    FrameBuffer     rotatorOutput;
    int             rotatorStride;
    int             rotatorOutputValid;
    int             initialInfoObtained;
    int             vc1BframeDisplayValid;
    int             mapType;
    int             tiled2LinearEnable;
    int             tiled2LinearMode;
    int             wtlEnable;
    int             wtlMode;
    FrameBufferFormat   wtlFormat;                      /*!<< default value: FORMAT_420 8bit */
    SecAxiInfo          secAxiInfo;
    MaverickCacheConfig cacheConfig;
    int chunkSize;
    int seqInitEscape;

    // Report Information
    PhysicalAddress userDataBufAddr;
    vpu_buffer_t    vbUserData;

    Uint32          userDataEnable;                    /* User Data Enable Flag
                                                          Non VP TRUE or FALSE
                                                          VPUxxx: Refer to H265_USERDATA_FLAG_xxx values in vpuapi.h */
    int             userDataBufSize;
    int             userDataReportMode;                // User Data report mode (0 : interrupt mode, 1 interrupt disable mode)


    LowDelayInfo    lowDelayInfo;
    int             frameStartPos;
    int             frameEndPos;
    int             frameDelay;
    vpu_buffer_t    vbSlice;            // AVC, VP8 only
    vpu_buffer_t    vbWork;
    vpu_buffer_t    vbTemp;
    vpu_buffer_t    vbReport;
    int             workBufferAllocExt;
    DecOutputInfo   decOutInfo[MAX_GDI_IDX];
    TiledMapConfig  mapCfg;
    int             reorderEnable;
    Uint32          avcErrorConcealMode;
    DRAMConfig      dramCfg;            //960 only
    int             thumbnailMode;
    int             seqChangeMask;      // VPU410
    Uint32          prevFrameEndPos;      //!<< VPU410v2: end position of previous frame
    Int32           targetSubLayerId;       //!<< H.265 temporal scalability

    int             rdPtrValidFlag;

    Int32           instanceQueueCount;
    Int32           totalQueueCount;
    Uint32          firstCycleCheck;
    Uint32          PrevDecodeEndTick;
    Uint32          cyclePerTick;
} DecInfo;

typedef struct {
    EncOpenParam        openParam;
    EncInitialInfo      initialInfo;
    PhysicalAddress     streamRdPtr;
    PhysicalAddress     streamWrPtr;
    int                 streamEndflag;
    PhysicalAddress     streamRdPtrRegAddr;
    PhysicalAddress     streamWrPtrRegAddr;
    PhysicalAddress     streamBufStartAddr;
    PhysicalAddress     streamBufEndAddr;
    PhysicalAddress     currentPC;
    PhysicalAddress     busyFlagAddr;
    int                 streamBufSize;
    int                 linear2TiledEnable;
    int                 linear2TiledMode;    // Non VP only
    TiledMapType        mapType;
    int                 userMapEnable;
    FrameBuffer         frameBufPool[MAX_REG_FRAME];
    vpu_buffer_t        vbFrame;
    vpu_buffer_t        vbPPU;
    int                 frameAllocExt;
    int                 ppuAllocExt;
    vpu_buffer_t        vbSubSampFrame;         /*!<< 960 only */
    vpu_buffer_t        vbMvcSubSampFrame;      /*!<< 960 only */
    int                 numFrameBuffers;
    int                 stride;
    int                 frameBufferHeight;
    int                 rotationEnable;
    int                 mirrorEnable;
    MirrorDirection     mirrorDirection;
    int                 rotationAngle;
    int                 initialInfoObtained;
    int                 ringBufferEnable;
    SecAxiInfo          secAxiInfo;
    MaverickCacheConfig cacheConfig;

    int                 sliceIntEnable;       /*!<< VPU420 only */

    int                 ActivePPSIdx;           /*!<< 980 */
    int                 frameIdx;               /*!<< 980 */
    int                 fieldDone;              /*!<< 980 */
    int                 lineBufIntEn;
    vpu_buffer_t        vbWork;
    vpu_buffer_t        vbScratch;

    vpu_buffer_t        vbTemp;                 //!< Temp buffer (VPU encoder )
    vpu_buffer_t        vbMV;                   //!< colMV buffer (VPU encoder)
    vpu_buffer_t        vbMVBL;                 //!< colMV buffer (VPU encoder)
    vpu_buffer_t        vbFbcYTbl;              //!< FBC Luma table buffer (VPU encoder)
    vpu_buffer_t        vbFbcCTbl;              //!< FBC Chroma table buffer (VPU encoder)
    vpu_buffer_t        vbFbcYTblBL;            //!< FBC Luma table buffer (VPU encoder)
    vpu_buffer_t        vbFbcCTblBL;            //!< FBC Chroma table buffer (VPU encoder)
    vpu_buffer_t        vbSubSamBuf;            //!< Sub-sampled buffer for ME (VPU encoder)
    vpu_buffer_t        vbSubSamBufBL;          //!< Sub-sampled buffer for ME (VPU encoder)

    TiledMapConfig      mapCfg;
    DRAMConfig          dramCfg;                /*!<< 960 */


    Int32   errorReasonCode;
    Uint64          curPTS;             /**! Current timestamp in 90KHz */
    Uint64          ptsMap[MAX_SRC_FRAME]; /**! PTS mapped with source frame index */
    FrameBuffer     srcBufMap[MAX_SRC_FRAME]; /**! source frame index map */
    Uint32          srcBufUseIndex[MAX_SRC_FRAME]; /**! flag the occupied src buf */
    Uint32          instanceQueueCount;
    Uint32          totalQueueCount;
    Uint32          encWrPtrSel;
    Uint32          firstCycleCheck;
    Uint32          PrevEncodeEndTick;
    Uint32          cyclePerTick;
} EncInfo;


typedef struct CodecInst {
    Int32   inUse;
    Int32   instIndex;
    Int32   coreIdx;
    Int32   codecMode;
    Int32   codecModeAux;
    Int32   productId;
    Int32   loggingEnable;
    Uint32  isDecoder;
    union {
        EncInfo encInfo;
        DecInfo decInfo;
    }* CodecInfo;
} CodecInst;

/*******************************************************************************
 * H.265 USER DATA(VUI and SEI)                                           *
 *******************************************************************************/
#define H265_MAX_DPB_SIZE               17
#define H265_MAX_NUM_SUB_LAYER          8
#define H265_MAX_NUM_ST_RPS             64
#define H265_MAX_CPB_CNT                32
#define H265_MAX_NUM_VERTICAL_FILTERS   5
#define H265_MAX_NUM_HORIZONTAL_FILTERS 3
#define H265_MAX_TAP_LENGTH             32
#define H265_MAX_NUM_KNEE_POINT         999

#define H265_MAX_NUM_TONE_VALUE				1024
#define H265_MAX_NUM_FILM_GRAIN_COMPONENT	3
#define H265_MAX_NUM_INTENSITY_INTERVALS	256
#define H265_MAX_NUM_MODEL_VALUES			5

#define H265_MAX_LUT_NUM_VAL				3
#define H265_MAX_LUT_NUM_VAL_MINUS1			33
#define H265_MAX_COLOUR_REMAP_COEFFS		3
typedef struct
{
    Uint32   offset;
    Uint32   size;
} user_data_entry_t;

typedef struct
{
    Int16   left;
    Int16   right;
    Int16   top;
    Int16   bottom;
} win_t;

typedef struct
{
    Uint8    nal_hrd_param_present_flag;
    Uint8    vcl_hrd_param_present_flag;
    Uint8    sub_pic_hrd_params_present_flag;
    Uint8    tick_divisor_minus2;
    Int8     du_cpb_removal_delay_inc_length_minus1;
    Int8     sub_pic_cpb_params_in_pic_timing_sei_flag;
    Int8     dpb_output_delay_du_length_minus1;
    Int8     bit_rate_scale;
    Int8     cpb_size_scale;
    Int8     initial_cpb_removal_delay_length_minus1;
    Int8     cpb_removal_delay_length_minus1;
    Int8     dpb_output_delay_length_minus1;

    Uint8    fixed_pic_rate_gen_flag[H265_MAX_NUM_SUB_LAYER];
    Uint8    fixed_pic_rate_within_cvs_flag[H265_MAX_NUM_SUB_LAYER];
    Uint8    low_delay_hrd_flag[H265_MAX_NUM_SUB_LAYER];
    Int8     cpb_cnt_minus1[H265_MAX_NUM_SUB_LAYER];
    Int16    elemental_duration_in_tc_minus1[H265_MAX_NUM_SUB_LAYER];

    Uint32   nal_bit_rate_value_minus1[H265_MAX_NUM_SUB_LAYER][H265_MAX_CPB_CNT];
    Uint32   nal_cpb_size_value_minus1[H265_MAX_NUM_SUB_LAYER][H265_MAX_CPB_CNT];
    Uint32   nal_cpb_size_du_value_minus1[H265_MAX_NUM_SUB_LAYER];
    Uint32   nal_bit_rate_du_value_minus1[H265_MAX_NUM_SUB_LAYER];
    Uint8    nal_cbr_flag[H265_MAX_NUM_SUB_LAYER][H265_MAX_CPB_CNT];

    Uint32   vcl_bit_rate_value_minus1[H265_MAX_NUM_SUB_LAYER][H265_MAX_CPB_CNT];
    Uint32   vcl_cpb_size_value_minus1[H265_MAX_NUM_SUB_LAYER][H265_MAX_CPB_CNT];
    Uint32   vcl_cpb_size_du_value_minus1[H265_MAX_NUM_SUB_LAYER];
    Uint32   vcl_bit_rate_du_value_minus1[H265_MAX_NUM_SUB_LAYER];
    Uint8    vcl_cbr_flag[H265_MAX_NUM_SUB_LAYER][H265_MAX_CPB_CNT];
} h265_hrd_param_t;

typedef struct
{
    Uint8    aspect_ratio_info_present_flag;
    Uint8    aspect_ratio_idc;
    Uint8    overscan_info_present_flag;
    Uint8    overscan_appropriate_flag;

    Uint8    video_signal_type_present_flag;
    Int8     video_format;

    Uint8    video_full_range_flag;
    Uint8    colour_description_present_flag;

    Uint16   sar_width;
    Uint16   sar_height;

    Uint8    colour_primaries;
    Uint8    transfer_characteristics;
    Uint8    matrix_coefficients;

    Uint8    chroma_loc_info_present_flag;
    Int8     chroma_sample_loc_type_top_field;
    Int8     chroma_sample_loc_type_bottom_field;

    Uint8    neutral_chroma_indication_flag;

    Uint8    field_seq_flag;

    Uint8    frame_field_info_present_flag;
    Uint8    default_display_window_flag;
    Uint8    vui_timing_info_present_flag;
    Uint8    vui_poc_proportional_to_timing_flag;

    Uint32   vui_num_units_in_tick;
    Uint32   vui_time_scale;

    Uint8    vui_hrd_parameters_present_flag;
    Uint8    bitstream_restriction_flag;

    Uint8    tiles_fixed_structure_flag;
    Uint8    motion_vectors_over_pic_boundaries_flag;
    Uint8    restricted_ref_pic_lists_flag;
    Int8     min_spatial_segmentation_idc;
    Int8     max_bytes_per_pic_denom;
    Int8     max_bits_per_mincu_denom;

    Int16    vui_num_ticks_poc_diff_one_minus1;
    Int8     log2_max_mv_length_horizontal;
    Int8     log2_max_mv_length_vertical;

    win_t    def_disp_win;
    h265_hrd_param_t hrd_param;
} h265_vui_param_t;

typedef struct
{
    Uint32   display_primaries_x[3];
    Uint32   display_primaries_y[3];
    Uint32   white_point_x                   : 16;
    Uint32   white_point_y                   : 16;
    Uint32   max_display_mastering_luminance : 32;
    Uint32   min_display_mastering_luminance : 32;
} h265_mastering_display_colour_volume_t;

typedef struct
{
    Uint32   ver_chroma_filter_idc               : 8;
    Uint32   hor_chroma_filter_idc               : 8;
    Uint32   ver_filtering_field_processing_flag : 1;
    Uint32   target_format_idc                   : 2;
    Uint32   num_vertical_filters                : 3;
    Uint32   num_horizontal_filters              : 3;
    Uint8    ver_tap_length_minus1[ H265_MAX_NUM_VERTICAL_FILTERS ];
    Uint8    hor_tap_length_minus1[ H265_MAX_NUM_HORIZONTAL_FILTERS ];
    Int32    ver_filter_coeff[ H265_MAX_NUM_VERTICAL_FILTERS ][ H265_MAX_TAP_LENGTH ];
    Int32    hor_filter_coeff[ H265_MAX_NUM_HORIZONTAL_FILTERS ][ H265_MAX_TAP_LENGTH ];
} h265_chroma_resampling_filter_hint_t;

typedef struct
{
    Uint32   knee_function_id;
    Uint8    knee_function_cancel_flag;

    Uint8    knee_function_persistence_flag;
    Uint32   input_disp_luminance;
    Uint32   input_d_range;
    Uint32   output_d_range;
    Uint32   output_disp_luminance;
    Uint16   num_knee_points_minus1;
    Uint16   input_knee_point[ H265_MAX_NUM_KNEE_POINT ];
    Uint16   output_knee_point[ H265_MAX_NUM_KNEE_POINT ];
} h265_knee_function_info_t;

typedef struct
{
    Uint16 max_content_light_level;
    Uint16 max_pic_average_light_level;
} h265_content_light_level_info_t;

typedef struct
{
    Uint32 colour_remap_id;
    Uint8  colour_remap_cancel_flag;
    Uint8  colour_remap_persistence_flag;
    Uint8  colour_remap_video_signal_info_present_flag;
    Uint8  colour_remap_full_range_flag;

    Uint8 colour_remap_primaries;
    Uint8 colour_remap_transfer_function;
    Uint8 colour_remap_matrix_coefficients;
    Uint8 colour_remap_input_bit_depth;

    Uint8  colour_remap_bit_depth;
    Uint8  pre_lut_num_val_minus1[H265_MAX_LUT_NUM_VAL];
    Uint16 pre_lut_coded_value[H265_MAX_LUT_NUM_VAL][H265_MAX_LUT_NUM_VAL_MINUS1];
    Uint16 pre_lut_target_value[H265_MAX_LUT_NUM_VAL][H265_MAX_LUT_NUM_VAL_MINUS1];

    Uint8  colour_remap_matrix_present_flag;
    Uint8  log2_matrix_denom;
    Uint8  colour_remap_coeffs[H265_MAX_COLOUR_REMAP_COEFFS][H265_MAX_COLOUR_REMAP_COEFFS];

    Uint8  post_lut_num_val_minus1[H265_MAX_LUT_NUM_VAL];
    Uint16 post_lut_coded_value[H265_MAX_LUT_NUM_VAL][H265_MAX_LUT_NUM_VAL_MINUS1];
    Uint16 post_lut_target_value[H265_MAX_LUT_NUM_VAL][H265_MAX_LUT_NUM_VAL_MINUS1];

} h265_colour_remapping_info_t;

typedef struct
{
    Uint8 film_grain_characteristics_cancel_flag;
    Uint8 film_grain_model_id;
    Uint8 separate_colour_description_present_flag;
    Uint8 film_grain_bit_depth_luma_minus8;
    Uint8 film_grain_bit_depth_chroma_minus8;
    Uint8 film_grain_full_range_flag;
    Uint8 film_grain_colour_primaries;
    Uint8 film_grain_transfer_characteristics;
    Uint8	film_grain_matrix_coeffs;

    Uint8 blending_mode_id;
    Uint8 log2_scale_factor;

    Uint8 comp_model_present_flag[H265_MAX_NUM_FILM_GRAIN_COMPONENT];
    Uint8 num_intensity_intervals_minus1[H265_MAX_NUM_FILM_GRAIN_COMPONENT];
    Uint8 num_model_values_minus1[H265_MAX_NUM_FILM_GRAIN_COMPONENT];
    Uint8 intensity_interval_lower_bound[H265_MAX_NUM_FILM_GRAIN_COMPONENT][H265_MAX_NUM_INTENSITY_INTERVALS];
    Uint8 intensity_interval_upper_bound[H265_MAX_NUM_FILM_GRAIN_COMPONENT][H265_MAX_NUM_INTENSITY_INTERVALS];
    Uint32 comp_model_value[H265_MAX_NUM_FILM_GRAIN_COMPONENT][H265_MAX_NUM_INTENSITY_INTERVALS][H265_MAX_NUM_MODEL_VALUES];

    Uint8 film_grain_characteristics_persistence_flag;
} h265_film_grain_characteristics_t;


typedef struct
{
    Uint32 tone_map_id;
    Uint8  tone_map_cancel_flag;

    Uint8  tone_map_persistence_flag;
    Uint32 coded_data_bit_depth;
    Uint32 target_bit_depth;
    Uint8  tone_map_model_id;
    Uint32 min_value;
    Uint32 max_value;
    Uint32 sigmoid_midpoint;
    Uint32 sigmoid_width;
    Uint16 start_of_coded_interval[H265_MAX_NUM_TONE_VALUE]; // [1 << target_bit_depth] // 10bits
    Uint16 num_pivots; // [(1 << coded_data_bit_depth)?1][(1 << target_bit_depth)-1] // 10bits
    Uint16 coded_pivot_value[H265_MAX_NUM_TONE_VALUE];
    Uint16 target_pivot_value[H265_MAX_NUM_TONE_VALUE];
    Uint8  camera_iso_speed_idc;
    Uint32 camera_iso_speed_value;
    Uint8  exposure_index_idc;
    Uint32 exposure_index_value;
    Uint8  exposure_compensation_value_sign_flag;
    Uint16 exposure_compensation_value_numerator;
    Uint16 exposure_compensation_value_denom_idc;
    Uint32 ref_screen_luminance_white;
    Uint32 extended_range_white_level;
    Uint16 nominal_black_level_code_value;
    Uint16 nominal_white_level_code_value;
    Uint16 extended_white_level_code_value;
} h265_tone_mapping_info_t;

typedef struct
{
    Int8   status;           // 0 : empty, 1 : occupied
    Int8   pic_struct;
    Int8   source_scan_type;
    Int8   duplicate_flag;
} h265_sei_pic_timing_t;

#ifdef __cplusplus
extern "C" {
#endif

RetCode InitCodecInstancePool(Uint32 coreIdx);
RetCode GetCodecInstance(Uint32 coreIdx, CodecInst ** ppInst);
void    FreeCodecInstance(CodecInst * pCodecInst);

RetCode CheckInstanceValidity(
    CodecInst * pCodecInst
    );

Int32 ConfigSecAXIVp(
    Uint32      coreIdx,
    Int32       codecMode,
    SecAxiInfo* sa,
    Uint32      width,
    Uint32      height,
    Uint32      profile,
    Uint32      level
    );

RetCode UpdateFrameBufferAddr(
    TiledMapType            mapType,
    FrameBuffer*            fbArr,
    Uint32                  numOfFrameBuffers,
    Uint32                  sizeLuma,
    Uint32                  sizeChroma
    );

RetCode CheckEncInstanceValidity(EncHandle handle);
RetCode CheckEncOpenParam(EncOpenParam * pop);
RetCode CheckEncParam(EncHandle handle, EncParam * param);

RetCode EnterLock(Uint32 coreIdx);
RetCode LeaveLock(Uint32 coreIdx);
RetCode EnterLock_noclk(Uint32 coreIdx);
RetCode LeaveLock_noclk(Uint32 coreIdx);
RetCode SetClockGate(Uint32 coreIdx, Uint32 on);

void SetPendingInst(Uint32 coreIdx, CodecInst *inst);
void ClearPendingInst(Uint32 coreIdx);
CodecInst *GetPendingInst(Uint32 coreIdx);
int GetPendingInstIdx(Uint32 coreIdx);

/**
 * \brief   It returns the stride of framebuffer in byte.
 *
 * \param   width           picture width in pixel.
 * \param   format          YUV format. see FrameBufferFormat structure in vpuapi.h
 * \param   cbcrInterleave
 * \param   mapType         map type. see TiledMapType in vpuapi.h
 */
Int32 CalcStride(
    Uint32              width,
    Uint32              height,
    FrameBufferFormat   format,
    BOOL                cbcrInterleave,
    TiledMapType        mapType,
    BOOL                isVP9
    );

Int32 CalcLumaSize(
    Int32               productId,
    Int32               stride,
    Int32               height,
    FrameBufferFormat   format,
    BOOL                cbcrIntl,
    TiledMapType        mapType,
    DRAMConfig*         pDramCfg
    );

Int32 CalcChromaSize(
    Int32               productId,
    Int32               stride,
    Int32               height,
    FrameBufferFormat   format,
    BOOL                cbcrIntl,
    TiledMapType        mapType,
    DRAMConfig*         pDramCfg
    );

/**
* @brief This structure is used for reporting bandwidth (only for VP5).
*/
typedef struct {
    Uint32 prpBwRead;
    Uint32 prpBwWrite;
    Uint32 fbdYRead;
    Uint32 fbcYWrite;
    Uint32 fbdCRead;
    Uint32 fbcCWrite;
    Uint32 priBwRead;
    Uint32 priBwWrite;
    Uint32 secBwRead;
    Uint32 secBwWrite;
    Uint32 procBwRead;
    Uint32 procBwWrite;
} VPUBWData;




int LevelCalculation(int MbNumX, int MbNumY, int frameRateInfo, int interlaceFlag, int BitRate, int SliceNum);

/* timescale: 1/90000 */
Uint64 GetTimestamp(
    EncHandle handle
    );

RetCode CalcEncCropInfo(
    CodecInst* instance,
    EncVpParam* param,
    int rotMode,
    int srcWidth,
    int srcHeight
    );


#if defined(SUPPORT_SW_UART) || defined(SUPPORT_SW_UART_V2)
void SwUartHandler(void *context);
int create_sw_uart_thread(unsigned long coreIdx);
void destory_sw_uart_thread();
#endif

#ifdef __cplusplus
}
#endif

#endif // endif VPUAPI_UTIL_H_INCLUDED

