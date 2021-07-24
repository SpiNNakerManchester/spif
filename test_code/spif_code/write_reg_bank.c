#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "axi_address_and_register_map.h"

unsigned int *reg_bank;


void setup_reg_bank () {
  int fd = open ("/dev/mem", O_RDWR);

  if (fd < 1) {
    printf ("error: unable to open /dev/mem\n");
    exit (-1);
  }

  // map the register bank - 64KB address space
  reg_bank = (unsigned int *) mmap (
    NULL, 0x10000, PROT_READ | PROT_WRITE, MAP_SHARED, fd, APB_BRIDGE
    );

  // close /dev/mem and drop root privileges
  close (fd);
  if (setuid (getuid ()) < 0) {
    exit (-1);
  }
}


int main (int argc, char* argv[]) {

  // setup memory-mapped register bank access and drop privileges
  setup_reg_bank ();

  // check that enough arguments are present
  if (argc < 3) {
    printf ("usage: %s <register_offset> <data>\n", argv[0]);
    exit (-1);
  }

  uint reg_addr = atoi (argv[1]);
  uint reg_data = atoi (argv[2]);

  // write to a register
  reg_bank[reg_addr] = reg_data;
  printf ("reg[%u] = %d (0x%08x)\n", reg_addr, reg_data, reg_data);

  return 0;
}
