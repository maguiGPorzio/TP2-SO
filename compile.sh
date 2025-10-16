
if [ "$1" == "buddy" ]; then
    MM="USE_BUDDY"
else
    MM=""
fi

# Name of the Docker container
CONTAINER_NAME="tpe_so_2q2025"

# Start the Docker container
docker start $CONTAINER_NAME

# Clean and build the project in the specified directories
docker exec -it $CONTAINER_NAME make -C /root/Toolchain clean
docker exec -it $CONTAINER_NAME make -C /root/Toolchain all MM="$MM"
MAKE_ROOT_EXIT_CODE=$?

# Execute the make commands
docker exec -it $CONTAINER_NAME make -C /root clean
docker exec -it $CONTAINER_NAME make -C /root all  MM="$MM"
MAKE_TOOLCHAIN_EXIT_CODE=$?


GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m' 

# Check if both make commands were successful
if [[ $MAKE_TOOLCHAIN_EXIT_CODE -eq 0 && $MAKE_ROOT_EXIT_CODE -eq 0 ]]; then
    echo -e "${GREEN}Compilation successful!${NC}"
    echo -e "${GREEN}Run './run.sh' to start the kernel${NC}"
else
    echo -e "${RED}Compilation failed!${NC}"
fi

# docker stop $CONTAINER_NAME