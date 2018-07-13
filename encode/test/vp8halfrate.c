#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#define VLC_PLAYER_HACK

struct ivf_header {
  char signature[4];
  uint16_t version;
  uint16_t length;
  char fourcc[4];
  uint16_t width;
  uint16_t height;
  uint32_t frame_rate;
  uint32_t time_scale;
  uint32_t num_frames;
  uint32_t reserved;
} __attribute__((packed));

struct frame_header {
  uint32_t frame_size;
  uint64_t timestamp;
} __attribute__((packed));

void main()
{
  struct ivf_header ivf;
  struct frame_header frame;
  char *frame_buffer;
  size_t fb_size = 1000000;
  uint64_t timestamp;
  int skip;

  frame_buffer = malloc(fb_size);

  fprintf(stderr,"sizeof frame_header: %ld sizeof ivf header %ld\n",sizeof(struct frame_header),sizeof(struct ivf_header));
  fread(&ivf, sizeof(struct ivf_header),1,stdin);
  fprintf(stderr,"num frames: %d\n",ivf.num_frames);
  fprintf(stderr,"frame_rate: %d, time_scale: %d\n", ivf.frame_rate, ivf.time_scale);
  if(ivf.frame_rate % 2 == 0) {
    ivf.frame_rate /= 2;
  } else {
    ivf.time_scale *= 2;
  }
  ivf.num_frames /= 2;

#ifdef VLC_PLAYER_HACK //VLC doens't accept fractional timebase 
  while(ivf.frame_rate % ivf.time_scale)
    ivf.frame_rate ++;
#endif

  fwrite(&ivf, sizeof(struct ivf_header),1,stdout);

  timestamp = 0;
  skip = 0;
  while(fread(&frame,sizeof(struct frame_header),1,stdin) == 1)
  {
    if(fb_size < frame.frame_size)
    {
      fb_size = frame.frame_size;
      frame_buffer = realloc(frame_buffer, fb_size);
    }

    //fprintf(stderr,"frame size: %d\n",frame.frame_size);
    fread(frame_buffer, frame.frame_size,1,stdin);
    if(!skip)
    {
      frame.timestamp = timestamp++; //rewrite timestamp
      fprintf(stderr,"Timestamp: %ld File pos: %lx\n",frame.timestamp, ftell(stdout));
      fwrite(&frame, sizeof(struct frame_header), 1, stdout);
      fwrite(frame_buffer, frame.frame_size, 1, stdout );
      skip = 1;
    } else {
      skip = 0;
    }

  }
  free(frame_buffer);
}

