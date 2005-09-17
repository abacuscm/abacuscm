#include <syslog.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <openssl/err.h>
#include <openssl/ssl.h>

#include "config.h"
#include "logger.h"

#define MAX_BUFFER_SIZE		1024

using namespace std;

static void (*actual_log)(int , const char*, va_list ap) = 0;
static const char *priostrings[] = {
	"EMERG",
	"ALERT",
	"CRIT",
	"ERR",
	"WARNING",
	"NOTICE",
	"INFO",
	"DEBUG"};

FILE* flog = stderr;

static void log_file(int prio, const char* format, va_list ap) {
	char *buffer = NULL;
	const char *priostring = "?!?";
	time_t t = time(NULL);
	char time_buf[26];
	
	if(0 <= prio && prio < (int)(sizeof(priostrings) / sizeof(*priostrings)))
		priostring = priostrings[prio];

	// The designers of this specific function must have been drunk,
	// this is needed to get rid of the trailing \n char.
	asctime_r(localtime(&t), time_buf);
	time_buf[strlen(time_buf) - 1] = 0;

	asprintf(&buffer, "%s [%s]: %s\n", time_buf, priostring, format);

	vfprintf(flog, buffer, ap);
	free(buffer);
}

void log(int priority, const char* format, ...) {
	if(!actual_log) {
		string log_method = Config::getConfig()["logger"]["method"];

		if(log_method == "file") {
			actual_log = log_file;
			
			string filename = Config::getConfig()["logger"]["filename"];

			if(filename == "stderr") {
				flog = stderr;
			} else if(filename == "stdout") {
				flog = stdout;
			} else {
				FILE *fp = fopen(filename.c_str(), "a");
				if(!fp) {
					fp = stderr;
					log(LOG_ERR, "Unable to open %s, using stderr instead", filename.c_str());
				}
				flog = fp;
			}
		} else {
			openlog("abacusd", LOG_PID, LOG_DAEMON);
			actual_log = vsyslog;
			if(log_method != "syslog")
				log(LOG_WARNING, "Unknown logging method %s, defaulting to syslog", log_method.c_str());
		}
	}

	va_list ap;
	va_start(ap, format);
	actual_log(priority, format, ap);
	va_end(ap);
}

void (*sigsegv_log)(int priority, const char* format, ...) = log;

void real_lerror(const char*fname, int line_num, const char* prefix) {
	log(LOG_ERR, "%s (%s:%d): %s", prefix, fname, line_num, strerror(errno));
}

void real_log_ssl_errors(const char* fname, int line_num, const char* prefix) {
	unsigned err = ERR_get_error();
	if(!err) {
		log(LOG_ERR, "%s (%s:%d): Unknown OpenSSL error!  This should _never_ happen!", prefix, fname, line_num);
	} else 	while(err) {
		log(LOG_ERR, "%s (%s:%d): %s (0x%x)", prefix, fname, line_num, ERR_reason_error_string(err), err);
		err = ERR_get_error();
	}
}

void log_buffer(int priority, const char* name, const unsigned char* buffer, int size) {
	int bfr_pos = 0;
	char bfr[MAX_BUFFER_SIZE];

	if(size <= 0)
		return;

	// each element in buffer takes 3 bytes, we also print the "name" of the
	// buffer and then there is the ': ' between the name and the buffer itself.
	if(size * 3 + strlen(name) + 2 > MAX_BUFFER_SIZE) {
		log(LOG_ERR, "Dumping buffer '%s' will result in a buffer overflow.",
				name);
		return;
	}

	bfr_pos += sprintf(&bfr[bfr_pos], "%s: %02x", name, *buffer++);
	while(--size)
		bfr_pos += sprintf(&bfr[bfr_pos], ":%02x", *buffer++);

	log(priority, "%s", bfr);
}
