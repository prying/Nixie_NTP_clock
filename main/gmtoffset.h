#ifndef _GMTOFFSET_H_
#define _GMTOFFSET_H_

//using timezonedb api to recive a .json file with gmtoffset and 
//daylight saving time. 
int get_GMT_offset(const char * apiKey);

#endif