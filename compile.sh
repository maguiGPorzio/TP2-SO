docker start tpe_arqui_1q2025
docker exec -it tpe_arqui_1q2025 make -C /root/Toolchain clean
docker exec -it tpe_arqui_1q2025 make -C /root/Toolchain all
docker exec -it tpe_arqui_1q2025 make -C /root clean
docker exec -it tpe_arqui_1q2025 make -C /root all
# docker stop tpe_arqui_1q2025