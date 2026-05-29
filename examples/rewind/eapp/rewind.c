#include <stdio.h>
#include <stdlib.h>

void eapp_print(char* str)
{
  printf("[EAPP] %s\n", str);
}


int main() {
    short counter = 0;
    
    eapp_print("Rewind enclave start");

    for (counter = 0; counter < 10; counter++) {
        
        char to_prt[50];
        sprintf(to_prt, "counter = %d", counter);
        eapp_print(to_prt);

        if (counter == 5) {
            eapp_print("Simulated crash");
            //asm volatile("unimp"); // returns illegal RISC-V instruction
            __builtin_trap();
            //return 16; // Keystone::Error::EnclaveInterrupted
        }
    }

    return 0;
} 