using System;
using System.IO;
using System.Runtime.InteropServices;
using ThermoFisher.CommonCore.RawFileReader;
using ThermoFisher.CommonCore.Data.Business;

namespace ThermoWrapperManaged
{
    public static class RawBridge
    {
        [UnmanagedCallersOnly(EntryPoint = "GetScanCount")]
        public static int GetScanCount(IntPtr filePathPtr)
        {
            try
            {
                string? filePath = Marshal.PtrToStringUTF8(filePathPtr);
                if (string.IsNullOrWhiteSpace(filePath) || !File.Exists(filePath)) return -1;

                var rawFile = RawFileReaderAdapter.FileFactory(filePath);
                if (rawFile == null) return -1;
                using (rawFile)
                {
                    rawFile.SelectInstrument(Device.MS, 1);
                    int lastScan = rawFile.RunHeader.LastSpectrum;
                    return lastScan;
                }
            }
            catch (Exception)
            {
                return -2;
            }
        }
    }
}
