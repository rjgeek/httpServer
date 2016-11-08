#include "http_header.h"

#include <time.h>
#include <stdio.h>
#include <string.h>


/* get the time on server,
   return: the ascii string of time , NULL on error
   argument: time_buf the buffer to store time_string
*/

char *get_date_str(char *time_buf)
{
    time_t    now_sec;
    struct tm    *time_now;
    if(    time(&now_sec) == -1)
    {
        perror("time() in get_time.c");
        return NULL;
    }
    if((time_now = gmtime(&now_sec)) == NULL)
    {
        perror("localtime in get_time.c");
        return NULL;
    }
    char *str_ptr = NULL;
    if((str_ptr = asctime(time_now)) == NULL)
    {
        perror("asctime in get_time.c");
        return NULL;
    }
    strcat(time_buf, str_ptr);
    return time_buf;
}

/*
 * get_filetype - derive file type from file name
 * Taken from Tiny server
 */
void get_content_type(char *filename, char *filetype)
{
    if (strstr(filename, ".html")){
    	strcpy(filetype, "text/html");
    }else if (strstr(filename, ".js")){
    	strcpy(filetype, "text/javascript");
    }else if (strstr(filename, ".xml")){
    	strcpy(filetype, "text/xml");
    }else if (strstr(filename, ".json")){
    	strcpy(filetype, "text/json");
    }else if (strstr(filename, ".gif")){
    	strcpy(filetype, "image/gif");
    }else if (strstr(filename, ".png")){
    	strcpy(filetype, "image/png");
    }else if (strstr(filename, ".jpg")){
    	strcpy(filetype, "image/jpeg");
    }else if (strstr(filename, ".css")){
    	strcpy(filetype, "text/css");
    }else{
    	strcpy(filetype, "text/plain");
    }
}
