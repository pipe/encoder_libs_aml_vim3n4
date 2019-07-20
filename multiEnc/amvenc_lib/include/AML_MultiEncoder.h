#ifndef AMLOGIC_HWENCODER_
#define AMLOGIC_HWENCODER_

#include "enc_define.h"
#include <stdbool.h>

#define amv_enc_handle_t long
#define NUM_CUSTOM_LAMBDA   (2*52)

typedef struct FrameIO_s {
  ulong YCbCr[3];
  // long long YCbCr[3];
  AMVEncBufferType type;
  AMVEncFrameFmt fmt;
  int pitch;
  int height;
  uint32 disp_order;
  uint is_reference;
  unsigned long long coding_timestamp;
  uint32 op_flag;
  uint32 canvas;
  uint32 bitrate;
  float frame_rate;
  uint32 scale_width;
  uint32 scale_height;
  uint32 crop_left;
  uint32 crop_right;
  uint32 crop_top;
  uint32 crop_bottom;
  uint32 num_planes;
  int shared_fd[3];
  // OUTPUT  bit-stream information
  int encoded_frame_type;  //define of PicType in vpuapi.h
} AMVMultiEncFrameIO;

typedef enum AMVGOPModeOPT_S {
  GOP_OPT_NONE,
  GOP_ALL_I,
  GOP_IP,
  GOP_IBBBP,
  GOP_MAX_OPT,
} AMVGOPModeOPT;

typedef struct EncInitParams_s {
  AMVEncStreamType stream_type; /* encoded stream type AVC/HEVC */
  /* if profile/level is set to zero, encoder will choose the closest one for
   * you */
  uint32 profile;   /* profile of the bitstream to be compliant with*/
  uint32 level;     /* level of the bitstream to be compliant with*/
  uint32 hevc_tier;  /*heve tie 0 Main, 1 high tie */
  uint32 refresh_type;

  int width;  /* width of an input frame in pixel */
  int height; /* height of an input frame in pixel */
  AMVGOPModeOPT GopPreset; /* preset GOP structure */

  int num_ref_frame;   /* number of reference frame used */
  int num_slice_group; /* number of slice group */

  uint32 nSliceHeaderSpacing;

  uint32 auto_scd; /* scene change detection on or off */
  int idr_period;   /* idr frame refresh rate in number of target encoded */
                    /* frame (no concept of actual time).*/
  bool prepend_spspps_to_idr_frames; /* if true, prepends spspps header into all
                                    idr frames.*/
  uint32 fullsearch; /* enable full-pel full-search mode */
  int search_range;   /* search range for motion vector in */
                      /* (-search_range, +search_range) pixels */
  // AVCFlag sub_pel;    /* enable sub pel prediction */
  // AVCFlag submb_pred; /* enable sub MB partition mode */

  uint32 rate_control; /* rate control enable, on: RC on, off: constant QP */
  uint32 bitrate;       /* target encoding bit rate in bits/second */
  uint32 CPB_size;      /* coded picture buffer in number of bits */
  uint32 init_CBP_removal_delay; /* initial CBP removal delay in msec */

  uint32 frame_rate;
  /* note, frame rate is only needed by the rate control, AVC is timestamp
   * agnostic. */

  uint32 MBsIntraRefresh;
  uint32 MBsIntraOverlap;

  uint32 out_of_band_param_set; /* flag to set whether param sets are to be */
                                 /* retrieved up front or not */
  uint32 FreeRun;
  uint32 BitrateScale;
  uint32 dev_id;     /* ID to identify the hardware encoder version */
  uint8 encode_once; /* flag to indicate encode once or twice */
  uint32 src_width;  /*src buffer width before crop and scale */
  uint32 src_height; /*src buffer height before crop and scale */
  uint32 dev_fd;     /*actual encoder device fd*/
  AMVEncFrameFmt fmt;
  uint32 rotate_angle; // input frame rotate angle 0, 90, 180, 270
  uint32 mirror; /*frame mirror: 0-none,1-vert, 2, hor, 3, both V and H.*/
  uint32 roi_enable; /* enable roi  */
  uint32 lambda_map_enable; /* enable lambda map */
  uint32 mode_map_enable; /* enable mode map*/

  int qp_mode;
  int maxQP;
  int minQP;
  int initQP;           /* initial QP */
  int initQP_P;         /* initial QP P frame*/
  int initQP_B;         /* initial QP B frame */
  int maxDeltaQP;            /* max QP delta*/
  int maxQP_I;            /* max QP*/
  int minQP_I;            /* min QP*/
  int maxQP_P;          /* max QP P frame */
  int minQP_P;          /* min QP P frame */
  int maxQP_B;          /* max QP B frame*/
  int minQP_B;          /* min QP B frame */
} AMVEncInitParams;

extern amv_enc_handle_t AML_MultiEncInitialize(AMVEncInitParams* encParam);
extern AMVEnc_Status AML_MultiEncSetInput(amv_enc_handle_t handle,
                                AMVMultiEncFrameIO *input);
extern AMVEnc_Status AML_MultiEncNAL(amv_enc_handle_t handle,
                                unsigned char* buffer,
                                unsigned int* buf_nal_size,
                                AMVMultiEncFrameIO *Retframe);
extern AMVEnc_Status AML_MultiEncHeader(amv_enc_handle_t handle,
                                unsigned char* buffer,
                                unsigned int* buf_nal_size);
extern AMVEnc_Status AML_MultiEncRelease(amv_enc_handle_t handle);

#endif