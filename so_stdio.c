/* 
Naumencu Mihai
336 CA
*/

#include "so_stdio.h"

// Default buffer for SO_FILE
#define BUFSIZE  4096

// Permissions for creat
#define PERM    0644

// Last operation types
#define NONE    0
#define READ    1
#define WRITE   2

// SO_FILE structure
typedef struct _so_file {
    int fd;                     // file descriptor
    char buf[BUFSIZE];          // buffer
    int flags;                  // open flags
    int buf_size;              	// buffer size
    int buf_offset;            	// buffer offset
	long cursor;                // file cursor
    int last_operation;         // last operation type (0-none, 1-read, 2-write)
	int eof;                    // end of file flag
	int errors;                 // error counter
} SO_FILE;

// --------- Helper functions ---------

void prepare_buffer(SO_FILE *stream, char c) {
	memset(stream->buf, 0, BUFSIZE);
	stream->buf_offset = 0;
	if (c == 'r')
		stream->buf_size = 0;
 	else if (c == 'w')
		stream->buf_size = BUFSIZE;
}

void incrememnt_pos(SO_FILE *stream, int inc) {
	stream->buf_offset += inc;
	stream->cursor += inc;
}

void set_last_operation(SO_FILE *stream, char c) {
	if (c == 'n')
		stream->last_operation = NONE;
	else if (c == 'w')
		stream->last_operation = WRITE;
	else if (c == 'r')
		stream->last_operation = READ;
}

void init_stream(SO_FILE *stream) {
    stream->cursor = 0;
    stream->eof = 0;
    stream->errors = 0;
}

void end_or_error_reached(SO_FILE *stream) {
	stream->eof += 1;
	stream->errors += 1;
}

// -------------------------------------

// Opens SO_FILE 
SO_FILE *so_fopen(const char *pathname, const char *mode) {
    SO_FILE *stream = (SO_FILE *)calloc(1, sizeof(SO_FILE));
    
    if (!stream) return NULL; // error

    // set flags according to specified opening mode
    if (!strcmp(mode, "r")) {
        stream->flags = O_RDONLY;
    } else if (!strcmp(mode, "r+")) {
        stream->flags = O_RDWR | O_CREAT;
    } else if (!strcmp(mode, "w")) {
        stream->flags = O_WRONLY | O_CREAT | O_TRUNC;
    } else if (!strcmp(mode, "w+")) {
        stream->flags = O_RDWR | O_CREAT | O_TRUNC;
    } else if (!strcmp(mode, "a")) {
        stream->flags = O_WRONLY | O_APPEND | O_CREAT;
    } else if (!strcmp(mode, "a+")) {
        stream->flags = O_RDWR | O_APPEND | O_CREAT;
    } else {
        free(stream);
        return NULL; // error
    }

	// PERM - permissions for creat (0644)
    stream->fd = open(pathname, stream->flags, PERM);

    if (stream->fd < 0) {
        free(stream);
        return NULL; // error
    }

	prepare_buffer(stream, 'r');
	set_last_operation(stream, 'n');
	init_stream(stream);

    return stream;
}

// Closes SO_FILE
int so_fclose(SO_FILE *stream) {
    int rc1, rc2;
	int err = 0;

	rc1 = so_fflush(stream);
    rc2 = close(stream->fd);

    if (rc1 < 0 || rc2 < 0) err -= 1;

	if (stream) free(stream);

    return err;
}

// Return file descriptor
int so_fileno(SO_FILE *stream) {
    return stream->fd;
}

// 0 if eof not reached
int so_feof(SO_FILE *stream) {
    return stream->eof;
}

// returns number of errors, 0 if none
int so_ferror(SO_FILE *stream) {
    return stream->errors;
}

// returns cursor, -1 if error occured
long so_ftell(SO_FILE *stream) {
	if (stream->cursor >= 0)
		return stream->cursor;
	else 
		return SO_EOF;
}

// flushes the buffer of given SO_FILE
int so_fflush(SO_FILE *stream) {
    // exits if last operation was not write (this is not an error)
    if (stream->last_operation != WRITE) return 0;

	int rc;

	for (int i = 0; i < stream->buf_offset; i += rc) {
		rc = write(stream->fd, stream->buf + i, stream->buf_offset - i);
		if (rc == SO_EOF) return rc;
	}

	prepare_buffer(stream, 'r');

	return 0;
}


int so_fgetc(SO_FILE *stream) {
	int rc;
	unsigned char ch;

	rc = so_fflush(stream);
	if (rc == SO_EOF) {
		end_or_error_reached(stream);
		return rc;
	}

	// loads buffer
	if (stream->buf_offset == stream->buf_size) {
		rc = read(stream->fd, stream->buf, BUFSIZE);
		if (rc == 0) {
			end_or_error_reached(stream);
			return SO_EOF;
		}
		stream->buf_offset = 0;
		stream->buf_size = rc;
	}

	ch = stream->buf[stream->buf_offset];

	//move cursor and buffer offset
	incrememnt_pos(stream, 1);

	// set last operation to read
	set_last_operation(stream, 'r');

	// return read char
	return ch;
}

int so_fputc(int c, SO_FILE *stream) {
	int rc;

	if (stream->last_operation != WRITE)
		prepare_buffer(stream, 'w');

	int pos = stream->buf_offset;

	// write char to buffer
	stream->buf[pos] = c;

	incrememnt_pos(stream, 1);

	// set last operation to write
	set_last_operation(stream, 'w');

	if (pos == stream->buf_size) {
		rc = so_fflush(stream);

		if (rc < 0) {
			end_or_error_reached(stream);
			return SO_EOF;
		}

		prepare_buffer(stream, 'w');
	}

	return c;
}

size_t so_fread(void *ptr, size_t size, size_t nmemb, SO_FILE *stream) {
	int rc = 0;
	char ch;

	if (stream->last_operation != READ) rc = so_fflush(stream);
	if (rc == SO_EOF) {
		end_or_error_reached(stream);
		return rc;
	}

	set_last_operation(stream, 'r');

	// reads one char at a time using so_fgetc
	for (int i = 0; i < nmemb; i++) {
		for (int j = 0; j < size; j++, ptr++) {
			ch = so_fgetc(stream);
			if (so_feof(stream)) {
				return 0;
			}
			memcpy(ptr, &ch, 1);
		}
	}
	
	return nmemb;
}

size_t so_fwrite(const void *ptr, size_t size, size_t nmemb, SO_FILE *stream) {
	int rc = 0;
	int ch = 0;

	// writes one char at a time using so_putc
	for (int i = 0; i < nmemb; i ++) {
		for (int j = 0; j < size; j++, ptr++) {
			memcpy(&ch, ptr, 1);
			rc = so_fputc(ch, stream);
			if (rc < 0) 
				return 0;
		}
	}

	return nmemb;
}

// moves cursor 
int so_fseek(SO_FILE *stream, long offset, int whence) {
	int rc;
	int last_op = stream->last_operation;

	if (last_op == WRITE) {
		rc = so_fflush(stream);

		if (rc < 0) {
			end_or_error_reached(stream);
			return SO_EOF;
		}
	} else if (last_op == READ)
		prepare_buffer(stream, 'r');

	rc = lseek(stream->fd, offset, whence);
	if (rc < 0) {
		end_or_error_reached(stream);
		return SO_EOF;
	}

	set_last_operation(stream, 'n');
	stream->cursor = rc;

	return 0;
}

SO_FILE *so_popen(const char *command, const char *type) {
	return NULL;
}

int so_pclose(SO_FILE *stream) {
	return 0;
}