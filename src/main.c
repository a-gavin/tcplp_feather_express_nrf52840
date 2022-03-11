/**
 * @brief TCPlp application
 * 
 * Copyright (c) Alex Gavin, Brandon Lavinsky.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <sys/printk.h>

int main() {
    while (1) {
        printk("Spinning\n");
    }
}