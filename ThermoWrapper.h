#pragma once
#using <C:\Users\shraddha\source\repos\ThermoWrapper\lib\ThermoFisher.CommonCore.RawFileReader.dll>
#using <C:\Users\shraddha\source\repos\ThermoWrapper\lib\ThermoFisher.CommonCore.Data.dll>
using namespace System;
using namespace System::Collections::Generic;
using namespace ThermoFisher::CommonCore::RawFileReader;
using namespace ThermoFisher::CommonCore::Data::Business;
using namespace ThermoFisher::CommonCore::Data::Interfaces;

namespace ThermoWrapper {
    public ref class ScanData {
    public:
        array<double>^ Masses;
        array<double>^ Intensities;
        double RetentionTime;
        int ScanNumber;
        String^ Filter;
    };

    public ref class RawBridge {
    public:
        static int GetScanCount(String^ filePath) {
            try {
                auto rawFile = RawFileReaderAdapter::FileFactory(filePath);
                if (rawFile == nullptr) return -1;
                rawFile->SelectInstrument(Device::MS, 1);
                int lastScan = rawFile->RunHeader->LastSpectrum;
                delete rawFile;
                return lastScan;
            }
            catch (Exception^) { return -2; }
        }

        static double GetRetentionTime(String^ filePath, int scanNumber) {
            try {
                auto rawFile = RawFileReaderAdapter::FileFactory(filePath);
                if (rawFile == nullptr) return -1;
                rawFile->SelectInstrument(Device::MS, 1);
                double rt = rawFile->RetentionTimeFromScanNumber(scanNumber);
                delete rawFile;
                return rt;
            }
            catch (Exception^) { return -2; }
        }

        static String^ GetScanFilter(String^ filePath, int scanNumber) {
            try {
                auto rawFile = RawFileReaderAdapter::FileFactory(filePath);
                if (rawFile == nullptr) return "error";
                rawFile->SelectInstrument(Device::MS, 1);
                auto scanFilter = rawFile->GetFilterForScanNumber(scanNumber);
                String^ filter = scanFilter->ToString();
                delete rawFile;
                return filter;
            }
            catch (Exception^ e) { return e->Message; }
        }

        static ScanData^ GetScan(String^ filePath, int scanNumber) {
            try {
                auto rawFile = RawFileReaderAdapter::FileFactory(filePath);
                if (rawFile == nullptr) return nullptr;
                rawFile->SelectInstrument(Device::MS, 1);
                auto result = gcnew ScanData();
                result->ScanNumber = scanNumber;
                result->RetentionTime = rawFile->RetentionTimeFromScanNumber(scanNumber);
                result->Filter = rawFile->GetFilterForScanNumber(scanNumber)->ToString();
                auto scanStats = rawFile->GetScanStatsForScanNumber(scanNumber);
                auto centroidStream = rawFile->GetCentroidStream(scanNumber, false);
                if (centroidStream != nullptr && centroidStream->Length > 0) {
                    int len = centroidStream->Length;
                    result->Masses = gcnew array<double>(len);
                    result->Intensities = gcnew array<double>(len);
                    for (int i = 0; i < len; i++) {
                        result->Masses[i] = centroidStream->Masses[i];
                        result->Intensities[i] = centroidStream->Intensities[i];
                    }
                }
                delete rawFile;
                return result;
            }
            catch (Exception^) { return nullptr; }
        }
    };
}
