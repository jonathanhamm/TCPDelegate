#ifndef __TCPDelegate__log__
#define __TCPDelegate__log__

#define USE_STDOUT

extern void log_init(void);

extern void log_debug(const char *format, ...);
extern void log_info(const char *format, ...);
extern void log_warn(const char *format, ...);
extern void log_error(const char *format, ...);

extern void log_flush(void);

extern void log_deinit(void);

#endif /* defined(__TCPDelegate__log__) */
