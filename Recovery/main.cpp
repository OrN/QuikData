#include "BufferedReader.h"
#include "ByteSwap.h"
#include "CRC32.h"

#include <algorithm>
#ifdef _LINUX_
#include <sys/stat.h>
#include <unistd.h>
#else
#include <direct.h>
#endif
#include <iostream>

#ifdef _LINUX_
#define _mkdir(dir) mkdir(dir, 666)
#define _sleep sleep
#endif

#pragma warning(disable:4996)

static uint8_t jpgtrailer[] = { 0xFF, 0xD9 };
static uint8_t pngtrailer[] = { 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82 };

static BufferedReader reader;

bool dumpJPEG(bool* partial) {
	uint8_t marker[2];
	uint16_t length;

	reader.read(&marker, 2);

	// JPEG markers start with 0xFF
	if (marker[0] == 0xFF) {
		// We can ignore most data, we just need to make sure the file is valid
		switch (marker[1]) {
			case 0xC0:
			case 0xC1:
			case 0xC2:
			case 0xC3:
			case 0xC5:
			case 0xC6:
			case 0xC7:
			case 0xC8:
			case 0xC9:
			case 0xCA:
			case 0xCB:
			case 0xCC:
			case 0xCD:
			case 0xCE:
			case 0xCF:
			case 0xC4:
			case 0xDA:
			case 0xDB:
				reader.read(&length, 2);
				length = SWAP_UINT16(length);
				reader.seek(length);
				break;
			case 0xD8:
				// Found another image, exit this one
				std::cout << "\tImage incomplete, attempting partial dump" << std::endl;
				reader.seek(-2);
				if (partial != nullptr)
					*partial = true;
				return true;
			case 0xD9:
				// Image finished
				if (partial != nullptr)
					*partial = false;
				return true;
			case 0xDD:
				reader.seek(2);
				break;
			case 0xE0:
			case 0xE1:
			case 0xE2:
			case 0xE3:
			case 0xE4:
			case 0xE5:
			case 0xE6:
			case 0xE7:
			case 0xE8:
			case 0xE9:
			case 0xEA:
			case 0xEB:
			case 0xEC:
			case 0xED:
			case 0xEE:
			case 0xEF:
				reader.read(&length, 2);
				length = SWAP_UINT16(length);
				if (marker[1] == 0xE0) {
					if (length >= 16)
						reader.seek(length);
					else
						reader.seek(-1);
				}
				else {
					reader.seek(length);
				}
				break;
			case 0xD0:
			case 0xD1:
			case 0xD2:
			case 0xD3:
			case 0xD4:
			case 0xD5:
			case 0xD6:
			case 0xD7:
			case 0xDE:
			case 0xF0:
			case 0xF1:
			case 0xF2:
			case 0xF3:
			case 0xF4:
			case 0xF5:
			case 0xF6:
			case 0xF7:
			case 0xF8:
			case 0xF9:
			case 0xFA:
			case 0xFB:
			case 0xFC:
			case 0xFD:
			case 0xFE:
			case 0xFF:
			case 0x00:
				break;
			default:
				std::cout << "\tUnexpected partition marker: 0x" << std::hex << (uint32_t)marker[1] << "; Dumping partially" << std::endl;
				reader.seek(-2);
				if (partial != nullptr)
					*partial = true;
				return true;
		}
	} else {
		reader.seek(-1);
	}
	return false;
}

bool dumpPNG(bool* partial, uint64_t* prevchunk) {
	uint32_t chunkLength;
	char chunkType[5];
	uint32_t chunkCRC32;

	reader.read(&chunkLength, 4);
	reader.read(&chunkType, 4);
	chunkLength = SWAP_UINT32(chunkLength);
	chunkType[4] = 0x00;

	uint64_t remaining = chunkLength;
	if (remaining > reader.remaining()) {
		std::cout << "\tChunk is too big for the filesystem; Dumping partially" << std::endl;
		reader.seek(-(int64_t)(reader.tell() - *prevchunk));
		if (partial != nullptr)
			*partial = true;
		return true;
	}

	// Calculate CRC32
	uint8_t buffer[512];
	crc32init();
	crc32buf((uint8_t*)chunkType, 4);
	while (remaining > 0) {
		uint64_t readsize = reader.read(buffer, std::min<size_t>(remaining, 512));
		crc32buf(buffer, readsize);
		remaining -= readsize;
	}
	uint32_t calculatedCRC32 = crc32finish();
	calculatedCRC32 = SWAP_UINT32(calculatedCRC32);

	reader.read(&chunkCRC32, 4);

	if (calculatedCRC32 != chunkCRC32) {
		std::cout << "\tCRC32 for chunk doesn't match; Dumping partially" << std::endl;
		reader.seek(-(int64_t)(reader.tell() - *prevchunk));
		if (partial != nullptr)
			*partial = true;
		return true;
	}

	if (strcmp(chunkType, "IEND") == 0) {
		if (partial != nullptr)
			*partial = false;
		return true;
	}

	*prevchunk = reader.tell();

	return false;
}

bool dumpMP4() {
	uint32_t chunkLength = 0;
	char chunkType[5];

	reader.read(&chunkLength, 4);
	chunkLength = SWAP_UINT32(chunkLength);
	reader.read(&chunkType, 4);
	chunkType[4] = 0x00;

	if (strcmp(chunkType, "ftyp") != 0 &&
		strcmp(chunkType, "mdat") != 0 &&
		strcmp(chunkType, "moov") != 0 &&
		strcmp(chunkType, "pnot") != 0 &&
		strcmp(chunkType, "udta") != 0 &&
		strcmp(chunkType, "uuid") != 0 &&
		strcmp(chunkType, "moof") != 0 &&
		strcmp(chunkType, "free") != 0 &&
		strcmp(chunkType, "skip") != 0 &&
		strcmp(chunkType, "jP2") != 0 &&
		strcmp(chunkType, "wide") != 0 &&
		strcmp(chunkType, "load") != 0 &&
		strcmp(chunkType, "ctab") != 0 &&
		strcmp(chunkType, "imap") != 0 &&
		strcmp(chunkType, "matt") != 0 &&
		strcmp(chunkType, "kmat") != 0 &&
		strcmp(chunkType, "clip") != 0 &&
		strcmp(chunkType, "crgn") != 0 &&
		strcmp(chunkType, "sync") != 0 &&
		strcmp(chunkType, "chap") != 0 &&
		strcmp(chunkType, "tmcd") != 0 &&
		strcmp(chunkType, "scpt") != 0 &&
		strcmp(chunkType, "ssrc") != 0 &&
		strcmp(chunkType, "PICT") != 0) {
		std::cout << "\tUnsupported type '" << chunkType << "', assuming end of file" << std::endl;
		reader.seek(-8);
		return true;
	} else {
		if (chunkLength == 0 || chunkLength > reader.remaining()) {
			std::cout << "\tInvalid chunk length, attempting partial dump" << std::endl;
			reader.seek(-8);
			return true;
		}
		reader.seek(chunkLength - 8);
	}

	return false;
}

bool dumpWAV(bool* valid) {
	char chunkType[5];
	uint32_t chunkLength = 0;

	chunkType[4] = 0x00;

	reader.read(chunkType, 4);
	reader.read(&chunkLength, 4);

	if (strcmp(chunkType, "data") == 0 && valid != nullptr)
		*valid = true;

	if (strcmp(chunkType, "fmt ") != 0 &&
		strcmp(chunkType, "data") != 0 &&
		strcmp(chunkType, "bext") != 0 &&
		strcmp(chunkType, "CDif") != 0) {
		std::cout << "\tUnsupported type '" << chunkType << "', assuming end of file" << std::endl;
		reader.seek(-8);
		return true;
	} else {
		if (chunkLength > reader.remaining()) {
			std::cout << "\tInvalid chunk length, attempting partial dump" << std::endl;
			reader.seek(-8);
			return true;
		}
		reader.seek(chunkLength);
	}

	return false;
}

void print_help() {
	std::cout << "No file specified" << std::endl;
}

int main(int argc, char* argv[]) {
	uint8_t header[4];
	char dumpname[512];
	uint64_t imagecount = 1;
	uint64_t videocount = 1;
	uint64_t audiocount = 1;

	if (argc < 2) {
		print_help();
		return 0;
	}

	reader.open(argv[1]);

	_mkdir("dump");
	_mkdir("dump/Photos");
	_mkdir("dump/Videos");
	_mkdir("dump/Audio");

	std::cout << "Filesize: " << (reader.getSize() / 1024 / 1024) << "MiB" << std::endl;

	for (;;) {
		uint64_t size = reader.read(header, 4);

		if (size < 4) {
			std::cout << "Finished" << std::endl;
			break;
		}

		if (	header[0] == 0xFF && header[1] == 0xD8 &&
				header[2] == 0xFF && (header[3] >= 0xE0 && header[3] <= 0xEF)) {

			bool partial;
			uint64_t start = reader.tell() - 4;
			std::cout << "Image (JPEG) found at 0x" << std::hex << start << std::endl;
			reader.seek(-2);
			while (!dumpJPEG(&partial));
			uint64_t end = reader.tell();

			sprintf(dumpname, "dump/Photos/%llu.jpg\0", imagecount++);
			if (partial)
				reader.dump(dumpname, start, end, jpgtrailer, 2);
			else
				reader.dump(dumpname, start, end, nullptr, 0);
		} else if (	header[0] == 0x89 && header[1] == 0x50 &&
					header[2] == 0x4E && header[3] == 0x47) {
			reader.read(header, 4);
			if (	header[0] == 0x0D && header[1] == 0x0A &&
					header[2] == 0x1A && header[3] == 0x0A) {
				bool partial;
				uint64_t prevchunk = reader.tell();
				uint64_t start = prevchunk - 8;
				std::cout << "Image (PNG) found at 0x" << std::hex << start << std::endl;
				while (!dumpPNG(&partial, &prevchunk));
				uint64_t end = reader.tell();

				sprintf(dumpname, "dump/Photos/%llu.png\0", imagecount++);
				if (partial)
					reader.dump(dumpname, start, end, pngtrailer, 12);
				else
					reader.dump(dumpname, start, end, nullptr, 0);
			} else {
				reader.seek(-7);
			}
		} else if ( header[0] == 0x52 && header[1] == 0x49 &&
					header[2] == 0x46 && header[3] == 0x46) {
			uint64_t start = reader.tell() - 4;
			uint32_t chunkLength = 0;
			bool valid = false;
			char chunkType[5];
			
			chunkType[4] = 0x00;

			reader.read(&chunkLength, 4);
			reader.read(chunkType, 4);
			if (strcmp(chunkType, "WAVE") == 0) {
				std::cout << "Audio (WAV) found at 0x" << std::hex << start << std::endl;
				while (!dumpWAV(&valid));
				uint64_t end = reader.tell();

				if (valid) {
					sprintf(dumpname, "dump/Audio/%llu.wav\0", audiocount++);
					reader.dump(dumpname, start, end, nullptr, 0);
				} else {
					std::cout << "\tFile isn't valid, ignoring" << std::endl;
					reader.seek(-(int64_t)(reader.tell() - (start + 1)));
				}
			} else {
				std::cout << "Audio (WAV) found at 0x" << std::hex << start << " with unsupported type '" << chunkType << "'" << std::endl;
				reader.seek(-11);
				_sleep(5000);
			}
		} else if (	header[0] == 0x66 && header[1] == 0x74 &&
					header[2] == 0x79 && header[3] == 0x70) {
			char headerType[5];
			headerType[4] = 0x00;
			reader.read(headerType, 4);
			if (	strcmp(headerType, "avc1") == 0 ||
					strcmp(headerType, "iso2") == 0 ||
					strcmp(headerType, "isom") == 0 ||
					strcmp(headerType, "mmp4") == 0 ||
					strcmp(headerType, "mp41") == 0 ||
					strcmp(headerType, "mp42") == 0 ||
					strcmp(headerType, "mp71") == 0 ||
					strcmp(headerType, "msnv") == 0 ||
					strcmp(headerType, "ndas") == 0 ||
					strcmp(headerType, "ndsc") == 0 ||
					strcmp(headerType, "ndsh") == 0 ||
					strcmp(headerType, "ndsm") == 0 ||
					strcmp(headerType, "ndsp") == 0 ||
					strcmp(headerType, "ndss") == 0 ||
					strcmp(headerType, "ndxc") == 0 ||
					strcmp(headerType, "ndxh") == 0 ||
					strcmp(headerType, "ndxm") == 0 ||
					strcmp(headerType, "ndxp") == 0 ||
					strcmp(headerType, "ndxs") == 0 ||
					strcmp(headerType, "3gp4") == 0 ||
					strcmp(headerType, "3gp7") == 0 ||
					strcmp(headerType, "M4A ") == 0) {
				reader.seek(-12);
				uint64_t start = reader.tell();
				std::cout << "Video/Audio (MP4) found at 0x" << std::hex << start << std::endl;
				while (!dumpMP4());
				uint64_t end = reader.tell();

				sprintf(dumpname, "dump/Videos/%llu.mp4\0", videocount++);
				reader.dump(dumpname, start, end, nullptr, 0);
				reader.seek(-(int64_t)(reader.tell() - (start + 5)));
			} else {
				std::cout << "Video (MP4) found at 0x" << (reader.tell() - 4) << " with unsupported type '" << headerType << "'" << std::endl;
				reader.seek(-7);
				_sleep(5000);
			}
		} else {
			reader.seek(-3);
		}
	}

	reader.close();

	return 0;
}
