#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

// Max int32
#define MAX_NUMBER_SIZE 64
#define MAX_STRING_SIZE 1024

typedef int (*StreamWriteBufferFunction)(const char *);

typedef enum _parse_mode_e { NORMAL, ARGUMENT, FORMAT_SPECIFIER } ParseMode;

static int print(const char *data) {
	size_t length = strlen(data);
	const unsigned char *bytes = (const unsigned char *)data;
	for (size_t i = 0; i < length; i++) {
		if (putchar(bytes[i]) == EOF) return i;
	}
	return (length);
}

typedef struct _stream_t {
	size_t length;
	size_t index;
	char *buffer;
	StreamWriteBufferFunction write_all;
} Stream;

/**
 * Try to print the buffer.
 *  returns:
 *      - 0 on success
 *      - non-0 on failure
 * */
static int print_stream_buffer(Stream *stream) {
	// Sets the final character as \0
	stream->buffer[stream->length] = '\0';
	// writes the buffer and return 1 on failure
    size_t written = (size_t)stream->write_all(stream->buffer);
	if (written != strlen(stream->buffer))
		return 1;

	// Clears the buffer
	memset(stream->buffer, 0, stream->length);
	stream->index = 0;
	return 0;
}

/**
 * If not enough space is available and forwards the return code
 * */
static int push_to_buffer(Stream *stream, char c) {
	stream->buffer[stream->index++] = c;

	// if buffer is full, flushes it.
	if (stream->index == stream->length - 2) {
		int error = print_stream_buffer(stream);
		if (error != 0) return error;
	}
	return 0;
}

/**
 * If there is few space after pushing the string or at the end,
 * it will push it to the buffer and try to flush it.
 * */
static int push_all_to_buffer(Stream *stream, const char *str) {
	int error;
	for (char c = *str; c != '\0'; c = *++str) {
		error = push_to_buffer(stream, c);
		if (error != 0) return error;
	}
	return 0;
}

/**
 * Pushes an Integer into the buffer,
 * if there's is not enough space writes it and flushes the buffer
 * */
static void push_int_to_buffer(Stream *stream, unsigned int val,
							   unsigned int base, int is_signed) {
	static char buffer[MAX_NUMBER_SIZE] = {0};
	int i = MAX_NUMBER_SIZE - 1;
	buffer[i] = '\0';

	if (is_signed) {
		int sval = (int)val;
		if (sval < 0) {
			push_to_buffer(stream, '-');
			val = (unsigned int)(-sval);
		} else {
			val = sval;
		}
	}

	if (val == 0) {
		push_to_buffer(stream, '0');
		return;
	}

	while (val && i > 0) {
		buffer[--i] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"[val % base];
		val /= base;
	}

	push_all_to_buffer(stream, &buffer[i]);
}

static int int_flag_to_base(char flag) {
	switch (flag) {
		case 'd':
			return 10;
		case 'x':
			return 16;
		case 'o':
			return 8;
		default:
			return 10;
	}
}

int vprintf(Stream *stream, const char *format, va_list args) {
	for (size_t i = 0; format[i] != '\0'; i++) {
		char current = format[i];

		// Any char
		if (current != '%') {
			push_to_buffer(stream, current);
			continue;
		}

		// %%
		if (format[i + 1] == '%') {
			push_to_buffer(stream, '%');
			i++;
			continue;
		}

		char spec = format[++i];
		int is_signed = 0;

		switch (spec) {
			case 'd': {
				int val = va_arg(args, int);
				is_signed = 1;
				push_int_to_buffer(stream, val, int_flag_to_base(spec),
								   is_signed);
				break;
			}
			case 'u':
			case 'x':
			case 'o': {
				unsigned int val = va_arg(args, unsigned int);
				push_int_to_buffer(stream, val, int_flag_to_base(spec),
								   is_signed);
				break;
			}
			case 'c': {
				int c = va_arg(args, int);
				push_to_buffer(stream, (char)c);
				break;
			}
			case 's': {
				const char *str = va_arg(args, const char *);
				push_all_to_buffer(stream, str ? str : "(null)");
				break;
			}
			default:
				push_to_buffer(stream, '%');
				push_to_buffer(stream, spec);
				break;
		}
	}
	int r = print_stream_buffer(stream);
	return r;
}

int printf(const char *__restrict format, ...) {
	va_list args;
	va_start(args, format);
	char buffer[MAX_STRING_SIZE];
	Stream stream = {sizeof(buffer), 0, buffer, print};
	int result = vprintf(&stream, format, args);
	va_end(args);
	return result;
}
