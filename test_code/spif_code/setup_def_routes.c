#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "axi_address_and_register_map.h"


uint const DEF_ENTRIES  = 8;
uint const DEF_KEYS[]   = {0, 1, 2, 3, 4, 5, 6, 7};
uint const DEF_MASKS[]  = {7, 7, 7, 7, 7, 7, 7, 7};
uint const DEF_ROUTES[] = {0, 1, 2, 3, 4, 5, 6, 7};


unsigned int * reg_bank;
unsigned int * key_regs;
unsigned int * mask_regs;
unsigned int * route_regs;


void setup_reg_bank () {
  int fd = open ("/dev/mem", O_RDWR);

  if (fd < 1) {
    printf ("error: unable to open /dev/mem\n");
    exit (-1);
  }

  // map the register bank - 16 regs
  reg_bank = (unsigned int *) mmap (
    NULL, 0x10000, PROT_READ | PROT_WRITE, MAP_SHARED, fd, APB_BRIDGE
    );

  // close /dev/mem and drop root privileges
  close (fd);
  if (setuid (getuid ()) < 0) {
    exit (-1);
  }

  // map the keys registers - 16 regs
  key_regs = &(reg_bank[16]);

  // map the mask registers - 16 regs
  mask_regs = &(reg_bank[32]); 

  // map the route registers  - 16 regs
  route_regs = &(reg_bank[48]);
}


int main (int argc, char* argv[]) {

  // setup memory-mapped register bank access and drop privileges
  setup_reg_bank ();

  // setup default routing
  for (uint i = 0; i < DEF_ENTRIES; i++) {
    key_regs[i]   = DEF_KEYS[i];
    mask_regs[i]  = DEF_MASKS[i];
    route_regs[i] = DEF_ROUTES[i];
    printf ("route %u: key/%u mask/%u route/%u\n",
	    i, DEF_KEYS[i], DEF_MASKS[i], DEF_ROUTES[i]);
  }

  return 0;
}
