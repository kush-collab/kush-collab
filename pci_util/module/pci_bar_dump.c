#include <linux/module.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/io.h>
#include <linux/moduleparam.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/ctype.h>

#define DRIVER_VERSION "12.0"
#define DEFAULT_DUMP_SIZE 64
#define PROC_NAME "pci_bar_dump"

static char *bdf = NULL;
static unsigned int offset = 0;
static unsigned int dump_size = DEFAULT_DUMP_SIZE;

static void __iomem *bar0_virt;
static unsigned int bar0_offset;
static struct pci_dev *pdev;
static struct proc_dir_entry *proc_entry;

module_param(bdf, charp, 0000);
module_param(offset, uint, 0000);
module_param(dump_size, uint, 0000);

MODULE_PARM_DESC(bdf, "PCI BDF in format 0000:00:00.0");
MODULE_PARM_DESC(offset, "Offset within BAR0 to dump");
MODULE_PARM_DESC(dump_size, "Number of bytes to dump from offset (default: 64)");

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kubandi");
MODULE_DESCRIPTION("PCI BAR0 register dump via /proc");
MODULE_VERSION(DRIVER_VERSION);

static int dump_proc_show(struct seq_file *m, void *v)
{
    u8 buf[16];
    int i, j, len;

    for (i = 0; i < dump_size; i += 16) {
        len = (i + 16 <= dump_size) ? 16 : dump_size - i;
        for (j = 0; j < len; j++)
            buf[j] = ioread8(bar0_virt + bar0_offset + i + j);

        seq_printf(m, "%08x  ", offset + i);
        for (j = 0; j < 16; j++) {
            if (j < len)
                seq_printf(m, "%02x ", buf[j]);
            else
                seq_puts(m, "   ");
            if (j == 7)
                seq_putc(m, ' ');
        }

        seq_puts(m, " |");
        for (j = 0; j < len; j++)
            seq_putc(m, isprint(buf[j]) ? buf[j] : '.');
        seq_puts(m, "|\n");
    }

    return 0;
}

static int dump_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, dump_proc_show, NULL);
}

static const struct proc_ops dump_proc_ops = {
    .proc_open = dump_proc_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};

static int __init pci_bar_dump_init(void)
{
    unsigned int domain, bus, dev, func, devfn;
    resource_size_t bar0_phys, bar0_size;

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
    bar0_size = pci_resource_len(pdev, 0);

    if (!bar0_phys || !bar0_size) {
        pr_err("BAR0 not available or size invalid\n");
        pci_disable_device(pdev);
        return -ENODEV;
    }

    if (offset >= bar0_size) {
        pr_err("Offset 0x%X exceeds BAR0 size %pa\n", offset, &bar0_size);
        pci_disable_device(pdev);
        return -EINVAL;
    }

    if (offset + dump_size > bar0_size) {
        pr_warn("Requested range exceeds BAR0 size (%pa), adjusting dump_size.\n", &bar0_size);
        dump_size = bar0_size - offset;
    }

    bar0_virt = ioremap(bar0_phys, bar0_size);
    if (!bar0_virt) {
        pr_err("ioremap failed\n");
        pci_disable_device(pdev);
        return -EIO;
    }

    bar0_offset = offset;

    proc_entry = proc_create(PROC_NAME, 0444, NULL, &dump_proc_ops);
    if (!proc_entry) {
        pr_err("Failed to create /proc/%s\n", PROC_NAME);
        iounmap(bar0_virt);
        pci_disable_device(pdev);
        return -ENOMEM;
    }

    pr_info("pci_bar_dump v%s initialized. Dump ready at /proc/%s (offset=0x%X, size=%u)\n",
            DRIVER_VERSION, PROC_NAME, offset, dump_size);
    return 0;
}

static void __exit pci_bar_dump_exit(void)
{
    proc_remove(proc_entry);
    iounmap(bar0_virt);
    pci_disable_device(pdev);
    pci_dev_put(pdev);
    pr_info("pci_bar_dump module unloaded.\n");
}

module_init(pci_bar_dump_init);
module_exit(pci_bar_dump_exit);
