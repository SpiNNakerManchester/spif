// -------------------------------------------------------------------------
//  spif-driver
//
// basic SpiNNaker peripheral interface kernel driver
//
// -------------------------------------------------------------------------
// AUTHOR
//  lap - luis.plana@manchester.ac.uk
// -------------------------------------------------------------------------
// DETAILS
//  Created on       : 21 Jul 2021
//  Last modified on : Sun  2 Oct 15:18:38 CEST 2022
//  Last modified by : lap
// -------------------------------------------------------------------------
// COPYRIGHT
//  Copyright (c) The University of Manchester, 2021-2022.
//  SpiNNaker Project
//  Advanced Processor Technologies Group
//  School of Computer Science
// -------------------------------------------------------------------------
// TODO
//  * add support for output pipes
// -------------------------------------------------------------------------

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/semaphore.h>

#include <asm/uaccess.h>


// match device with these properties
static struct of_device_id spif_driver_of_match[] = {
  { .compatible = "uom,spif", },
  { /* end of list */ },
};


// module information
MODULE_LICENSE      ("GPL");
MODULE_AUTHOR       ("lap - The University of Manchester");
MODULE_DESCRIPTION  ("spif - SpiNNaker peripheral interface driver");
MODULE_DEVICE_TABLE (of, spif_driver_of_match);


// -------------------------------------------------------------------------
// spif constants
// -------------------------------------------------------------------------
#define SPIF_DRV_VERSION     "0.1.0"
#define SPIF_DRV_NAME        "spif"

// driver ioctl unreserved magic number (seq 0xf0 - 0xff)
#define SPIF_IOCTL_TYPE      'i'
#define SPIF_IOCTL_SEQ       0xf0
#define SPIF_NUM_REGS        128

// device file permissions (r/w for user, group and others)
#define SPIF_DEV_MODE        ((umode_t) (S_IRUGO | S_IWUGO))

// interrupt configuration flags
//NOTE: must agree with xilinx-dma-controller flags!
#define SPIF_IRQ_FLAGS_MODE  (IRQF_SHARED | IRQF_TRIGGER_HIGH)

// spif registers
#define SPIF_STATUS_REG      14
#define SPIF_VERSION_REG     15

// spif register field masks and shifts
#define SPIF_SEC_CODE        0x5ec00000
#define SPIF_SEC_MSK         0xfff00000
#define SPIF_HS_MSK          0x00010000
#define SPIF_PAT_VER_MSK     0x000000ff
#define SPIF_MIN_VER_MSK     0x0000ff00
#define SPIF_MAJ_VER_MSK     0x00ff0000
#define SPIF_PIPES_MSK       0x0f000000
#define SPIF_OUTPS_MSK       0xf0000000
#define SPIF_PAT_VER_SHIFT   0
#define SPIF_MIN_VER_SHIFT   8
#define SPIF_MAJ_VER_SHIFT   16
#define SPIF_PIPES_SHIFT     24
#define SPIF_OUTPS_SHIFT     28

// spif ioctl operation fields
//NOTE: size field used to encode spif register!
#define SPIF_OP_TYPE         (SPIF_IOCTL_TYPE << _IOC_TYPESHIFT)
#define SPIF_OP_REQ_MSK      (((1 << (_IOC_NRBITS + _IOC_TYPEBITS)) - 1) << _IOC_NRSHIFT)
#define SPIF_OP_REQ_SHIFT    _IOC_NRSHIFT
#define SPIF_OP_REG_MSK      ((SPIF_NUM_REGS - 1) << _IOC_SIZESHIFT)
#define SPIF_OP_REG_SHIFT    _IOC_SIZESHIFT
#define SPIF_OP_RD           (_IOC_READ  << _IOC_DIRSHIFT)
#define SPIF_OP_WR           (_IOC_WRITE << _IOC_DIRSHIFT)
#define SPIF_OP_CMD          (_IOC_NONE  << _IOC_DIRSHIFT)
#define SPIF_OP_REQ(r)       (SPIF_OP_CMD | ((SPIF_OP_TYPE | (SPIF_IOCTL_SEQ + r))  << _IOC_NRSHIFT))

// spif operations
#define SPIF_REG_RD          SPIF_OP_REQ(0)
#define SPIF_REG_WR          SPIF_OP_REQ(1)
#define SPIF_STATUS_RD       SPIF_OP_REQ(2)
#define SPIF_TRANSFER        SPIF_OP_REQ(3)

// DMA controller registers
#define SPIF_DMAC_CR         0   // control
#define SPIF_DMAC_SR         1   // status
#define SPIF_DMAC_SA         6   // source
#define SPIF_DMAC_LEN        10  // length

// DMA controller commands/status
#define SPIF_DMAC_RESET      0x00000004
#define SPIF_DMAC_RUN        0x00000001
#define SPIF_DMAC_STOP       0x00000000
#define SPIF_DMAC_IDLE       0x00000002
#define SPIF_DMAC_IRQ_EN     0x00007000
#define SPIF_DMAC_IRQ_SET    SPIF_DMAC_IRQ_EN
#define SPIF_DMAC_IRQ_CLR    SPIF_DMAC_IRQ_EN
// -------------------------------------------------------------------------

// -------------------------------------------------------------------------
// spif data types
// -------------------------------------------------------------------------
struct spif_pipe_data {
         unsigned long    rsvd_pa;      // device memory address
         unsigned int     rsvd_sz;      // device memory size
         dev_t            dev_num;      // character device number
  struct cdev             dev_cdev;     // pipe character device 
         int              dev_open;     // device is open
         void *           dmar_va;      // dma controller registers
         int              dma_irq;      // dma controller interrupt
         int              dma_init;     // dma controller at init state
  struct semaphore        open_sem;     // grab for access to open
  struct spif_drv_data *  drv_data;     // driver data
  struct spif_pipe_data * next;         // linked list of pipes
};


// private data required for operation and cleanup
struct spif_drv_data {
  struct class *          dev_class;    // device class
         dev_t            dev_num;      // first device number
         int              pipes;        // number of hw pipes
         int              outps;        // number of hw output pipes
  struct spif_pipe_data * pipe_list;    // start of pipe linked list
         unsigned long    rsvd_pa;      // reserved memory address
         unsigned int     rsvd_sz;      // reserved memory size
         void *           apbr_va;      // spif registers address
};
// -------------------------------------------------------------------------


// -------------------------------------------------------------------------
// character device operations
// -------------------------------------------------------------------------
// ++++++++++++++++++++++++++++
// set up access to spif device
// ++++++++++++++++++++++++++++
static int spif_open (struct inode * ino, struct file * fp)
{
  struct spif_pipe_data * pipe;
  struct spif_drv_data *  drv;
         int *            dma_regs;
         int              dma_cmd;

  // find pipe data
  pipe = container_of (ino->i_cdev, struct spif_pipe_data, dev_cdev);

  // guarantee exclusive access
  if (down_interruptible (&pipe->open_sem)) {
    return -ERESTARTSYS;
  }

  // allow only one file handle to spif - disallow concurrent accesses
  if (pipe->dev_open) {
    // release mutex
    up (&pipe->open_sem);
    return -EBUSY;
  }

  // associate character device data with device file
  fp->private_data = pipe;

  // access driver data
  drv = pipe->drv_data;

  // start the DMA controller - maybe enable interrupts
  dma_regs = (int *) pipe->dmar_va;
  dma_cmd = SPIF_DMAC_RUN;

  if (pipe->dma_irq > 0) {
    dma_cmd |= SPIF_DMAC_IRQ_EN;
  }

  iowrite32 (dma_cmd, (void *) &dma_regs[SPIF_DMAC_CR]);

  // write spif buffer physical address to DMA controller
  iowrite32 ((uint) pipe->rsvd_pa, (void *) &dma_regs[SPIF_DMAC_SA]);

  // mark the DMA controller at init state
  pipe->dma_init = 1;

  // mark the device as in use to avoid concurrent accesses
  pipe->dev_open = 1;

  // release mutex
  up (&pipe->open_sem);
  return 0;
}


// ++++++++++++++++++++++++++++
// clean up and close
// ++++++++++++++++++++++++++++
static int spif_release (struct inode * ino, struct file * fp)
{
  struct spif_pipe_data * pipe;
         int *            dma_regs;

  // access device data
  pipe = (struct spif_pipe_data *) fp->private_data;

  // guarantee exclusive access
  if (down_interruptible (&pipe->open_sem)) {
    return -ERESTARTSYS;
  }

  // stop the DMA controller
  dma_regs = (int *) pipe->dmar_va;
  iowrite32 (SPIF_DMAC_STOP, (void *) &dma_regs[SPIF_DMAC_CR]);

  // mark as unsed to allow new accesses
  pipe->dev_open = 0;

  // release mutex
  up (&pipe->open_sem);
  return 0;
}


// ++++++++++++++++++++++++++++
// service spif user requests
// ++++++++++++++++++++++++++++
static long spif_ioctl (struct file * fp, unsigned int req , unsigned long arg)
{
  struct spif_pipe_data * pipe;
  struct spif_drv_data *  drv;
         int *            apb_regs;
         int *            dma_regs;
         int              dma_busy;
         int              data;
         int              loc_req;
         int              loc_reg;

  // access device and driver data
  pipe = (struct spif_pipe_data *) fp->private_data;
  drv = pipe->drv_data;

  // prepare to service request
  //NOTE: req encodes a request type and its associated register
  loc_req  = (req & SPIF_OP_REQ_MSK) >> SPIF_OP_REQ_SHIFT;

  // execute request
  switch (loc_req) {
  case SPIF_REG_RD:  // read spif register
    loc_reg  = (req & SPIF_OP_REG_MSK) >> SPIF_OP_REG_SHIFT;
    apb_regs = (int *) drv->apbr_va;

    // arg is address of return variable
    data = ioread32 ((void *) &apb_regs[loc_reg]);
    __put_user (data, (int *) arg);

    return 0;

  case SPIF_REG_WR:  // write spif register
    loc_reg  = (req & SPIF_OP_REG_MSK) >> SPIF_OP_REG_SHIFT;
    apb_regs = (int *) drv->apbr_va;

    // arg is the data to write to register
    iowrite32 ((uint) arg, (void *) &apb_regs[loc_reg]);

    return 0;

  case SPIF_STATUS_RD:  // read spif dma status
    dma_regs = (int *) pipe->dmar_va;

    // check dma status
    //NOTE: the DMA controller *not* reported idle at init state!
    dma_busy = !pipe->dma_init &&
      !(ioread32 ((void *) &dma_regs[SPIF_DMAC_SR]) & SPIF_DMAC_IDLE);

    // arg is address of return variable
    __put_user (dma_busy, (int *) arg);

    return 0;

  case SPIF_TRANSFER:  // transfer spif buffer content to SpiNNaker
    dma_regs = (int *) pipe->dmar_va;

    // check dma status
    //NOTE: the DMA controller *not* reported idle at init state!
    if (!pipe->dma_init &&
        !(ioread32 ((void *) &dma_regs[SPIF_DMAC_SR]) & SPIF_DMAC_IDLE)) {
      return -EBUSY;
    }

    // arg is transfer length in bytes
    // write length to DMA controller length register to trigger transfer
    iowrite32 ((uint) arg, (void *) &dma_regs[SPIF_DMAC_LEN]);

    // mark DMA controller as *not* in init state
    pipe->dma_init = 0;

    return 0;

  default:

    return -EINVAL;
  }
}


// ++++++++++++++++++++++++++++
// map spif buffers to user space
// ++++++++++++++++++++++++++++
static int spif_mmap (struct file * fp, struct vm_area_struct * vma)
{
  struct spif_pipe_data * pipe;
         unsigned long    vsize;

  // access device data
  pipe = (struct spif_pipe_data *) fp->private_data;

  // check that the requested size fits
  vsize  = vma->vm_end - vma->vm_start;
  if (vsize > pipe->rsvd_sz) {
    return -EINVAL;
  }

  // make sure that this "normal" memory region is not cached
  //NOTE: writecombine sets memory type to:
  //      BUFFERABLE = Normal memory / non-cacheable
  vma->vm_page_prot = pgprot_writecombine (vma->vm_page_prot);

  // request map
  return remap_pfn_range (vma, vma->vm_start, __phys_to_pfn (pipe->rsvd_pa),
                          vsize, vma->vm_page_prot);
}


// ++++++++++++++++++++++++++++
// associate spif file operations with callbacks above
// ++++++++++++++++++++++++++++
static struct file_operations spif_file_ops = {
  .owner          = THIS_MODULE,
  .open           = spif_open,
  .release        = spif_release,
  .unlocked_ioctl = spif_ioctl,
  .mmap           = spif_mmap
};
// -------------------------------------------------------------------------


// -------------------------------------------------------------------------
// DMAC interrupt handler
// -------------------------------------------------------------------------
static irqreturn_t spif_irq_handler (int irq, void * p)
{
  struct spif_pipe_data * pipe; 
         int *            dma_regs;

  pipe = (struct spif_pipe_data *) p;
  dma_regs = (int *) pipe->dmar_va;

  // check if need to service (shared) interrupt
  if ((ioread32 ((void *) &dma_regs[SPIF_DMAC_SR]) & SPIF_DMAC_IRQ_SET) == 0) {
    // interrupt was not from this device
    return IRQ_NONE;
  }

  // clear interrupt
  iowrite32 (SPIF_DMAC_IRQ_CLR, (void *) &dma_regs[SPIF_DMAC_SR]);

  return IRQ_HANDLED;
}
// -------------------------------------------------------------------------


// -------------------------------------------------------------------------
// remove/clean up auxiliary functions
// -------------------------------------------------------------------------
// ++++++++++++++++++++++++++++
// remove spif devices, one per pipe
// ++++++++++++++++++++++++++++
static void spif_pipes_remove (struct spif_drv_data * drv)
{
  struct spif_pipe_data * pipe;
  struct spif_pipe_data * next_pipe;

  // free and unregister all resources
  next_pipe = drv->pipe_list;
  while (next_pipe != NULL) {
    pipe = next_pipe;
    next_pipe = pipe->next;
    cdev_del (&pipe->dev_cdev);
    device_destroy (drv->dev_class, pipe->dev_num);
    memunmap (pipe->dmar_va);

    if (pipe->dma_irq > 0) {
      free_irq (pipe->dma_irq, pipe);
    }

    kfree (pipe);
  }

  unregister_chrdev_region (drv->dev_num, drv->pipes);
  class_destroy (drv->dev_class);
}
// -------------------------------------------------------------------------


// -------------------------------------------------------------------------
// probe auxiliary functions
// -------------------------------------------------------------------------
// ++++++++++++++++++++++++++++
// set device class node mode (rwx permissions)
// ++++++++++++++++++++++++++++
static char * spif_class_devnode (struct device * dev, umode_t * mode)
{
  if (mode != NULL) {
    *mode = SPIF_DEV_MODE;
  }

  return NULL;
}


// ++++++++++++++++++++++++++++
// initialise spif APB register access
// ++++++++++++++++++++++++++++
static int spif_apbr_init (struct device_node * pn, struct spif_drv_data * drv)
{
  struct device_node * an;
  struct resource      apbr;
         int *         apb_regs;
         int           data;
         int           rc;

  // try to find APB device tree node
  an = of_parse_phandle (pn, "apb", 0);
  if (an == NULL) {
    printk (KERN_WARNING "%s: cannot find APB node\n", SPIF_DRV_NAME);
    rc = -ENODEV;
    goto error0;
  }

  // try to find the address of the APB registers
  if (of_address_to_resource (an, 0, &apbr)) {
    printk (KERN_WARNING "%s: no APB address assigned\n", SPIF_DRV_NAME);
    rc = -ENODEV;
    goto error0;
  }

  // map APB registers into virtual memory
  drv->apbr_va = ioremap (apbr.start, resource_size (&apbr));
  if (drv->apbr_va == NULL) {
    printk (KERN_WARNING "%s: cannot remap APB registers\n", SPIF_DRV_NAME);
    rc = -ENOMEM;
    goto error0;
  }

  // check if spif board is present
  apb_regs = (int *) drv->apbr_va;
  data = ioread32 ((void *) &apb_regs[SPIF_STATUS_REG]);
  if ((data & SPIF_SEC_MSK) != SPIF_SEC_CODE) {
    printk (KERN_WARNING "%s: interface not found\n", SPIF_DRV_NAME);
    rc = -ENODEV;
    goto error1;
  }

  // report hardware version number and number of event pipes
  data = ioread32 ((void *) &apb_regs[SPIF_VERSION_REG]);
  printk (KERN_INFO
    "%s: interface found [hw version %d.%d.%d - event pipes: in/%d out/%d]\n",
    SPIF_DRV_NAME,
    (data & SPIF_MAJ_VER_MSK) >> SPIF_MAJ_VER_SHIFT,
    (data & SPIF_MIN_VER_MSK) >> SPIF_MIN_VER_SHIFT,
    (data & SPIF_PAT_VER_MSK) >> SPIF_PAT_VER_SHIFT,
    (data & SPIF_PIPES_MSK)   >> SPIF_PIPES_SHIFT,
    (data & SPIF_OUTPS_MSK)   >> SPIF_OUTPS_SHIFT
    );

  // remember number of event-processing pipes
  drv->pipes = (data & SPIF_PIPES_MSK) >> SPIF_PIPES_SHIFT;
  drv->outps = (data & SPIF_OUTPS_MSK) >> SPIF_OUTPS_SHIFT;

  return 0;

  // deal with initialisation errors here
error1:
  memunmap (drv->apbr_va);

error0:
  return rc;
}


// ++++++++++++++++++++++++++++
// initialise spif reserved memory
// ++++++++++++++++++++++++++++
static int spif_rsvd_init (struct device_node * pn, struct spif_drv_data * drv)
{
  struct device_node * rn;
  struct resource      rsvd;

  // try to find the reserved memory device tree node
  rn = of_parse_phandle (pn, "memory-region", 0);
  if (rn == NULL) {
    printk (KERN_WARNING "%s: cannot find reserved memory\n", SPIF_DRV_NAME);
    return -ENOMEM;
  }

  // try to find memory reserved for spif
  if (of_address_to_resource (rn, 0, &rsvd)) {
    printk (KERN_WARNING "%s: no memory reserved for device\n", SPIF_DRV_NAME);
    return -ENOMEM;
  }

  drv->rsvd_pa = rsvd.start;
  drv->rsvd_sz = resource_size (&rsvd);

  return 0;
}


// ++++++++++++++++++++++++++++
// initialise DMA controller
// ++++++++++++++++++++++++++++
static int spif_dmac_init (int pipe_num, struct device_node * pn,
        struct spif_pipe_data * pipe, uint is_outp)
{
  struct device_node * dn;
  struct resource      dmar;
         int *         dma_regs;
  struct resource      dmai;
         int           rc;
         char          name[] = { 'd', 'm', 'a', 'x', '\0' };

  // create DMA controller name from pipe number
  //NOTE: replace 'x' with pipe number
  name[3] = pipe_num + '0';


  // try to find DMA controller device tree node
  dn = of_parse_phandle (pn, name, 0);
  if (dn == NULL) {
    printk (KERN_WARNING "%s: cannot find DMA controller %s\n", SPIF_DRV_NAME, name);
    rc = -EIO;
    goto error0;
  }

  if (is_outp) {
    // try to find the assigned interrupt
    //NOTE: returns <= 0 on failure!
    pipe->dma_irq = of_irq_to_resource (dn, 0, &dmai);
    if (pipe->dma_irq > 0) {
      // try to get control of the interrupt
      rc = request_irq (pipe->dma_irq, &spif_irq_handler, SPIF_IRQ_FLAGS_MODE, SPIF_DRV_NAME, pipe);
      if (rc) {
        printk (KERN_WARNING "%s: DMAC %s interrupt request failed\n", SPIF_DRV_NAME, name);
        rc = -EIO;
        goto error0;
      }
    }
  } else {
	// interrupts used only on existing output pipes
    pipe->dma_irq = 0;
  }

  // try to find the address of the DMA controller registers
  if (of_address_to_resource (dn, 0, &dmar)) {
    printk (KERN_WARNING "%s: no DMAC address assigned\n", SPIF_DRV_NAME);
    rc = -EIO;
    goto error1;
  }

  // map DMAC registers into virtual memory
  pipe->dmar_va = ioremap (dmar.start, resource_size (&dmar));
  if (pipe->dmar_va == NULL) {
    printk (KERN_WARNING "%s: cannot remap DMAC registers\n", SPIF_DRV_NAME);
    rc = -ENOMEM;
    goto error1;
  }

  // reset DMA controller - resets interrupts
  dma_regs = (int *) pipe->dmar_va;
  iowrite32 (SPIF_DMAC_RESET, (void *) &dma_regs[SPIF_DMAC_CR]);

  return 0;

  // deal with initialisation errors here
error1:
  if (pipe->dma_irq > 0) {
    free_irq (pipe->dma_irq, pipe);
  }

error0:
  return rc;
}


// ++++++++++++++++++++++++++++
// create and initialise character device
// ++++++++++++++++++++++++++++
static int spif_cdev_init (struct spif_pipe_data * pipe,
  dev_t cdev_num, struct class * spif_class
  )
{
  int  rc;
  char name[] = { 's', 'p', 'i', 'f', 'x', '\0' };

  // create device name from device minor
  //NOTE: replace 'x' with minor number
  name[4] = MINOR (cdev_num) + '0';

  // create device
  //NOTE: no need for further access to this device
  if (device_create (spif_class, NULL, cdev_num, NULL, name) == NULL) {
    printk (KERN_WARNING "%s: cannot create device\n", SPIF_DRV_NAME);
    rc = -ENODEV;
    goto error0;
  }

  // mark pipe as closed
  pipe->dev_open = 0;

  // create and initialise character device - associate file ops
  //NOTE: pipe->dev_cdev is not a pointer!
  cdev_init (&pipe->dev_cdev, &spif_file_ops);
  pipe->dev_cdev.owner = THIS_MODULE;

  // make character device available for use
  rc = cdev_add (&pipe->dev_cdev, cdev_num, 1);
  if (rc < 0) {
    printk (KERN_WARNING "%s: cannot add character device\n", SPIF_DRV_NAME);
    goto error1;
  }

  return 0;

  // deal with initialisation errors here
error1:
  device_destroy (spif_class, cdev_num);

error0:
  return rc;
}


// ++++++++++++++++++++++++++++
// create and initialise spif devices, one per pipe
// ++++++++++++++++++++++++++++
static int spif_pipes_init (struct device_node * pn, struct spif_drv_data * drv)
{
  struct class *          spif_class;
         dev_t            cdev_num;
         int              cdev_major;
  struct spif_pipe_data * pipe;
  struct spif_pipe_data * next_pipe;
         unsigned long    pmem_pa;
         unsigned int     pmem_sz;
         int              is_outp;
         int              rc;
         int              i;

  // create device class
  spif_class = class_create (THIS_MODULE , "spif");
  if (spif_class == NULL) {
    printk (KERN_WARNING "%s: cannot create device class\n", SPIF_DRV_NAME);
    rc = -ENODEV;
    goto error0;
  }

  // register function to set device class permissions (mode)
  spif_class->devnode = spif_class_devnode;

  // update driver data
  drv->dev_class = spif_class;

  // allocate major/minor numbers for spif devices
  rc = alloc_chrdev_region (&cdev_num, 0, drv->pipes, "spif");
  if (rc < 0) {
    printk (KERN_WARNING "%s: cannot allocate device major/minor\n", SPIF_DRV_NAME);
    goto error1;
  }

  // update driver data
  drv->dev_num = cdev_num;

  // character device major number is constant for all devices
  cdev_major = MAJOR (cdev_num);

  // compute first pipe memory address and size
  //NOTE: reserved memory is split between input and output pipes
  pmem_pa = drv->rsvd_pa;
  pmem_sz = drv->rsvd_sz / (2 * drv->pipes);

  // assemble pipes in a linked list
  next_pipe = NULL;

  // create and initialise each event-processing pipe (dmac and character device)
  for (i = 0; i < drv->pipes; i++) {
    // allocate memory for persistent character device data
    pipe = (struct spif_pipe_data *) kmalloc (sizeof (struct spif_pipe_data), GFP_KERNEL);
    if (pipe == NULL) {
      printk (KERN_WARNING "%s: cannot allocate cdev data struct\n", SPIF_DRV_NAME);
      rc = -ENOMEM;
      goto error2;
    }

    // associate driver data with pipe data
    pipe->drv_data = drv;

    // initialise spif DMA controller
    //NOTE: interrupts are used on existing output pipes only
    is_outp = i < drv->outps ? 1 : 0;
    rc = spif_dmac_init (i, pn, pipe, is_outp);
    if (rc) {
      printk (KERN_WARNING "%s: DMAC initialisation failed\n", SPIF_DRV_NAME);
      goto error3;
    }

    // update pipe data
    cdev_num = MKDEV (cdev_major, i);
    pipe->dev_num = cdev_num;
    pipe->rsvd_pa = pmem_pa;
    pipe->rsvd_sz = pmem_sz;

    // create and initialise character device
    rc = spif_cdev_init (pipe, cdev_num, spif_class);
    if (rc) {
      printk (KERN_WARNING "%s: character device initialisation failed\n", SPIF_DRV_NAME);
      goto error4;
    }

    // initialise the open mutex
    sema_init (&pipe->open_sem, 1);

    // compute memory address for next pipe
    pmem_pa = pmem_pa + pmem_sz;

    // update pipe list data
    pipe->next = next_pipe;
    next_pipe  = pipe;
  }

  // update driver data
  drv->pipe_list = next_pipe;

  return 0;

  // deal with initialisation errors here
error4:
  // release current pipe
  memunmap (pipe->dmar_va);

  if (pipe->dma_irq > 0) {
    free_irq (pipe->dma_irq, pipe);
  }

error3:
  kfree (pipe);

error2:
  // release previously allocated pipes
  while (next_pipe != NULL) {
    pipe = next_pipe;
    next_pipe = pipe->next;
    cdev_del (&pipe->dev_cdev);
    device_destroy (spif_class, pipe->dev_num);
    memunmap (pipe->dmar_va);

    if (pipe->dma_irq > 0) {
      free_irq (pipe->dma_irq, pipe);
    }

    kfree (pipe);
  }

  unregister_chrdev_region (drv->dev_num, drv->pipes);

error1:
  class_destroy (spif_class);

error0:
  return rc;
}
// -------------------------------------------------------------------------


// -------------------------------------------------------------------------
// driver registration and probing (platform bus)
// -------------------------------------------------------------------------
// ++++++++++++++++++++++++++++
// find device resources, allocate memory and initialise character device
// ++++++++++++++++++++++++++++
static int spif_driver_probe (struct platform_device * pdev)
{
  struct spif_drv_data *  drv;
  struct device *         pdev_dev;
  struct device_node *    pnode;
         int              rc;

  // allocate memory for persistent driver data
  drv = (struct spif_drv_data *) kmalloc (sizeof (struct spif_drv_data), GFP_KERNEL);
  if (drv == NULL) {
    printk (KERN_WARNING "%s: cannot allocate driver data struct\n", SPIF_DRV_NAME);
    rc = -ENOMEM;
    goto error0;
  }

  // collect platform device resources
  pdev_dev = &(pdev->dev);
  pnode    = pdev_dev->of_node;

  // initialise access to spif APB registers
  rc = spif_apbr_init (pnode, drv);
  if (rc) {
    printk (KERN_WARNING "%s: APB register initialisation failed\n", SPIF_DRV_NAME);
    goto error1;
  }

  // initialise access to spif reserved memory
  rc = spif_rsvd_init (pnode, drv);
  if (rc) {
    printk (KERN_WARNING "%s: reserved memory initialisation failed\n", SPIF_DRV_NAME);
    goto error2;
  }

  // create and initialise spif devices, one per event-processing pipe
  rc = spif_pipes_init (pnode, drv);
  if (rc) {
    printk (KERN_WARNING "%s: device initialisation failed\n", SPIF_DRV_NAME);
    goto error2;
  }
    
  // associate driver data with platform device for exit cleanup
  dev_set_drvdata (pdev_dev, drv);

  return 0;

  // deal with initialisation errors here
error2:
  memunmap (drv->apbr_va);

error1:
  kfree (drv);

error0:
  return rc;
}


// ++++++++++++++++++++++++++++
// free driver resources
// ++++++++++++++++++++++++++++
static int spif_driver_remove (struct platform_device *pdev)
{
  struct device *         pdev_dev;
  struct spif_drv_data *  drv;
  struct spif_pipe_data * pipe;
  struct spif_pipe_data * next_pipe;

  // get driver data
  pdev_dev = &pdev->dev;
  drv = dev_get_drvdata (pdev_dev);

  // free and unregister all resources
  spif_pipes_remove (drv);

  memunmap (drv->apbr_va);

  kfree (drv);
  dev_set_drvdata (pdev_dev, NULL);

  return 0;
}


static struct platform_driver spif_driver = {
  .driver = {
    .name           = SPIF_DRV_NAME,
    .owner          = THIS_MODULE,
    .of_match_table = spif_driver_of_match,
  },
  .probe  = spif_driver_probe,
  .remove = spif_driver_remove,
};
// -------------------------------------------------------------------------


// -------------------------------------------------------------------------
// initialiser
// -------------------------------------------------------------------------
static int __init spif_driver_init (void)
{
  printk (KERN_INFO
          "%s: SpiNNaker peripheral interface driver [version %s]\n",
          SPIF_DRV_NAME, SPIF_DRV_VERSION);

  return platform_driver_probe (&spif_driver, &spif_driver_probe);
}
// -------------------------------------------------------------------------


// -------------------------------------------------------------------------
// clean up
// -------------------------------------------------------------------------
static void __exit spif_driver_exit (void)
{
  platform_driver_unregister (&spif_driver);

  printk (KERN_INFO "%s: removed\n", SPIF_DRV_NAME);
}
// -------------------------------------------------------------------------


module_init (spif_driver_init);
module_exit (spif_driver_exit);
