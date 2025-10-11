#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

// Max int32
constexpr size_t MAX_NUMBER_SIZE = 64;
constexpr size_t MAX_STRING_SIZE = 1024;

constexpr const char *DIGIT_DICTIONARY = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
constexpr const char *NULL_STRING_MESSAGE = "(null)";
constexpr char NEGATIVE_PREFIX = '-';
constexpr char SPEC_DELIMITER = '%';
constexpr char END_OF_STRING = '\0';
constexpr char CHAR_ZERO = '0';
constexpr int INT_ZERO = 0;
constexpr size_t STEP = 1;

constexpr int DEFAULT_INDEX = 0;
constexpr int SUCCESS = 0;
constexpr int ERROR = -1;
constexpr int EMPTY = 0;

// TYPES
typedef int (*StreamWriteBufferFunction)(const char *);

typedef enum _spec_type_e {
	SPEC_DIGIT = 'd',
	SPEC_UNSIGNED = 'u',
	SPEC_HEX = 'x',
	SPEC_OCTAL = 'o',
	SPEC_CHAR = 'c',
	SPEC_STRING = 's'
} SpecType;

typedef struct _stream_t {
	size_t capacity;
	size_t index;
	char *buffer;
	StreamWriteBufferFunction write_all;
} Stream;

// Auxiliar functions
/**
 * Writes a buffer char by char
 * returns the length of the written data
 * */
static int print(const char *data) {
	size_t length = strlen(data);
	const unsigned char *bytes = (const unsigned char *)data;
	for (size_t i = 0; i < length; i++) {
		if (putchar(bytes[i]) == EOF) return i;
	}
	return (length);
}

/**
 * Try to print the buffer.
 *  returns:
 *      - 0 on success
 *      - non-0 on failure
 * */
static int print_stream_buffer(Stream *stream) {
	// Sets the final character as \0
	stream->buffer[stream->index] = END_OF_STRING;
	// writes the buffer and return 1 on failure
	size_t written = (size_t)stream->write_all(stream->buffer);
	if (written != stream->index) return ERROR;

	// Clears the buffer
	memset(stream->buffer, EMPTY, stream->capacity);
	stream->index = DEFAULT_INDEX;
	return SUCCESS;
}

/**
 * If not enough space is available and forwards the return code
 * */
static int push_to_buffer(Stream *stream, char c) {
	stream->buffer[stream->index++] = c;

	// if buffer is full, flushes it.
	if (stream->index == stream->capacity - 2) {
		int result = print_stream_buffer(stream);
		if (result != SUCCESS) return result;
	}
	return SUCCESS;
}

/**
 * If there is few space after pushing the string or at the end,
 * it will push it to the buffer and try to flush it.
 * */
static int push_all_to_buffer(Stream *stream, const char *str) {
	int result;
	for (char c = *str; c != END_OF_STRING; c = *++str) {
		result = push_to_buffer(stream, c);
		if (result != SUCCESS) return result;
	}
	return SUCCESS;
}

/**
 * Pushes an Integer into the buffer,
 * if there's is not enough space writes it and flushes the buffer
 * */
static void push_int_to_buffer(Stream *stream, unsigned int val,
							   unsigned int base, int is_signed) {
	static char buffer[MAX_NUMBER_SIZE] = {EMPTY};
	int i = MAX_NUMBER_SIZE - 1;
	buffer[i] = END_OF_STRING;

	if (is_signed) {
		int sval = (int)val;
		if (sval < INT_ZERO) {
			push_to_buffer(stream, NEGATIVE_PREFIX);
			val = (unsigned int)(-sval);
		} else {
			val = sval;
		}
	}

	if (val == INT_ZERO) {
		push_to_buffer(stream, CHAR_ZERO);
		return;
	}

	while (val && i > INT_ZERO) {
		buffer[--i] = DIGIT_DICTIONARY[val % base];
		val /= base;
	}

	push_all_to_buffer(stream, &buffer[i]);
}

static int int_flag_to_base(SpecType flag) {
	switch (flag) {
		case SPEC_DIGIT:
		case SPEC_UNSIGNED:
			return 10;
		case SPEC_HEX:
			return 16;
		case SPEC_OCTAL:
			return 8;
		default:
			return 10;
	}
}

int vprintf(Stream *stream, const char *format, va_list args) {
	for (size_t i = 0; format[i] != END_OF_STRING; i++) {
		char current = format[i];

		// Any char
		if (current != SPEC_DELIMITER) {
			push_to_buffer(stream, current);
			continue;
		}

		// %%
		if (format[i + STEP] == SPEC_DELIMITER) {
			push_to_buffer(stream, SPEC_DELIMITER);
			i++;
			continue;
		}

		SpecType spec = (SpecType)format[++i];
		int is_signed = 0;

		switch (spec) {
			case SPEC_DIGIT: {
				int val = va_arg(args, int);
				is_signed = 1;
				push_int_to_buffer(stream, val, int_flag_to_base(spec),
								   is_signed);
				break;
			}
			case SPEC_UNSIGNED:
			case SPEC_HEX:
			case SPEC_OCTAL: {
				unsigned int val = va_arg(args, unsigned int);
				push_int_to_buffer(stream, val, int_flag_to_base(spec),
								   is_signed);
				break;
			}
			case SPEC_CHAR: {
				int c = va_arg(args, int);
				push_to_buffer(stream, (char)c);
				break;
			}
			case SPEC_STRING: {
				const char *str = va_arg(args, const char *);
				push_all_to_buffer(stream, str ? str : NULL_STRING_MESSAGE);
				break;
			}
			default:
				push_to_buffer(stream, SPEC_DELIMITER);
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
	Stream stream = {.capacity = sizeof(buffer),
					 .index = DEFAULT_INDEX,
					 .buffer = buffer,
					 .write_all = print};
	int result = vprintf(&stream, format, args);
	va_end(args);
	return result;
}
