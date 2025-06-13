#include <linux/module.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/io.h>
#include <linux/moduleparam.h>

#define DUMP_SIZE 64  // bytes

static char *bdf = NULL;
static unsigned int offset = 0;
module_param(bdf, charp, 0000);
module_param(offset, uint, 0000);
MODULE_PARM_DESC(bdf, "PCI BDF in format 0000:00:00.0");
MODULE_PARM_DESC(offset, "Offset within BAR0 to dump");

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kubandi");
MODULE_DESCRIPTION("PCI BAR0 register dump driver");

static int __init pci_bar_dump_init(void)
{
    struct pci_dev *pdev = NULL;
    unsigned int domain, bus, devfn;
    unsigned int dev, func;
    void __iomem *bar0_virt = NULL;
    resource_size_t bar0_phys;
    int i;
    u32 val;

    if (!bdf || sscanf(bdf, "%x:%x:%x.%x", &domain, &bus, &dev, &func) != 4) {
        pr_err("Invalid or missing BDF. Expected format: 0000:00:00.0\n");
        return -EINVAL;
    }

    devfn = PCI_DEVFN(dev, func);
    pdev = pci_get_domain_bus_and_slot(domain, bus, devfn);
    if (!pdev) {
        pr_err("PCI device %s not found.\n", bdf);
        return -ENODEV;
    }

    if (pci_enable_device(pdev)) {
        pr_err("Failed to enable PCI device %s\n", bdf);
        return -EIO;
    }

    bar0_phys = pci_resource_start(pdev, 0);
    if (!bar0_phys) {
        pr_err("BAR0 not available for PCI device %s\n", bdf);
        pci_disable_device(pdev);
        return -ENODEV;
    }

    bar0_virt = ioremap(bar0_phys + offset, DUMP_SIZE);
    if (!bar0_virt) {
        pr_err("ioremap failed at BAR0 + 0x%x\n", offset);
        pci_disable_device(pdev);
        return -EIO;
    }

    pr_info("Dumping 64 bytes from BAR0 + 0x%X of PCI device %s\n", offset, bdf);
    for (i = 0; i < DUMP_SIZE; i += 4) {
        val = ioread32(bar0_virt + i);
        pr_info("Offset 0x%03X: 0x%08X\n", offset + i, val);
    }

    iounmap(bar0_virt);
    pci_disable_device(pdev);
    pci_dev_put(pdev);

    return 0;
}

static void __exit pci_bar_dump_exit(void)
{
    pr_info("pci_bar_dump module unloaded.\n");
}

module_init(pci_bar_dump_init);
module_exit(pci_bar_dump_exit);
