#ifndef VIDEO_HPP
#define VIDEO_HPP
#include <stddef.h>

class Video {
  public:
	virtual void init() = 0;
	virtual void clear() = 0;
	virtual void putChar(char c) = 0;
	virtual void write(const char *data, size_t size) = 0;
	virtual void writeString(const char *data) = 0;
	virtual ~Video() = default;
};

#endif // !VIDEO_HPP
