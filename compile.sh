docker start tpe_so_2q2025
docker exec -it tpe_so_2q2025 make -C /root/Toolchain clean
docker exec -it tpe_so_2q2025 make -C /root/Toolchain all
docker exec -it tpe_so_2q2025 make -C /root clean
docker exec -it tpe_so_2q2025 make -C /root all
# docker stop tpe_so_2q2025