# Linux Device Driver - Session 2 Quick README

> Quick-study notes for Device Driver Session 2.  
> Focus: `/proc`, VFS, and real Character Device Drivers under `/dev`.

---

## 1. Big Picture

Linux does not access all hardware directly from user space.

A **Device Driver** is kernel code that translates user requests into hardware actions.

```text
User Space
   |
   | open / read / write
   v
Linux Kernel
   |
   v
Device Driver
   |
   v
Hardware
```

Example:

```bash
echo 1 > /dev/myled
cat /dev/mysensor
```

The user sees files, but Linux calls driver functions inside the kernel.

---

## 2. Linux Device Types

```text
Linux Devices
├── Character Devices
│   ├── GPIO
│   ├── UART
│   ├── I2C
│   ├── SPI
│   └── LEDs / Sensors
│
├── Block Devices
│   ├── HDD
│   ├── SSD
│   └── SD Card
│
└── Network Devices
    ├── eth0
    ├── wlan0
    └── can0
```

### Character Device

A character device handles data as a stream or simple byte-based access.

Examples:

```text
/dev/ttyUSB0
/dev/i2c-1
/dev/spidev0.0
/dev/mygpio
```

This session focuses on **Character Device Drivers**.

---

## 3. `/proc`, `/dev`, and `/sys`

```text
/
├── proc  -> procfs   -> virtual kernel files
├── dev   -> devtmpfs -> device nodes
└── sys   -> sysfs    -> device / driver information
```

### `/proc`

`/proc` is a pseudo filesystem.

It is not stored on disk. The kernel generates the content when the user reads it.

Examples:

```bash
cat /proc/cpuinfo
cat /proc/meminfo
```

In the first simple driver, we used:

```text
/proc/led_toggle
```

This is good for learning and debugging.

---

### `/dev`

`/dev` contains device nodes.

Examples:

```text
/dev/mygpio
/dev/ttyUSB0
/dev/i2c-1
```

When the user writes to a device node:

```bash
echo 1 > /dev/mygpio
```

Linux calls the driver's `write()` function.

---

### `/sys`

`/sys` exposes kernel information about devices, drivers, buses, and classes.

Examples:

```bash
ls /sys/class
ls /sys/devices
```

---

## 4. VFS - Virtual File System

Linux uses **VFS** to make different file systems look similar to user space.

```text
User Space
   |
   | open / read / write / close
   v
+-------------------+
|        VFS        |
+-------------------+
   |       |       |
   v       v       v
 procfs  sysfs  devtmpfs
 /proc   /sys   /dev
```

So commands look similar:

```bash
cat file
echo data > file
```

But internally the target may be:

```text
/proc -> kernel-generated information
/dev  -> device driver interface
/sys  -> kernel object information
```

---

## 5. procfs Flow

To create a file under `/proc`, we can use:

```c
proc_create("gpio", 0666, NULL, &proc_ops);
```

This creates:

```text
/proc/gpio
```

Then the user can do:

```bash
echo 1 > /proc/gpio
cat /proc/gpio
```

### Flow

```text
User Space
   |
   | echo 1 > /proc/gpio
   v
/proc/gpio
   |
   v
VFS
   |
   v
procfs
   |
   v
driver write callback
   |
   v
kernel code executes
```

---

## 6. `proc_ops`

For procfs, we use `struct proc_ops`.

```c
static const struct proc_ops my_proc_ops = {
    .proc_open    = my_open,
    .proc_read    = my_read,
    .proc_write   = my_write,
    .proc_release = my_release,
};
```

Meaning:

```text
open /proc/gpio    -> my_open()
cat /proc/gpio     -> my_read()
echo > /proc/gpio  -> my_write()
close file         -> my_release()
```

---

## 7. From `/proc` to Real Character Device

`/proc` is useful for simple kernel communication.

But the real character device style is:

```text
/dev/mydevice
```

Example:

```bash
echo 1 > /dev/myled
cat /dev/myled
```

To build this, we need:

```text
1. Allocate device number
2. Create cdev object
3. Connect cdev with file_operations
4. Add cdev to the kernel
5. Create /dev node
```

---

## 8. Major and Minor Numbers

Every device node under `/dev` has two numbers:

```text
Major Number -> identifies the driver
Minor Number -> identifies the device instance
```

Example:

```text
/dev/led0 -> major 506, minor 0
/dev/led1 -> major 506, minor 1
/dev/led2 -> major 506, minor 2
```

All devices may use the same driver, but each one has a different minor number.

```text
Major = Driver
Minor = Instance
```

---

## 9. `dev_t`

Linux stores the device number inside a type called:

```c
dev_t
```

`dev_t` contains both major and minor numbers.

```text
dev_t
├── Major Number
└── Minor Number
```

Useful macros:

```c
MAJOR(dev);
MINOR(dev);
MKDEV(major, minor);
```

Example:

```c
printk("Major = %d\n", MAJOR(dev));
printk("Minor = %d\n", MINOR(dev));
```

---

## 10. Allocate Character Device Region

First, ask the kernel to allocate a free major/minor range.

```c
dev_t dev;

alloc_chrdev_region(&dev, 0, 1, "mygpio");
```

Meaning:

```text
&dev     -> kernel stores the device number here
0        -> base minor number
1        -> number of devices
"mygpio" -> device/driver name
```

After this, the kernel gives us a major and minor number.

---

## 11. `cdev` and `file_operations`

A `cdev` represents a character device inside the kernel.

```c
struct cdev my_cdev;
```

The `file_operations` structure contains the driver's callbacks.

```c
static const struct file_operations fops = {
    .owner   = THIS_MODULE,
    .open    = my_open,
    .read    = my_read,
    .write   = my_write,
    .release = my_release,
};
```

Meaning:

```text
open()    -> called when user opens /dev/mygpio
read()    -> called when user reads from /dev/mygpio
write()   -> called when user writes to /dev/mygpio
release() -> called when user closes /dev/mygpio
```

---

## 12. `cdev_init()`

`cdev_init()` connects the `cdev` object with the `file_operations`.

```c
cdev_init(&my_cdev, &fops);
```

Meaning:

```text
This character device will use these open/read/write/release functions.
```

Diagram:

```text
struct cdev my_cdev
        |
        v
struct file_operations fops
        |
        ├── my_open()
        ├── my_read()
        ├── my_write()
        └── my_release()
```

---

## 13. `cdev_add()`

`cdev_add()` registers the character device inside the kernel.

```c
cdev_add(&my_cdev, dev, 1);
```

Meaning:

```text
Register my_cdev with the allocated major/minor number.
```

Now the kernel knows:

```text
If this device number is opened,
call the file_operations connected to my_cdev.
```

Important difference:

```text
cdev_init() -> preparation
cdev_add()  -> registration
```

---

## 14. Creating `/dev` Node Manually

After `cdev_add()`, the kernel knows the device.

But user space still needs a file under `/dev`.

Manual way:

```bash
sudo mknod /dev/mygpio c <major> <minor>
```

Example:

```bash
sudo mknod /dev/mygpio c 506 0
```

Meaning:

```text
mknod       -> create device node
/dev/mygpio -> node name
c           -> character device
506         -> major number
0           -> minor number
```

Check it:

```bash
ls -l /dev/mygpio
```

Expected style:

```text
crw-r--r-- 1 root root 506, 0 /dev/mygpio
```

The first letter `c` means character device.

---

## 15. Creating `/dev` Node Automatically

Instead of using `mknod`, the driver can create the node automatically.

We use:

```c
class_create();
device_create();
```

### `class_create()`

Creates a class under `/sys/class`.

```c
my_class = class_create("my_class");
```

Result:

```text
/sys/class/my_class
```

---

### `device_create()`

Creates a device node under `/dev`.

```c
device_create(my_class, NULL, dev, NULL, "mygpio");
```

Result:

```text
/dev/mygpio
```

---

## 16. Full Character Driver Init Flow

```text
sudo insmod mydriver.ko
        |
        v
module_init()
        |
        v
alloc_chrdev_region()
        |
        v
Kernel allocates major/minor
        |
        v
cdev_init()
        |
        v
Connect cdev with file_operations
        |
        v
cdev_add()
        |
        v
Register character device in kernel
        |
        v
class_create()
        |
        v
Create class under /sys/class
        |
        v
device_create()
        |
        v
Create /dev/mygpio
        |
        v
User can use open/read/write
```

---

## 17. Full Character Driver Exit Flow

When removing the module, destroy everything in reverse order.

```text
sudo rmmod mydriver
        |
        v
module_exit()
        |
        v
device_destroy()
        |
        v
class_destroy()
        |
        v
cdev_del()
        |
        v
unregister_chrdev_region()
```

Code order:

```c
device_destroy(my_class, dev);
class_destroy(my_class);
cdev_del(&my_cdev);
unregister_chrdev_region(dev, 1);
```

---

## 18. User Buffer in `write()`

When the user writes:

```bash
echo 1 > /dev/mygpio
```

The data comes from user-space memory.

The driver receives a user pointer:

```c
const char __user *user_buffer
```

The kernel must not access this pointer directly.

Wrong idea:

```c
char value = user_buffer[0];
```

Correct idea:

```c
copy_from_user(kernel_buffer, user_buffer, size);
```

### Write Flow

```text
User Space Buffer
        |
        | copy_from_user()
        v
Kernel Buffer
        |
        v
Driver handles data
        |
        v
Hardware action
```

---

## 19. User Buffer in `read()`

When the user reads:

```bash
cat /dev/mygpio
```

The driver must send data from kernel space to user space.

Use:

```c
copy_to_user(user_buffer, kernel_buffer, size);
```

### Read Flow

```text
Kernel Buffer
        |
        | copy_to_user()
        v
User Space Buffer
        |
        v
cat prints the data
```

For procfs or character devices, `*offset` or `*ppos` is important to avoid infinite repeated reads.

Common pattern:

```c
if (*offset > 0)
    return 0;
```

Meaning:

```text
If the user already read once, return EOF.
```

---

## 20. Final Example: LED Character Device

### Goal

Create a character device:

```text
/dev/myled
```

User commands:

```bash
echo 1 > /dev/myled   # LED ON
echo 0 > /dev/myled   # LED OFF
cat /dev/myled        # show LED state
```

---

### Driver Concept

```text
User Space
   |
   | echo 1 > /dev/myled
   v
/dev/myled
   |
   v
VFS
   |
   v
Character Device Layer
   |
   v
file_operations.write()
   |
   v
copy_from_user()
   |
   v
Parse command
   |
   v
Set GPIO
   |
   v
LED ON
```

---

### Minimal Driver Skeleton

```c
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>

static dev_t dev_num;
static struct cdev my_cdev;
static struct class *my_class;
static int led_state;

static int my_open(struct inode *inode, struct file *file)
{
    return 0;
}

static int my_release(struct inode *inode, struct file *file)
{
    return 0;
}

static ssize_t my_write(struct file *file, const char __user *user_buffer, size_t size, loff_t *offset)
{
    char kernel_buffer[16];

    if (size > sizeof(kernel_buffer) - 1)
        size = sizeof(kernel_buffer) - 1;

    if (copy_from_user(kernel_buffer, user_buffer, size))
        return -EFAULT;

    kernel_buffer[size] = '\0';

    if (kernel_buffer[0] == '1')
        led_state = 1;
    else if (kernel_buffer[0] == '0')
        led_state = 0;

    return size;
}

static ssize_t my_read(struct file *file, char __user *user_buffer, size_t size, loff_t *offset)
{
    char kernel_buffer[32];
    int len;

    if (*offset > 0)
        return 0;

    len = snprintf(kernel_buffer, sizeof(kernel_buffer), "LED state = %d\n", led_state);

    if (copy_to_user(user_buffer, kernel_buffer, len))
        return -EFAULT;

    *offset += len;

    return len;
}

static const struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = my_open,
    .read = my_read,
    .write = my_write,
    .release = my_release,
};

static int __init my_driver_init(void)
{
    alloc_chrdev_region(&dev_num, 0, 1, "myled");

    cdev_init(&my_cdev, &fops);

    cdev_add(&my_cdev, dev_num, 1);

    my_class = class_create("myled_class");

    device_create(my_class, NULL, dev_num, NULL, "myled");

    return 0;
}

static void __exit my_driver_exit(void)
{
    device_destroy(my_class, dev_num);

    class_destroy(my_class);

    cdev_del(&my_cdev);

    unregister_chrdev_region(dev_num, 1);
}

module_init(my_driver_init);
module_exit(my_driver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ayman Abohamed");
MODULE_DESCRIPTION("Simple Character Device Driver Example");
```

---

## 21. Commands to Test

Build and insert the module:

```bash
make
sudo insmod mydriver.ko
```

Check module:

```bash
lsmod | grep mydriver
```

Check device node:

```bash
ls -l /dev/myled
```

Write:

```bash
echo 1 | sudo tee /dev/myled
echo 0 | sudo tee /dev/myled
```

Read:

```bash
cat /dev/myled
```

Remove module:

```bash
sudo rmmod mydriver
```

Check kernel logs:

```bash
dmesg | tail -30
```

---

## 22. Quick Revision

```text
/proc
    Simple pseudo file interface
    Created by proc_create()
    Uses struct proc_ops

/dev
    Real device node interface
    Uses major/minor numbers
    Uses struct file_operations
    Uses struct cdev

/sys
    Kernel object information
    class_create() creates entry under /sys/class

VFS
    Common layer for open/read/write

dev_t
    Holds major and minor numbers

major
    Identifies driver

minor
    Identifies instance

cdev_init()
    Connects cdev with file_operations

cdev_add()
    Registers cdev with kernel

device_create()
    Creates /dev node automatically

copy_from_user()
    Safely copies data from user space to kernel space

copy_to_user()
    Safely copies data from kernel space to user space
```

---

## 23. One-Line Memory Map

```text
User command
 -> /dev node
 -> VFS
 -> major/minor
 -> cdev
 -> file_operations
 -> driver callback
 -> copy_from_user / copy_to_user
 -> hardware action
```

---

## 24. Most Important Questions

### Q1: What is the difference between `/proc` and `/dev`?

`/proc` is a pseudo filesystem used to expose kernel information or simple control files.  
`/dev` contains device nodes used to communicate with real devices through drivers.

### Q2: What is VFS?

VFS is a Linux kernel layer that provides one common interface for different file systems and device files.

### Q3: What is the major number?

The major number identifies the driver.

### Q4: What is the minor number?

The minor number identifies a specific device instance handled by the same driver.

### Q5: Why do we use `copy_from_user()`?

Because kernel space must not directly access user-space memory.

### Q6: Why do we use `copy_to_user()`?

Because data returned from kernel space must be safely copied to user-space memory.

### Q7: What is `cdev_init()`?

It connects a `struct cdev` with `struct file_operations`.

### Q8: What is `cdev_add()`?

It registers the character device inside the kernel.

### Q9: What does `device_create()` do?

It creates the device node under `/dev` automatically.

### Q10: What is the correct cleanup order?

Reverse the init order:

```text
device_destroy()
class_destroy()
cdev_del()
unregister_chrdev_region()
```
