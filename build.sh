#!/usr/bin/env bash

set -Eeuo pipefail

readonly SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
readonly BUILD_DIR="${SCRIPT_DIR}/build-release"
readonly RELEASE_DIR="${SCRIPT_DIR}/releases"
readonly JUCE_DIR="${SCRIPT_DIR}/JUCE"
readonly JUCE_TAG="8.0.10"
readonly RELEASE_BUNDLE="${RELEASE_DIR}/Ass Effect.vst3"

cleanup() {
    if [[ "${KEEP_BUILD:-0}" != "1" && -d "${BUILD_DIR}" ]]; then
        cmake -E remove_directory "${BUILD_DIR}"
    fi
}
trap cleanup EXIT

die() {
    echo "error: $*" >&2
    exit 1
}

require_command() {
    command -v "$1" >/dev/null 2>&1 || die "falta el comando '$1'"
}

check_linux_dependencies() {
    local missing=()
    local module
    local -a modules=(freetype2 fontconfig x11 xext xinerama xrandr xcursor)

    require_command pkg-config
    for module in "${modules[@]}"; do
        if ! pkg-config --exists "${module}"; then
            missing+=("${module}")
        fi
    done

    if (( ${#missing[@]} > 0 )); then
        echo "Dependencias pkg-config ausentes: ${missing[*]}" >&2
        echo "En Debian/Ubuntu instala: libfreetype6-dev libfontconfig1-dev libx11-dev libxext-dev libxinerama-dev libxrandr-dev libxcursor-dev" >&2
        exit 1
    fi
}

echo "== Ass Effect · release build =="
require_command cmake
require_command git

[[ "$(uname -s)" == "Linux" ]] || die "Ass Effect es exclusivo para Linux"
check_linux_dependencies

if [[ ! -f "${JUCE_DIR}/CMakeLists.txt" ]]; then
    if [[ -e "${JUCE_DIR}" ]]; then
        die "${JUCE_DIR} existe pero no contiene una copia válida de JUCE"
    fi
    echo "Descargando JUCE ${JUCE_TAG}..."
    git clone --depth 1 --branch "${JUCE_TAG}" https://github.com/juce-framework/JUCE.git "${JUCE_DIR}"
else
    echo "JUCE local encontrado; no se vuelve a descargar."
fi

cmake -E remove_directory "${BUILD_DIR}"
cmake -E make_directory "${BUILD_DIR}"

native_optimizations="${NATIVE_OPTIMIZATIONS:-OFF}"
case "${native_optimizations}" in
    ON|OFF) ;;
    *) die "NATIVE_OPTIMIZATIONS debe ser ON u OFF" ;;
esac

generator_args=()
if command -v ninja >/dev/null 2>&1; then
    generator_args=(-G Ninja)
fi

echo "Configurando (optimizaciones nativas: ${native_optimizations})..."
cmake -S "${SCRIPT_DIR}" -B "${BUILD_DIR}" \
    "${generator_args[@]}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DASS_EFFECT_NATIVE_OPTIMIZATIONS="${native_optimizations}"

if [[ -n "${JOBS:-}" ]]; then
    [[ "${JOBS}" =~ ^[1-9][0-9]*$ ]] || die "JOBS debe ser un entero positivo"
    build_jobs="${JOBS}"
elif command -v nproc >/dev/null 2>&1; then
    build_jobs="$(nproc)"
else
    build_jobs=2
fi

# JUCE translation units are large; a conservative cap avoids exhausting RAM on
# machines that report many logical cores. JOBS can still override this value.
if [[ -z "${JOBS:-}" && "${build_jobs}" -gt 8 ]]; then
    build_jobs=8
fi

echo "Compilando con ${build_jobs} trabajos..."
cmake --build "${BUILD_DIR}" --config Release --parallel "${build_jobs}"

built_bundle="$(find "${BUILD_DIR}" -type d -name 'Ass Effect.vst3' -print -quit)"
[[ -n "${built_bundle}" ]] || die "CMake terminó, pero no se encontró Ass Effect.vst3"

cmake -E make_directory "${RELEASE_DIR}"
if [[ -d "${RELEASE_BUNDLE}" ]]; then
    cmake -E remove_directory "${RELEASE_BUNDLE}"
fi
cmake -E copy_directory "${built_bundle}" "${RELEASE_BUNDLE}"

plugin_binary="$(find "${RELEASE_BUNDLE}" -type f \
    \( -name 'Ass Effect.so' -o -name 'Ass Effect' -o -name 'Ass Effect.vst3' \) \
    -print -quit)"
[[ -n "${plugin_binary}" && -s "${plugin_binary}" ]] || die "el bundle no contiene un binario VST3 válido"

if command -v sha256sum >/dev/null 2>&1; then
    (
        cd "${RELEASE_DIR}"
        sha256sum "${plugin_binary#${RELEASE_DIR}/}" > AssEffect-v1.0.0.sha256
    )
fi

echo
echo "Release lista: ${RELEASE_BUNDLE}"
echo "Los artefactos temporales se han eliminado."
if [[ "${KEEP_BUILD:-0}" == "1" ]]; then
    echo "KEEP_BUILD=1: se conserva ${BUILD_DIR} para diagnóstico."
fi
