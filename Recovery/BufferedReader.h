#pragma once

#include <stdio.h>
#include <string>

enum {
	BUFFERSIZE = 4096,
};

class BufferedReader {
public:
	BufferedReader();
	~BufferedReader();

	bool open(std::string fname);
	void close();

	uint64_t read(void* dst, uint64_t count);
	void seek(int64_t count);
	void dump(std::string fname, uint64_t start, uint64_t end, void* extradata, size_t extrasize);

	uint64_t getSize();
	uint64_t tell();
	uint64_t remaining();
private:
	void _fill();

	FILE* m_file;
	uint64_t m_size;
	uint64_t m_position;
	uint64_t m_available;
	uint64_t m_offset;

	uint8_t* m_buffer;
};
