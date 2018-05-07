#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <inttypes.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <assert.h>
#include <time.h>
#include <stdlib.h>
#include <pthread.h>

#include <va/va.h>
#include <va/va_enc_vp8.h>
#include "va_display.h"

#define MAX_XY_RESOLUTION 16364

#define TRUE 1
#define FALSE 0

#ifndef N_ELEMENTS
#define N_ELEMENTS(array) (sizeof(array)/sizeof(array[0]))
#endif

#ifndef CHECK_VASTATUS
#define CHECK_VASTATUS(va_status,func)                                  \
    if (va_status != VA_STATUS_SUCCESS) {                               \
        fprintf(stderr,"%s:%s (%d) failed,exit\n", __func__, func, __LINE__); \
        exit(1);                                                        \
    }
#endif

#define KEY_FRAME               0
#define INTER_FRAME             1

#define NUM_INPUT_SURFACES 1
#define NUM_REF_SURFACES 4
#define NUM_SURFACES (NUM_REF_SURFACES+NUM_INPUT_SURFACES)
#define SID_INPUT_PICTURE NUM_REF_SURFACES

#define NUM_BUFFERS 10

static const struct option long_opts[] = {
    {"help", no_argument, NULL, 0 },
    {"rcmode", required_argument, NULL, 1 },
    {"qp", required_argument, NULL, 2 },
    {"intra_period", required_argument, NULL, 3 },
    {"fb", required_argument, NULL, 4 },
    {"lf_level", required_argument, NULL, 6 },
//    {"opt_header", required_argument, NULL, 7},
    {"hrd_win", required_argument, NULL, 8},
    {"vbr_max", required_argument, NULL, 9},
    {"fn_num", required_argument, NULL, 10},
    {"low_power", required_argument, NULL, 11},
    {NULL, no_argument, NULL, 0 }
};


static int rc_default_mode[4] = {
    VA_RC_CQP,
    VA_RC_CBR,
    VA_RC_VBR,
    VA_RC_NONE
};

struct vp8enc_settings {
  int width;
  int height;
  int frame_rate;
  int frame_size;
  int loop_filter_level;
  int clamp_qindex_low;
  int clamp_qindex_high;
  int intra_period;
  int quantization_parameter;
  int frame_bitrate;
  int max_variable_bitrate;
  int rc_mode;
  int num_frames;
  VAEntrypoint vaapi_entry_point;
  int codedbuf_size;
  int vbr_max;
  int hrd_window;
  uint32_t error_resilient;
};

static struct vp8enc_settings settings =
  {
//HACK-Warning - everything hardcoded at the moment TODO: add command line options
    .width = 352,
    .height = 288,
    .frame_rate = 30,
    .loop_filter_level = 19,
    .clamp_qindex_low = 9,
    .clamp_qindex_high = 127,
    .intra_period = 30,
    .quantization_parameter = 60,
    .frame_bitrate = -1,
    .max_variable_bitrate = -1,
    .error_resilient = 0,
    .rc_mode = VA_RC_CQP,
    .vaapi_entry_point = VAEntrypointEncSlice,
    .num_frames = 0,
    //.num_ref_frames
 };

 struct vp8enc_vaapi_context {
     VADisplay display;
     VAProfile profile;
     VAContextID context_id;
     VAConfigID config_id;
     VAEncSequenceParameterBufferVP8 seq_param;
     VAEncPictureParameterBufferVP8 pic_param;
     VAQMatrixBufferVP8 q_matrix;
     VASurfaceID surfaces[NUM_SURFACES];
     VABufferID codedbuf_buf_id;
     VABufferID va_buffers[NUM_BUFFERS];
     int num_va_buffers;
     struct {
       VAEncMiscParameterBuffer header;
       VAEncMiscParameterHRD data;
     } hrd_param;
     struct {
       VAEncMiscParameterBuffer header;
       VAEncMiscParameterFrameRate data;
     } frame_rate_param;
     struct {
       VAEncMiscParameterBuffer header;
       VAEncMiscParameterRateControl data;
     } rate_control_param;

};

static struct vp8enc_vaapi_context vaapi_context;


/********************************************
*
* START: IVF Container Releated Stuff
*
********************************************/
static void
vp8enc_write_word(char *ptr, uint32_t value)
{
    uint8_t *tmp;

    tmp = (uint8_t *)ptr;
    *(tmp) = (value >> 0) & 0XFF;
    *(tmp + 1) = (value >> 8) & 0XFF;
}

static void
vp8enc_write_dword(char *ptr, uint32_t value)
{
    uint8_t *tmp;

    tmp = (uint8_t *)ptr;
    *(tmp) = (value >> 0) & 0XFF;
    *(tmp + 1) = (value >> 8) & 0XFF;
    *(tmp + 2) = (value >> 16) & 0XFF;
    *(tmp + 3) = (value >> 24) & 0XFF;
}

static void
vp8enc_write_frame_header(FILE *vp8_output,int data_length)
{
  char header[12];

  vp8enc_write_dword(header, (uint32_t)data_length);
  vp8enc_write_dword(header + 4, 0);
  vp8enc_write_dword(header + 8, 0);

  fwrite(header, 1, 12, vp8_output);
}

static void
vp8enc_write_ivf_header(FILE *vp8_file)
{


#define VP8_FOURCC    0x30385056

    char header[32];

    header[0] = 'D';
    header[1] = 'K';
    header[2] = 'I';
    header[3] = 'F';

    vp8enc_write_word(header + 4, 0);
    vp8enc_write_word(header + 6, 32);
    vp8enc_write_dword(header + 8, VP8_FOURCC);
    vp8enc_write_word(header + 12, settings.width);
    vp8enc_write_word(header + 14, settings.height);
    vp8enc_write_dword(header + 16, 1);
    vp8enc_write_dword(header + 20, settings.frame_rate);
    vp8enc_write_dword(header + 24, settings.num_frames);
    vp8enc_write_dword(header + 28, 0);

    fwrite(header, 1, 32, vp8_file);
}

/********************************************
*
* END: IVF Container Releated Stuff
*
********************************************/


/********************************************
*
* START: Read YUV Input File Releated Stuff
*
********************************************/
static void
vp8enc_upload_yuv_to_surface(FILE *yuv_fp, VASurfaceID surface_id, int current_frame)
{
    VAImage surface_image;
    VAStatus va_status;
    void *surface_p = NULL;
    uint8_t *y_src, *u_src, *v_src;
    uint8_t *y_dst, *u_dst, *v_dst;
    int y_size = settings.width * settings.height;
    int u_size = (settings.width >> 1) * (settings.height >> 1);
    int row, col;
    char *yuv_mmap_ptr = NULL;
    unsigned long long frame_start_pos, mmap_start;
    int mmap_size;

    frame_start_pos = (unsigned long long)current_frame * settings.frame_size;

    mmap_start = frame_start_pos & (~0xfff);
    mmap_size = (settings.frame_size + (frame_start_pos & 0xfff) + 0xfff) & (~0xfff);
    yuv_mmap_ptr = mmap(0, mmap_size, PROT_READ, MAP_SHARED,
                fileno(yuv_fp), mmap_start);

    if (yuv_mmap_ptr == MAP_FAILED) {
      fprintf(stderr,"Failed to mmap YUV file.\n");
      assert(0);
    }

    y_src = (uint8_t*)yuv_mmap_ptr+(frame_start_pos & 0xfff);
    u_src = y_src + y_size; /* UV offset for NV12 */
    v_src = y_src + y_size + u_size;


    va_status = vaDeriveImage(vaapi_context.display, surface_id, &surface_image);
    CHECK_VASTATUS(va_status,"vaDeriveImage");

    vaMapBuffer(vaapi_context.display, surface_image.buf, &surface_p);
    assert(VA_STATUS_SUCCESS == va_status);


    y_dst = surface_p + surface_image.offsets[0];
    u_dst = surface_p + surface_image.offsets[1]; /* UV offset for NV12 */
    v_dst = surface_p + surface_image.offsets[2];

    /* Y plane */
    for (row = 0; row < surface_image.height; row++) {
        memcpy(y_dst, y_src, surface_image.width);
        y_dst += surface_image.pitches[0];
        y_src += settings.width;
    }

    if (surface_image.format.fourcc == VA_FOURCC_NV12) { /* UV plane */
        for (row = 0; row < surface_image.height / 2; row++) {
            for (col = 0; col < surface_image.width / 2; col++) {
                u_dst[col * 2] = u_src[col];
                u_dst[col * 2 + 1] = v_src[col];
            }

            u_dst += surface_image.pitches[1];
            u_src += (settings.width / 2);
            v_src += (settings.width / 2);
        }
    } else if (surface_image.format.fourcc == VA_FOURCC_YV12 ||
               surface_image.format.fourcc == VA_FOURCC_I420) {
        const int U = surface_image.format.fourcc == VA_FOURCC_I420 ? 1 : 2;
        const int V = surface_image.format.fourcc == VA_FOURCC_I420 ? 2 : 1;

        u_dst = surface_p + surface_image.offsets[U];
        v_dst = surface_p + surface_image.offsets[V];

        for (row = 0; row < surface_image.height / 2; row++) {
            memcpy(u_dst, u_src, surface_image.width / 2);
            memcpy(v_dst, v_src, surface_image.width / 2);
            u_dst += surface_image.pitches[U];
            v_dst += surface_image.pitches[V];
            u_src += (settings.width / 2);
            v_src += (settings.width / 2);
        }
    }

    vaUnmapBuffer(vaapi_context.display, surface_image.buf);
    vaDestroyImage(vaapi_context.display, surface_image.image_id);

    if (yuv_mmap_ptr)
      munmap(yuv_mmap_ptr, mmap_size);
}
/********************************************
*
* END: Read YUV Input File Releated Stuff
*
********************************************/

void vp8enc_init_QMatrix(VAQMatrixBufferVP8 *qMatrix)
{

  for (size_t i = 0; i < N_ELEMENTS(qMatrix->quantization_index); i++) {
      // What is the quantization matrix exactly???
      qMatrix->quantization_index[i] = settings.quantization_parameter;
  }

  for (size_t i = 0; i < N_ELEMENTS(qMatrix->quantization_index_delta); i++) {
      qMatrix->quantization_index_delta[i] = 0;
  }

}

void vp8enc_init_SequenceParameterBuffer(VAEncSequenceParameterBufferVP8* seqParam)
{

  memset(seqParam, 0, sizeof(VAEncSequenceParameterBufferVP8));

  seqParam->frame_width = settings.width;
  seqParam->frame_height = settings.height;

  if(settings.frame_bitrate > 0)
    seqParam->bits_per_second = settings.frame_bitrate * 1000;
  else
    seqParam->bits_per_second = 0;

  //HACK-Warning: Hardcoded to I-Frames
  seqParam->kf_max_dist = 1;
  seqParam->kf_min_dist = 1;
  seqParam->intra_period = 1;

  //seqParam->intra_period = settings.intra_period;
  seqParam->error_resilient = settings.error_resilient;

  for (size_t i = 0; i < N_ELEMENTS(seqParam->reference_frames); i++)
     seqParam->reference_frames[i] = VA_INVALID_ID;
}

void vp8enc_init_PictureParameterBuffer(VAEncPictureParameterBufferVP8 *picParam)
{
  memset(picParam, 0, sizeof(VAEncPictureParameterBufferVP8));

  picParam->ref_last_frame = VA_INVALID_SURFACE;
  picParam->ref_gf_frame = VA_INVALID_SURFACE;
  picParam->ref_arf_frame = VA_INVALID_SURFACE;

  /* always show it */
  picParam->pic_flags.bits.show_frame = 1;

  for (size_t i = 0; i < N_ELEMENTS(picParam->loop_filter_level); i++) {
      picParam->loop_filter_level[i] = settings.loop_filter_level;
  }

  picParam->clamp_qindex_low = settings.clamp_qindex_low;
  picParam->clamp_qindex_high = settings.clamp_qindex_high;
}

void vp8enc_update_picture_parameter(int frame_type)
{
  vaapi_context.pic_param.reconstructed_frame = vaapi_context.surfaces[SID_INPUT_PICTURE]; //HACK-Warning surfaces
  vaapi_context.pic_param.ref_flags.bits.force_kf = 1;
}

void vp8enc_init_MiscParameterBuffers(VAEncMiscParameterHRD *hrd, VAEncMiscParameterFrameRate *frame_rate, VAEncMiscParameterRateControl *rate_control)
{
  if(hrd != NULL)
  {
    if(settings.frame_bitrate)
    {
      hrd->initial_buffer_fullness = settings.frame_bitrate * settings.hrd_window / 2;
      hrd->buffer_size = settings.frame_bitrate * settings.hrd_window;
    } else {
      hrd->initial_buffer_fullness = 0;
      hrd->buffer_size = 0;
    }
  }

  if(frame_rate != NULL)
  {
    frame_rate->framerate = settings.frame_rate;
  }

  if (rate_control != NULL)
  {
    rate_control->window_size = settings.hrd_window;
    rate_control->initial_qp = settings.quantization_parameter;
    if(settings.rc_mode == VA_RC_VBR)
    {
      rate_control->bits_per_second = settings.vbr_max * 1000;
      rate_control->target_percentage = (settings.frame_bitrate * 100) / settings.vbr_max;
    } else {
      rate_control->bits_per_second = settings.frame_bitrate * 1000;
      rate_control->target_percentage = 95;

    }
  }
}

void vp8enc_create_EncoderPipe()
{
  VAEntrypoint entrypoints[5];
  int num_entrypoints;
  int entrypoint_found;
  VAConfigAttrib conf_attrib[2];
  VASurfaceAttrib surface_attrib;
  int major_ver, minor_ver;
  VAStatus va_status;

  vaapi_context.display = va_open_display();
  va_status = vaInitialize(vaapi_context.display, &major_ver, &minor_ver);
  CHECK_VASTATUS(va_status, "vaInitialize");

  vaQueryConfigEntrypoints(vaapi_context.display, vaapi_context.profile, entrypoints,
                           &num_entrypoints);

  entrypoint_found = FALSE;
  for(int i = 0; i < num_entrypoints;i++)
  {
    if (entrypoints[i] == settings.vaapi_entry_point)
      entrypoint_found = TRUE;
  }

  if(entrypoint_found == FALSE)
  {
    fprintf(stderr,"VAEntrypoint not found!\n");
    assert(0);
  }

  /* find out the format for the render target, and rate control mode */
  conf_attrib[0].type = VAConfigAttribRTFormat;
  conf_attrib[1].type = VAConfigAttribRateControl;
  vaGetConfigAttributes(vaapi_context.display, vaapi_context.profile, settings.vaapi_entry_point,
                        &conf_attrib[0], 2);

  if ((conf_attrib[0].value & VA_RT_FORMAT_YUV420) == 0) {
      fprintf(stderr, "Input colorspace YUV420 not supported, exit\n");
      assert(0);
  }

  if ((conf_attrib[1].value & settings.rc_mode) == 0) {
      /* Can't find matched RC mode */
      fprintf(stderr, "Can't find the desired RC mode, exit\n");
      assert(0);
  }

  conf_attrib[0].value = VA_RT_FORMAT_YUV420; /* set to desired RT format */
  conf_attrib[1].value = settings.rc_mode; /* set to desired RC mode */

  va_status = vaCreateConfig(vaapi_context.display, vaapi_context.profile, settings.vaapi_entry_point,
                             &conf_attrib[0], 2,&vaapi_context.config_id);
  CHECK_VASTATUS(va_status, "vaCreateConfig");

  surface_attrib.type = VASurfaceAttribPixelFormat;
  surface_attrib.flags = VA_SURFACE_ATTRIB_SETTABLE;
  surface_attrib.value.type = VAGenericValueTypeInteger;
  surface_attrib.value.value.i = VA_FOURCC_NV12;

  // Create surface (Reference Surfaces + Input Surfaces)
  va_status = vaCreateSurfaces(
      vaapi_context.display,
      VA_RT_FORMAT_YUV420, settings.width, settings.height,
      vaapi_context.surfaces, NUM_SURFACES,
      &surface_attrib, 1
  );

  CHECK_VASTATUS(va_status, "vaCreateSurfaces");

  /* Create a context for this Encoder pipe */
  /* the surface is added to the render_target list when creating the context */
  va_status = vaCreateContext(vaapi_context.display, vaapi_context.config_id,
                              settings.width, settings.height,
                              VA_PROGRESSIVE,
                              vaapi_context.surfaces, NUM_SURFACES,
                              &vaapi_context.context_id);

  CHECK_VASTATUS(va_status, "vaCreateContext");


}

void vp8enc_destory_EncoderPipe()
{
    vaDestroySurfaces(vaapi_context.display, vaapi_context.surfaces, NUM_SURFACES);
    vaDestroyContext(vaapi_context.display,vaapi_context.context_id);
    vaDestroyConfig(vaapi_context.display,vaapi_context.config_id);
    vaTerminate(vaapi_context.display);
    va_close_display(vaapi_context.display);
}


void vp8enc_init_VaapiContext()
{
  vaapi_context.profile = VAProfileVP8Version0_3;

  vp8enc_init_SequenceParameterBuffer(&vaapi_context.seq_param);
  vp8enc_init_PictureParameterBuffer(&vaapi_context.pic_param);
  vp8enc_init_QMatrix(&vaapi_context.q_matrix);

  vaapi_context.hrd_param.header.type = VAEncMiscParameterTypeHRD;
  vaapi_context.frame_rate_param.header.type = VAEncMiscParameterTypeFrameRate;
  vaapi_context.rate_control_param.header.type = VAEncMiscParameterTypeRateControl;
  vp8enc_init_MiscParameterBuffers(&vaapi_context.hrd_param.data, &vaapi_context.frame_rate_param.data,&vaapi_context.rate_control_param.data);

  for(size_t i = 0; i < N_ELEMENTS(vaapi_context.va_buffers);i++)
    vaapi_context.va_buffers[i] = VA_INVALID_ID;
  vaapi_context.num_va_buffers = 0;

}



static int
vp8enc_store_coded_buffer(FILE *vp8_fp)
{
    VACodedBufferSegment *coded_buffer_segment;
    uint8_t *coded_mem;
    int data_length;
    VAStatus va_status;
    VASurfaceStatus surface_status;
    size_t w_items;

    va_status = vaSyncSurface(vaapi_context.display, vaapi_context.surfaces[SID_INPUT_PICTURE]); //Hack-Warning Surface
    CHECK_VASTATUS(va_status,"vaSyncSurface");

    surface_status = 0;
    va_status = vaQuerySurfaceStatus(vaapi_context.display, vaapi_context.surfaces[SID_INPUT_PICTURE], &surface_status); //Hack-Warning Surface
    CHECK_VASTATUS(va_status,"vaQuerySurfaceStatus");

    va_status = vaMapBuffer(vaapi_context.display, vaapi_context.codedbuf_buf_id, (void **)(&coded_buffer_segment));
    CHECK_VASTATUS(va_status,"vaMapBuffer");
    coded_mem = coded_buffer_segment->buf;

    if (coded_buffer_segment->status & VA_CODED_BUF_STATUS_SLICE_OVERFLOW_MASK) {
        fprintf(stderr,"CodeBuffer Size too small\n");
        vaUnmapBuffer(vaapi_context.display, vaapi_context.codedbuf_buf_id);
        assert(0);
    }

    data_length = coded_buffer_segment->size;

    vp8enc_write_frame_header(vp8_fp, data_length);

    do {
        w_items = fwrite(coded_mem, data_length, 1, vp8_fp);
    } while (w_items != 1);

    fprintf(stderr,"Bytes written %d\n",data_length); //HACK-Warning

    vaUnmapBuffer(vaapi_context.display, vaapi_context.codedbuf_buf_id);

    return 0;
}

int vp8enc_get_FileSize(FILE *fp)
{
  struct stat st;
  fstat(fileno(fp), &st);
  return st.st_size;
}

int vp8enc_prepare_buffers(int frame_type)
{
  int num_buffers = 0;
  VABufferID *va_buffers;
  VAStatus va_status;

  va_buffers = vaapi_context.va_buffers;
  /* coded buffer */
  va_status = vaCreateBuffer(vaapi_context.display,
                               vaapi_context.context_id,
                               VAEncCodedBufferType,
                               settings.codedbuf_size, 1, NULL,
                               &vaapi_context.codedbuf_buf_id);
  CHECK_VASTATUS(va_status,"vaCreateBuffer");

  /* sequence parameter set */
  va_status = vaCreateBuffer(vaapi_context.display,
                             vaapi_context.context_id,
                             VAEncSequenceParameterBufferType,
                             sizeof(vaapi_context.seq_param), 1, &vaapi_context.seq_param,
                             va_buffers);
  CHECK_VASTATUS(va_status,"vaCreateBuffer");

  va_buffers ++; num_buffers++;

  /* picture parameter set */
  vaapi_context.pic_param.coded_buf = vaapi_context.codedbuf_buf_id;

  va_status = vaCreateBuffer(vaapi_context.display,
                             vaapi_context.context_id,
                             VAEncPictureParameterBufferType,
                             sizeof(vaapi_context.pic_param), 1, &vaapi_context.pic_param,
                             va_buffers);
  CHECK_VASTATUS(va_status,"vaCreateBuffer");
  va_buffers ++; num_buffers++;



  /* hrd parameter */
  vaCreateBuffer(vaapi_context.display,
                 vaapi_context.context_id,
                 VAEncMiscParameterBufferType,
                 sizeof(vaapi_context.hrd_param), 1, &vaapi_context.hrd_param,
                 va_buffers);
  CHECK_VASTATUS(va_status, "vaCreateBuffer");

  va_buffers ++; num_buffers++;

  /* QMatrix */
  va_status = vaCreateBuffer(vaapi_context.display,
                       vaapi_context.context_id,
                       VAQMatrixBufferType,
                       sizeof(vaapi_context.q_matrix), 1, &vaapi_context.q_matrix,
                       va_buffers);
  CHECK_VASTATUS(va_status,"vaCreateBuffer");


  va_buffers ++; num_buffers++;
  /* Create the Misc FR/RC buffer under non-CQP mode */
  if (settings.rc_mode != VA_RC_CQP && frame_type == KEY_FRAME) {
    vaCreateBuffer(vaapi_context.display,
                       vaapi_context.context_id,
                       VAEncMiscParameterBufferType,
                       sizeof(vaapi_context.frame_rate_param),1,&vaapi_context.frame_rate_param,
                       va_buffers);
    CHECK_VASTATUS(va_status, "vaCreateBuffer");

    va_buffers ++; num_buffers++;

    vaCreateBuffer(vaapi_context.display,
                       vaapi_context.context_id,
                       VAEncMiscParameterBufferType,
                       sizeof(vaapi_context.rate_control_param),1,&vaapi_context.rate_control_param,
                       va_buffers);
    CHECK_VASTATUS(va_status, "vaCreateBuffer");

    va_buffers ++; num_buffers++;
  }

  vaapi_context.num_va_buffers = num_buffers;

  return num_buffers;
}


static void
vp8enc_render_picture()
{
    VAStatus va_status;

    va_status = vaBeginPicture(vaapi_context.display,
                               vaapi_context.context_id,
                               vaapi_context.surfaces[SID_INPUT_PICTURE]); //HACK-Warning
    CHECK_VASTATUS(va_status,"vaBeginPicture");


    va_status = vaRenderPicture(vaapi_context.display,
                                vaapi_context.context_id,
                                vaapi_context.va_buffers,
                                vaapi_context.num_va_buffers);
    CHECK_VASTATUS(va_status,"vaRenderPicture");

    va_status = vaEndPicture(vaapi_context.display, vaapi_context.context_id);
    CHECK_VASTATUS(va_status,"vaEndPicture");

}

void vp8enc_destroy_buffers()
{
  VAStatus va_status;

  for(int i = 0; i < vaapi_context.num_va_buffers; i++) {
    if (vaapi_context.va_buffers[i] != VA_INVALID_ID) {
      va_status = vaDestroyBuffer(vaapi_context.display, vaapi_context.va_buffers[i]);
      CHECK_VASTATUS(va_status,"vaDestroyBuffer");
      vaapi_context.va_buffers[i] = VA_INVALID_ID;
    }
  }

  if (vaapi_context.codedbuf_buf_id != VA_INVALID_ID) {
    va_status = vaDestroyBuffer(vaapi_context.display, vaapi_context.codedbuf_buf_id);
    CHECK_VASTATUS(va_status,"vaDestroyBuffer");
    vaapi_context.codedbuf_buf_id = VA_INVALID_ID;
  }

}
void vp8enc_show_help ()
{
  printf("Usage: vp8enc_go <width> <height> <input_yuvfile> <output_vp8> additional_option\n");
  printf("output_vp8 should use *.ivf\n");
  printf("The additional option is listed\n");
  printf("-f <frame rate> \n");
  printf("--intra_period <key_frame interval>\n");
  printf("--qp <quantization parameter> \n");
  printf("--rcmode <rate control mode> 0: CQP, 1: CBR, 2: VBR\n");
  printf("--fb <bitrate> (kbps unit)\n");
  printf("--lf_level <loop filter level>  [0-63]\n");
  printf("--hrd_win <num>  [1000-8000]\n");
  printf("--vbr_max <num> (kbps unit. It should be greater than fb)\n");
  //printf("--opt_header \n  write the uncompressed header manually. without this, the driver will add those headers by itself\n");
  printf("--fn_num <num>\n  how many frames to be encoded\n");
  printf("--low_power <num> 0: Normal mode, 1: Low power mode, others: auto mode\n");
}

void parameter_check(const char *param, int val, int min, int max)
{
  if(val < min || val > max)
  {
    fprintf(stderr,"%s out of range (%d..%d) - %d\n",param,min,max,val);
    assert(0);
  }
}

void parse_options(int ac,char *av[])
{
  int c, long_index, tmp_input;
  while (1) {
      c = getopt_long_only(ac,av,"hf:?",long_opts,&long_index);

      if (c == -1)
          break;

      switch(c) {
      case 'h':
      case 0:
          vp8enc_show_help();
          exit(0);
          break;
      case 'f':
          settings.frame_rate = atoi(optarg);
          break;
      case 1:
          tmp_input = atoi(optarg);
          if (tmp_input > 3 || tmp_input < 0)
              tmp_input = 0;
          settings.rc_mode = rc_default_mode[tmp_input];
          break;
      case 2:
          tmp_input = atoi(optarg);
          if (tmp_input < 0 || tmp_input > 255)
              tmp_input = 60;
          settings.quantization_parameter = tmp_input;
          break;
      case 3:
          tmp_input = atoi(optarg);
          if (tmp_input < 0)
              tmp_input = 30;
          settings.intra_period = tmp_input;
          break;
      case 4:
          tmp_input = atoi(optarg);
          settings.frame_bitrate = tmp_input;
          break;
      case 6:
          tmp_input = atoi(optarg);
          if (tmp_input < 0 || tmp_input > 63)
              tmp_input = 10;
          settings.loop_filter_level = tmp_input;
          break;
/*      case 7:
          tmp_input = atoi(optarg);
          if (tmp_input)
              tmp_input = 1;
          settings.opt_header = TRUE;
          break;*/
      case 8:
          tmp_input = atoi(optarg);
          if (tmp_input > 8000 || tmp_input < 1000)
              tmp_input = 1500;
          settings.hrd_window = tmp_input;
          break;
      case 9:
          tmp_input = atoi(optarg);
          if (tmp_input < 0)
              tmp_input = 20000;
          else if (tmp_input > 100000)
              tmp_input = 100000;
          settings.max_variable_bitrate = tmp_input;
          break;
      case 10:
          tmp_input = atoi(optarg);
          settings.num_frames = tmp_input;
          break;
      case 11:
          tmp_input = atoi(optarg);
          if (tmp_input == 1)
            settings.vaapi_entry_point = VAEntrypointEncSliceLP;
          else
            settings.vaapi_entry_point = VAEntrypointEncSlice;
          break;

      default:
          vp8enc_show_help();
          exit(0);
          break;
    }
  }
}


int main(int argc, char *argv[])
{
  int current_frame;
  FILE *fp_vp8_output, *fp_yuv_input;

  if(argc < 5) {
      vp8enc_show_help();
      exit(0);
  }

  fp_vp8_output = fopen(argv[4],"wb");
  if(fp_vp8_output == NULL)
  {
    fprintf(stderr,"Couldn't open output file.\n");
    return -1;
  }

  fp_yuv_input = fopen(argv[3],"rb");
  if(fp_vp8_output == NULL)
  {
    fprintf(stderr,"Couldn't open input file.\n");
    return -1;
  }

  settings.width = atoi(argv[1]);
  parameter_check("Width", settings.width, 1, MAX_XY_RESOLUTION);

  settings.height = atoi(argv[2]);
  parameter_check("Height", settings.height, 1, MAX_XY_RESOLUTION);


  parse_options(argc-4, &argv[4]);

  settings.frame_size = settings.width * settings.height * 3 / 2; //NV12 Colorspace - For a 2x2 group of pixels, you have 4 Y samples and 1 U and 1 V sample.
  if(!settings.num_frames)
    settings.num_frames = vp8enc_get_FileSize(fp_yuv_input)/settings.frame_size;
  settings.codedbuf_size = settings.width * settings.height; //just a generous test


  fprintf(stderr,"num_frames: %d\n",settings.num_frames);

  vp8enc_init_VaapiContext();
  vp8enc_create_EncoderPipe();

  vp8enc_write_ivf_header(fp_vp8_output);

  current_frame = 0;
  while (current_frame < settings.num_frames)
  {
    vp8enc_upload_yuv_to_surface(fp_yuv_input, vaapi_context.surfaces[SID_INPUT_PICTURE],current_frame);
    vp8enc_update_picture_parameter(KEY_FRAME);
    vp8enc_prepare_buffers(KEY_FRAME); //HACK-Warning
    vp8enc_render_picture();

    vp8enc_store_coded_buffer(fp_vp8_output);  // HACK-Warning fixed to Keyframe
    vp8enc_destroy_buffers();

    current_frame++;
  }

  vp8enc_destory_EncoderPipe();
  fclose(fp_vp8_output);
  fclose(fp_yuv_input);

}
