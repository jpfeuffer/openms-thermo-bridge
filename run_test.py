import pythonnet
pythonnet.load("coreclr")
import clr, sys
sys.path.append(r"C:\Users\shraddha\source\repos\ThermoTest")
clr.AddReference("ThermoFisher.CommonCore.Data")
clr.AddReference("ThermoFisher.CommonCore.RawFileReader")
clr.AddReference("ThermoWrapper")
from ThermoWrapper import RawBridge

def analyze_raw_file(file_path):
    print(f"\n{'='*50}")
    print(f"Analyzing: {file_path}")
    print(f"{'='*50}")
    scan_count = RawBridge.GetScanCount(file_path)
    if scan_count < 0:
        print("Could not open file!")
        return
    print(f"Total Scans: {scan_count}")
    print(f"\nFirst 5 scans:")
    print(f"{'Scan':<8} {'Retention Time':<20} {'Filter'}")
    print("-" * 60)
    for i in range(1, min(6, scan_count + 1)):
        rt = RawBridge.GetRetentionTime(file_path, i)
        filt = RawBridge.GetScanFilter(file_path, i)
        print(f"{i:<8} {rt:<20.4f} {filt}")
    print(f"\nDetailed data for Scan 1:")
    scan = RawBridge.GetScan(file_path, 1)
    if scan and scan.Masses:
        print(f"  Number of peaks: {len(scan.Masses)}")
        print(f"  First 5 m/z values:")
        for i in range(min(5, len(scan.Masses))):
            print(f"    m/z: {scan.Masses[i]:.4f}  Intensity: {scan.Intensities[i]:.2f}")
    else:
        print("  No centroid data for this scan")

analyze_raw_file(r"C:\Users\shraddha\Desktop\test_data.raw")
