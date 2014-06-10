#ifndef __MISC_H__
#define	__MISC_H__


#define TIME_FILE 			"/tmp/time"

#define Trace(...);    //do { printf("%s:%u:TRACE: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf(" [%s]\n", __FUNCTION__); } while (0);
#define Debug(...);    //do { printf("%s:%u:DEBUG: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf(" [%s]\n", __FUNCTION__); } while (0);
#define Info(...);		do { printf("%s:%u:INFO: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf(" [%s]\n", __FUNCTION__); } while (0);
#define Error(...);   	do { printf("%s:%u:ERROR: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf(" [%s]\n", __FUNCTION__); } while (0);

//-------------------- snio.c ---------------------------------------
void printhex(const void *buf, const int len);
void dump(const char *msg, const void *buf, int len);

#endif /* __MISC_H__ */

