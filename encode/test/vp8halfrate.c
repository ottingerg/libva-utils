#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

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
  char frame_buffer[1000000];
  int skip;

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

  skip = 0;
  while(fread(&frame,sizeof(struct frame_header),1,stdin) == 1)
  {
    //fprintf(stderr,"frame size: %d\n",frame.frame_size);
    fread(&frame_buffer, frame.frame_size,1,stdin);
    if(!skip)
    {
      frame.timestamp /= 2;
      fprintf(stderr,"Timestamp: %ld File pos: %lx\n",frame.timestamp, ftell(stdout));
      fwrite(&frame, sizeof(struct frame_header), 1, stdout);
      fwrite(&frame_buffer, frame.frame_size, 1, stdout );
      skip = 1;
    } else {
      skip = 0;
    }

  }
}
