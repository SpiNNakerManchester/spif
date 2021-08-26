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
//  Last modified on : Sat 21 Aug 19:10:17 BST 2021
//  Last modified by : lap
// -------------------------------------------------------------------------
// COPYRIGHT
//  Copyright (c) The University of Manchester, 2021.
//  SpiNNaker Project
//  Advanced Processor Technologies Group
//  School of Computer Science
// -------------------------------------------------------------------------
// TODO
//  * everything
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

#include <asm/uaccess.h>


/// module information
MODULE_LICENSE ("GPL");
MODULE_AUTHOR  ("lap - The University of Manchester");
MODULE_DESCRIPTION
    ("spif - SpiNNaker peripheral interface driver");


// match devices with these properties
static struct of_device_id spif_driver_of_match[] = {
  { .compatible = "uom,spif", },
  { /* end of list */ },
};

MODULE_DEVICE_TABLE (of, spif_driver_of_match);


// -------------------------------------------------------------------------
// spif constants
// -------------------------------------------------------------------------
#define SPIF_DRV_VERSION     "0.0.1"
#define SPIF_DRV_NAME        "spif-driver"

// driver ioctl unreserved magic number (seq 0xf0 - 0xff)
#define SPIF_IOCTL_TYPE      'i'
#define SPIF_IOCTL_SEQ       0xf0
#define SPIF_NUM_REGS        128
// device file permissions (r/w for user, group and others)
#define SPIF_DEV_MODE        ((umode_t) (S_IRUGO|S_IWUGO))
#define SPIF_IRQ_FLAGS_MODE  0x84

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
#define SPIF_PIPES_MSK       0xff000000
#define SPIF_PAT_VER_SHIFT   0
#define SPIF_MIN_VER_SHIFT   8
#define SPIF_MAJ_VER_SHIFT   16
#define SPIF_PIPES_SHIFT     24

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

// spif operations
#define SPIF_OP_REQ(r)       (SPIF_OP_CMD | ((SPIF_OP_TYPE | (SPIF_IOCTL_SEQ + r))  << _IOC_NRSHIFT))
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
#define SPIF_DMAC_RESET      0x04
#define SPIF_DMAC_RUN        0x01
#define SPIF_DMAC_STOP       0x00
#define SPIF_DMAC_IDLE       0x02
#define SPIF_DMAC_IRQ_EN     0x7000
#define SPIF_DMAC_IRQ_CLR    SPIF_DMAC_IRQ_EN
// -------------------------------------------------------------------------

// -------------------------------------------------------------------------
// spif data types
// -------------------------------------------------------------------------
// private data required for operation and cleanup
struct spif_driver_prv {
         dev_t          dev_num;
  struct class *        dev_class;
  struct device *       dev;
  struct cdev           dev_cdev;
         int            dev_open;
         int            dma_busy;
         unsigned long  rsvd_pa;
         unsigned int   rsvd_sz;
         void *         apbr_va;
         void *         dmar_va;
         int            dma_irq;
};
// -------------------------------------------------------------------------


// -------------------------------------------------------------------------
// character device operations
// -------------------------------------------------------------------------
static int spif_open (struct inode * ino, struct file * fp)
{
  struct spif_driver_prv * prv;
         int *             dma_regs;

  // associate driver private data with device file
  fp->private_data = container_of (ino->i_cdev, struct spif_driver_prv, dev_cdev);

  // access driver private data
  prv = (struct spif_driver_prv *) fp->private_data;

  // allow only one file handle to spif - disallow concurrent accesses
  if (prv->dev_open) {
    return -EBUSY;
  }

  // start the DMA controller - enable interrupts
  dma_regs = (int *) prv->dmar_va;
//lap  iowrite32 ((SPIF_DMAC_RUN | SPIF_DMAC_IRQ_EN), (void *) &dma_regs[SPIF_DMAC_CR]);
  iowrite32 (SPIF_DMAC_RUN, (void *) &dma_regs[SPIF_DMAC_CR]);

  // write spif buffer physical address to DMA controller
  iowrite32 ((uint) prv->rsvd_pa, (void *) &dma_regs[SPIF_DMAC_SA]);

  // mark the DMA controller as not busy
  prv->dma_busy = 0;

  // mark the device as in use to avoid concurrent accesses
  prv->dev_open = 1;

  return 0;
}


// clean up
static int spif_release (struct inode * ino, struct file * fp)
{
  struct spif_driver_prv * prv;
         int *             dma_regs;

  // access driver private data
  prv = (struct spif_driver_prv *) fp->private_data;

  // stop the DMA controller
  dma_regs = (int *) prv->dmar_va;
  iowrite32 (SPIF_DMAC_STOP, (void *) &dma_regs[SPIF_DMAC_CR]);

  // mark the DMA controller as not busy
  prv->dma_busy = 0;

  // mark as unsed to allow new accesses
  prv->dev_open = 0;

  return 0;
}


static long spif_ioctl (struct file * fp, unsigned int req , unsigned long arg)
{
  struct spif_driver_prv * prv;
         int *             apb_regs;
         int *             dma_regs;
	 int               data;
	 int               loc_req;
	 int               loc_reg;

  // access driver private data
  prv = (struct spif_driver_prv *) fp->private_data;

  // prepare to service request
  //NOTE: req encodes a request type and its associated register
  loc_req  = (req & SPIF_OP_REQ_MSK) >> SPIF_OP_REQ_SHIFT;

  // execute request
  switch (loc_req) {
  case SPIF_REG_RD:  // read spif register
    loc_reg  = (req & SPIF_OP_REG_MSK) >> SPIF_OP_REG_SHIFT;
    apb_regs = (int *) prv->apbr_va;

    // arg is address of return variable
    data = ioread32 ((void *) &apb_regs[loc_reg]);
    __put_user (data, (int *) arg);

    return 0;

  case SPIF_REG_WR:  // write spif register
    loc_reg  = (req & SPIF_OP_REG_MSK) >> SPIF_OP_REG_SHIFT;
    apb_regs = (int *) prv->apbr_va;

    // arg is the data to write to register
    iowrite32 ((uint) arg, (void *) &apb_regs[loc_reg]);

    return 0;

  case SPIF_STATUS_RD:  // read spif dma status
    dma_regs = (int *) prv->dmar_va;

    // check dma status
    //NOTE: the DMA controller is *not* idle before the first transfer!
    if (ioread32 ((void *) &dma_regs[SPIF_DMAC_SR]) & SPIF_DMAC_IDLE) {
      prv->dma_busy = 0;
    }

    // arg is address of return variable
    __put_user (prv->dma_busy, (int *) arg);

    return 0;

  case SPIF_TRANSFER:  // transfer spif buffer content to SpiNNaker
    // check dma status
    //NOTE: the DMA controller is *not* idle before the first transfer!
    dma_regs = (int *) prv->dmar_va;
    if (prv->dma_busy && !(ioread32 ((void *) &dma_regs[SPIF_DMAC_SR]) & SPIF_DMAC_IDLE)) {
      return -EBUSY;
    }

    // arg is transfer length in bytes
    // write length to DMA controller length regiter to trigger transfer
    iowrite32 ((uint) arg, (void *) &dma_regs[SPIF_DMAC_LEN]);

    // mark DMA controller as busy
    prv->dma_busy = 1;

    return 0;

  default:

    return -EINVAL;
  }
}


static int spif_mmap (struct file * fp, struct vm_area_struct * vma)
{
  struct spif_driver_prv * prv;

  unsigned long vsize;

  // access driver private data
  prv = (struct spif_driver_prv *) fp->private_data;

  // check that the requested size fits
  vsize  = vma->vm_end - vma->vm_start;
  if (vsize > prv->rsvd_sz) {
    return -EINVAL;
  }

  // make sure that this memory region is not cached
  vma->vm_page_prot = pgprot_noncached (vma->vm_page_prot);

  // request map
  return remap_pfn_range (vma, vma->vm_start, __phys_to_pfn (prv->rsvd_pa),
			  vsize, vma->vm_page_prot);
}


static struct file_operations dm_fops = {
  .owner          = THIS_MODULE,
  .open           = spif_open,
  .release        = spif_release,
  .unlocked_ioctl = spif_ioctl,
  .mmap           = spif_mmap
};
// -------------------------------------------------------------------------


// -------------------------------------------------------------------------
// probe auxiliary functions
// -------------------------------------------------------------------------
// set device class node mode (rwx permissions)
static char * spif_class_devnode (struct device * dev, umode_t * mode)
{
  if (mode != NULL) {
    *mode = SPIF_DEV_MODE;
  }

  return NULL;
}


// DMAC interrupt handler
static irqreturn_t spif_irq (int irq, void * p)
{
  struct spif_driver_prv * prv; 
         int *             dma_regs;

  // mark DMA controller as not busy
  prv = (struct spif_driver_prv *) p;
  prv->dma_busy = 0;

  // clear interrupt
  dma_regs = (int *) prv->dmar_va;
  iowrite32 (SPIF_DMAC_IRQ_CLR, (void *) &dma_regs[SPIF_DMAC_SR]);

  return IRQ_HANDLED;
}


// initialise spif reserved memory
static int spif_rsvd_init (struct device_node * pn, struct spif_driver_prv * prv)
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

  prv->rsvd_pa = rsvd.start;
  prv->rsvd_sz = resource_size (&rsvd);

  return 0;
}


// initialise spif APB register access
static int spif_apbr_init (struct device_node * pn, struct spif_driver_prv * prv)
{
  struct device_node * an;
  struct resource      apbr;
         int *         apb_regs;
	 int           data;

  // try to find APB device tree node
  an = of_parse_phandle (pn, "apb", 0);
  if (an == NULL) {
    printk (KERN_WARNING "%s: cannot find APB node\n", SPIF_DRV_NAME);
    return -ENODEV;
  }

  // try to find the address of the APB registers
  if (of_address_to_resource (an, 0, &apbr)) {
    printk (KERN_WARNING "%s: no APB address assigned\n", SPIF_DRV_NAME);
    return -ENODEV;
  }

  // map APB registers into virtual memory
  prv->apbr_va = ioremap (apbr.start, resource_size (&apbr));
  if (prv->apbr_va == NULL) {
    printk (KERN_WARNING "%s: cannot remap APB registers\n", SPIF_DRV_NAME);
    return -ENOMEM;
  }

  // check if spif board is present
  apb_regs = (int *) prv->apbr_va;
  data = ioread32 ((void *) &apb_regs[SPIF_STATUS_REG]);
  if ((data & SPIF_SEC_MSK) != SPIF_SEC_CODE) {
    printk (KERN_WARNING "%s: interface board not found\n", SPIF_DRV_NAME);
    return -ENODEV;
  }

  // report hardware version number and number of event pipes
  data = ioread32 ((void *) &apb_regs[SPIF_VERSION_REG]);
  printk (KERN_INFO
	  "%s: spif board found [hw version %d.%d.%d - event pipes: %d]\n",
          SPIF_DRV_NAME,
	  (data & SPIF_MAJ_VER_MSK) >> SPIF_MAJ_VER_SHIFT,
	  (data & SPIF_MIN_VER_MSK) >> SPIF_MIN_VER_SHIFT,
	  (data & SPIF_PAT_VER_MSK) >> SPIF_PAT_VER_SHIFT,
	  (data & SPIF_PIPES_MSK)   >> SPIF_PIPES_SHIFT
    );

  return 0;
}


// initialise DMA controller
static int spif_dmac_init (struct device_node * pn, struct spif_driver_prv * prv)
{
  struct device_node * dn;
  struct resource      dmar;
         int *         dma_regs;
  struct resource      dmai;

  int rc;

  // try to find DMA controller device tree node
  dn = of_parse_phandle (pn, "dma", 0);
  if (dn == NULL) {
    printk (KERN_WARNING "%s: cannot find DMA controller\n", SPIF_DRV_NAME);
    rc = -EIO;
    goto error0;
  }

  // try to find the assigned interrupt
  //NOTE: returns 0 on failure!
  prv->dma_irq = of_irq_to_resource (dn, 0, &dmai);
  if (prv->dma_irq == 0) {
    printk (KERN_WARNING "%s: no DMAC interrupt found\n", SPIF_DRV_NAME);
    rc = -EIO;
    goto error0;
  }

  // try to get control of the interrupt
  rc = request_irq (prv->dma_irq, &spif_irq, SPIF_IRQ_FLAGS_MODE, SPIF_DRV_NAME, prv);
  if (rc) {
    printk (KERN_WARNING "%s: DMAC interrupt request failed\n", SPIF_DRV_NAME);
    rc = -EIO;
    goto error0;
  }

  // try to find the address of the DMA controller registers
  if (of_address_to_resource (dn, 0, &dmar)) {
    printk (KERN_WARNING "%s: no DMAC address assigned\n", SPIF_DRV_NAME);
    rc = -EIO;
    goto error1;
  }

  // map DMAC registers into virtual memory
  prv->dmar_va = ioremap (dmar.start, resource_size (&dmar));
  if (prv->dmar_va == NULL) {
    printk (KERN_WARNING "%s: cannot remap DMAC registers\n", SPIF_DRV_NAME);
    rc = -ENOMEM;
    goto error1;
  }

  // reset DMA controller - resets interrupts
  dma_regs = (int *) prv->dmar_va;
  iowrite32 (SPIF_DMAC_RESET, (void *) &dma_regs[SPIF_DMAC_CR]);

  // mark DMA controller as not busy
  prv->dma_busy = 0;

  return 0;

  // deal with initialisation errors here
error1:
  free_irq (prv->dma_irq, prv);

error0:

  return rc;
}


// create and initialise character device
static int spif_cdev_init (struct spif_driver_prv * prv)
{
         int      rc;
         dev_t    spif_num;
  struct class *  spif_class;
  struct device * spif_dev;

  // create and initialise character device - associate file ops
  cdev_init (&prv->dev_cdev, &dm_fops);
  prv->dev_cdev.owner = THIS_MODULE;

  // allocate major/minor numbers for spif device
  rc = alloc_chrdev_region (&spif_num, 0, 1, "spif");
  if (rc < 0) {
    printk (KERN_WARNING "%s: cannot allocate device major/minor\n", SPIF_DRV_NAME);
    goto error0;
  }

  // update private data
  prv->dev_num = spif_num;

  // create device class
  spif_class = class_create (THIS_MODULE , "spif");
  if (spif_class == NULL) {
    printk (KERN_WARNING "%s: cannot create device class\n", SPIF_DRV_NAME);
    rc = -ENODEV;
    goto error1;
  }

  // update private data
  prv->dev_class = spif_class;

  // register function to set device class permissions (mode)
  spif_class->devnode = spif_class_devnode;

  // create device
  spif_dev = device_create (spif_class, NULL, spif_num, NULL, "spif");
  if (spif_dev == NULL) {
    printk (KERN_WARNING "%s: cannot create device\n", SPIF_DRV_NAME);
    rc = -ENODEV;
    goto error2;
  }

  // update private data
  prv->dev = spif_dev;
  prv->dev_open = 0;

  // make character device available for use
  rc = cdev_add (&prv->dev_cdev, spif_num, 1);
  if (rc < 0) {
    printk (KERN_WARNING "%s: cannot add character device\n", SPIF_DRV_NAME);
    goto error3;
  }

  return 0;

  // deal with initialisation errors here
error3:
  device_destroy (spif_class, spif_num);

error2:
  class_destroy (spif_class);

error1:
  unregister_chrdev_region (spif_num, 1);

error0:
  return rc;
}
// -------------------------------------------------------------------------


// -------------------------------------------------------------------------
// driver registration and probing (platform bus)
// -------------------------------------------------------------------------
// find device resources, allocate memory and initialise character device
static int spif_driver_probe (struct platform_device * pdev)
{
         int               rc;
  struct spif_driver_prv * prv;
  struct device *          pdev_dev;

  // allocate memory for persistent driver private data
  prv = (struct spif_driver_prv *) kmalloc (sizeof (struct spif_driver_prv), GFP_KERNEL);
  if (prv == NULL) {
    printk (KERN_WARNING "%s: cannot allocate private struct\n", SPIF_DRV_NAME);
    rc = -ENOMEM;
    goto error0;
  }

  // collect platform device resources
  pdev_dev = &(pdev->dev);

  // initialise access to reserved memory
  rc = spif_rsvd_init (pdev_dev->of_node, prv);
  if (rc) {
    printk (KERN_WARNING "%s: reserved memory initialisation failed\n", SPIF_DRV_NAME);
    goto error1;
  }

  // initialise access to spif APB registers
  rc = spif_apbr_init (pdev_dev->of_node, prv);
  if (rc) {
    printk (KERN_WARNING "%s: APB register initialisation failed\n", SPIF_DRV_NAME);
    goto error1;
  }

  // initialise spif DMA controller
  rc = spif_dmac_init (pdev_dev->of_node, prv);
  if (rc) {
    printk (KERN_WARNING "%s: DMAC initialisation failed\n", SPIF_DRV_NAME);
    goto error2;
  }

  // create and initialise character device
  rc = spif_cdev_init (prv);
  if (rc) {
    printk (KERN_WARNING "%s: character device initialisation failed\n", SPIF_DRV_NAME);
    goto error2;
  }


  // associate private data with platform device for exit cleanup
  dev_set_drvdata (pdev_dev, prv);

  return 0;

  // deal with initialisation errors here
error2:
  memunmap (prv->apbr_va);

error1:
  kfree (prv);

error0:
  return rc;
}


// free driver resources
static int spif_driver_remove (struct platform_device *pdev)
{
  struct device *          pdev_dev;
  struct spif_driver_prv * prv;

  // get private data
  pdev_dev = &pdev->dev;
  prv = dev_get_drvdata (pdev_dev);

  // free and unregister all resources
  memunmap (prv->dmar_va);
  free_irq (prv->dma_irq, prv);
  memunmap (prv->apbr_va);

  device_destroy (prv->dev_class, prv->dev_num);
  class_destroy (prv->dev_class);
  cdev_del (&prv->dev_cdev);
  unregister_chrdev_region (prv->dev_num, 1);

  kfree (prv);
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
