#include "usrlib.h"

int date_main(int argc, char *argv[]) {
    uint8_t buf[3];
    sys_date(buf);
    
    char number_buf[4];
    char output_buf[10]; // dd/mm/yy\n\0
    
    for (int i = 0; i < 3; i++) {
        uint64_t digits = num_to_str(buf[i], number_buf, 16);
        if (digits == 1) {
            output_buf[3*i] = '0';
            output_buf[3*i + 1] = number_buf[0];
        } else {
            output_buf[3*i] = number_buf[0];
            output_buf[3*i + 1] = number_buf[1];
        }
        output_buf[3*i + 2] = '/';
    }
    output_buf[8] = '\n';
    output_buf[9] = 0;
    print(output_buf);
    putchar(EOF);

    
    return 0;
}
