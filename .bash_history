ls
cd 
ls
cat ./Userland/include/usrlib.h 
clear
exit
pvs-studio-analyzer analyze -o PVS-report.log
pvs-studio-analyzer trace -- make all
ls
cd home
ls
ls
cd ..
ls
pvs-studio-analyzer analyze -o PVS-report.log
docker exec -it tpe_so_2q2025 make -C /root/Toolchain clean
exit
ls
make -C /root/Toolchain clean
make -C /root clean
pvs-studio-analyzer trace -- bash -lc 'make -C /root/Toolchain clean && make -C /root clean && \
 make -C /root/Toolchain all MM="'$MM'" && make -C /root all MM="'$MM'"'
pvs-studio-analyzer analyze -eula -o /root/PVS-report.log
plog-converter -a 'GA:1,2;64' -t fullhtml -o /root/PVS-report.html /root/PVS-report.log
pvs-studio-analyzer analyze -eula -o /root/PVS-report.log
pvs-studio-analyzer analyze -o PVS-report.log
pvs-studio-analyzer credentials "PVS-Studio Free" "FREE-FREE-FREE-FREE"
pvs-studio-analyzer credentials --view
pvs-studio-analyzer analyze -o /root/PVS-report.log
plog-converter -a 'GA:1,2;64' -t fullhtml -o PVS-report.html PVS-report.log
plog-converter -a 'GA:1,2;64' -t fullhtml -o PVS-report.html root/PVS-report.log
ls -lh /root/PVS-report.log
plog-converter -a 'GA:1,2;64' -t fullhtml -o /root/PVS-report /root/PVS-report.log
ls -lah /root/PVS-report | head
ls
exit
