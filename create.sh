#!/bin/bash

# Script para crear la imagen Docker y el container para TPE Arqui
echo "=== Creando imagen Docker y container para TPE Arqui ==="

# Nombre de la imagen y del container
IMAGE_NAME="agodio/itba-so-multi-platform:3.0"
CONTAINER_NAME="tpe_arqui_1q2025"

# Verificar si Docker está corriendo
if ! docker info > /dev/null 2>&1; then
    echo "Error: Docker no está corriendo. Por favor inicia Docker y vuelve a intentar."
    exit 1
fi

# Verificar si el container ya existe
if docker ps -a --format "table {{.Names}}" | grep -q "^${CONTAINER_NAME}$"; then
    echo "El container '${CONTAINER_NAME}' ya existe."
    read -p "¿Deseas eliminarlo y recrearlo? (y/n): " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        echo "Eliminando container existente..."
        docker stop ${CONTAINER_NAME} 2>/dev/null || true
        docker rm ${CONTAINER_NAME}
    else
        echo "Operación cancelada."
        exit 0
    fi
fi

# Descargar la imagen Docker si no existe
echo "Verificando imagen Docker '${IMAGE_NAME}'..."
if ! docker image inspect ${IMAGE_NAME} > /dev/null 2>&1; then
    echo "Descargando imagen Docker..."
    if docker pull --platform linux/amd64 ${IMAGE_NAME}; then
        echo "✓ Imagen Docker descargada exitosamente"
    else
        echo "✗ Error al descargar la imagen Docker"
        exit 1
    fi
else
    echo "✓ Imagen Docker ya existe localmente"
fi

# Crear y ejecutar el container
echo "Creando y ejecutando container '${CONTAINER_NAME}'..."
if docker run --platform linux/amd64 -d -v ${PWD}:/root --security-opt seccomp:unconfined -it --name ${CONTAINER_NAME} ${IMAGE_NAME}; then
    echo "✓ Container '${CONTAINER_NAME}' creado y ejecutado exitosamente"
else
    echo "✗ Error al crear el container"
    exit 1
fi

echo ""
echo "=== Setup completado ==="
echo "Container name: ${CONTAINER_NAME}"
echo "Image name: ${IMAGE_NAME}"
echo ""
echo "Para usar el container:"
echo "  docker exec -it ${CONTAINER_NAME} /bin/bash"
echo ""
echo "Para compilar el proyecto:"
echo "  ./compile.sh"
echo ""
echo "Para ejecutar el proyecto:"
echo "  ./run.sh"
