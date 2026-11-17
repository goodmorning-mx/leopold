"""Resolve guarded runtime modes for private firmware builds."""
from __future__ import annotations

from collections.abc import Mapping
import ipaddress
import re
from urllib.parse import urlsplit


def resolve_build_mode(environment: Mapping[str, str]) -> dict[str, object]:
    local_development = environment.get("LEOPOLD_LOCAL_DEVELOPMENT") == "1"
    ota_test_mode = environment.get("LEOPOLD_OTA_TEST_MODE") == "1"
    local_ota_endpoint = environment.get("LEOPOLD_LOCAL_OTA_TEST_ENDPOINT") == "1"
    if ota_test_mode and not local_development:
        raise ValueError("OTA test mode requires the local-development UI guards.")
    if local_ota_endpoint and not ota_test_mode:
        raise ValueError("A local OTA endpoint requires OTA test mode.")

    if ota_test_mode:
        runtime_mode = "local-development-ota-test"
    elif local_development:
        runtime_mode = "local-development"
    else:
        runtime_mode = "standard"

    compile_defines = []
    if local_development:
        compile_defines.append("LEOPOLD_LOCAL_DEVELOPMENT=1")
    if ota_test_mode:
        compile_defines.append("LEOPOLD_OTA_TEST_MODE=1")

    ota_polling_enabled = not local_development or ota_test_mode
    build_identity = {
        "local_development": local_development,
        "ota_test_mode": ota_test_mode,
        "ota_polling_enabled": ota_polling_enabled,
        "runtime_mode": runtime_mode,
    }
    if local_ota_endpoint:
        build_identity["local_ota_test_endpoint"] = True

    return {
        "local_development": local_development,
        "ota_test_mode": ota_test_mode,
        "local_ota_test_endpoint": local_ota_endpoint,
        "ota_polling_enabled": ota_polling_enabled,
        "runtime_mode": runtime_mode,
        "compile_defines": compile_defines,
        "build_identity": build_identity,
    }


def private_usb_environment(
    environment: Mapping[str, str], ota_test_mode: bool,
    local_ota_test_endpoint: bool = False,
) -> dict[str, str]:
    result = dict(environment)
    result["LEOPOLD_LOCAL_DEVELOPMENT"] = "1"
    # Do not inherit an opt-in from the caller's shell for a default USB build.
    result.pop("LEOPOLD_OTA_TEST_MODE", None)
    result.pop("LEOPOLD_LOCAL_OTA_TEST_ENDPOINT", None)
    if ota_test_mode:
        result["LEOPOLD_OTA_TEST_MODE"] = "1"
    if local_ota_test_endpoint:
        if not ota_test_mode:
            raise ValueError("A local OTA endpoint requires OTA test mode.")
        result["LEOPOLD_LOCAL_OTA_TEST_ENDPOINT"] = "1"
    return result


def validate_nonproduction_api_origin(
    config_text: str, *, allow_local_endpoint: bool = False
) -> None:
    match = re.search(
        r'^\s*#\s*define\s+LEOPOLD_API_BASE_URL\s+"([^"\\]+)"\s*$',
        config_text,
        re.M,
    )
    if match is None:
        raise ValueError("OTA test mode requires a configured HTTPS API origin.")

    try:
        parsed = urlsplit(match.group(1))
        hostname = (parsed.hostname or "").lower()
        has_port = parsed.port is not None
    except ValueError as error:
        raise ValueError("OTA test mode API origin is malformed.") from error

    common_invalid = (
        parsed.scheme != "https"
        or not hostname
        or parsed.username is not None
        or parsed.password is not None
        or parsed.path != ""
        or parsed.query
        or parsed.fragment
    )
    if common_invalid:
        raise ValueError("OTA test mode requires an HTTPS API origin without credentials or path.")

    if allow_local_endpoint:
        try:
            address = ipaddress.ip_address(hostname)
        except ValueError:
            address = None
        valid_local_dns = (
            hostname.endswith(".local")
            and len(hostname) <= 253
            and all(
                1 <= len(label) <= 63
                and re.fullmatch(r"[a-z0-9](?:[a-z0-9-]*[a-z0-9])?", label) is not None
                for label in hostname.split(".")
            )
        )
        private_ipv4 = (
            isinstance(address, ipaddress.IPv4Address)
            and any(address in network for network in (
                ipaddress.ip_network("10.0.0.0/8"),
                ipaddress.ip_network("172.16.0.0/12"),
                ipaddress.ip_network("192.168.0.0/16"),
            ))
        )
        if not (valid_local_dns or private_ipv4) or not has_port or parsed.port is None or parsed.port < 1024:
            raise ValueError("A local OTA endpoint requires a private IPv4 address or valid .local DNS name and port 1024–65535.")
        return

    if (
        re.fullmatch(r"[a-z0-9.-]+", hostname) is None
        or any(not label for label in hostname.split("."))
        or has_port
    ):
        raise ValueError("OTA test mode requires an HTTPS API origin without credentials or path.")

    nonproduction_labels = {"dev", "development", "stage", "staging", "test", "testing"}
    host_labels = hostname.split(".")[:-1]
    if not nonproduction_labels.intersection(host_labels):
        raise ValueError("OTA test mode requires a dev, staging, or test API hostname.")


def append_compile_defines(platform_env: object, mode: Mapping[str, object]) -> None:
    defines = mode["compile_defines"]
    if defines:
        platform_env.Append(CPPDEFINES=defines)
