# Raspberry Pi 3 Procfs LED Kernel Module

## 1. Task Overview

This task creates a simple Linux Kernel Module for Raspberry Pi 3.

The module creates a virtual file under `/proc`:

```bash
/proc/rpi_led
```

This file is used to control an LED connected to a Raspberry Pi GPIO pin.

The user can control the LED from the terminal using simple commands:

```bash
cat /proc/rpi_led
echo 1 > /proc/rpi_led
echo 0 > /proc/rpi_led
echo t > /proc/rpi_led
```

---

## 2. Main Goal

The goal is to understand how user space can communicate with kernel space using `procfs`.

Instead of controlling the LED directly from a normal user application, we create a kernel module that exposes a `/proc` file.

```text
User Space
------------------------------------------------
cat /proc/rpi_led
echo 1 > /proc/rpi_led
echo 0 > /proc/rpi_led
echo t > /proc/rpi_led

        |
        v

Kernel Space
------------------------------------------------
/proc/rpi_led
Kernel Module
GPIO Driver API

        |
        v

Hardware
------------------------------------------------
Raspberry Pi GPIO27
LED
```

---

## 3. Hardware Connection

The LED is connected to Raspberry Pi GPIO27.

```text
BCM GPIO27 = Physical Pin 13
GND        = Physical Pin 6 or 9 or 14
```

Connection:

```text
Raspberry Pi 3

GPIO27 / Pin 13 ---- Resistor 220Ω/330Ω ---- LED Anode (+)
LED Cathode (-) ----------------------------- GND
```

Diagram:

```text
+----------------------+
| Raspberry Pi 3       |
|                      |
| Pin 13 / GPIO27 -----+----[220Ω]---->|---- GND
|                      |              LED
| Pin 6 / GND ---------+
+----------------------+
```

---

## 4. Project Structure

The project contains two main files:

```text
rpi_proc_led_driver/
├── proc_led.c
└── Makefile
```

### `proc_led.c`

This file contains the kernel module source code.

It includes:

```text
- Kernel headers
- GPIO definitions
- procfs read function
- procfs write function
- proc_ops structure
- module init function
- module exit function
- module metadata
```

### `Makefile`

This file tells the Linux kernel build system to build `proc_led.c` as a loadable kernel module.

The output file will be:

```text
proc_led.ko
```

---

## 5. Full Software Flow

The complete flow is:

```text
PC
 |
 | Write proc_led.c and Makefile
 v

Copy project to Raspberry Pi
 |
 | scp -r rpi_proc_led_driver user@rpi_ip:/home/user/
 v

Raspberry Pi
 |
 | make
 v

proc_led.ko
 |
 | sudo insmod proc_led.ko
 v

Kernel Module Loaded
 |
 | Creates /proc/rpi_led
 | Requests GPIO27
 | Sets GPIO27 as output
 v

User controls LED from terminal
```

---

## 6. Runtime Flow

After loading the module, the user interacts with:

```bash
/proc/rpi_led
```

### Read Flow

Command:

```bash
cat /proc/rpi_led
```

Flow:

```text
cat /proc/rpi_led
        |
        v
read() system call
        |
        v
/proc/rpi_led
        |
        v
proc_led_read()
        |
        v
Read GPIO27 state
        |
        v
Prepare status message
        |
        v
copy_to_user()
        |
        v
User sees LED state
```

Expected output example:

```text
GPIO27 LED state: ON
Available commands: 1, 0, t
```

---

### Write Flow

Command:

```bash
echo 1 > /proc/rpi_led
```

Flow:

```text
echo 1 > /proc/rpi_led
        |
        v
write() system call
        |
        v
/proc/rpi_led
        |
        v
proc_led_write()
        |
        v
copy_from_user()
        |
        v
Check command
        |
        v
Control GPIO27
        |
        v
LED changes state
```

Supported commands:

```text
1 -> Turn LED ON
0 -> Turn LED OFF
t -> Toggle LED
```

---

## 7. Main Kernel Concepts Used

### 7.1 `procfs`

`procfs` is a virtual filesystem in Linux.

It is mounted at:

```bash
/proc
```

Files under `/proc` are usually not real files stored on disk.

They are virtual files created by the kernel.

In this task, our module creates:

```bash
/proc/rpi_led
```

This file is used as an interface between user space and kernel space.

---

### 7.2 `proc_create()`

`proc_create()` is used to create a file under `/proc`.

In this task, it creates:

```bash
/proc/rpi_led
```

Conceptually:

```text
proc_create()
    |
    +--> File name: rpi_led
    |
    +--> Permissions
    |
    +--> Parent directory: /proc
    |
    +--> Operations: read and write callbacks
```

---

### 7.3 `struct proc_ops`

`struct proc_ops` connects the `/proc` file to callback functions.

```text
/proc/rpi_led
      |
      +---- read  ---> proc_led_read()
      |
      +---- write ---> proc_led_write()
```

When the user runs:

```bash
cat /proc/rpi_led
```

The kernel calls:

```text
proc_led_read()
```

When the user runs:

```bash
echo 1 > /proc/rpi_led
```

The kernel calls:

```text
proc_led_write()
```

---

### 7.4 `copy_to_user()`

`copy_to_user()` copies data from kernel space to user space.

It is used in the read function.

```text
Kernel Space                         User Space
------------------------------------------------
kernel_buffer   ----copy_to_user---> user_buffer
```

Used when the kernel wants to send data to the user.

Example use case:

```text
Kernel prepares:
"GPIO27 LED state: ON"

copy_to_user sends it to:
cat /proc/rpi_led
```

---

### 7.5 `copy_from_user()`

`copy_from_user()` copies data from user space to kernel space.

It is used in the write function.

```text
User Space                           Kernel Space
------------------------------------------------
user_buffer   ----copy_from_user---> kernel_buffer
```

Used when the user sends a command to the kernel.

Example:

```bash
echo 1 > /proc/rpi_led
```

The kernel receives:

```text
'1'
```

Then the module turns the LED ON.

---

### 7.6 GPIO Kernel API

The module uses the Linux GPIO API.

Important GPIO operations:

```text
gpio_request()             -> Reserve the GPIO pin
gpio_direction_output()    -> Set GPIO as output
gpio_set_value()           -> Write 0 or 1 to GPIO
gpio_get_value()           -> Read GPIO current value
gpio_free()                -> Release GPIO when module exits
```

---

## 8. Module Load Flow

When the module is loaded:

```bash
sudo insmod proc_led.ko
```

The kernel calls the module init function.

Init flow:

```text
proc_led_init()
        |
        v
Print loading message
        |
        v
Request GPIO27
        |
        v
Set GPIO27 as output
        |
        v
Set LED initial state to OFF
        |
        v
Create /proc/rpi_led
        |
        v
Module ready
```

If any step fails, the module must clean up and return an error.

Example:

```text
GPIO request success
GPIO direction success
proc_create failed
        |
        v
Free GPIO
Return error
```

---

## 9. Module Exit Flow

When the module is removed:

```bash
sudo rmmod proc_led
```

The kernel calls the module exit function.

Exit flow:

```text
proc_led_exit()
        |
        v
Remove /proc/rpi_led
        |
        v
Turn LED OFF
        |
        v
Free GPIO27
        |
        v
Print removed message
```

This cleanup is important because the module must not leave the GPIO reserved after removal.

---

## 10. Build Flow on Raspberry Pi

The module should be built on the Raspberry Pi itself because kernel modules must match the running kernel version.

Check kernel version:

```bash
uname -r
```

Install needed tools:

```bash
sudo apt update
sudo apt install -y build-essential raspberrypi-kernel-headers
```

Build the module:

```bash
cd rpi_proc_led_driver
make
```

Expected output file:

```text
proc_led.ko
```

Clean build files:

```bash
make clean
```

---

## 11. Load and Test

Load the module:

```bash
sudo insmod proc_led.ko
```

Check kernel logs:

```bash
dmesg | tail
```

Check that the proc file exists:

```bash
ls -l /proc/rpi_led
```

Read LED state:

```bash
cat /proc/rpi_led
```

Turn LED ON:

```bash
echo 1 | sudo tee /proc/rpi_led
```

Turn LED OFF:

```bash
echo 0 | sudo tee /proc/rpi_led
```

Toggle LED:

```bash
echo t | sudo tee /proc/rpi_led
```

Remove the module:

```bash
sudo rmmod proc_led
```

Check logs again:

```bash
dmesg | tail
```

---

## 12. Why Use `tee` Instead of Redirection?

This command may fail:

```bash
sudo echo 1 > /proc/rpi_led
```

Because `sudo` applies to `echo`, but the redirection `>` is handled by the current shell, not by `sudo`.

Better command:

```bash
echo 1 | sudo tee /proc/rpi_led
```

This writes to `/proc/rpi_led` with root permission.

---

## 13. Expected Commands Summary

```bash
# Build
make

# Load module
sudo insmod proc_led.ko

# Check logs
dmesg | tail

# Read LED state
cat /proc/rpi_led

# Turn LED ON
echo 1 | sudo tee /proc/rpi_led

# Turn LED OFF
echo 0 | sudo tee /proc/rpi_led

# Toggle LED
echo t | sudo tee /proc/rpi_led

# Remove module
sudo rmmod proc_led

# Clean build files
make clean
```

---

## 14. Important Notes

* This is a learning task for understanding `procfs`.
* `/proc/rpi_led` is not a real file stored on disk.
* It is created dynamically by the kernel module.
* `cat /proc/rpi_led` triggers the read callback.
* `echo 1 > /proc/rpi_led` triggers the write callback.
* The module must be compiled on the same Raspberry Pi kernel where it will run.
* Always remove the module using `rmmod` before editing and rebuilding.

---

## 15. Final System Diagram

```text
+--------------------------+
|        User Space        |
|--------------------------|
| cat /proc/rpi_led        |
| echo 1 > /proc/rpi_led   |
| echo 0 > /proc/rpi_led   |
| echo t > /proc/rpi_led   |
+------------+-------------+
             |
             v
+--------------------------+
|         procfs           |
|--------------------------|
|      /proc/rpi_led       |
+------------+-------------+
             |
             v
+--------------------------+
|      Kernel Module       |
|--------------------------|
| proc_led_read()          |
| proc_led_write()         |
| proc_led_init()          |
| proc_led_exit()          |
+------------+-------------+
             |
             v
+--------------------------+
|       GPIO Kernel API    |
|--------------------------|
| gpio_request()           |
| gpio_set_value()         |
| gpio_get_value()         |
| gpio_free()              |
+------------+-------------+
             |
             v
+--------------------------+
|        Hardware          |
|--------------------------|
| Raspberry Pi GPIO27      |
| LED                      |
+--------------------------+
```

---

## 16. Task Summary

In this task, we created a simple Raspberry Pi kernel module that controls an LED using `procfs`.

The module creates:

```bash
/proc/rpi_led
```

Reading from this file shows the LED state.

Writing to this file controls the LED.

Main learning points:

```text
- Kernel module lifecycle
- procfs interface
- read and write callbacks
- copy_to_user()
- copy_from_user()
- GPIO control from kernel space
- Kernel build system using Makefile
```
