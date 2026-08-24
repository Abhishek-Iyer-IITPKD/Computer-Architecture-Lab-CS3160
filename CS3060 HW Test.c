//gcc -Wall CS3060\ HW\ Test.c -o CS3060\ HW\ Test.o && ./CS3060\ HW\ Test.o

#include <stdio.h>
#include <limits.h>
#include <unistd.h>
#include <time.h>

int main(){
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    double sum = 1;
    long LOOP_END = LONG_MAX;
    for(long i=0;i<LOOP_END;i++)
        sum = (sum+i)/sum;
    clock_gettime(CLOCK_MONOTONIC, &end);
    double diff;
    diff = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec)/1e9;
    printf("Sum is %lf. Loop end is %ld. Time taken (in seconds) is %lf. \n", sum, LOOP_END, diff);
}