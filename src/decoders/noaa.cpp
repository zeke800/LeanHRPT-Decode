/*
 * LeanHRPT Decode
 * Copyright (C) 2021 Xerbo
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <QFile>
#include <QDataStream>

#include "noaa.h"
#include "generic/deframer.h"

// Gets a bit...
template <typename T>
inline bool getBit(T data, unsigned int bit) {
    return (data >> bit) & 1;
}

NOAADecoder::NOAADecoder() {
    image = new RawImage(2048, 5);
}

bool NOAADecoder::decodeFile(std::string filename) {
	QFile fileSource(filename.c_str());
    if (!fileSource.open(QIODevice::ReadOnly))
        return false;
    QDataStream dataStream(&fileSource);

    ArbitraryDeframer<uint64_t, 0b101000010001011011111101011100011001110110000011110010010101, 60, 11090 * 10> deframer(9, true);

    uint8_t *buffer = new uint8_t[BUFFER_SIZE];
    uint8_t *frame = new uint8_t[13863]; // Actually 4 bits too big

    while(dataStream.readRawData((char *)buffer, BUFFER_SIZE)) {
        if(deframer.work(buffer, frame, BUFFER_SIZE)) {
            image->push10Bit(frame, 750);
        }
    }

    delete[] buffer;
    delete[] frame;

    fileSource.close();
    return true;
}
