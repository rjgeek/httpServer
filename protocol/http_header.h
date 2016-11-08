/* server.h */
#ifndef __HTTP_HEADER__
#define __HTTP_HEADER__

#define TIME_BUFFER_SIZE    40    /* buffer size of time_buffer */

char *get_date_str(char *time_buf);
void  get_content_type(char *filename, char *filetype);


#endif
