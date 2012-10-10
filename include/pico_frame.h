#ifndef _INCLUDE_PICO_FRAME
#define _INCLUDE_PICO_FRAME


#define PICO_FRAME_FLAG_BCAST (0x0001)
#define IS_BCAST(f) ((f->flags & PICO_FRAME_FLAG_BCAST) == PICO_FRAME_FLAG_BCAST)

struct pico_socket;


struct pico_frame {

  /* Connector for queues */
  struct pico_frame *next;

  /* Start of the whole buffer, total frame length. */
  unsigned char *buffer;
  uint32_t      buffer_len;

  /* For outgoing packets: this is the meaningful buffer. */
  unsigned char *start;
  uint32_t      len;

  /* Pointer to usage counter */
  uint32_t *usage_count;

  /* Pointer to protocol headers */
  uint8_t *datalink_hdr;
  int  datalink_len;
  uint8_t *net_hdr;
  int net_len;
  uint8_t *transport_hdr;
  int transport_len;
  uint8_t *app_hdr;
  int app_len;

  /* Pointer to the phisical device this packet belongs to.
   * Should be valid in both routing directions
   */
  struct pico_device *dev;


  /* Failures due to bad datalink addressing. */
  uint16_t failure_count;

  /* Protocol over IP */
  uint8_t  proto;

  /* PICO_FRAME_FLAG_* */
  uint8_t flags;

  /* Pointer to payload */
  unsigned char *payload;
  int payload_len;

  /* Pointer to socket */
  struct pico_socket *sock;
};

/** frame alloc/dealloc/copy **/
void pico_frame_discard(struct pico_frame *f);
struct pico_frame *pico_frame_copy(struct pico_frame *f);
struct pico_frame *pico_frame_alloc(int size);
uint16_t pico_checksum(void *inbuf, int len);

#endif
