#ifndef _GMTOFFSET_H_
#define _GMTOFFSET_H_

//using timezonedb api to recive a .json file with gmtoffset and 
//daylight saving time. returns -1 for failed gmtoffset
int get_GMT_offset(const char * HTTPheader, int* gmtOffset);

#endif