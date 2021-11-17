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

#include "commandline.h"
#include "satinfo.h"
#include "fingerprint.h"
#include "imagecompositor.h"
#include "decoders/meteor.h"
#include "decoders/noaa.h"
#include "decoders/fengyun.h"
#include "decoders/metop.h"
#include "preset.h"
#include "geometry.h"

#include <QLocale>
#include <QDir>
#include <iostream>

static ulong str2ulong(QString str) {
    QLocale l(QLocale::C);
    return l.toULong(str);
}

int parseCommandLine(QCommandLineParser &parser) {
    QString filename = parser.positionalArguments().first();
    if (filename.isEmpty()) return 1;

    if (!parser.isSet("ini")) {
        std::cout << "Missing required argument -i/--ini" << std::endl;
        return 1;
    }

    QString outdir = parser.value("out");
    if (!outdir.isEmpty() && !QDir(outdir).exists()) {
        std::cout << "Output directory doesn't exist, creating it" << std::endl;
        QDir().mkdir(outdir);
    }

    std::cout << "Fingerprinting \"" << filename.toStdString() << "\"" << std::endl;

    std::pair<SatID, FileType> info = Fingerprint().file(filename.toStdString());
    SatID sat = info.first;
    if (sat == SatID::Unknown) {
        std::cout << "Unable to identify satellite" << std::endl;
        return 1;
    } else {
        std::cout << "Satellite is " << satellite_info.at(sat).name << std::endl;
    }
    Mission mission = satellite_info.at(sat).mission;

    Decoder *decoder;
    switch (mission) {
        case Mission::FengYun3: decoder = new FengyunDecoder(sat); break;
        case Mission::MeteorM:  decoder = new MeteorDecoder; break;
        case Mission::MetOp:    decoder = new MetOpDecoder; break;
        case Mission::POES:     decoder = new NOAADecoder; break;
        default: throw std::runtime_error("invalid value in enum `Mission`");
    }

    std::cout << "Decoding" << std::endl;;
    decoder->decodeFile(filename.toStdString(), info.second);
    Data data = decoder->get();
    std::cout << "Finished decoding" << std::endl;

    // Timestamp is the middle of the pass on the main imager
    QDateTime timestamp;
    {
        Imager default_imager = satellite_info.at(sat).default_imager;
        std::vector<double> medianv = data.timestamps[default_imager];
        std::sort(medianv.begin(), medianv.end());
        medianv.erase(std::remove(medianv.begin(), medianv.end(), 0.0), medianv.end());
        timestamp.setSecsSinceEpoch(medianv[medianv.size()/2]);
    }

    inipp::Ini<char> ini;
    {
        std::ifstream ifs(parser.value("ini").toStdString());
        if (!ifs.is_open()) {
            std::cout << "Could not open composite definition file" << std::endl;
            return 1;
        }
        ini.parse(ifs);
        ifs.close();
    }

    PresetManager preset_manager;

    std::map<Imager, ImageCompositor> compositors;
    for (auto &sensor_data : data.imagers) {
        Imager imager = sensor_data.first;
        RawImage *image = sensor_data.second;

        compositors[imager].import(image, sat, imager, data.caldata);

        for (auto &file : ini.sections) {
            // Replace template strings in filename
            QString filename = QString::fromStdString(file.first);
            filename = filename.replace("{sat}", QString::fromStdString(satellite_info.at(sat).name));
            filename = filename.replace("{time}", timestamp.toString("yyyy-MM-dd hh:mm:ss"));

            // Parse settings
            std::string sensors;
            if (file.second.count("sensors")) {
                sensors = file.second["sensors"];
            } else {
                std::cout << "Image \"" << file.first << "\" doesn't specify what sensors it works on, skipping" << std::endl;
                continue;
            }
            std::string equalization   = file.second.count("equalization")   ? file.second["equalization"]   : "none";
            std::string brightnessonly = file.second.count("brightnessonly") ? file.second["brightnessonly"] : "true";
            std::string corrected      = file.second.count("corrected")      ? file.second["corrected"]      : "true";

            // Skip if it doesn't work for this imager
            if (sensors.find(sensor_info.at(imager).name) == std::string::npos) {
                continue;
            }

            // Generate the image
            QImage image(compositors[imager].width(), compositors[imager].height(), QImage::Format_RGBX64);
            if (file.second.count("preset")) {
                // Preset
                std::string preset = file.second["preset"];
                std::string expression = preset_manager.presets.at(preset).expression;
                compositors[imager].getExpression(image, expression);
            } else if (file.second.count("channel")) {
                // Single channel
                compositors[imager].getChannel(image, str2ulong(QString::fromStdString(file.second["channel"])));
            } else if (file.second.count("composite")) {
                // RGB composite
                QStringList _channels = QString::fromStdString(file.second["composite"]).split(",");
                std::array<size_t, 3> channels = { str2ulong(_channels[0]), str2ulong(_channels[1]), str2ulong(_channels[2]) };
                compositors[imager].getComposite(image, channels);
            } else {
                std::cout << "Image \"" << file.first << "\" has no source, skipping" << std::endl;
                continue;
            }

            // Equalize the image
            if (equalization == "histogram") {
                ImageCompositor::equalise(image, Equalization::Histogram, 1.0f, brightnessonly == "true");
            } else if (equalization == "stretch") {
                ImageCompositor::equalise(image, Equalization::Stretch, 1.0f, brightnessonly == "true");
            } else if (equalization == "none") {
                ImageCompositor::equalise(image, Equalization::None, 1.0f, brightnessonly == "true");
            } else {
                std::cout << "Image \"" << file.first << "\" uses an unknown equalization, skipping" << std::endl;
                continue;
            }

            if (parser.isSet("flip")) {
                image = image.mirrored(true, true);
            }

            if (corrected == "true") {
                image = correct_geometry(image, sat, imager);
            }

            std::cout << "Writing \"" << filename.toStdString() << "\"" << std::endl;
            image.save(QDir(outdir).filePath(filename));
        }
    }

    return 0;
}
