from __future__ import annotations

import argparse
import ctypes as ct
from ctypes import wintypes as wt
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import re
import statistics
import struct
import sys
import time
import uuid


def energy_interval(first: tuple[int, int], second: tuple[int, int]) -> dict:
    energy = second[0] - first[0]
    elapsed = second[1] - first[1]
    if energy < 0 or elapsed <= 0:
        return {"status": "counter_reset_or_unchanged_time", "joules": None, "watts": None}
    joules = energy * 3.6e-9
    return {"status": "ok", "joules": joules, "watts": joules / (elapsed * 1e-7)}


def engine_totals(items: list[dict], pids: set[int]) -> dict:
    engines: dict[str, float] = {}
    rejected = 0
    for item in items:
        match = re.fullmatch(r"pid_(\d+)_(luid_.+?_phys_\d+)_eng_(\d+)_engtype_(.+)", item["instance"])
        if not match or int(match[1]) not in pids:
            continue
        value = item["value"]
        if item["status"] not in (0, 1) or value is None or not math.isfinite(value) or value < 0:
            rejected += 1
            continue
        key = f"{match[2]}:eng_{match[3]}:{match[4]}"
        engines[key] = engines.get(key, 0.0) + value
    busiest: dict[str, float] = {}
    for key, value in engines.items():
        adapter = key.split(":", 1)[0]
        busiest[adapter] = max(busiest.get(adapter, 0.0), value)
    return {"status": "partial" if rejected else "ok", "engines_percent": engines,
            "busiest_engine_percent": busiest, "rejected_instances": rejected}


def metric_summary(values: list[float]) -> dict:
    if not values:
        return {"count": 0, "mean": None, "median": None, "p95": None, "stdev": None}
    ordered = sorted(values)
    return {"count": len(values), "mean": statistics.mean(values), "median": statistics.median(values),
            "p95": ordered[max(0, math.ceil(len(values) * 0.95) - 1)],
            "stdev": statistics.stdev(values) if len(values) > 1 else None}


def process_counter_map(raw: dict) -> dict:
    counters = raw.get("counters", {})
    values = {key: {item["instance"]: item["value"] for item in entries
                    if item["status"] in (0, 1) and item["value"] is not None and math.isfinite(item["value"])}
              for key, entries in counters.items()}
    result = {}
    for instance, pid in values.get("process_id", {}).items():
        if pid <= 0 or pid != int(pid):
            continue
        result[int(pid)] = {"instance": instance,
                            "cpu_core_percent": values.get("process_cpu", {}).get(instance),
                            "elapsed_seconds": values.get("process_elapsed", {}).get(instance),
                            "private_bytes": values.get("process_private_bytes", {}).get(instance),
                            "working_set_bytes": values.get("process_working_set", {}).get(instance)}
    return result


def process_interval(item: dict, old: dict, elapsed: float, cpu_count: int,
                     counter: dict | None, prior_counter: dict | None) -> dict:
    delta = None
    if item["status"] == old["status"] == "ok" and item["creation_filetime"] == old["creation_filetime"]:
        value = item["cpu_seconds"] - old["cpu_seconds"]
        if math.isfinite(value) and value >= 0:
            delta = value
    result = item | {"cpu_interval_seconds": delta, "cpu_machine_percent":
                     delta / elapsed / cpu_count * 100 if delta is not None else None,
                     "cpu_source": "process_times" if delta is not None else "unavailable"}
    counter_identity = (counter and prior_counter and counter["instance"] == prior_counter["instance"] and
                        counter["elapsed_seconds"] is not None and prior_counter["elapsed_seconds"] is not None and
                        counter["elapsed_seconds"] >= prior_counter["elapsed_seconds"])
    if delta is None and item["status"] == "open_failed" and counter_identity:
        value = counter["cpu_core_percent"]
        if value is not None and math.isfinite(value) and value >= 0:
            result |= {"status": "ok_pdh", "cpu_source": "pdh_process_v2",
                       "cpu_machine_percent": value / cpu_count,
                       "counter_instance": counter["instance"],
                       "process_elapsed_seconds": counter["elapsed_seconds"],
                       "private_bytes": counter["private_bytes"], "working_set_bytes": counter["working_set_bytes"]}
    return result


def selected_counter_rows(raw: dict, pids: set[int], processes: dict) -> dict:
    selected = {}
    instances = {value["instance"] for pid, value in processes.items() if pid in pids}
    for key, items in raw.get("counters", {}).items():
        if key.startswith("process_"):
            selected[key] = [item for item in items if item["instance"] in instances]
        else:
            selected[key] = [item for item in items if re.match(r"pid_(\d+)_", item["instance"]) and
                             int(re.match(r"pid_(\d+)_", item["instance"])[1]) in pids]
    return raw | {"counters": selected}


def bind(dll, name, result, *arguments):
    function = getattr(dll, name)
    function.restype = result
    function.argtypes = arguments
    return function


class FileTime(ct.Structure):
    _fields_ = [("low", wt.DWORD), ("high", wt.DWORD)]

    def ticks(self):
        return self.low | self.high << 32


class ProcessMemory(ct.Structure):
    _fields_ = [("size", wt.DWORD), ("page_faults", wt.DWORD)] + [
        (name, ct.c_size_t) for name in ("peak_working_set", "working_set", "peak_paged_pool", "paged_pool",
                                       "peak_nonpaged_pool", "nonpaged_pool", "pagefile", "peak_pagefile", "private")]


class Process:
    def __init__(self, kernel, psapi, label, pid):
        self.label, self.pid = label, pid
        self.kernel = kernel
        self.handle = kernel.OpenProcess(0x1000 | 0x100000, False, pid)
        self.get_memory = psapi.GetProcessMemoryInfo
        self.open_error = ct.get_last_error() if not self.handle else None
        self.path = None
        self.sha256 = None
        if self.handle:
            size = wt.DWORD(32768)
            path = ct.create_unicode_buffer(size.value)
            if kernel.QueryFullProcessImageNameW(self.handle, 0, path, ct.byref(size)):
                self.path = path.value
                try:
                    self.sha256 = hashlib.sha256(Path(self.path).read_bytes()).hexdigest()
                except OSError:
                    pass

    def sample(self):
        if not self.handle:
            return {"status": "open_failed", "error": self.open_error, "cpu_seconds": None}
        if self.kernel.WaitForSingleObject(self.handle, 0) != 258:
            return {"status": "exited", "cpu_seconds": None}
        created, exited, kernel, user = (FileTime() for _ in range(4))
        if not self.kernel.GetProcessTimes(self.handle, ct.byref(created), ct.byref(exited), ct.byref(kernel), ct.byref(user)):
            return {"status": "times_failed", "error": ct.get_last_error(), "cpu_seconds": None}
        memory = ProcessMemory()
        memory.size = ct.sizeof(memory)
        has_memory = self.get_memory(self.handle, ct.byref(memory), memory.size)
        return {"status": "ok", "creation_filetime": created.ticks(),
                "cpu_seconds": (kernel.ticks() + user.ticks()) * 1e-7,
                "working_set_bytes": memory.working_set if has_memory else None,
                "private_bytes": memory.private if has_memory else None,
                "memory_error": None if has_memory else ct.get_last_error()}

    def close(self):
        if self.handle:
            self.kernel.CloseHandle(self.handle)
            self.handle = None


class FormattedValue(ct.Structure):
    _fields_ = [("status", wt.DWORD), ("value", ct.c_double)]


class FormattedItem(ct.Structure):
    _fields_ = [("name", wt.LPWSTR), ("formatted", FormattedValue)]


class Counters:
    def __init__(self):
        self.dll = ct.WinDLL("pdh", use_last_error=True)
        bind(self.dll, "PdhOpenQueryW", wt.LONG, wt.LPCWSTR, ct.c_size_t, ct.POINTER(wt.HANDLE))
        bind(self.dll, "PdhAddEnglishCounterW", wt.LONG, wt.HANDLE, wt.LPCWSTR, ct.c_size_t, ct.POINTER(wt.HANDLE))
        bind(self.dll, "PdhCollectQueryData", wt.LONG, wt.HANDLE)
        bind(self.dll, "PdhGetFormattedCounterArrayW", wt.LONG, wt.HANDLE, wt.DWORD, ct.POINTER(wt.DWORD),
             ct.POINTER(wt.DWORD), ct.c_void_p)
        bind(self.dll, "PdhCloseQuery", wt.LONG, wt.HANDLE)
        self.query = wt.HANDLE()
        if self.dll.PdhOpenQueryW(None, 0, ct.byref(self.query)):
            raise OSError("PdhOpenQueryW failed")
        self.handles = {}
        self.errors = {}
        for key, path in {"gpu_engines": r"\GPU Engine(*)\Utilization Percentage",
                          "gpu_dedicated_bytes": r"\GPU Process Memory(*)\Dedicated Usage",
                          "gpu_shared_bytes": r"\GPU Process Memory(*)\Shared Usage",
                          "process_id": r"\Process V2(*)\Process ID",
                          "process_cpu": r"\Process V2(*)\% Processor Time",
                          "process_elapsed": r"\Process V2(*)\Elapsed Time",
                          "process_private_bytes": r"\Process V2(*)\Private Bytes",
                          "process_working_set": r"\Process V2(*)\Working Set",
                          "system_cpu": r"\Processor Information(*)\% Processor Time"}.items():
            handle = wt.HANDLE()
            status = self.dll.PdhAddEnglishCounterW(self.query, path, 0, ct.byref(handle))
            if status:
                self.errors[key] = status & 0xffffffff
            else:
                self.handles[key] = handle

    def sample(self):
        collected = self.dll.PdhCollectQueryData(self.query)
        result = {"collect_status": collected & 0xffffffff, "counters": {}, "errors": dict(self.errors)}
        if collected:
            return result
        for key, handle in self.handles.items():
            for _ in range(3):
                size, count = wt.DWORD(), wt.DWORD()
                status = self.dll.PdhGetFormattedCounterArrayW(handle, 0x200 | 0x8000, ct.byref(size), ct.byref(count), None)
                if status & 0xffffffff != 0x800007d2 or not size.value:
                    result["errors"][key] = status & 0xffffffff
                    break
                buffer = ct.create_string_buffer(size.value)
                status = self.dll.PdhGetFormattedCounterArrayW(handle, 0x200 | 0x8000, ct.byref(size), ct.byref(count), buffer)
                if status & 0xffffffff == 0x800007d2:
                    continue
                if status:
                    result["errors"][key] = status & 0xffffffff
                    break
                items = ct.cast(buffer, ct.POINTER(FormattedItem))
                result["counters"][key] = [
                    {"instance": items[i].name, "status": items[i].formatted.status,
                     "value": items[i].formatted.value if items[i].formatted.status in (0, 1) and
                     math.isfinite(items[i].formatted.value) else None} for i in range(count.value)]
                break
            else:
                result["errors"][key] = 0x800007d2
        return result

    def close(self):
        if self.query:
            self.dll.PdhCloseQuery(self.query)
            self.query = None


class InterfaceData(ct.Structure):
    _fields_ = [("size", wt.DWORD), ("guid", ct.c_ubyte * 16), ("flags", wt.DWORD), ("reserved", ct.c_size_t)]


class EnergyMeters:
    def __init__(self, kernel):
        self.kernel, self.devices, self.errors = kernel, [], []
        setup = ct.WinDLL("setupapi", use_last_error=True)
        bind(setup, "SetupDiGetClassDevsW", wt.HANDLE, ct.c_void_p, wt.LPCWSTR, wt.HWND, wt.DWORD)
        bind(setup, "SetupDiEnumDeviceInterfaces", wt.BOOL, wt.HANDLE, ct.c_void_p, ct.c_void_p, wt.DWORD, ct.POINTER(InterfaceData))
        bind(setup, "SetupDiGetDeviceInterfaceDetailW", wt.BOOL, wt.HANDLE, ct.POINTER(InterfaceData), ct.c_void_p,
             wt.DWORD, ct.POINTER(wt.DWORD), ct.c_void_p)
        bind(setup, "SetupDiDestroyDeviceInfoList", wt.BOOL, wt.HANDLE)
        guid = ct.create_string_buffer(uuid.UUID("45bd8344-7ed6-49cf-a440-c276c933b053").bytes_le)
        devices = setup.SetupDiGetClassDevsW(guid, None, None, 0x12)
        if devices == ct.c_void_p(-1).value:
            self.errors.append({"stage": "enumerate", "error": ct.get_last_error()})
            return
        try:
            index = 0
            while True:
                entry = InterfaceData()
                entry.size = ct.sizeof(entry)
                if not setup.SetupDiEnumDeviceInterfaces(devices, None, guid, index, ct.byref(entry)):
                    if ct.get_last_error() != 259:
                        self.errors.append({"stage": "interface", "error": ct.get_last_error()})
                    break
                index += 1
                size = wt.DWORD()
                setup.SetupDiGetDeviceInterfaceDetailW(devices, ct.byref(entry), None, 0, ct.byref(size), None)
                if size.value < 8 or size.value > 1_048_576:
                    self.errors.append({"stage": "path_size", "error": ct.get_last_error()})
                    continue
                detail = ct.create_string_buffer(size.value)
                ct.cast(detail, ct.POINTER(wt.DWORD))[0] = 8 if ct.sizeof(ct.c_void_p) == 8 else 6
                if not setup.SetupDiGetDeviceInterfaceDetailW(devices, ct.byref(entry), detail, size, None, None):
                    self.errors.append({"stage": "path", "error": ct.get_last_error()})
                    continue
                path = ct.wstring_at(ct.addressof(detail) + 4)
                handle = kernel.CreateFileW(path, 0x80000000, 3, None, 3, 0, None)
                if handle == ct.c_void_p(-1).value:
                    self.errors.append({"stage": "open", "error": ct.get_last_error()})
                    continue
                try:
                    version = struct.unpack("<H", self.read(handle, 0, 2))[0]
                    metadata_size = struct.unpack("<I", self.read(handle, 1, 4))[0]
                    if metadata_size > 1_048_576:
                        raise ValueError("Energy metadata exceeds limit")
                    metadata = self.read(handle, 2, metadata_size)
                    channels = []
                    if version == 1:
                        unit, name_size = struct.unpack_from("<I", metadata)[0], struct.unpack_from("<H", metadata, 70)[0]
                        if name_size % 2 or 72 + name_size > len(metadata):
                            raise ValueError("Invalid energy meter metadata")
                        channels.append({"unit": unit, "name": metadata[72:72 + name_size].decode("utf-16-le").rstrip("\0")})
                    elif version == 2:
                        count = struct.unpack_from("<H", metadata, 66)[0]
                        offset = 68
                        for _ in range(count):
                            unit, name_size = struct.unpack_from("<IH", metadata, offset)
                            end = offset + 6 + name_size
                            if end > len(metadata) or name_size % 2:
                                raise ValueError("Invalid energy channel metadata")
                            channels.append({"unit": unit, "name": metadata[offset + 6:end].decode("utf-16-le").rstrip("\0")})
                            offset = end
                    else:
                        raise ValueError(f"Unsupported energy interface version {version}")
                    if not channels or any(channel["unit"] != 0 for channel in channels):
                        raise ValueError("Unsupported energy measurement units")
                    self.devices.append({"handle": handle, "version": version, "channels": channels})
                except (OSError, ValueError, struct.error, UnicodeError) as error:
                    self.errors.append({"stage": "metadata", "error": str(error)})
                    kernel.CloseHandle(handle)
        finally:
            setup.SetupDiDestroyDeviceInfoList(devices)

    def read(self, handle, operation, size):
        buffer = ct.create_string_buffer(size)
        returned = wt.DWORD()
        if not self.kernel.DeviceIoControl(handle, 0x224000 | operation << 2, None, 0, buffer, size, ct.byref(returned), None):
            raise ct.WinError(ct.get_last_error())
        return buffer.raw[:returned.value]

    def sample(self):
        result = {}
        for device_index, device in enumerate(self.devices):
            try:
                data = self.read(device["handle"], 3, len(device["channels"]) * 16)
                if len(data) != len(device["channels"]) * 16:
                    raise ValueError("Incomplete energy measurement")
                for index, channel in enumerate(device["channels"]):
                    energy, timestamp = struct.unpack_from("<QQ", data, index * 16)
                    key = f"{device_index}:{index}:{channel['name']}"
                    result[key] = {"status": "ok", "name": channel["name"], "absolute_picowatt_hours": energy,
                                   "absolute_time_100ns": timestamp}
            except (OSError, ValueError) as error:
                result[f"{device_index}:unavailable"] = {"status": "read_failed", "error": str(error)}
        return result

    def close(self):
        for device in self.devices:
            self.kernel.CloseHandle(device["handle"])
        self.devices.clear()


class Nvidia:
    def __init__(self):
        self.devices, self.errors, self.dll = [], [], None
        try:
            dll = ct.CDLL("nvml.dll")
            bind(dll, "nvmlInit_v2", ct.c_int)
            bind(dll, "nvmlShutdown", ct.c_int)
            bind(dll, "nvmlDeviceGetCount_v2", ct.c_int, ct.POINTER(ct.c_uint))
            bind(dll, "nvmlDeviceGetHandleByIndex_v2", ct.c_int, ct.c_uint, ct.POINTER(ct.c_void_p))
            bind(dll, "nvmlDeviceGetName", ct.c_int, ct.c_void_p, ct.c_void_p, ct.c_uint)
            bind(dll, "nvmlDeviceGetUUID", ct.c_int, ct.c_void_p, ct.c_void_p, ct.c_uint)
            bind(dll, "nvmlDeviceGetPowerUsage", ct.c_int, ct.c_void_p, ct.POINTER(ct.c_uint))
            status = dll.nvmlInit_v2()
            if status:
                raise OSError(f"nvmlInit_v2 status {status}")
            self.dll = dll
            count = ct.c_uint()
            if dll.nvmlDeviceGetCount_v2(ct.byref(count)):
                raise OSError("NVML device enumeration failed")
            for index in range(count.value):
                handle = ct.c_void_p()
                status = dll.nvmlDeviceGetHandleByIndex_v2(index, ct.byref(handle))
                if status:
                    self.errors.append(f"NVML device {index} handle status {status}")
                    continue
                name, identity = ct.create_string_buffer(128), ct.create_string_buffer(128)
                name_status = dll.nvmlDeviceGetName(handle, name, len(name))
                uuid_status = dll.nvmlDeviceGetUUID(handle, identity, len(identity))
                if name_status or uuid_status:
                    self.errors.append(f"NVML device {index} identity statuses {name_status}, {uuid_status}")
                self.devices.append({"handle": handle, "index": index, "name": name.value.decode(errors="replace"),
                                     "uuid": identity.value.decode(errors="replace") if uuid_status == 0 else f"index:{index}"})
        except (OSError, AttributeError) as error:
            self.errors.append(str(error))

    def sample(self):
        result = []
        for device in self.devices:
            power = ct.c_uint()
            status = self.dll.nvmlDeviceGetPowerUsage(device["handle"], ct.byref(power))
            result.append({key: value for key, value in device.items() if key != "handle"} |
                          {"status": status, "board_power_watts": power.value / 1000 if status == 0 else None})
        return result

    def close(self):
        if self.dll:
            self.dll.nvmlShutdown()
            self.dll = None


def windows_libraries():
    kernel, psapi = ct.WinDLL("kernel32", use_last_error=True), ct.WinDLL("psapi", use_last_error=True)
    bind(kernel, "OpenProcess", wt.HANDLE, wt.DWORD, wt.BOOL, wt.DWORD)
    bind(kernel, "CloseHandle", wt.BOOL, wt.HANDLE)
    bind(kernel, "GetProcessTimes", wt.BOOL, wt.HANDLE, *([ct.POINTER(FileTime)] * 4))
    bind(kernel, "WaitForSingleObject", wt.DWORD, wt.HANDLE, wt.DWORD)
    bind(kernel, "QueryFullProcessImageNameW", wt.BOOL, wt.HANDLE, wt.DWORD, wt.LPWSTR, ct.POINTER(wt.DWORD))
    bind(kernel, "GetActiveProcessorCount", wt.DWORD, wt.WORD)
    bind(kernel, "CreateFileW", wt.HANDLE, wt.LPCWSTR, wt.DWORD, wt.DWORD, ct.c_void_p, wt.DWORD, wt.DWORD, wt.HANDLE)
    bind(kernel, "DeviceIoControl", wt.BOOL, wt.HANDLE, wt.DWORD, ct.c_void_p, wt.DWORD, ct.c_void_p,
         wt.DWORD, ct.POINTER(wt.DWORD), ct.c_void_p)
    bind(psapi, "GetProcessMemoryInfo", wt.BOOL, wt.HANDLE, ct.POINTER(ProcessMemory), wt.DWORD)
    return kernel, psapi


def collect(arguments):
    kernel, psapi = windows_libraries()
    resources = []
    try:
        processes = []
        for value in arguments.process:
            label, pid = value.rsplit(":", 1)
            process = Process(kernel, psapi, label, int(pid))
            processes.append(process)
            resources.append(process)
        observer = Process(kernel, psapi, "observer", os.getpid())
        resources.append(observer)
        counters = Counters()
        resources.append(counters)
        meters = EnergyMeters(kernel)
        resources.append(meters)
        nvidia = Nvidia()
        resources.append(nvidia)
        cpu_count = kernel.GetActiveProcessorCount(0xffff)
        if not cpu_count:
            raise OSError("Logical processor count unavailable")
        path = Path(arguments.output).resolve()
        path.parent.mkdir(parents=True, exist_ok=True)
        with path.open("x", encoding="utf-8") as stream:
            def write(value):
                stream.write(json.dumps(value, allow_nan=False) + "\n")
                stream.flush()

            write({"type": "metadata", "schema": 1, "scenario": arguments.scenario, "revision": arguments.revision,
                   "utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()), "platform": platform.platform(),
                   "logical_processors": cpu_count, "interval_seconds": arguments.interval,
                   "duration_seconds": arguments.seconds,
                   "processes": [{"label": p.label, "pid": p.pid, "path": p.path, "sha256": p.sha256} for p in processes],
                   "observer_pid": observer.pid,
                   "energy_errors": meters.errors, "nvidia_errors": nvidia.errors,
                   "cpu_unit": "percent_of_all_logical_processors",
                   "power_scope": "whole_package_and_whole_gpu_board_not_process_attribution",
                   "gpu_memory_validation": "supplemental_counters_require_crosscheck",
                   "latency": {"status": "unavailable", "reason": "No validated displayed-presentation timing channel"}})
            previous_process = {p.label: p.sample() for p in processes + [observer]}
            previous_energy = meters.sample()
            previous_counter_processes = process_counter_map(counters.sample())
            started = previous_time = time.perf_counter()
            samples = []
            gpu_power = []
            index = 1
            while time.perf_counter() - started < arguments.seconds:
                time.sleep(max(0, started + min(index * arguments.interval, arguments.seconds) - time.perf_counter()))
                sampling_started = time.perf_counter()
                raw = counters.sample()
                counter_processes = process_counter_map(raw)
                all_gpu_items = raw.get("counters", {}).get("gpu_engines", [])
                gpu_pids = {int(match[1]) for item in all_gpu_items
                            if (match := re.match(r"pid_(\d+)_", item["instance"]))}
                energy = meters.sample()
                gpu_power = nvidia.sample()
                current = {p.label: p.sample() for p in processes + [observer]}
                now = time.perf_counter()
                elapsed = now - previous_time
                row = {"type": "sample", "qpc_seconds": now, "elapsed_seconds": now - started, "interval_seconds": elapsed,
                       "collector_elapsed_seconds": now - sampling_started, "processes": {}, "observer": {},
                       "gpu_counters": selected_counter_rows(raw, {p.pid for p in processes} | {observer.pid}, counter_processes),
                       "system_gpu": engine_totals(all_gpu_items, gpu_pids) if "gpu_engines" in raw["counters"] else {"status": "unavailable"},
                       "energy_channels": {}, "nvidia": gpu_power}
                system_cpu = [item for item in raw["counters"].get("system_cpu", []) if item["instance"] == "_Total"]
                row["system_cpu_machine_percent"] = system_cpu[0]["value"] if system_cpu and system_cpu[0]["status"] in (0, 1) else None
                row["gpu_counters"]["counters"]["system_cpu"] = system_cpu
                alive = set()
                combined_cpu_percent = 0.0
                complete_cpu = True
                for p in processes + [observer]:
                    item, old = current[p.label], previous_process[p.label]
                    counter, prior_counter = counter_processes.get(p.pid), previous_counter_processes.get(p.pid)
                    result = process_interval(item, old, elapsed, cpu_count, counter, prior_counter)
                    if p is observer:
                        row["observer"] = result
                    else:
                        row["processes"][p.label] = result
                        if result["status"] in ("ok", "ok_pdh"):
                            alive.add(p.pid)
                        if result["cpu_machine_percent"] is None:
                            complete_cpu = False
                        else:
                            combined_cpu_percent += result["cpu_machine_percent"]
                row["combined_cpu_machine_percent"] = combined_cpu_percent if complete_cpu else None
                if "gpu_engines" in raw["counters"]:
                    missing = sorted({p.pid for p in processes} - alive)
                    row["combined_gpu"] = engine_totals(raw["counters"]["gpu_engines"], alive)
                    row["combined_gpu"] |= {"status": "partial" if missing or row["combined_gpu"]["rejected_instances"] else "ok",
                                            "included_pids": sorted(alive), "unavailable_pids": missing}
                else:
                    row["combined_gpu"] = {"status": "unavailable"}
                for key, value in energy.items():
                    old = previous_energy.get(key)
                    interval = {"status": "unavailable", "joules": None, "watts": None}
                    if old and value["status"] == old["status"] == "ok":
                        interval = energy_interval((old["absolute_picowatt_hours"], old["absolute_time_100ns"]),
                                                   (value["absolute_picowatt_hours"], value["absolute_time_100ns"]))
                    row["energy_channels"][key] = value | {"interval": interval}
                write(row)
                samples.append(row)
                previous_process, previous_energy, previous_time = current, energy, now
                previous_counter_processes = counter_processes
                index = max(index + 1, math.floor((now - started) / arguments.interval) + 1)
            write({"type": "summary", "samples": len(samples),
                   "combined_cpu_machine_percent": metric_summary([s["combined_cpu_machine_percent"] for s in samples
                                                                    if s["combined_cpu_machine_percent"] is not None]),
                   "observer_cpu_machine_percent": metric_summary([s["observer"]["cpu_machine_percent"] for s in samples
                                                                    if s["observer"]["cpu_machine_percent"] is not None]),
                   "collector_elapsed_seconds": metric_summary([s["collector_elapsed_seconds"] for s in samples]),
                   "system_cpu_machine_percent": metric_summary([s["system_cpu_machine_percent"] for s in samples
                                                                  if s["system_cpu_machine_percent"] is not None]),
                   "package_power_watts": {key: metric_summary([s["energy_channels"][key]["interval"]["watts"] for s in samples
                                                               if key in s["energy_channels"] and
                                                               s["energy_channels"][key]["interval"]["watts"] is not None])
                                           for key, value in previous_energy.items() if re.fullmatch(r"RAPL_Package\d+_PKG", value.get("name", ""))},
                   "gpu_board_power_watts": {device["uuid"]: metric_summary([value["board_power_watts"] for s in samples
                                                                           for value in s["nvidia"] if value["uuid"] == device["uuid"] and
                                                                           value["board_power_watts"] is not None]) for device in gpu_power},
                   "combined_gpu_engines_percent": {key: metric_summary([s["combined_gpu"]["engines_percent"][key] for s in samples
                                                                         if key in s["combined_gpu"].get("engines_percent", {})])
                                                    for key in {key for s in samples for key in s["combined_gpu"].get("engines_percent", {})}}})
        print(path)
    finally:
        for resource in reversed(resources):
            resource.close()


def main():
    parser = argparse.ArgumentParser(description="Read-only Windows workload capture; never launches or terminates a measured process.")
    parser.add_argument("--process", action="append", required=True, metavar="LABEL:PID")
    parser.add_argument("--scenario", required=True)
    parser.add_argument("--revision", default="unknown")
    parser.add_argument("--seconds", type=float, default=30)
    parser.add_argument("--interval", type=float, default=1)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()
    if sys.platform != "win32":
        parser.error("Windows is required")
    if not 1 <= args.seconds <= 3600 or not 0.2 <= args.interval <= 60:
        parser.error("Duration must be 1..3600 seconds and interval 0.2..60 seconds")
    labels = []
    pids = set()
    for value in args.process:
        try:
            label, pid = value.rsplit(":", 1)
            if not label or not 0 < int(pid) <= 0xffffffff or int(pid) == os.getpid() or int(pid) in pids or label == "observer" or label in labels:
                raise ValueError()
            labels.append(label)
            pids.add(int(pid))
        except ValueError:
            parser.error("Processes need unique non-observer labels and unique positive DWORD PIDs other than the collector")
    collect(args)


if __name__ == "__main__":
    main()
