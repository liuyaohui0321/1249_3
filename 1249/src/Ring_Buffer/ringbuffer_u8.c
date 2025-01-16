#include "ringbuffer_u8.h"

/**
 * @file
 * Implementation of ring buffer functions.
 */

void u8_ring_buffer_init(u8_ring_buffer_t *buffer, char *buf, size_t buf_size) {
  RING_BUFFER_ASSERT(RING_BUFFER_IS_POWER_OF_TWO(buf_size) == 1);
  buffer->buffer = buf;
  buffer->buffer_mask = buf_size - 1;
  buffer->tail_index = 0;
  buffer->head_index = 0;
}

void u8_ring_buffer_queue(u8_ring_buffer_t *buffer, char data) {
  /* Is buffer full */
  if(u8_ring_buffer_is_full(buffer)) {
    /* Is going to overwrite the oldest byte */
    /* Increase tail index */
    buffer->tail_index = ((buffer->tail_index + 1) & RING_BUFFER_MASK(buffer));
  }

  /* Place data in buffer */
  buffer->buffer[buffer->head_index] = data;
  buffer->head_index = ((buffer->head_index + 1) & RING_BUFFER_MASK(buffer));
}

void u8_ring_buffer_queue_arr(u8_ring_buffer_t *buffer, const char *data, ring_buffer_size_t size) {
  /* Add bytes; one by one */
  ring_buffer_size_t i;
  for(i = 0; i < size; i++) {
    u8_ring_buffer_queue(buffer, data[i]);
  }
}

uint8_t u8_ring_buffer_dequeue(u8_ring_buffer_t *buffer, char *data) {
  if(u8_ring_buffer_is_empty(buffer)) {
    /* No items */
    return 0;
  }
  
  *data = buffer->buffer[buffer->tail_index];
  buffer->tail_index = ((buffer->tail_index + 1) & RING_BUFFER_MASK(buffer));
  return 1;
}

ring_buffer_size_t u8_ring_buffer_dequeue_arr(u8_ring_buffer_t *buffer, char *data, ring_buffer_size_t len) {
  if(u8_ring_buffer_is_empty(buffer)) {
    /* No items */
    return 0;
  }

  char *data_ptr = data;
  ring_buffer_size_t cnt = 0;
  while((cnt < len) && u8_ring_buffer_dequeue(buffer, data_ptr)) {
    cnt++;
    data_ptr++;
  }
  return cnt;
}

uint8_t u8_ring_buffer_peek(u8_ring_buffer_t *buffer, char *data, ring_buffer_size_t index) {
  if(index >= u8_ring_buffer_num_items(buffer)) {
    /* No items at index */
    return 0;
  }
  
  /* Add index to pointer */
  ring_buffer_size_t data_index = ((buffer->tail_index + index) & RING_BUFFER_MASK(buffer));
  *data = buffer->buffer[data_index];
  return 1;
}

extern inline uint8_t u8_ring_buffer_is_empty(u8_ring_buffer_t *buffer);
extern inline uint8_t u8_ring_buffer_is_full(u8_ring_buffer_t *buffer);
extern inline ring_buffer_size_t u8_ring_buffer_num_items(u8_ring_buffer_t *buffer);

