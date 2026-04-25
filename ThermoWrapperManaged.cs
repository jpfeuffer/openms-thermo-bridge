using System;
using System.IO;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using ThermoFisher.CommonCore.RawFileReader;
using ThermoFisher.CommonCore.Data.Business;

namespace ThermoWrapperManaged
{
    public static class RawBridge
    {
        // Return codes:
        //   >= 0  : scan count (success)
        //   -1    : file not found or null
        //   -2    : managed exception during processing
        //   -3    : platform not supported by Thermo RawFileReader
        [UnmanagedCallersOnly(EntryPoint = "GetScanCount")]
        public static int GetScanCount(IntPtr filePathPtr)
        {
            try
            {
                string? filePath = Marshal.PtrToStringUTF8(filePathPtr);
                if (string.IsNullOrWhiteSpace(filePath) || !File.Exists(filePath)) return -1;

                // The Thermo RawFileReader native components only support
                // Windows and Linux.  On other platforms (macOS), calling into
                // the library causes a native crash.  Guard here so that the
                // JIT compiler never needs to resolve Thermo types on
                // unsupported platforms (they live in GetScanCountImpl).
                if (!RuntimeInformation.IsOSPlatform(OSPlatform.Windows) &&
                    !RuntimeInformation.IsOSPlatform(OSPlatform.Linux))
                {
                    return -3;
                }

                return GetScanCountImpl(filePath);
            }
            catch (Exception)
            {
                return -2;
            }
        }

        // Thermo type references are isolated here so the JIT does not try to
        // resolve RawFileReaderAdapter / Device when compiling GetScanCount on
        // platforms where the Thermo libraries are not supported.
        [MethodImpl(MethodImplOptions.NoInlining)]
        private static int GetScanCountImpl(string filePath)
        {
            var rawFile = RawFileReaderAdapter.FileFactory(filePath);
            if (rawFile == null) return -1;
            using (rawFile)
            {
                rawFile.SelectInstrument(Device.MS, 1);
                int lastScan = rawFile.RunHeader.LastSpectrum;
                return lastScan;
            }
        }
    }
}
