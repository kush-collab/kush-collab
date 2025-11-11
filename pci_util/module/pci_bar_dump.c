#include <linux/module.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/io.h>
#include <linux/moduleparam.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/ctype.h>
#include <linux/slab.h>

#define DRIVER_VERSION "1.2"
#define DEFAULT_DUMP_SIZE 64
#define PROC_DUMP_NAME "pci_bar_dump"
#define PROC_CTRL_NAME "pci_bar_dump_control"

static char current_bdf[16] = "0000:00:00.0";
static unsigned int offset = 0;
static unsigned int dump_size = DEFAULT_DUMP_SIZE;

static void __iomem *bar0_virt;
static unsigned int bar0_offset;
static struct pci_dev *pdev;
static struct proc_dir_entry *proc_dump_entry;
static struct proc_dir_entry *proc_ctrl_entry;

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kubandi");
MODULE_DESCRIPTION("PCI BAR0 register dump via /proc with dynamic control");
MODULE_VERSION(DRIVER_VERSION);

static void cleanup_mapping(void)
{
    if (bar0_virt) {
        iounmap(bar0_virt);
        bar0_virt = NULL;
    }
    if (pdev) {
        pci_disable_device(pdev);
        pci_dev_put(pdev);
        pdev = NULL;
    }
}

static int setup_pci_mapping(const char *bdf_str, unsigned int off, unsigned int size)
{
    unsigned int domain, bus, dev, func, devfn;
    resource_size_t bar0_phys, bar0_size;

    cleanup_mapping();

    if (sscanf(bdf_str, "%x:%x:%x.%x", &domain, &bus, &dev, &func) != 4) {
        pr_err("Invalid BDF format. Use 0000:00:00.0\n");
        return -EINVAL;
    }

    devfn = PCI_DEVFN(dev, func);
    pdev = pci_get_domain_bus_and_slot(domain, bus, devfn);
    if (!pdev) {
        pr_err("PCI device %s not found.\n", bdf_str);
        return -ENODEV;
    }

    if (pci_enable_device(pdev)) {
        pr_err("Failed to enable PCI device %s\n", bdf_str);
        pci_dev_put(pdev);
        pdev = NULL;
        return -EIO;
    }

    bar0_phys = pci_resource_start(pdev, 0);
    bar0_size = pci_resource_len(pdev, 0);

    if (!bar0_phys || !bar0_size) {
        pr_err("BAR0 not available for %s\n", bdf_str);
        pci_disable_device(pdev);
        pci_dev_put(pdev);
        pdev = NULL;
        return -ENODEV;
    }

    if (off >= bar0_size) {
        pr_err("Offset 0x%X exceeds BAR0 size %pa\n", off, &bar0_size);
        cleanup_mapping();
        return -EINVAL;
    }

    if (off + size > bar0_size) {
        pr_warn("Requested range exceeds BAR0, adjusting dump_size.\n");
        size = bar0_size - off;
    }

    bar0_virt = ioremap(bar0_phys, bar0_size);
    if (!bar0_virt) {
        pr_err("ioremap failed\n");
        cleanup_mapping();
        return -EIO;
    }

    bar0_offset = off;
    offset = off;
    dump_size = size;
    strscpy(current_bdf, bdf_str, sizeof(current_bdf));

    pr_info("pci_bar_dump mapped %s offset=0x%X size=%u\n", current_bdf, offset, dump_size);
    return 0;
}

static int dump_proc_show(struct seq_file *m, void *v)
{
    u8 buf[16];
    int i, j, len;

    if (!bar0_virt)
        return seq_puts(m, "No valid PCI device mapped. Use /proc/pci_bar_dump_control to configure.\n");

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

static ssize_t control_proc_write(struct file *file, const char __user *ubuf,
                                  size_t count, loff_t *ppos)
{
    char *kbuf;
    char new_bdf[16];
    unsigned int new_offset = 0, new_size = DEFAULT_DUMP_SIZE;
    int ret;

    if (count > 255)
        return -EINVAL;

    kbuf = kzalloc(count + 1, GFP_KERNEL);
    if (!kbuf)
        return -ENOMEM;

    if (copy_from_user(kbuf, ubuf, count)) {
        kfree(kbuf);
        return -EFAULT;
    }

    kbuf[count] = '\0';

    ret = sscanf(kbuf, "bdf=%15s offset=%u dump_size=%u", new_bdf, &new_offset, &new_size);
    kfree(kbuf);

    if (ret < 2) {
        pr_err("Invalid input. Format: bdf=0000:01:00.0 offset=256 dump_size=128\n");
        return -EINVAL;
    }

    if (setup_pci_mapping(new_bdf, new_offset, new_size) != 0)
        return -EIO;

    return count;
}

static const struct proc_ops dump_proc_ops = {
    .proc_open = dump_proc_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};

static const struct proc_ops control_proc_ops = {
    .proc_write = control_proc_write,
};

static int __init pci_bar_dump_init(void)
{
    proc_dump_entry = proc_create(PROC_DUMP_NAME, 0444, NULL, &dump_proc_ops);
    proc_ctrl_entry = proc_create(PROC_CTRL_NAME, 0222, NULL, &control_proc_ops);

    if (!proc_dump_entry || !proc_ctrl_entry) {
        pr_err("Failed to create /proc entries\n");
        return -ENOMEM;
    }

    pr_info("pci_bar_dump v%s loaded. Configure with /proc/%s\n", DRIVER_VERSION, PROC_CTRL_NAME);
    return 0;
}

static void __exit pci_bar_dump_exit(void)
{
    proc_remove(proc_dump_entry);
    proc_remove(proc_ctrl_entry);
    cleanup_mapping();
    pr_info("pci_bar_dump unloaded.\n");
}

module_init(pci_bar_dump_init);
module_exit(pci_bar_dump_exit);
