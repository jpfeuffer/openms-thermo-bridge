#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
vendor_dir="${repo_root}/vendor/thermo-feed"
publish_dir="${repo_root}/artifacts/publish"
managed_dir="${publish_dir}/managed"
dotnet_source="https://api.nuget.org/v3/index.json"
thermo_commit="db08ff32cbb626d259b6d4d98492c83d6f2f6871"

mkdir -p "${vendor_dir}" "${managed_dir}"

download_package() {
  local file_name="$1"
  local url="https://raw.githubusercontent.com/thermofisherlsms/RawFileReader/${thermo_commit}/Libs/NetCore/Net8/${file_name}"

  if [[ ! -f "${vendor_dir}/${file_name}" ]]; then
    curl -fsSL "${url}" -o "${vendor_dir}/${file_name}"
  fi
}

download_package "ThermoFisher.CommonCore.BackgroundSubtraction.8.0.6.nupkg"
download_package "ThermoFisher.CommonCore.Data.8.0.6.nupkg"
download_package "ThermoFisher.CommonCore.MassPrecisionEstimator.8.0.6.nupkg"
download_package "ThermoFisher.CommonCore.RandomAccessReaderPlugin.8.0.6.nupkg"
download_package "ThermoFisher.CommonCore.RawfileReader.8.0.6.nupkg"

dotnet publish \
  "${repo_root}/ThermoWrapperManaged.csproj" \
  -c Release \
  -r linux-x64 \
  --self-contained false \
  -o "${managed_dir}" \
  --source "${vendor_dir}" \
  --source "${dotnet_source}"

nethost_base="$(find /usr/share/dotnet/packs/Microsoft.NETCore.App.Host.linux-x64 -mindepth 1 -maxdepth 1 -type d | sort -V | tail -n 1)"
if [[ -z "${nethost_base}" ]]; then
  echo "Could not locate Microsoft.NETCore.App.Host.linux-x64 in /usr/share/dotnet/packs" >&2
  exit 1
fi

nethost_dir="${nethost_base}/runtimes/linux-x64/native"
if [[ ! -d "${nethost_dir}" ]]; then
  echo "Could not locate nethost native directory: ${nethost_dir}" >&2
  exit 1
fi

g++ \
  -std=c++17 \
  -O2 \
  "${repo_root}/thermo_host.cpp" \
  -o "${publish_dir}/thermo_host" \
  -I"${nethost_dir}" \
  -L"${nethost_dir}" \
  -Wl,-rpath,"${nethost_dir}" \
  -lnethost \
  -ldl

echo "Built ${publish_dir}/thermo_host"
