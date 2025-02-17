/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/offline_compiler/source/ocloc_arg_helper.h"
#include "shared/source/helpers/string.h"

#include "gtest/gtest.h"

#include <algorithm>
#include <map>
#include <optional>
#include <string>

class MockOclocArgHelper : public OclocArgHelper {
  public:
    using OclocArgHelper::hasOutput;
    using OclocArgHelper::headers;
    using OclocArgHelper::inputs;
    using OclocArgHelper::messagePrinter;

    using OclocArgHelper::findSourceFile;

    using FileName = std::string;
    using FileData = std::string;
    using FilesMap = std::map<FileName, FileData>;

    FilesMap &filesMap;
    bool interceptOutput{false};
    bool shouldLoadDataFromFileReturnZeroSize{false};
    FilesMap interceptedFiles;
    bool callBaseFileExists = false;
    bool callBaseReadBinaryFile = false;
    bool callBaseLoadDataFromFile = false;
    bool callBaseReadFileToVectorOfStrings = false;
    bool shouldReturnEmptyVectorOfStrings = false;

    MockOclocArgHelper(FilesMap &filesMap) : OclocArgHelper(0, nullptr, nullptr, nullptr, 0, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr),
                                             filesMap(filesMap){};

    void setAllCallBase(bool value) {
        callBaseFileExists = value;
        callBaseReadBinaryFile = value;
        callBaseLoadDataFromFile = value;
        callBaseReadFileToVectorOfStrings = value;
    }

    void readFileToVectorOfStrings(const std::string &filename, std::vector<std::string> &lines) override {
        if (callBaseReadFileToVectorOfStrings) {
            return OclocArgHelper::readFileToVectorOfStrings(filename, lines);
        }

        if (shouldReturnEmptyVectorOfStrings) {
            lines.clear();
        } else {
            auto ss = std::stringstream(filesMap[filename]);
            for (std::string line; std::getline(ss, line);) {
                lines.push_back(line);
            }
        }
    }

    std::vector<char> readBinaryFile(const std::string &filename) override {
        if (callBaseReadBinaryFile) {
            return OclocArgHelper::readBinaryFile(filename);
        }
        auto file = filesMap[filename];
        return std::vector<char>(file.begin(), file.end());
    }

  protected:
    bool fileExists(const std::string &filename) const override {
        if (callBaseFileExists) {
            return OclocArgHelper::fileExists(filename);
        }
        return filesMap.find(filename) != filesMap.end();
    }

    std::unique_ptr<char[]> loadDataFromFile(const std::string &filename, size_t &retSize) override {
        if (callBaseLoadDataFromFile) {
            return OclocArgHelper::loadDataFromFile(filename, retSize);
        }

        if (shouldLoadDataFromFileReturnZeroSize) {
            retSize = 0;
            return {};
        }

        if (!fileExists(filename)) {
            return OclocArgHelper::loadDataFromFile(filename, retSize);
        }

        const auto &file = filesMap[filename];

        std::unique_ptr<char[]> result{new char[file.size() + 1]};
        std::copy(file.begin(), file.end(), result.get());
        result[file.size()] = '\0';

        retSize = file.size() + 1;

        return result;
    }

    void saveOutput(const std::string &filename, const void *pData, const size_t &dataSize) override {
        if (interceptOutput) {
            auto &fileContent = interceptedFiles[filename];
            fileContent.resize(dataSize, '\0');

            memcpy_s(fileContent.data(), fileContent.size(), pData, dataSize);
        } else {
            OclocArgHelper::saveOutput(filename, pData, dataSize);
        }
    }
};
