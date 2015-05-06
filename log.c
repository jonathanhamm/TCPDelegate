#include "general.h"
#include "log.h"
#include <stdarg.h>
#include <string.h>

#define MAX_QUEUE_SIZE 64
#define TQ(p) ((p) % MAX_QUEUE_SIZE)

typedef enum task_e task_e;
typedef struct log_s log_s;
typedef struct task_node_s task_node_s;

enum task_e{
    TASK_DEBUG,
    TASK_INFO,
    TASK_WARN,
    TASK_ERROR
};

struct log_s {
    volatile bool running;
    pthread_mutex_t status_lock;
    FILE *files[4];
    pthread_t thread;
};

struct task_node_s {
    task_e task;
    buf_s str;
};

#define LOG_DIRECTORY "./logs"
#define LOG_DEBUG LOG_DIRECTORY "/debug.log"
#define LOG_INFO LOG_DIRECTORY "/info.log"
#define LOG_WARN LOG_DIRECTORY "/warn.log"
#define LOG_ERROR LOG_DIRECTORY "/error.log"

static log_s logger = {.status_lock = PTHREAD_MUTEX_INITIALIZER};
static unsigned qfpos, qbpos;
static task_node_s logqueue[MAX_QUEUE_SIZE];
static pthread_mutex_t queuelock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t queue_fullcond = PTHREAD_COND_INITIALIZER;
static pthread_cond_t queue_emptycond = PTHREAD_COND_INITIALIZER;

static void *log_loop(void *arg);
static void log_enqueue(buf_s str, task_e task);
static void log_task_consume(void);
static buf_s mk_logstr(const char *type, const char *format, va_list args);
static void insert_time(buf_s *b);
static unsigned overflow;

void log_init(void)
{
    assert(!logger.running);
    logger.running = true;
    
#ifndef USE_STDOUT
    logger.files[TASK_DEBUG] = fopen(LOG_DEBUG, "w");
    if(!logger.files[TASK_DEBUG]) {
        perror("Failed to open debug log file");
    }
    
    logger.files[TASK_INFO] = fopen(LOG_INFO, "w");
    if(!logger.files[TASK_INFO]) {
        perror("Failed to open info log file");
    }
    
    logger.files[TASK_WARN] = fopen(LOG_WARN, "w");
    if(!logger.files[TASK_WARN]) {
        perror("Failed to open warn log file");
    }
    
    logger.files[TASK_ERROR] = fopen(LOG_ERROR, "w");
    if(!logger.files[TASK_ERROR]) {
        perror("Failed to open error log file");
    }
#else
    logger.files[TASK_DEBUG] = stdout;
    logger.files[TASK_INFO] = stdout;
    logger.files[TASK_WARN] = stdout;
    logger.files[TASK_ERROR] = stdout;
#endif
    
    int status = pthread_create(&logger.thread, NULL, log_loop, NULL);
    if(status) {
        perror("Error creating logger thread");
        exit(EXIT_FAILURE);
    }
}

void log_debug(const char *format, ...)
{
    assert(logger.running);
    buf_s buf;
    
    va_list args;
    va_start(args, format);
    buf = mk_logstr("*DEBUG*", format, args);
    va_end(args);
    
    log_enqueue(buf, TASK_DEBUG);
}

void log_info(const char *format, ...)
{
    assert(logger.running);
    buf_s buf;
    
    va_list args;
    va_start(args, format);
    buf = mk_logstr("*INFO*", format, args);
    va_end(args);
    
    log_enqueue(buf, TASK_INFO);

}

void log_warn(const char *format, ...)
{
    assert(logger.running);
    buf_s buf;
    
    va_list args;
    va_start(args, format);
    buf = mk_logstr("*WARN*", format, args);
    va_end(args);
    
    log_enqueue(buf, TASK_WARN);
}

void log_error(const char *format, ...)
{
    assert(logger.running);
    buf_s buf;
    
    va_list args;
    va_start(args, format);
    buf = mk_logstr("*ERROR*", format, args);
    va_end(args);
    
    log_enqueue(buf, TASK_ERROR);
}

void log_flush(void)
{
    task_node_s *qptr;
    
    do {
        pthread_mutex_lock(&queuelock);
        while(TQ(qfpos) != TQ(qbpos)) {
            qfpos = TQ(qfpos + 1);
            qptr = &logqueue[qfpos];
            fputs(qptr->str.data, logger.files[qptr->task]);
            buf_destroy(&qptr->str);
        }
        pthread_cond_broadcast(&queue_fullcond);
        pthread_cond_broadcast(&queue_emptycond);
        pthread_mutex_unlock(&queuelock);
        pthread_yield_np();
    } while(overflow);
}

void log_deinit(void)
{
    assert(logger.running);
    
    log_flush();
    
    logger.running = false;
    
    pthread_mutex_lock(&queuelock);
    pthread_cond_signal(&queue_emptycond);
    pthread_mutex_unlock(&queuelock);
    
    pthread_join(logger.thread, NULL);

    
    if(logger.files[TASK_DEBUG]) {
        fclose(logger.files[TASK_DEBUG]);
    }
    if(logger.files[TASK_INFO]) {
        fclose(logger.files[TASK_INFO]);
    }
    if(logger.files[TASK_WARN]) {
        fclose(logger.files[TASK_WARN]);
    }
    if(logger.files[TASK_ERROR]) {
        fclose(logger.files[TASK_ERROR]);
    }
    
    pthread_mutex_destroy(&logger.status_lock);
    pthread_mutex_destroy(&queuelock);
    pthread_cond_destroy(&queue_fullcond);
    pthread_cond_destroy(&queue_emptycond);

}

void *log_loop(void *arg)
{
    while(logger.running) {
        log_task_consume();
    }
    
    pthread_exit(NULL);
}

void log_enqueue(buf_s str, task_e task)
{
    task_node_s *qptr;

    pthread_mutex_lock(&queuelock);
    if(TQ(qbpos + 1) == TQ(qfpos)) {
        overflow++;
        while(TQ(qbpos + 1) == TQ(qfpos)) {
            pthread_cond_wait(&queue_fullcond, &queuelock);
        }
        overflow--;
    }
    qptr = &logqueue[qbpos];
    *qptr = (task_node_s) {
        .str = str,
        .task = task
    };
    qbpos = TQ(qbpos + 1);
    pthread_cond_signal(&queue_emptycond);
    pthread_mutex_unlock(&queuelock);
}

void log_task_consume(void)
{
    task_node_s *qptr;
    
    pthread_mutex_lock(&queuelock);
    while(TQ(qbpos) == TQ(qfpos) && logger.running) {
        pthread_cond_wait(&queue_emptycond, &queuelock);
    }
    if(logger.running) {
        qptr = &logqueue[TQ(qfpos)];
        fputs(qptr->str.data, logger.files[qptr->task]);
        fflush(logger.files[qptr->task]);
        buf_destroy(&qptr->str);
        qfpos = TQ(qfpos + 1);
        pthread_cond_signal(&queue_fullcond);
    }
    pthread_mutex_unlock(&queuelock);
}

buf_s mk_logstr(const char *type, const char *format, va_list args)
{
    enum {INIT_STR_SIZE = 64};
    const char *ptr = format;
    buf_s buf;
    union {
        char *s;
        int d;
        unsigned u;
        long l;
        unsigned long lu;
        double f;
    } val;
    
    buf_init(&buf);
    do {
        buf_addc(&buf, *type);
    } while(*++type);
    buf_addc(&buf, ' ');
    insert_time(&buf);
    while(*ptr) {
        if(*ptr == '%') {
            switch(*(ptr + 1)) {
                case 's':
                    val.s = va_arg(args, char *);
                    do {
                        buf_addc(&buf, *val.s);
                    } while(*++val.s);
                    ptr += 2;
                    break;
                case 'd':
                    val.d = va_arg(args, int);
                    buf_addlong(&buf, val.d);
                    ptr += 2;
                    break;
                case 'u':
                    val.u = va_arg(args, unsigned);
                    buf_addunsigned(&buf, val.u);
                    ptr += 2;
                    break;
                case 'l':
                    switch(*(ptr + 2)) {
                        case 'd':
                            val.l = va_arg(args, long);
                            buf_addlong(&buf, val.l);
                            break;
                        case 'u':
                            val.lu = va_arg(args, unsigned long);
                            buf_addunsigned(&buf, val.lu);
                            break;
                        default:
                            buf_addc(&buf, '%');
                            buf_addc(&buf, 'l');
                            buf_addc(&buf, *(ptr + 2));
                            break;
                    }
                    ptr += 3;
                    break;
                case 'f':
                    val.f = va_arg(args, double);
                    buf_addddouble(&buf, val.f);
                    ptr += 2;
                    break;
                case '%':
                    buf_addc(&buf, '%');
                    ptr += 2;
                    break;
                default:
                    buf_addc(&buf, '%');
                    buf_addc(&buf, *(ptr + 1));
                    ptr += 2;
                    break;
            }
        }
        else {
            buf_addc(&buf, *ptr);
            ptr++;
        }
    }
    buf_addc(&buf, '\n');
    buf_addc(&buf, '\0');
    return buf;
}

void insert_time(buf_s *b)
{
    enum {MIN_TIMEBUF_SIZE = 26};
    time_t t;
    char timebuf[MIN_TIMEBUF_SIZE], *tptr = timebuf;
    
    time(&t);
    ctime_r(&t, timebuf);
    
    do {
        buf_addc(b, *tptr);
    } while(*++tptr);
    b->data[b->size - 1] = ':';
    buf_addc(b, ' ');
    buf_addc(b, ' ');
}

