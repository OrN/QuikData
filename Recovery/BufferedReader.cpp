#include "BufferedReader.h"

#include <algorithm>
#include <cstring>

#pragma warning(disable:4996)

#ifdef _LINUX_
#define _fseeki64 fseeko64
#define _ftelli64 ftello64
#endif

BufferedReader::BufferedReader() {
	m_file = nullptr;
	m_buffer = new uint8_t[BUFFERSIZE];
	m_position = 0;
}

BufferedReader::~BufferedReader() {
	delete[] m_buffer;
}

bool BufferedReader::open(std::string fname) {
	m_file = fopen(fname.c_str(), "rb");
	_fseeki64(m_file, 0L, SEEK_END);
	m_size = _ftelli64(m_file);
	_fseeki64(m_file, 0x0, SEEK_SET);
	m_position = 0;
	_fill();
	return (m_file != nullptr);
}

void BufferedReader::close() {
	fclose(m_file);
}

uint64_t BufferedReader::read(void* dst, uint64_t count) {
	uint64_t readamount = 0;
	uint64_t retries = 0;
	while (readamount < count) {
		uint64_t readsize = std::min<uint64_t>(count - readamount, m_available - m_offset);
		memcpy((uint8_t*)dst + readamount, m_buffer + m_offset, readsize);
		readamount += readsize;
		m_position += readsize;
		m_offset += readsize;

		if (readamount != count) {
			_fill();
			if (m_available == 0)
				return readamount;
		}
	}
	return readamount;
}

void BufferedReader::dump(std::string fname, uint64_t start, uint64_t end, void* extradata, size_t extrasize) {
	uint8_t buffer[BUFFERSIZE];
	uint64_t dumpsize = end - start;

	_fseeki64(m_file, start, SEEK_SET);

	FILE* f = fopen(fname.c_str(), "wb");
	while (dumpsize > 0) {
		uint64_t readsize = fread(buffer, 1, std::min<size_t>(BUFFERSIZE, dumpsize), m_file);
		dumpsize -= readsize;
		fwrite(buffer, 1, readsize, f);
	}
	if (extradata != nullptr)
		fwrite(extradata, 1, extrasize, f);
	fclose(f);

	_fseeki64(m_file, m_position + m_available, SEEK_SET);
}

void BufferedReader::seek(int64_t count) {
	uint64_t oldposition = m_position;
	m_position += count;
	m_offset += count;
	if (m_offset >= BUFFERSIZE)
		_fill();
}

uint64_t BufferedReader::getSize() {
	return m_size;
}

uint64_t BufferedReader::tell() {
	return m_position;
}

uint64_t BufferedReader::remaining() {
	return m_size - m_position;
}

void BufferedReader::_fill() {
	_fseeki64(m_file, m_position, SEEK_SET);
	m_available = fread(m_buffer, 1, BUFFERSIZE, m_file);
	m_offset = 0;
}
