#include <linux/module.h>      /* Needed for all kernel modules */
#include <linux/kernel.h>      /* Needed for printk() */
#include <linux/init.h>        /* Needed for __init and __exit macros */
#include <linux/proc_fs.h>     /* Needed for procfs functions */
#include <linux/uaccess.h>     /* Needed for copy_from_user() */
#include <linux/gpio.h>        /* Needed for GPIO functions */
#include <linux/fs.h>          /* Needed for simple_read_from_buffer() */
#include <linux/string.h>      /* Needed for sysfs_streq() */


#define PROC_FILE_NAME "rpi_led"
#define LED_GPIO_PIN 17  /* GPIO pin number for the LED */
#define USER_BUFFER_SIZE 128u

static struct proc_dir_entry *proc_file;
static int led_state = 0;

char kernel_buffer[USER_BUFFER_SIZE]; /* Create a kernel-space buffer to store the text message before sending it to user space */


static ssize_t proc_led_read(struct file *file, char __user *user_buffer, size_t count, loff_t *ppos);
static ssize_t proc_led_write(struct file *file, const char __user *user_buffer, size_t count, loff_t *ppos);

static const struct proc_ops proc_led_ops = {
    .proc_read = proc_led_read,
    .proc_write = proc_led_write,
};

static ssize_t proc_led_read(struct file *file, char __user *user_buffer, size_t count, loff_t *ppos) /* This function is called when user reads from /proc/rpi_led */
{
    
    int len; /* Store the total length of the prepared message */

    size_t bytes_to_copy; /* Store how many bytes we will copy to user space in this read call */

    led_state = gpio_get_value(LED_GPIO_PIN); /* Read the current value of the GPIO pin and store it in led_state */

    len = snprintf(kernel_buffer, /* Write the formatted message into kernel_buffer */
                   sizeof(kernel_buffer), /* Limit the write size to avoid buffer overflow */
                   "GPIO%d LED state: %s\nAvailable commands: 1, 0, on, off, toggle\n", /* Message format that will be shown to the user */
                   LED_GPIO_PIN, /* Replace %d with the GPIO pin number */
                   led_state ? "ON" : "OFF"); /* Replace %s with ON if led_state is 1, otherwise OFF */

    if (*ppos >= len) /* Check if the file offset reached the end of the message */
    {
        return 0; /* Return 0 to tell user space there is no more data to read */
    }

    bytes_to_copy = len - *ppos; /* Calculate the remaining bytes that were not read yet */

    if (bytes_to_copy > count) /* Check if remaining data is bigger than the user buffer size */
    {
        bytes_to_copy = count; /* Copy only the amount requested by user space */
    }

    if (copy_to_user(user_buffer, kernel_buffer + *ppos, bytes_to_copy) != 0) /* Copy data safely from kernel_buffer to user_buffer */
    {
        return -EFAULT; /* Return error if copying data to user space failed */
    }

    *ppos += bytes_to_copy; /* Update the file offset by the number of bytes copied */

    return bytes_to_copy; /* Return the number of bytes successfully copied to user space */
}

static ssize_t proc_led_write(struct file *file, const char __user *user_buffer, size_t count, loff_t *ppos) /* This function is called when user writes to /proc/rpi_led */
{
    char command; /* Store only one command character from user space */

    if (count == 0) /* Check if user did not send any data */
    {
        return 0; /* Return 0 because there is nothing to handle */
    }

    if (copy_from_user(&command, user_buffer, 1) != 0) /* Copy only one byte from user space to kernel space */
    {
        return -EFAULT; /* Return error if copying from user space failed */
    }

    if (command == '1') /* Check if user wants to turn LED ON */
    {
        led_state = 1; /* Save LED state as ON */

        gpio_set_value(LED_GPIO_PIN, led_state); /* Set GPIO pin to HIGH */
    }
    else if (command == '0') /* Check if user wants to turn LED OFF */
    {
        led_state = 0; /* Save LED state as OFF */

        gpio_set_value(LED_GPIO_PIN, led_state); /* Set GPIO pin to LOW */
    }
    else if (command == 't') /* Check if user wants to toggle LED */
    {
        led_state = gpio_get_value(LED_GPIO_PIN); /* Read current GPIO value */

        led_state = !led_state; /* Invert current LED state */

        gpio_set_value(LED_GPIO_PIN, led_state); /* Write new LED state to GPIO pin */
    }
    else /* Handle unsupported command */
    {
        return -EINVAL; /* Return invalid argument error */
    }

    return count; /* Return number of bytes received from user space */
}

static int __init proc_led_init(void) /* This function runs when the module is loaded */
{
    int ret; /* Store return values from GPIO functions */

    printk(KERN_INFO "proc_led: module loading\n"); /* Print a kernel log message */

    ret = gpio_request(LED_GPIO_PIN, "proc_led_gpio"); /* Request ownership of the GPIO pin */
    if (ret != 0) /* Check if GPIO request failed */
    {
        printk(KERN_ERR "proc_led: failed to request GPIO%d\n", LED_GPIO_PIN); /* Print error message */
        return ret; /* Stop module loading and return the error */
    }

    ret = gpio_direction_output(LED_GPIO_PIN, 0); /* Set GPIO pin as output and make initial value LOW */
    if (ret != 0) /* Check if setting GPIO direction failed */
    {
        printk(KERN_ERR "proc_led: failed to set GPIO%d as output\n", LED_GPIO_PIN); /* Print error message */
        gpio_free(LED_GPIO_PIN); /* Free the GPIO because request succeeded before */
        return ret; /* Stop module loading and return the error */
    }

    led_state = 0; /* Save initial LED state as OFF */

    proc_file = proc_create(PROC_FILE_NAME, 0666, NULL, &proc_led_ops); /* Create /proc/rpi_led and connect it to proc_led_ops */
    if (proc_file == NULL) /* Check if proc file creation failed */
    {
        printk(KERN_ERR "proc_led: failed to create /proc/%s\n", PROC_FILE_NAME); /* Print error message */
        gpio_set_value(LED_GPIO_PIN, 0); /* Turn LED OFF before cleanup */
        gpio_free(LED_GPIO_PIN); /* Free the GPIO before leaving */
        return -ENOMEM; /* Return memory allocation error */
    }

    printk(KERN_INFO "proc_led: /proc/%s created successfully\n", PROC_FILE_NAME); /* Print success message */
    printk(KERN_INFO "proc_led: using GPIO%d\n", LED_GPIO_PIN); /* Print used GPIO pin number */

    return 0; /* Return 0 to tell kernel that module loaded successfully */
}

static void __exit proc_led_exit(void) /* This function runs when the module is removed */
{
    remove_proc_entry(PROC_FILE_NAME, NULL); /* Remove /proc/rpi_led from procfs */

    gpio_set_value(LED_GPIO_PIN, 0); /* Turn LED OFF before removing the module */

    gpio_free(LED_GPIO_PIN); /* Release the GPIO pin so other drivers can use it */

    printk(KERN_INFO "proc_led: module removed\n"); /* Print kernel log message */
}

module_init(proc_led_init); /* Tell the kernel to call proc_led_init when module is loaded */

module_exit(proc_led_exit); /* Tell the kernel to call proc_led_exit when module is removed */


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ayman");
MODULE_DESCRIPTION("Raspberry Pi LED control using procfs");
MODULE_VERSION("1.0");