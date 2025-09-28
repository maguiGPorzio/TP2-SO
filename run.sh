#!/bin/bash

# Detectar el sistema operativo
OS=$(uname -s)
AUDIO_CONFIG=""

# Detectar si estamos en WSL
if [[ -n "$WSL_DISTRO_NAME" ]] || [[ "$(uname -r)" == *microsoft* ]] || [[ "$(uname -r)" == *WSL* ]]; then
    # WSL - usar configuración específica para WSL
    AUDIO_CONFIG="-audiodev dsound,id=speaker -machine pcspk-audiodev=speaker"
    echo "Detectado WSL - usando DirectSound"
elif [[ "$OS" == "Linux" ]] && [[ -f /proc/version ]] && grep -qi microsoft /proc/version; then
    # Otra forma de detectar WSL
    AUDIO_CONFIG="-audiodev dsound,id=speaker -machine pcspk-audiodev=speaker"
    echo "Detectado WSL (alternativo) - usando DirectSound"
else
    case $OS in
        "Darwin")
            # macOS - usar CoreAudio
            AUDIO_CONFIG="-audiodev coreaudio,id=speaker -machine pcspk-audiodev=speaker"
            echo "Detectado macOS - usando CoreAudio"
            ;;
        "Linux")
            # Linux - comprobar si PulseAudio está disponible, sino usar ALSA
            if command -v pulseaudio >/dev/null 2>&1; then
                AUDIO_CONFIG="-audiodev pa,id=speaker -machine pcspk-audiodev=speaker"
                echo "Detectado Linux con PulseAudio"
            else
                AUDIO_CONFIG="-audiodev alsa,id=speaker -machine pcspk-audiodev=speaker"
                echo "Detectado Linux - usando ALSA"
            fi
            ;;
        CYGWIN*|MINGW*|MSYS*)
            # Windows con Cygwin/MinGW/MSYS
            AUDIO_CONFIG="-audiodev dsound,id=speaker -machine pcspk-audiodev=speaker"
            echo "Detectado Windows - usando DirectSound"
            ;;
        *)
            # Sistema desconocido - usar configuración por defecto sin audio específico
            AUDIO_CONFIG=""
            echo "Sistema operativo desconocido ($OS) - ejecutando sin configuración de audio específica"
            ;;
    esac
fi

# Ejecutar QEMU con la configuración de audio apropiada
echo "Ejecutando: qemu-system-x86_64 -hda Image/x64BareBonesImage.qcow2 -m 512 $AUDIO_CONFIG"
qemu-system-x86_64 -hda Image/x64BareBonesImage.qcow2 -m 512 $AUDIO_CONFIG