
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
