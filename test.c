#include <stdio.h>
int __func() {
    printf("In __func()\n");
    return 0;
}
int func() __attribute__((alias("__func")));
int main() {
    func();
    return 0;
}
