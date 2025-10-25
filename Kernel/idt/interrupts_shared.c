// Define the shared symbols used by ASM handlers and C code for context switching
// ASM writes current_kernel_rsp on entry; C writes switch_to_rsp to request a switch
void *current_kernel_rsp = 0;
void *switch_to_rsp = 0;
